from __future__ import annotations

import os
import sys
from collections import OrderedDict
from pathlib import Path
from typing import Dict, List, Tuple

from PySide6 import QtCore, QtGui, QtWidgets


IMAGE_EXTENSIONS = {
    ".jpg",
    ".jpeg",
    ".png",
    ".bmp",
    ".gif",
    ".tif",
    ".tiff",
    ".webp",
}


class PixmapCache:
    """A small LRU cache to keep recently used pixmaps in memory."""

    def __init__(self, capacity: int):
        self.capacity = capacity
        self._store: OrderedDict[str | Tuple[str, int], QtGui.QPixmap] = OrderedDict()

    def get(self, key):
        pixmap = self._store.get(key)
        if pixmap is not None:
            self._store.move_to_end(key)
        return pixmap

    def put(self, key, pixmap: QtGui.QPixmap):
        self._store[key] = pixmap
        self._store.move_to_end(key)
        if len(self._store) > self.capacity:
            self._store.popitem(last=False)


class ThumbnailSignals(QtCore.QObject):
    finished = QtCore.Signal(str, QtGui.QPixmap)


class ThumbnailWorker(QtCore.QRunnable):
    """Loads a thumbnail without blocking the UI thread."""

    def __init__(
        self,
        path: Path,
        target: int,
        cache: PixmapCache,
        signals: ThumbnailSignals,
    ):
        super().__init__()
        self.path = path
        self.target = target
        self.cache = cache
        self.signals = signals

    @QtCore.Slot()
    def run(self):
        key = (str(self.path), self.target)
        cached = self.cache.get(key)
        if cached is not None:
            self.signals.finished.emit(str(self.path), cached)
            return

        # Quick low-res pass to get something on screen fast
        quick_edge = min(self.target // 2, 96)
        if quick_edge >= 32:
            quick_key = (str(self.path), quick_edge)
            quick_cached = self.cache.get(quick_key)
            if quick_cached:
                self.signals.finished.emit(str(self.path), quick_cached)
            else:
                quick_reader = QtGui.QImageReader(str(self.path))
                quick_reader.setAutoTransform(True)
                source_size = quick_reader.size()
                if source_size.isValid():
                    quick_size = source_size.scaled(
                        QtCore.QSize(quick_edge, quick_edge),
                        QtCore.Qt.KeepAspectRatio,
                    )
                    quick_reader.setScaledSize(quick_size)
                quick_image = quick_reader.read()
                if not quick_image.isNull():
                    quick_pix = QtGui.QPixmap.fromImage(quick_image)
                    if quick_pix.width() > quick_edge or quick_pix.height() > quick_edge:
                        quick_pix = quick_pix.scaled(
                            quick_edge,
                            quick_edge,
                            QtCore.Qt.KeepAspectRatio,
                            QtCore.Qt.FastTransformation,
                        )
                    self.cache.put(quick_key, quick_pix)
                    self.signals.finished.emit(str(self.path), quick_pix)

        reader = QtGui.QImageReader(str(self.path))
        reader.setAutoTransform(True)

        source_size = reader.size()
        if source_size.isValid():
            target_size = source_size.scaled(
                QtCore.QSize(self.target, self.target),
                QtCore.Qt.KeepAspectRatio,
            )
            reader.setScaledSize(target_size)

        image = reader.read()
        if image.isNull():
            return

        pixmap = QtGui.QPixmap.fromImage(image)
        if pixmap.width() > self.target or pixmap.height() > self.target:
            pixmap = pixmap.scaled(
                self.target,
                self.target,
                QtCore.Qt.KeepAspectRatio,
                QtCore.Qt.SmoothTransformation,
            )

        self.cache.put(key, pixmap)
        self.signals.finished.emit(str(self.path), pixmap)


class ImageSignals(QtCore.QObject):
    finished = QtCore.Signal(str, QtGui.QPixmap)


class ImageWorker(QtCore.QRunnable):
    """Loads a full-resolution image off the UI thread."""

    def __init__(self, path: Path, cache: PixmapCache, signals: ImageSignals):
        super().__init__()
        self.path = path
        self.cache = cache
        self.signals = signals

    @QtCore.Slot()
    def run(self):
        key = str(self.path)
        cached = self.cache.get(key)
        if cached is not None:
            self.signals.finished.emit(str(self.path), cached)
            return

        reader = QtGui.QImageReader(str(self.path))
        reader.setAutoTransform(True)
        image = reader.read()
        if image.isNull():
            return

        pixmap = QtGui.QPixmap.fromImage(image)
        self.cache.put(key, pixmap)
        self.signals.finished.emit(str(self.path), pixmap)


class ThumbnailGrid(QtWidgets.QListWidget):
    imageSelected = QtCore.Signal(str)
    imageActivated = QtCore.Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.thread_pool = QtCore.QThreadPool.globalInstance()
        ideal = QtCore.QThread.idealThreadCount()
        if ideal > 0:
            self.thread_pool.setMaxThreadCount(max(ideal * 2, 6))
        else:
            self.thread_pool.setMaxThreadCount(max(6, self.thread_pool.maxThreadCount()))
        self.thumbnail_cache = PixmapCache(capacity=256)
        self.thumb_signals = ThumbnailSignals()
        self.thumb_signals.finished.connect(self._apply_thumbnail)

        self._target_size = 170
        self._paths: List[Path] = []
        self._items: Dict[str, QtWidgets.QListWidgetItem] = {}

        self.setViewMode(QtWidgets.QListView.IconMode)
        self.setResizeMode(QtWidgets.QListView.Adjust)
        self.setSelectionMode(QtWidgets.QAbstractItemView.SingleSelection)
        self.setMovement(QtWidgets.QListView.Static)
        self.setSpacing(12)
        self.setUniformItemSizes(True)
        self.setWordWrap(True)
        self.setContextMenuPolicy(QtCore.Qt.NoContextMenu)
        self.setHorizontalScrollMode(QtWidgets.QAbstractItemView.ScrollPerPixel)
        self.setVerticalScrollMode(QtWidgets.QAbstractItemView.ScrollPerPixel)
        self.setLayoutMode(QtWidgets.QListView.Batched)
        self.setBatchSize(128)
        QtWidgets.QScroller.grabGesture(
            self.viewport(), QtWidgets.QScroller.LeftMouseButtonGesture
        )
        self.setIconSize(QtCore.QSize(self._target_size, self._target_size))
        self.setGridSize(QtCore.QSize(self._target_size + 28, self._target_size + 36))
        self.itemClicked.connect(self._on_item_clicked)
        self.itemActivated.connect(self._on_item_activated)

    def set_thumbnail_size(self, size: int):
        self._target_size = size
        self.setIconSize(QtCore.QSize(size, size))
        self.setGridSize(QtCore.QSize(size + 28, size + 36))
        # Reload thumbnails at the new resolution
        if self._paths:
            current = self.currentItem().data(QtCore.Qt.UserRole) if self.currentItem() else None
            self.populate(self._paths)
            if current:
                self.select_by_path(current)

    def populate(self, paths: List[Path]):
        self.clear()
        self._paths = paths
        self._items.clear()
        placeholder = self._placeholder_icon()

        for path in paths:
            item = QtWidgets.QListWidgetItem(placeholder, path.name)
            item.setData(QtCore.Qt.UserRole, str(path))
            item.setToolTip(str(path))
            item.setSizeHint(QtCore.QSize(self._target_size + 28, self._target_size + 36))
            self.addItem(item)
            self._items[str(path)] = item
            self._load_thumbnail(path)

    def _placeholder_icon(self) -> QtGui.QIcon:
        size = self._target_size
        pixmap = QtGui.QPixmap(size, size)
        pixmap.fill(QtGui.QColor("#1f1f24"))

        painter = QtGui.QPainter(pixmap)
        painter.setRenderHint(QtGui.QPainter.Antialiasing)
        painter.setPen(QtGui.QPen(QtGui.QColor("#45454f"), 2))
        painter.setBrush(QtGui.QBrush(QtGui.QColor("#2b2b33")))
        radius = size * 0.18
        rect_size = size * 0.48
        center = size / 2
        painter.drawRoundedRect(
            QtCore.QRectF(
                center - rect_size / 2,
                center - rect_size / 2,
                rect_size,
                rect_size,
            ),
            radius,
            radius,
        )
        painter.end()
        return QtGui.QIcon(pixmap)

    def _load_thumbnail(self, path: Path):
        worker = ThumbnailWorker(path, self._target_size, self.thumbnail_cache, self.thumb_signals)
        worker.setAutoDelete(True)
        self.thread_pool.start(worker)

    @QtCore.Slot(str, QtGui.QPixmap)
    def _apply_thumbnail(self, path: str, pixmap: QtGui.QPixmap):
        item = self._items.get(path)
        if item:
            item.setIcon(QtGui.QIcon(pixmap))

    @QtCore.Slot(QtWidgets.QListWidgetItem)
    def _on_item_clicked(self, item: QtWidgets.QListWidgetItem):
        path = item.data(QtCore.Qt.UserRole)
        if path:
            self.imageSelected.emit(path)

    @QtCore.Slot(QtWidgets.QListWidgetItem)
    def _on_item_activated(self, item: QtWidgets.QListWidgetItem):
        path = item.data(QtCore.Qt.UserRole)
        if path:
            self.imageActivated.emit(path)

    def select_by_path(self, path: str):
        item = self._items.get(path)
        if item:
            blocker = QtCore.QSignalBlocker(self)
            self.setCurrentItem(item)
            self.scrollToItem(item, QtWidgets.QAbstractItemView.PositionAtCenter)
            del blocker


class ImageViewer(QtWidgets.QGraphicsView):
    stepRequested = QtCore.Signal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setScene(QtWidgets.QGraphicsScene(self))
        self.pixmap_item = QtWidgets.QGraphicsPixmapItem()
        self.scene().addItem(self.pixmap_item)

        self.setRenderHints(
            QtGui.QPainter.Antialiasing
            | QtGui.QPainter.SmoothPixmapTransform
            | QtGui.QPainter.TextAntialiasing
        )

        self.setDragMode(QtWidgets.QGraphicsView.ScrollHandDrag)
        self.setTransformationAnchor(QtWidgets.QGraphicsView.AnchorUnderMouse)
        self.setResizeAnchor(QtWidgets.QGraphicsView.AnchorViewCenter)
        self.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.setBackgroundBrush(QtGui.QColor("#0f1115"))
        self.setFocusPolicy(QtCore.Qt.StrongFocus)

        self._zoom_level = 0
        self._fit_mode = True
        self._current_path: str | None = None
        self._rotation = 0

    def set_pixmap(self, pixmap: QtGui.QPixmap, path: str | None = None):
        self._current_path = path
        self.pixmap_item.setPixmap(pixmap)
        self.pixmap_item.setTransformOriginPoint(pixmap.rect().center())
        self._rotation = 0
        self.pixmap_item.setRotation(0)
        self.resetTransform()
        self._zoom_level = 0
        self._fit_mode = True
        self.fit_to_view()

    def fit_to_view(self):
        if self.pixmap_item.pixmap().isNull():
            return
        self._fit_mode = True
        self.resetTransform()
        self._zoom_level = 0
        self.fitInView(self.pixmap_item, QtCore.Qt.KeepAspectRatio)

    def wheelEvent(self, event: QtGui.QWheelEvent):
        if self.pixmap_item.pixmap().isNull():
            return
        angle = event.angleDelta().y()
        if angle == 0:
            return

        factor = 1.15 if angle > 0 else 1 / 1.15
        self._fit_mode = False
        self._zoom_level += 1 if angle > 0 else -1
        self.scale(factor, factor)
        event.accept()

    def resizeEvent(self, event: QtGui.QResizeEvent):
        super().resizeEvent(event)
        if self._fit_mode:
            QtCore.QTimer.singleShot(0, self.fit_to_view)

    def keyPressEvent(self, event: QtGui.QKeyEvent):
        if event.key() == QtCore.Qt.Key_Space and self.pixmap_item.pixmap():
            self.fit_to_view()
            event.accept()
            return
        super().keyPressEvent(event)

    def current_path(self) -> str | None:
        return self._current_path

    def rotate(self, degrees: int):
        if self.pixmap_item.pixmap().isNull():
            return
        self._rotation = (self._rotation + degrees) % 360
        self.pixmap_item.setRotation(self._rotation)
        if self._fit_mode:
            self.fit_to_view()

    def mousePressEvent(self, event: QtGui.QMouseEvent):
        if event.button() == QtCore.Qt.BackButton:
            self.stepRequested.emit(-1)
            event.accept()
            return
        if event.button() == QtCore.Qt.ForwardButton:
            self.stepRequested.emit(1)
            event.accept()
            return
        super().mousePressEvent(event)


class ViewerWindow(QtWidgets.QMainWindow):
    stepRequested = QtCore.Signal(int)

    def __init__(self):
        super().__init__()
        self.setWindowTitle("\\u539f\\u56fe\\u9884\\u89c8")
        self.setMinimumSize(900, 680)
        self.setFocusPolicy(QtCore.Qt.StrongFocus)

        self.viewer = ImageViewer()
        self.viewer.stepRequested.connect(lambda delta: self.stepRequested.emit(delta))

        self.info_label = QtWidgets.QLabel("\u672a\u9009\u62e9\u56fe\u7247", self.viewer.viewport())
        self.info_label.setStyleSheet(
            """
            background: rgba(0, 0, 0, 140);
            color: #e6ecff;
            padding: 6px 10px;
            border-radius: 10px;
            font-weight: 700;
            font-size: 12px;
            """
        )
        self.info_label.setAttribute(QtCore.Qt.WA_TransparentForMouseEvents, True)
        self.info_label.move(12, 12)

        QtGui.QShortcut(QtGui.QKeySequence(QtCore.Qt.Key_Left), self.viewer, activated=lambda: self.stepRequested.emit(-1))
        QtGui.QShortcut(QtGui.QKeySequence(QtCore.Qt.Key_A), self.viewer, activated=lambda: self.stepRequested.emit(-1))
        QtGui.QShortcut(QtGui.QKeySequence(QtCore.Qt.Key_Right), self.viewer, activated=lambda: self.stepRequested.emit(1))
        QtGui.QShortcut(QtGui.QKeySequence(QtCore.Qt.Key_D), self.viewer, activated=lambda: self.stepRequested.emit(1))
        QtGui.QShortcut(QtGui.QKeySequence(QtCore.Qt.Key_Up), self.viewer, activated=lambda: self.viewer.rotate(-90))
        QtGui.QShortcut(QtGui.QKeySequence(QtCore.Qt.Key_Down), self.viewer, activated=lambda: self.viewer.rotate(90))

        container = QtWidgets.QWidget()
        layout = QtWidgets.QVBoxLayout(container)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(6)
        layout.addWidget(self.viewer)

        self.setCentralWidget(container)
        self._set_style()

    def _set_style(self):
        self.setStyleSheet(
            """
            QMainWindow { background-color: #090b10; }
            """
        )

    def show_image(self, pixmap: QtGui.QPixmap, path: Path):
        self.viewer.set_pixmap(pixmap, str(path))
        self.info_label.setText(f"{path.name} \u00b7 {pixmap.width()}x{pixmap.height()}")
        self.setWindowTitle(f"{path.name} - \u539f\u56fe\u9884\u89c8")
        if not self.isVisible():
            self.showMaximized()
        else:
            self.show()
        self.raise_()
        self.activateWindow()
        self.viewer.setFocus()

    def fit_to_view(self):
        self.viewer.fit_to_view()

    def clear(self):
        self.viewer.pixmap_item.setPixmap(QtGui.QPixmap())
        self.info_label.setText("\u672a\u9009\u62e9\u56fe\u7247")
        self.setWindowTitle("\u539f\u56fe\u9884\u89c8")

    def current_path(self) -> str | None:
        return self.viewer.current_path()

    def keyPressEvent(self, event: QtGui.QKeyEvent):
        key = event.key()
        if key in (QtCore.Qt.Key_Right, QtCore.Qt.Key_D):
            self.stepRequested.emit(1)
            return
        if key in (QtCore.Qt.Key_Left, QtCore.Qt.Key_A):
            self.stepRequested.emit(-1)
            return
        if key in (QtCore.Qt.Key_Escape, QtCore.Qt.Key_Space):
            self.viewer.fit_to_view()
            return
        super().keyPressEvent(event)


class MainWindow(QtWidgets.QMainWindow):
    SIZE_PRESETS = [
        ("\u5c0f", 120),
        ("\u4e2d", 170),
        ("\u5927", 230),
        ("\u7279\u5927", 420),
    ]

    def __init__(self, start_folder: Path | None = None):
        super().__init__()
        self.setWindowTitle("\u5feb\u770b\u76f8\u518c - \u8f7b\u91cf\u9884\u89c8")
        self.setMinimumSize(1000, 640)
        self.setFocusPolicy(QtCore.Qt.StrongFocus)

        self.viewer_window = ViewerWindow()
        self.viewer_window.stepRequested.connect(self._step)

        self.thumbs = ThumbnailGrid()
        self.thumbs.imageSelected.connect(self.open_image)
        self.thumbs.imageActivated.connect(self.open_image)
        self.thumbs.itemSelectionChanged.connect(self._on_selection_change)
        self.thumbs.installEventFilter(self)
        self.installEventFilter(self)

        self.full_cache = PixmapCache(capacity=24)
        self.image_signals = ImageSignals()
        self.image_signals.finished.connect(self._on_image_loaded)
        self.thread_pool = QtCore.QThreadPool.globalInstance()
        self.full_pool = QtCore.QThreadPool()
        ideal = QtCore.QThread.idealThreadCount()
        if ideal > 0:
            self.full_pool.setMaxThreadCount(max(2, min(ideal, 4)))
        else:
            self.full_pool.setMaxThreadCount(2)
        self._pending_image_path: str | None = None

        self.folder_label = QtWidgets.QLabel()
        self.folder_label.setTextInteractionFlags(QtCore.Qt.TextSelectableByMouse)
        self.folder_label.setStyleSheet("color: #c5c9d7;")

        self.size_combo = QtWidgets.QComboBox()
        for name, size in self.SIZE_PRESETS:
            self.size_combo.addItem(f"{name} ({size}px)", size)
        self.size_combo.setCurrentIndex(1)
        self.size_combo.currentIndexChanged.connect(self._on_size_change)
        self.size_combo.setStyleSheet(
            """
            QComboBox {
                background-color: #1c1f26;
                border: 1px solid #2b303a;
                border-radius: 6px;
                padding: 6px 10px;
                color: #dfe3ed;
                min-width: 110px;
            }
            QComboBox::drop-down {
                border: 0;
            }
            QComboBox QAbstractItemView {
                background-color: #181b22;
                selection-background-color: #2f6fed;
                border: 1px solid #2b303a;
                color: #eaeef9;
            }
            """
        )

        self.folder_btn = QtWidgets.QPushButton("\u9009\u62e9\u6587\u4ef6\u5939")
        self.folder_btn.clicked.connect(self._select_folder)
        self.folder_btn.setCursor(QtCore.Qt.PointingHandCursor)
        self.folder_btn.setStyleSheet(
            """
            QPushButton {
                background-color: #2f6fed;
                color: white;
                border: 0;
                border-radius: 8px;
                padding: 8px 14px;
                font-weight: 600;
                letter-spacing: 0.3px;
            }
            QPushButton:hover { background-color: #3f7dff; }
            QPushButton:pressed { background-color: #255dd6; }
            """
        )

        self.refresh_btn = QtWidgets.QPushButton("\u5237\u65b0")
        self.refresh_btn.clicked.connect(self._refresh_folder)
        self.refresh_btn.setCursor(QtCore.Qt.PointingHandCursor)
        self.refresh_btn.setStyleSheet(
            """
            QPushButton {
                background-color: #1c1f26;
                color: #dfe3ed;
                border: 1px solid #2b303a;
                border-radius: 8px;
                padding: 8px 14px;
                font-weight: 600;
            }
            QPushButton:hover { background-color: #222631; }
            """
        )

        self.open_btn = QtWidgets.QPushButton("\u6253\u5f00\u539f\u56fe")
        self.open_btn.setCursor(QtCore.Qt.PointingHandCursor)
        self.open_btn.setEnabled(False)
        self.open_btn.clicked.connect(self._open_current_selection)
        self.open_btn.setStyleSheet(
            """
            QPushButton {
                background-color: #2f6fed;
                color: white;
                border: 0;
                border-radius: 8px;
                padding: 8px 14px;
                font-weight: 700;
                letter-spacing: 0.4px;
            }
            QPushButton:disabled { background-color: #1c1f26; color: #7f8494; }
            QPushButton:hover:!disabled { background-color: #3f7dff; }
            QPushButton:pressed:!disabled { background-color: #255dd6; }
            """
        )

        control_bar = QtWidgets.QHBoxLayout()
        control_bar.setSpacing(10)
        control_bar.addWidget(self.folder_btn)
        control_bar.addWidget(self.refresh_btn)
        control_bar.addSpacing(12)
        control_bar.addWidget(QtWidgets.QLabel("\u7f29\u7565\u56fe\u5927\u5c0f"))
        control_bar.addWidget(self.size_combo)
        control_bar.addSpacing(6)
        control_bar.addWidget(self.open_btn)
        control_bar.addStretch()
        control_bar.addWidget(self.folder_label)

        thumb_panel = self._wrap_panel(self.thumbs, title="\u7f29\u7565\u56fe")
        hint = QtWidgets.QLabel("\u70b9\u51fb\u6216\u53cc\u51fb\u7f29\u7565\u56fe\u5373\u53ef\u6253\u5f00\u539f\u56fe\uff1b\u4e5f\u53ef\u70b9\u201c\u6253\u5f00\u539f\u56fe\u201d/Enter\uff1b\u6eda\u8f6e\u7f29\u653e\uff0c\u5de6\u53f3\u5207\u6362\uff0c\u7a7a\u683c/ESC \u590d\u4f4d\u3002")
        hint.setStyleSheet("color: #8f96a9;")
        hint.setWordWrap(True)

        root = QtWidgets.QWidget()
        root_layout = QtWidgets.QVBoxLayout(root)
        root_layout.setContentsMargins(14, 14, 14, 14)
        root_layout.setSpacing(10)
        root_layout.addLayout(control_bar)
        root_layout.addWidget(thumb_panel)
        root_layout.addWidget(hint)
        self.setCentralWidget(root)

        self._set_style()

        self.image_paths: List[Path] = []
        self.current_index: int = -1
        self.current_folder = start_folder or Path.cwd()
        self.load_folder(self.current_folder)

    def _wrap_panel(self, widget: QtWidgets.QWidget, title: str) -> QtWidgets.QWidget:
        wrapper = QtWidgets.QWidget()
        layout = QtWidgets.QVBoxLayout(wrapper)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(6)

        label = QtWidgets.QLabel(title)
        label.setStyleSheet("color: #9da4b7; font-weight: 700; letter-spacing: 0.5px;")
        layout.addWidget(label)
        layout.addWidget(widget)

        wrapper.setStyleSheet(
            """
            QWidget {
                background-color: #0f1115;
                border: 1px solid #1c2028;
                border-radius: 12px;
            }
            """
        )
        return wrapper

    def _set_style(self):
        self.setStyleSheet(
            """
            QMainWindow { background-color: #08090c; }
            QListWidget {
                background-color: #0f1115;
                border: 0;
                color: #dde3f0;
                font-size: 12px;
            }
            QListWidget::item {
                border-radius: 10px;
                padding: 8px;
                margin: 0;
            }
            QListWidget::item:selected {
                background-color: rgba(63, 125, 255, 0.14);
                border: 1px solid rgba(63, 125, 255, 0.4);
            }
            QLabel {
                color: #dfe3ed;
                font-weight: 500;
            }
            QSplitter::handle {
                background: #10141b;
            }
            """
        )

    def _on_size_change(self, index: int):
        size = self.size_combo.currentData(QtCore.Qt.UserRole)
        if size is None:
            size = self.SIZE_PRESETS[index][1]
        self.thumbs.set_thumbnail_size(int(size))

    def _select_folder(self):
        directory = QtWidgets.QFileDialog.getExistingDirectory(
            self,
            "\u9009\u62e9\u56fe\u7247\u6587\u4ef6\u5939",
            str(self.current_folder),
            QtWidgets.QFileDialog.ShowDirsOnly,
        )
        if directory:
            self.load_folder(Path(directory))

    def _refresh_folder(self):
        self.load_folder(self.current_folder)

    def load_folder(self, folder: Path):
        folder = folder.expanduser().resolve()
        if not folder.exists():
            QtWidgets.QMessageBox.warning(self, "\u8def\u5f84\u4e0d\u5b58\u5728", f"{folder} \u4e0d\u5b58\u5728")
            return

        self.current_folder = folder
        self.folder_label.setText(str(folder))
        entries = []
        with os.scandir(folder) as it:
            for entry in it:
                if entry.is_file() and Path(entry.name).suffix.lower() in IMAGE_EXTENSIONS:
                    entries.append(entry)
        entries.sort(key=lambda e: e.name.lower())
        paths = [Path(folder, e.name) for e in entries]

        self.image_paths = paths
        self.current_index = -1
        self._pending_image_path = None
        self.thumbs.populate(paths)
        self.open_btn.setEnabled(False)

        if not paths:
            self.viewer_window.clear()

    def open_image(self, path: str | Path):
        path = Path(path)
        if not path.exists():
            return

        if path not in self.image_paths:
            return
        self.current_index = self.image_paths.index(path)
        self._pending_image_path = str(path)
        self.thumbs.select_by_path(str(path))

        cached = self.full_cache.get(str(path))
        if cached:
            self.viewer_window.show_image(cached, path)
            return

        worker = ImageWorker(path, self.full_cache, self.image_signals)
        worker.setAutoDelete(True)
        self.full_pool.start(worker)

    @QtCore.Slot(str, QtGui.QPixmap)
    def _on_image_loaded(self, path: str, pixmap: QtGui.QPixmap):
        if path != self._pending_image_path:
            return
        if pixmap.isNull():
            QtWidgets.QMessageBox.warning(self, "\u8bfb\u53d6\u5931\u8d25", f"\u65e0\u6cd5\u6253\u5f00\u56fe\u7247\uff1a{path}")
            return
        self.viewer_window.show_image(pixmap, Path(path))

    def keyPressEvent(self, event: QtGui.QKeyEvent):
        if event.key() in (QtCore.Qt.Key_Right, QtCore.Qt.Key_D):
            self._step(1)
            event.accept()
            return
        if event.key() in (QtCore.Qt.Key_Left, QtCore.Qt.Key_A):
            self._step(-1)
            event.accept()
            return
        if event.key() in (QtCore.Qt.Key_Return, QtCore.Qt.Key_Enter):
            self._open_current_selection()
            event.accept()
            return
        if event.key() == QtCore.Qt.Key_Escape:
            if self.viewer_window.isVisible():
                self.viewer_window.fit_to_view()
            event.accept()
            return
        super().keyPressEvent(event)

    def _step(self, delta: int):
        if not self.image_paths:
            return
        if self.current_index < 0:
            item = self.thumbs.currentItem()
            if item:
                path = item.data(QtCore.Qt.UserRole)
                if path:
                    self.open_image(path)
            return
        new_index = (self.current_index + delta) % len(self.image_paths)
        target = self.image_paths[new_index]
        self.open_image(target)

    def _open_current_selection(self):
        item = self.thumbs.currentItem()
        if not item:
            return
        path = item.data(QtCore.Qt.UserRole)
        if not path:
            return
        self.open_image(path)

    def _on_selection_change(self):
        item = self.thumbs.currentItem()
        has_selection = bool(item and item.data(QtCore.Qt.UserRole))
        self.open_btn.setEnabled(has_selection)

    def eventFilter(self, obj, event):
        if event.type() == QtCore.QEvent.KeyPress:
            key = event.key()
            if key in (QtCore.Qt.Key_Right, QtCore.Qt.Key_D):
                self._step(1)
                return True
            if key in (QtCore.Qt.Key_Left, QtCore.Qt.Key_A):
                self._step(-1)
                return True
            if key in (QtCore.Qt.Key_Return, QtCore.Qt.Key_Enter):
                self._open_current_selection()
                return True
            if key in (QtCore.Qt.Key_Escape, QtCore.Qt.Key_Space):
                if self.viewer_window.isVisible():
                    self.viewer_window.fit_to_view()
                return True
        return super().eventFilter(obj, event)


def main():
    if hasattr(QtGui.QGuiApplication, "setHighDpiScaleFactorRoundingPolicy"):
        QtGui.QGuiApplication.setHighDpiScaleFactorRoundingPolicy(
            QtCore.Qt.HighDpiScaleFactorRoundingPolicy.PassThrough
        )
    app = QtWidgets.QApplication(sys.argv)
    start_folder = Path.cwd()
    window = MainWindow(start_folder=start_folder)
    window.showMaximized()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()

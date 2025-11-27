import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.platform 1.1 as Platform

ApplicationWindow {
    id: root
    width: 1200
    height: 720
    visible: true
    color: "#08090c"
    title: "MyPhotos (C++/Qt Quick)"

    property alias folderPath: folderField.text
    property int thumbSize: parseInt(thumbSizeSelector.currentText)
    property int preloadRadius: 3
    property url folderUrl: ""
    property int viewerIndex: -1
    property real viewerScale: 1.0
    property int viewerRotation: 0
    property string viewerSource: ""
    property string viewerTitle: ""

    function fitViewer() {
        var w = viewerImage.paintedWidth
        var h = viewerImage.paintedHeight
        if (w <= 0 || h <= 0) {
            viewerScale = 1.0
            return
        }
        var rot = viewerRotation % 180
        var bw = rot === 0 ? w : h
        var bh = rot === 0 ? h : w
        var sx = viewerWindow.width / bw
        var sy = viewerWindow.height / bh
        var s = Math.min(sx, sy)
        if (!isFinite(s) || s <= 0) {
            s = 1.0
        }
        viewerScale = s
        centerViewer()
    }

    function centerViewer() {
        if (viewerFlick) {
            viewerFlick.contentX = (viewerFlick.contentWidth - viewerFlick.width) / 2
            viewerFlick.contentY = (viewerFlick.contentHeight - viewerFlick.height) / 2
        }
    }

    function toLocalPath(u) {
        var s = ""
        if (u && u.toString) {
            s = u.toString()
        } else {
            s = "" + u
        }
        // Already a plain path
        if (s.startsWith("/")) {
            return s
        }
        // URL string forms
        if (s.startsWith("file://")) {
            return s.slice(7)
        }
        if (s.startsWith("file:")) {
            return s.slice(5)
        }
        return s
    }

    function openViewerForIndex(idx) {
        var total = grid.count
        if (total <= 0)
            return
        var next = (idx % total + total) % total
        var p = imageModel.pathAt(next)
        if (!p || p.length === 0)
            return
        viewerIndex = next
        grid.currentIndex = viewerIndex
        var src = "image://thumbs/full/" + p
        fullImage.source = src
        viewerSource = src
        viewerTitle = p.split("/").pop()
        viewerRotation = 0
        viewerWindow.visible = true
        viewerWindow.visibility = Window.Maximized
        viewerWindow.raise()
        viewerWindow.requestActivate()
        // Fit after image is ready (see viewerImage onStatusChanged)
    }

    function stepImage(delta) {
        var base = viewerIndex >= 0 ? viewerIndex : grid.currentIndex
        if (base < 0)
            base = 0
        openViewerForIndex(base + delta)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            spacing: 8
            Layout.fillWidth: true

            TextField {
                id: folderField
                Layout.fillWidth: true
                placeholderText: "选择图片文件夹"
                selectByMouse: true
                readOnly: true
            }

            Button {
                Layout.preferredWidth: 110
                text: "选择文件夹"
                onClicked: {
                    folderDialog.folder = folderUrl && folderUrl.toString().length > 0
                                         ? folderUrl
                                         : Platform.StandardPaths.writableLocation(Platform.StandardPaths.PicturesLocation)
                    folderDialog.open()
                }
            }

            ComboBox {
                id: thumbSizeSelector
                model: ["120", "170", "230", "420"]
                currentIndex: 1
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            SplitView.preferredWidth: 1200

            Frame {
                SplitView.fillWidth: true
                SplitView.fillHeight: true
                background: Rectangle { color: "#0f1115"; radius: 8; border.color: "#1c2028" }

                GridView {
                    id: grid
                    anchors.fill: parent
                    cellWidth: thumbSize + 24
                    cellHeight: thumbSize + 32
                    model: imageModel
                    clip: true
                    cacheBuffer: 400

                    delegate: MouseArea {
                        width: grid.cellWidth
                        height: grid.cellHeight
                        hoverEnabled: true

                        Rectangle {
                            anchors.fill: parent
                            color: containsMouse ? "#111621" : "transparent"
                            radius: 10
                            border.color: ListView.isCurrentItem ? "#3f7dff" : "#202534"
                            border.width: ListView.isCurrentItem ? 1 : 0
                        }

                        Column {
                            anchors.centerIn: parent
                            spacing: 6
                            Image {
                                id: thumb
                                source: "image://thumbs/" + thumbSize + "/" + path
                                sourceSize.width: thumbSize
                                sourceSize.height: thumbSize
                                fillMode: Image.PreserveAspectFit
                                cache: true
                                asynchronous: true
                                width: thumbSize
                                height: thumbSize
                            }
                            Text {
                                text: fileName
                                color: "#dfe3ed"
                                elide: Text.ElideMiddle
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                width: thumbSize + 8
                            }
                        }

                        onClicked: {
                            openViewerForIndex(index)
                        }

                        Component.onCompleted: {
                            // Prefetch neighbors
                            thumbBridge.prefetchAround(index, preloadRadius, thumbSize)
                        }
                    }
                }
            }

            Frame {
                SplitView.fillWidth: true
                SplitView.fillHeight: true
                background: Rectangle { color: "#0f1115"; radius: 8; border.color: "#1c2028" }

                Flickable {
                    anchors.fill: parent
                    contentWidth: fullImage.paintedWidth
                    contentHeight: fullImage.paintedHeight
                    clip: true
                    interactive: true
                    contentX: (contentWidth - width) / 2
                    contentY: (contentHeight - height) / 2

                    Image {
                        id: fullImage
                        anchors.centerIn: parent
                        source: ""
                        fillMode: Image.PreserveAspectFit
                        cache: true
                        asynchronous: true
                        smooth: true
                        sourceSize.width: 2400
                        sourceSize.height: 2400
                    }
                }
            }
        }
    }

    Connections {
        target: imageModel
        function onFolderChanged() {
            folderField.text = imageModel.folder
            folderUrl = imageModel.folder
        }
    }

    Component.onCompleted: {
        const def = Platform.StandardPaths.writableLocation(Platform.StandardPaths.PicturesLocation)
        if (def && def.length > 0) {
            folderField.text = def
            folderUrl = def
            imageModel.folder = def
        }
    }

    Window {
        id: viewerWindow
        visible: false
        width: 1100
        height: 820
        color: "#0f1115"
        title: viewerTitle.length > 0 ? viewerTitle : "原图预览"
        flags: Qt.Window

        Flickable {
            id: viewerFlick
            anchors.fill: parent
            contentWidth: Math.max(viewerImage.paintedWidth * viewerScale, width)
            contentHeight: Math.max(viewerImage.paintedHeight * viewerScale, height)
            clip: true
            interactive: true
            boundsBehavior: Flickable.StopAtBounds
            flickDeceleration: 2500
            focus: true

            Image {
                id: viewerImage
                anchors.centerIn: parent
                source: viewerSource
                fillMode: Image.PreserveAspectFit
                cache: true
                asynchronous: true
                smooth: true
                sourceSize.width: 3200
                sourceSize.height: 3200
                transformOrigin: Item.Center
                transform: [
                    Scale { xScale: viewerScale; yScale: viewerScale; origin.x: viewerImage.paintedWidth / 2; origin.y: viewerImage.paintedHeight / 2 },
                    Rotation { angle: viewerRotation; origin.x: viewerImage.paintedWidth / 2; origin.y: viewerImage.paintedHeight / 2 }
                ]

                onStatusChanged: {
                    if (status === Image.Ready) {
                        viewerRotation = 0
                        fitViewer()
                    }
                }

                PinchHandler {
                    id: pinch
                    target: null // manual control
                    property real startScale: 1.0
                    onActiveChanged: {
                        if (active) {
                            startScale = viewerScale
                        }
                    }
                    onScaleChanged: {
                        if (!active)
                            return
                        var s = startScale * scale
                        viewerScale = Math.max(0.1, Math.min(10, s))
                        centerViewer()
                    }
                }

                WheelHandler {
                    id: wheel
                    target: null
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onWheel: function(event) {
                        var dy = event.angleDelta.y
                        if (dy === 0)
                            return
                        var factor = dy > 0 ? 1.12 : 1 / 1.12
                        viewerScale = Math.max(0.1, Math.min(10, viewerScale * factor))
                        centerViewer()
                        event.accepted = true
                    }
                }

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    gesturePolicy: TapHandler.WithinBounds
                    onDoubleTapped: {
                        viewerRotation = 0
                        fitViewer()
                    }
                }
            }

            onWidthChanged: centerViewer()
            onHeightChanged: centerViewer()
            onContentWidthChanged: centerViewer()
            onContentHeightChanged: centerViewer()
        }

        Shortcut {
            sequences: [StandardKey.Close, StandardKey.Cancel]
            onActivated: viewerWindow.visible = false
        }
        Shortcut {
            sequences: ["Left"]
            context: Qt.ApplicationShortcut
            onActivated: stepImage(-1)
        }
        Shortcut {
            sequences: ["Right"]
            context: Qt.ApplicationShortcut
            onActivated: stepImage(1)
        }
        Shortcut {
            sequences: ["Up"]
            context: Qt.ApplicationShortcut
            onActivated: {
                viewerRotation = (viewerRotation + 90) % 360
                fitViewer()
            }
        }
        Shortcut {
            sequences: ["Down"]
            context: Qt.ApplicationShortcut
            onActivated: {
                viewerRotation = (viewerRotation + 270) % 360
                fitViewer()
            }
        }
    }

    Platform.FolderDialog {
        id: folderDialog
        title: "选择图片文件夹"
        folder: folderUrl && folderUrl.toString().length > 0
                ? folderUrl
                : Platform.StandardPaths.writableLocation(Platform.StandardPaths.PicturesLocation)
        onAccepted: {
            const path = toLocalPath(folder)
            folderField.text = path
            folderUrl = folder
            imageModel.folder = path
        }
    }
}

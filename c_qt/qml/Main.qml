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
    property int thumbSize: 170
    property int preloadRadius: 3
    property url folderUrl: ""
    property int viewerIndex: -1
    property real viewerScale: 1.0
    property int viewerRotation: 0
    property string viewerSource: ""
    property string viewerTitle: ""
    property string viewerDim: ""
    property bool viewerLoading: false

    // Palette
    property color cBg: "#080b12"
    property color cPanel: "#0f131d"
    property color cAccent: "#3f7dff"
    property color cAccent2: "#5ed2ff"
    property color cStroke: "#1f2533"
    property color cText: "#e7ecf7"
    property color cSub: "#9aa2b5"

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
        viewerDim = ""
        viewerLoading = true
        viewerRotation = 0
        viewerWindow.visible = true
        if (!viewerWindow.visible) {
            viewerWindow.visibility = Window.Maximized
        }
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

        // Hidden storage for folder path (for alias binding)
        TextField {
            id: folderField
            visible: false
            text: ""
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: toolbar.implicitHeight + 10
            radius: 10
            color: cPanel
            border.color: cStroke
            border.width: 1

            RowLayout {
                id: toolbar
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    radius: 8
                    color: "#0f141f"
                    border.color: "#1e2433"
                    Row {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 6
                        Rectangle { width: 3; radius: 2; color: cAccent; height: 18; anchors.verticalCenter: parent.verticalCenter }
                        Text {
                            text: folderField.text.length > 0 ? folderField.text : "未选择"
                            color: cText
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                            Layout.fillWidth: true
                            font.pixelSize: 12
                        }
                    }
                }

                Button {
                    Layout.preferredHeight: 30
                    Layout.preferredWidth: 92
                    text: "选择文件夹"
                    font.pixelSize: 11
                    onClicked: {
                        folderDialog.folder = folderUrl && folderUrl.toString().length > 0
                                             ? folderUrl
                                             : Platform.StandardPaths.writableLocation(Platform.StandardPaths.PicturesLocation)
                        folderDialog.open()
                    }
                    background: Rectangle {
                        radius: 8
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: cAccent }
                            GradientStop { position: 1.0; color: cAccent2 }
                        }
                        border.color: "#4c6fe6"
                        border.width: 0.5
                    }
                }

                RowLayout {
                    spacing: 6
                    Layout.alignment: Qt.AlignVCenter
                    Repeater {
                        model: [
                            { name: "小", size: 120 },
                            { name: "中", size: 170 },
                            { name: "大", size: 230 },
                            { name: "特大", size: 420 }
                        ]
                        delegate: Rectangle {
                            radius: 7
                            width: 44
                            height: 26
                            color: thumbSize === modelData.size ? cAccent : "#0f141f"
                            border.color: thumbSize === modelData.size ? cAccent2 : "#1e2433"
                            Text {
                                anchors.centerIn: parent
                                text: modelData.name
                                color: thumbSize === modelData.size ? "white" : cText
                                font.pixelSize: 11
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: thumbSize = modelData.size
                            }
                        }
                    }
                    Rectangle {
                        radius: 7
                        width: 64
                        height: 26
                        color: "#0f141f"
                        border.color: "#1e2433"
                        Text {
                            anchors.centerIn: parent
                            text: "自定义"
                            color: cText
                            font.pixelSize: 11
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                customSizeSlider.value = thumbSize
                                customSizeDialog.open()
                            }
                        }
                    }
                }
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            SplitView.preferredWidth: 1200

            Frame {
                SplitView.fillWidth: true
                SplitView.fillHeight: true
                background: Rectangle {
                    color: cPanel
                    radius: 12
                    border.color: cStroke
                }

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
                        property bool thumbReady: false

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
                            Rectangle {
                                width: thumbSize
                                height: thumbSize
                                radius: 10
                                color: "#1f1f24"
                                border.color: "#2b3033"
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: !thumbReady
                            }
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
                                visible: thumbReady
                                onStatusChanged: {
                                    if (status === Image.Ready) {
                                        thumbReady = true
                                    }
                                }
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
                background: Rectangle {
                    color: cPanel
                    radius: 12
                    border.color: cStroke
                }

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
        title: viewerDim.length > 0 && viewerTitle.length > 0
               ? "(" + viewerDim + ") " + viewerTitle
               : (viewerTitle.length > 0 ? viewerTitle : "原图预览")
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
                    if (status === Image.Loading) {
                        viewerLoading = true
                        viewerDim = ""
                    }
                    if (status === Image.Ready) {
                        viewerRotation = 0
                        var w = viewerImage.implicitWidth > 0 ? viewerImage.implicitWidth : viewerImage.paintedWidth
                        var h = viewerImage.implicitHeight > 0 ? viewerImage.implicitHeight : viewerImage.paintedHeight
                        viewerDim = Math.max(1, Math.round(w)) + "x" + Math.max(1, Math.round(h))
                        viewerLoading = false
                        fitViewer()
                    }
                    if (status === Image.Error) {
                        viewerLoading = false
                        viewerDim = ""
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

            Rectangle {
                anchors.centerIn: parent
                width: 120
                height: 120
                radius: 12
                color: "#161922"
                border.color: "#252a36"
                visible: viewerLoading || viewerSource.length === 0
                Text {
                    anchors.centerIn: parent
                    text: "Loading..."
                    color: "#8892a7"
                    font.pixelSize: 14
                    font.bold: true
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

    Dialog {
        id: customSizeDialog
        title: "自定义缩略图大小"
        modal: true
        implicitWidth: 360
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: {
            thumbSize = Math.round(customSizeSlider.value)
        }
        x: (parent ? parent.width : Screen.width) / 2 - implicitWidth / 2
        y: (parent ? parent.height : Screen.height) / 2 - implicitHeight / 2
        background: Rectangle {
            radius: 12
            color: cPanel
            border.color: "transparent"
            border.width: 0
        }
        contentItem: Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 14

                Label {
                    text: "预览缩略图分辨率"
                    color: cText
                    font.pixelSize: 14
                    font.bold: true
                }
                Label {
                    text: "拖动滑块设置像素"
                    color: cSub
                    font.pixelSize: 11
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Slider {
                        id: customSizeSlider
                        from: 80
                        to: 600
                        stepSize: 10
                        Layout.fillWidth: true
                    }
                    Rectangle {
                        Layout.preferredWidth: 64
                        Layout.preferredHeight: 28
                        radius: 7
                        color: "#0f141f"
                        border.color: "#1e2433"
                        Text {
                            anchors.centerIn: parent
                            text: Math.round(customSizeSlider.value) + "px"
                            color: cText
                            font.pixelSize: 11
                        }
                    }
                }

                Item { Layout.fillWidth: true; Layout.preferredHeight: 12 }
            }
        }
    }
}

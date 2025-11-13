import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import Log_analyzer 1.0
import QtQuick.Dialogs 6.3

VehicleReviewPage {
    id: root
    anchors.fill: parent
    
    // 文件加载状态
    property bool isFileLoading: false
    
    Rectangle {
        anchors.fill: parent
        color: "#F5F5F5"
        
        // 加载指示器
        BusyIndicator {
            id: loadingIndicator
            anchors.centerIn: parent
            z: 1000
            running: isFileLoading
            visible: running
        }
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 10
            
            // 播放控制导航栏
            Rectangle {
                id: playbackNavBar
                Layout.fillWidth: true
                Layout.preferredHeight: 45
                color: "#FAFAFA"
                border.color: "#E5E5E5"
                border.width: 1
                visible: mapDataManager.vehicleTrackCount > 0
                
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8
                    
                    // 时间显示
                    Text {
                        font.pixelSize: 12
                        color: "#333333"
                        Layout.preferredWidth: 220
                        Layout.alignment: Qt.AlignVCenter
                        text: {
                            if (mapViewer && mapViewer.playIndex !== undefined) {
                                return "回看时间: " + mapViewer.formatTime(mapViewer.playIndex)
                            }
                            return "回看时间: ---- -- -- --:--:--.---"
                        }
                    }
                    
                    Button {
                        text: "加载文件"
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 36
                        onClicked: {
                            fileDialog.open()
                        }
                    }
                    Item { Layout.fillWidth: true }
                    
                    // 播放控制按钮
                    Button {
                        text: "▶ 播放"
                        Layout.preferredWidth: 90
                        Layout.preferredHeight: 36
                        onClicked: if (mapViewer) mapViewer.startPlayback()
                    }
                    Button {
                        text: "⏸ 暂停"
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 36
                        onClicked: if (mapViewer) mapViewer.pausePlayback()
                    }
                    Button {
                        text: "⏹ 停止"
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 36
                        onClicked: if (mapViewer) mapViewer.stopPlayback()
                    }
                    
                    // 帧控制按钮
                    Button {
                        text: "◀ 上帧"
                        Layout.preferredWidth: 90
                        Layout.preferredHeight: 36
                        onClicked: if (mapViewer) mapViewer.stepBackward()
                    }
                    Button {
                        text: "下帧 ▶"
                        Layout.preferredWidth: 90
                        Layout.preferredHeight: 36
                        onClicked: if (mapViewer) mapViewer.stepForward()
                    }
                    
                    // 倍速控制
                    Text {
                        text: "倍速："
                        font.pixelSize: 12
                        Layout.alignment: Qt.AlignVCenter
                    }
                    ComboBox {
                        id: speedBox
                        Layout.preferredWidth: 75
                        Layout.preferredHeight: 36
                        model: [0.25, 0.5, 1.0, 2.0, 4.0]
                        currentIndex: 2
                        onActivated: {
                            if (mapViewer) {
                                mapViewer.speedFactor = parseFloat(currentText)
                            }
                        }
                    }
                    
                    // 自动跟踪控制
                    Button {
                        text: (mapViewer && mapViewer.autoFollowVehicle) ? "🎯 跟踪中" : "🎯 启用跟踪"
                        Layout.preferredWidth: 130
                        Layout.preferredHeight: 36
                        onClicked: {
                            if (mapViewer) {
                                if (mapViewer.autoFollowVehicle) {
                                    mapViewer.autoFollowVehicle = false
                                } else {
                                    mapViewer.autoFollowVehicle = true
                                    mapViewer.zoomLevel = mapViewer.autoFollowZoom
                                }
                            }
                        }
                    }
                }
            }
            
            // 地图显示区域
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#FFFFFF"
                border.color: "#E0E0E0"
                border.width: 1
                radius: 6
                clip: true
                
                MapViewer {
                    id: mapViewer
                    anchors.fill: parent
                    anchors.margins: 1
                    
                    Component.onCompleted: {
                        // 延迟加载，确保应用程序初始化完成
                        Qt.callLater(function() {
                            // 直接尝试加载地图，MapDataManager会在loadMapData时处理
                            mapViewer.loadMap();
                        });
                    }
                }
                
                // 空状态提示
                Column {
                    anchors.centerIn: parent
                    spacing: 20
                    visible: !mapDataManager.isLoaded
                    
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "🗺️"
                        font.pixelSize: 48
                    }
                    
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "地图数据未加载"
                        font.pixelSize: 16
                        color: "#666666"
                    }
                    
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "请点击加载地图按钮加载地图数据"
                        font.pixelSize: 12
                        color: "#999999"
                    }
                }
            }
           
        }
    }
     FileDialog {
        id: fileDialog
        title: "选择文本文件或ZIP压缩包"
        fileMode: FileDialog.OpenFile
        nameFilters: [
            "支持的文件 (*.txt *.md *.csv *.zip)",
            "文本文件 (*.txt *.md *.csv)",
            "压缩文件 (*.zip)",
            "所有文件 (*)"
        ]

        // 强制使用非原生对话框以确保过滤器正常工作
        // options: FileDialog.DontUseNativeDialog

        onAccepted: function() {
            // 处理文件选择
            if (typeof sqliteTextHandler !== 'undefined') {
                console.log("选择的文件:", selectedFile)
                
                // 开始加载文件
                isFileLoading = true
                sqliteTextHandler.loadTextFileAsync(selectedFile)
            }
        }

        onRejected: function() {
            console.log("用户取消了文件选择")
        }
    }
    
    // 监听 sqliteTextHandler 的加载状态
    Connections {
        target: typeof sqliteTextHandler !== 'undefined' ? sqliteTextHandler : null
        enabled: typeof sqliteTextHandler !== 'undefined'
        
        function onFileLoaded(content) {
            console.log("文件加载完成，开始加载地图")
            isFileLoading = true
            // 文件加载完成后，延迟加载地图以确保数据已写入数据库
            Qt.callLater(function() {
                if (mapViewer) {
                    mapViewer.loadMap()
                }
            })
        }
        
        function onLoadError(errorMessage) {
            console.error("文件加载失败:", errorMessage)
            isFileLoading = false
            errorDialog.errorText = errorMessage
            errorDialog.open()
        }
    }

    Connections {
        target: mapDataManager
        function onVehicleTrackLoaded() {
            isFileLoading = false
            console.log("车辆轨迹加载完成")
        }
    }
    
    // 错误对话框
    Dialog {
        id: errorDialog
        title: "文件加载错误"
        anchors.centerIn: parent
        width: 400
        height: 150
        modal: true
        standardButtons: Dialog.Ok
        
        property string errorText: ""
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 15
            
            Text {
                Layout.fillWidth: true
                text: "❌ " + errorDialog.errorText
                wrapMode: Text.Wrap
                font.pixelSize: 14
                color: "#333333"
            }
        }
    }

} 
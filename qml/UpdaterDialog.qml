import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt5Compat.GraphicalEffects

ApplicationWindow {
    id: root
    width: 480
    height: 360
    minimumWidth: 480
    minimumHeight: 400
    maximumWidth: 480
    maximumHeight: 40
    
    title: qsTr("软件更新")
    flags: Qt.Dialog | Qt.WindowCloseButtonHint
    modality: Qt.ApplicationModal
    
    property QtObject updater: null
    
    color: "transparent"
    
    Rectangle {
        id: mainContainer
        anchors.fill: parent
        radius: 12
        color: "#f8f9fa"
        
        // 阴影效果
        layer.enabled: true
        layer.effect: DropShadow {
            horizontalOffset: 0
            verticalOffset: 4
            radius: 16
            samples: 33
            color: "#40000000"
        }
        
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            
            // 头部区域
            Rectangle {
                id: headerArea
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                color: "#f8f9fa"
                radius: 12
                
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 16
                    
                    Image {
                        id: updateIcon
                        Layout.preferredWidth: 48
                        Layout.preferredHeight: 48
                        source: "qrc:/resources/icons/app_icon.ico"
                        fillMode: Image.PreserveAspectFit
                    }
                    
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        
                        Text {
                            id: titleLabel
                            text: updater ? updater.titleText : qsTr("软件更新")
                            font.pixelSize: 18
                            font.weight: Font.Bold
                            color: "#212529"
                            Layout.fillWidth: true
                            
                            Behavior on text {
                                PropertyAnimation {
                                    duration: 300
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }
                        
                        Text {
                            id: versionLabel
                            text: updater && updater.newVersion ? qsTr("新版本：%1").arg(updater.newVersion) : ""
                            font.pixelSize: 14
                            color: "#6c757d"
                            visible: text !== ""
                        }
                    }
                }
            }
            
            // 内容区域
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 0
                    spacing: 16
                    
                    // 状态文本
                    Text {
                        id: statusLabel
                        width: parent.width  // 使用父容器的全宽
                        horizontalAlignment: Text.AlignLeft  // 文本内部左对齐
                        text: updater ? updater.statusText : qsTr("正在连接服务器检查更新，请稍候...")
                        font.pixelSize: 14
                        color: "#495057"
                        wrapMode: Text.WordWrap
                        lineHeight: 1.4
                        
                        Behavior on text {
                            PropertyAnimation {
                                duration: 300
                                easing.type: Easing.OutCubic
                            }
                        }
                    }
                    
                    // 进度条
                    ProgressBar {
                        id: downloadProgress
                        Layout.fillWidth: true
                        visible: updater ? updater.showProgress : false
                        value: updater ? updater.downloadProgress / 100.0 : 0
                        
                        background: Rectangle {
                            implicitWidth: parent.width
                            implicitHeight: 4
                            color: "#e9ecef"
                            radius: 4
                        }
                        
                        contentItem: Rectangle {
                            width: downloadProgress.visualPosition * parent.width
                            height: 4
                            radius: 4
                            color: "#007acc"
                            
                            Behavior on width {
                                PropertyAnimation {
                                    duration: 300
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }
                        
                    }
                    
                    // 版本说明
                    ScrollView {
                        id: releaseNotesView
                        Layout.fillWidth: true
                        Layout.preferredHeight: 170  // 增加高度
                        visible: updater ? updater.showReleaseNotes : false
                        
                        TextArea {
                            text: updater ? 
                                  '<style>' +
                                  'body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif; line-height: 1.6; font-size: 14px; color: #495057; }' +
                                  'h2 { color: #212529; font-size: 16px; margin-bottom: 10px; font-weight: bold; }' +
                                  'h3 { color: #495057; font-size: 15px; margin-bottom: 8px; font-weight: bold; }' +
                                  'h4 { color: #6c757d; font-size: 14px; margin-bottom: 6px; font-weight: bold; }' +
                                  'ul { margin-left: 20px; margin-bottom: 10px; padding-left: 15px; list-style-type: none; }' +
                                  'li { margin-bottom: 5px; color: #495057; position: relative; padding-left: 15px; }' +
                                  'li::before { content: "•"; color: #007acc; position: absolute; left: -15px; }' +
                                  'code { background-color: #f1f3f5; padding: 2px 4px; border-radius: 3px; font-family: monospace; color: #d63384; }' +
                                  'pre { background-color: #f1f3f5; padding: 10px; border-radius: 4px; overflow-x: auto; border: 1px solid #e9ecef; }' +
                                  'br { line-height: 1.6; }' +
                                  '</style>' +
                                  '<body>' + updater.releaseNotes + '</body>'
                                  : ""
                            readOnly: true
                            font.pixelSize: 14
                            color: "#495057"
                            textFormat: TextEdit.RichText  // 使用富文本
                            wrapMode: TextArea.Wrap
                            background: Rectangle {
                                color: "#f8f9fa"
                            }
                        }
                    }
                }
            }
            
            // 创建快捷方式选项
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                color: "transparent"
                visible: updater ? updater.showCreateShortcut : false
                
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 12
                    
                    CheckBox {
                        id: shortcutCheckBox
                        checked: updater ? updater.createShortcutChecked : true
                        text: qsTr("创建桌面快捷方式")
                        font.pixelSize: 14
                        
                        onCheckedChanged: {
                            if (updater) {
                                updater.createShortcutChecked = checked
                            }
                        }
                        
                        indicator: Rectangle {
                            implicitWidth: 18
                            implicitHeight: 18
                            x: shortcutCheckBox.leftPadding
                            y: parent.height / 2 - height / 2
                            radius: 3
                            border.color: shortcutCheckBox.checked ? "#007acc" : "#d0d0d0"
                            border.width: shortcutCheckBox.checked ? 2 : 1
                            color: shortcutCheckBox.checked ? "#007acc" : "#ffffff"
                            
                            Rectangle {
                                width: 8
                                height: 4
                                x: 5
                                y: 6
                                color: "#ffffff"
                                visible: shortcutCheckBox.checked
                                transform: [
                                    Rotation { angle: 45; origin.x: 4; origin.y: 2 },
                                    Scale { xScale: 0.8; yScale: 1.2 }
                                ]
                            }
                        }
                        
                        contentItem: Text {
                            text: shortcutCheckBox.text
                            font: shortcutCheckBox.font
                            color: "#495057"
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: shortcutCheckBox.indicator.width + shortcutCheckBox.spacing
                        }
                    }
                }
            }
            
            // 按钮区域
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                color: "#f8f9fa"
                
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 1
                    color: "#dee2e6"
                }
                
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 12
                    
                    Item {
                        Layout.fillWidth: true
                    }
                    
                    // 创建快捷方式按钮
                    Button {
                        visible: updater ? updater.showCreateShortcut && updater.createShortcutChecked : false
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 36
                        
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? "#0056b3" : "#007acc"
                            border.width: 0
                            
                            Behavior on color {
                                PropertyAnimation {
                                    duration: 150
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }
                        
                        contentItem: Text {
                            text: qsTr("创建快捷方式")
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            color: "#ffffff"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        
                        onClicked: {
                            if (updater) {
                                updater.createDesktopShortcut()
                            }
                        }
                    }
                    
                    // 更新按钮
                    Button {
                        visible: updater ? updater.showUpdateButton : false
                        Layout.preferredWidth: 100
                        Layout.preferredHeight: 36
                        
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? "#0056b3" : "#007acc"
                            border.width: 0
                            
                            Behavior on color {
                                PropertyAnimation {
                                    duration: 150
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }
                        
                        contentItem: Text {
                            text: qsTr("立即更新")
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            color: "#ffffff"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        
                        onClicked: {
                            if (updater) {
                                updater.startUpdate()
                            }
                        }
                    }
                    
                    // 取消/完成按钮
                    Button {
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 36
                        
                        background: Rectangle {
                            radius: 6
                            color: parent.hovered ? "#e9ecef" : "#f8f9fa"
                            border.color: "#dee2e6"
                            border.width: 1
                            
                            Behavior on color {
                                PropertyAnimation {
                                    duration: 150
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }
                        
                        contentItem: Text {
                            text: updater ? updater.cancelButtonText : qsTr("取消")
                            font.pixelSize: 13
                            color: "#495057"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        
                        onClicked: {
                            if (updater) {
                                updater.cancelUpdate()
                            }
                        }
                    }
                }
            }
        }
    }
    
    Component.onCompleted: {
        // if (updater) {
        //     updater.checkForUpdates()
        // }
    }
    
    Connections {
        target: updater
        function onUpdateCompleted() {
            root.close()
        }
        function onUpdateFailed(error) {
            // 可以在这里添加错误处理
            console.log("Update failed:", error)
        }
    }
} 
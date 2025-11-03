import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import QtQuick.Dialogs 6.3
import Log_analyzer 1.0
import QtQuick.Window 2.15

ApplicationWindow {
    id: root
    visible: true
    width: 1200
    height: 800
    title: "车辆分析器"
    color: "#FFFFFF"

    // 使用 Material 主题
    Material.theme: Material.Light
    Material.accent: Material.Blue

    // 窗口属性
    minimumWidth: 800
    minimumHeight: 600
    
    // 页面管理
    property int currentPage: 0  // 0: 日志查看, 1: 车辆回看
    
    // 窗口位置居中
    Component.onCompleted: {
        x = Screen.width / 2 - width / 2
        y = Screen.height / 2 - height / 2
    }

    // 导航栏
    Rectangle {
        id: navigationBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 60
        color: "#FFFFFF"
        z: 100
        
        // 阴影效果
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: parent.height
            height: 2
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#20000000" }
                GradientStop { position: 1.0; color: "#00000000" }
            }
        }
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: 15
            spacing: 20
            
            // // 应用标题
            // Text {
            //     text: "车辆分析器"
            //     font.pixelSize: 20
            //     font.bold: true
            //     color: Material.accent
            //     Layout.alignment: Qt.AlignVCenter
            // }
            
            Item { Layout.fillWidth: true }
            
            // 导航按钮
            Row {
                spacing: 10
                
                // 查看日志按钮
                Rectangle {
                    id: logButton
                    width: 120
                    height: 40
                    radius: 8
                    color: currentPage === 0 ? Material.accent : "#F1F5F9"
                    border.color: currentPage === 0 ? Material.accent : "#E2E8F0"
                    border.width: 1
                    
                    Behavior on color {
                        ColorAnimation { duration: 200 }
                    }
                    
                    Row {
                        anchors.centerIn: parent
                        spacing: 8
                        
                        Text {
                            text: "📄"
                            font.pixelSize: 16
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        
                        Text {
                            text: "查看日志"
                            font.pixelSize: 14
                            font.bold: currentPage === 0
                            color: currentPage === 0 ? "#FFFFFF" : "#64748B"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        
                        onClicked: {
                            currentPage = 0
                        }
                        
                        onEntered: {
                            if (currentPage !== 0) {
                                parent.color = "#E2E8F0"
                            }
                        }
                        
                        onExited: {
                            if (currentPage !== 0) {
                                parent.color = "#F1F5F9"
                            }
                        }
                    }
                }
                
                // 车辆回看按钮
                Rectangle {
                    id: vehicleButton
                    width: 120
                    height: 40
                    radius: 8
                    color: currentPage === 1 ? Material.accent : "#F1F5F9"
                    border.color: currentPage === 1 ? Material.accent : "#E2E8F0"
                    border.width: 1
                    
                    Behavior on color {
                        ColorAnimation { duration: 200 }
                    }
                    
                    Row {
                        anchors.centerIn: parent
                        spacing: 8
                        
                        Text {
                            text: "🚗"
                            font.pixelSize: 16
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        
                        Text {
                            text: "车辆回看"
                            font.pixelSize: 14
                            font.bold: currentPage === 1
                            color: currentPage === 1 ? "#FFFFFF" : "#64748B"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        
                        onClicked: {
                            currentPage = 1
                        }
                        
                        onEntered: {
                            if (currentPage !== 1) {
                                parent.color = "#E2E8F0"
                            }
                        }
                        
                        onExited: {
                            if (currentPage !== 1) {
                                parent.color = "#F1F5F9"
                            }
                        }
                    }
                }
                
                // 黑盒子按钮
                Rectangle {
                    id: blackBoxButton
                    width: 120
                    height: 40
                    radius: 8
                    color: currentPage === 2 ? Material.accent : "#F1F5F9"
                    border.color: currentPage === 2 ? Material.accent : "#E2E8F0"
                    border.width: 1
                    
                    Behavior on color {
                        ColorAnimation { duration: 200 }
                    }
                    
                    Row {
                        anchors.centerIn: parent
                        spacing: 8
                        
                        Text {
                            text: "⚫" // 黑点图标
                            font.pixelSize: 16
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        
                        Text {
                            text: "黑盒子"
                            font.pixelSize: 14
                            font.bold: currentPage === 2
                            color: currentPage === 2 ? "#FFFFFF" : "#64748B"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        
                        onClicked: {
                            currentPage = 2 // 设置currentPage为2，表示黑盒子页面
                        }
                        
                        onEntered: {
                            if (currentPage !== 2) {
                                parent.color = "#E2E8F0"
                            }
                        }
                        
                        onExited: {
                            if (currentPage !== 2) {
                                parent.color = "#F1F5F9"
                            }
                        }
                    }
                }
            }
        }
    }

    // 更新通知卡片
    UpdateNotificationCard {
        id: updateNotificationCard
        anchors.centerIn: parent
        visible: false
        z: 1000
        
        // 信号处理
        onUpdateClicked: {
            if (appManager) { // 修改为 appManager
                appManager.CheckForUpdates() // 调用 AppManager 的 CheckForUpdates 函数
            }
            hide()
        }
        
        onCloseClicked: {
            hide()
        }
        
        onLaterClicked: {
            hide()
        }
    }

    // 页面容器
    Rectangle {
        id: pageContainer
        anchors.top: navigationBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: "#FFFFFF"
        
        // 日志查看页面
        TextAnalyzerPage {
            id: textAnalyzerPage
            anchors.fill: parent
            visible: currentPage === 0
            opacity: visible ? 1 : 0  // 必须设置opacity动画
            Behavior on opacity {
                NumberAnimation { duration: 300 }
            }
        }
        
        // 车辆回看页面
        VehicleReviewPage {
            id: vehicleReviewPage
            anchors.fill: parent
            visible: currentPage === 1
            opacity: visible ? 1 : 0 // 必须设置opacity动画
            Behavior on opacity {
                NumberAnimation { duration: 300 }
            }

            Loader {
                id: vehicleReviewLoader
                anchors.fill: parent
                source: "qrc:/VehicleReviewPage.qml"
                active: vehicleReviewPage.visible // 当容器可见时才加载组件
            }
        }

        // 黑盒子页面
        Rectangle {
            id: blackBoxPageContainer
            anchors.fill: parent
            visible: currentPage === 2
            opacity: visible ? 1 : 0
            Behavior on opacity {
                NumberAnimation { duration: 300 }
            }
            
            // 使用 Loader 来加载 BlackBoxPage.qml
            Loader {
                id: blackBoxLoader
                anchors.fill: parent
                source: "qrc:/BlackBoxPage.qml"
                active: blackBoxPageContainer.visible // 当容器可见时才加载组件
            }
        }
    }
    
    // 连接更新检查器信号
    Connections {
        target: updateChecker
        
        function onNewVersionFound(version, notes, downloadUrl, currentVer) {
            // 确保属性在主线程中设置
            Qt.callLater(function() {
                updateNotificationCard.newVersion = version
                updateNotificationCard.releaseNotes = notes
                updateNotificationCard.currentVersion = currentVer
                updateNotificationCard.show()
            })
        }
    }
}

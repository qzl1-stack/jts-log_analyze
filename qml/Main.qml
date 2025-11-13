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

    // 左侧导航栏
    Rectangle {
        id: navigationBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: 80
        color: "#FAFAFA"
        z: 100
        
        // 右侧分隔线
        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: "#E5E5E5"
        }
        
        // 导航按钮容器
        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 20
            spacing: 8
            
            // 查看日志按钮
            Rectangle {
                id: logButton
                width: parent.width
                height: 64
                color: currentPage === 0 ? "#E3F2FD" : "transparent"
                
                Behavior on color {
                    ColorAnimation { duration: 150 }
                }
                
                // 选中指示器
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 3
                    color: Material.accent
                    visible: currentPage === 0
                }
                
                Column {
                    anchors.centerIn: parent
                    spacing: 4
                    
                    // 文档图标
                    Canvas {
                        id: logIcon
                        width: 24
                        height: 24
                        anchors.horizontalCenter: parent.horizontalCenter
                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.clearRect(0, 0, width, height);
                            ctx.strokeStyle = currentPage === 0 ? Material.accent : "#666666";
                            ctx.lineWidth = 2;
                            ctx.beginPath();
                            // 文档主体
                            ctx.rect(4, 6, 12, 14);
                            // 折角
                            ctx.moveTo(4, 6);
                            ctx.lineTo(10, 6);
                            ctx.lineTo(10, 10);
                            ctx.lineTo(16, 10);
                            ctx.stroke();
                        }
                        Connections {
                            target: root
                            function onCurrentPageChanged() {
                                logIcon.requestPaint();
                            }
                        }
                    }
                    
                    Text {
                        text: "日志"
                        font.pixelSize: 12
                        color: currentPage === 0 ? Material.accent : "#666666"
                        anchors.horizontalCenter: parent.horizontalCenter
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
                            parent.color = "#F5F5F5"
                        }
                    }
                    
                    onExited: {
                        if (currentPage !== 0) {
                            parent.color = "transparent"
                        }
                    }
                }
            }
            
            // 车辆回看按钮
            Rectangle {
                id: vehicleButton
                width: parent.width
                height: 64
                color: currentPage === 1 ? "#E3F2FD" : "transparent"
                
                Behavior on color {
                    ColorAnimation { duration: 150 }
                }
                
                // 选中指示器
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 3
                    color: Material.accent
                    visible: currentPage === 1
                }
                
                Column {
                    anchors.centerIn: parent
                    spacing: 4
                    
                    // 回看图标（播放按钮+时间轴）
                    Canvas {
                        id: vehicleIcon
                        width: 24
                        height: 24
                        anchors.horizontalCenter: parent.horizontalCenter
                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.clearRect(0, 0, width, height);
                            ctx.strokeStyle = currentPage === 1 ? Material.accent : "#666666";
                            ctx.fillStyle = currentPage === 1 ? Material.accent : "#666666";
                            ctx.lineWidth = 2;
                            // 播放三角形
                            ctx.beginPath();
                            ctx.moveTo(8, 7);
                            ctx.lineTo(8, 17);
                            ctx.lineTo(16, 12);
                            ctx.closePath();
                            ctx.fill();
                            // 时间轴线条
                            ctx.beginPath();
                            ctx.moveTo(4, 12);
                            ctx.lineTo(6, 12);
                            ctx.stroke();
                        }
                        Connections {
                            target: root
                            function onCurrentPageChanged() {
                                vehicleIcon.requestPaint();
                            }
                        }
                    }
                    
                    Text {
                        text: "回看"
                        font.pixelSize: 12
                        color: currentPage === 1 ? Material.accent : "#666666"
                        anchors.horizontalCenter: parent.horizontalCenter
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
                            parent.color = "#F5F5F5"
                        }
                    }
                    
                    onExited: {
                        if (currentPage !== 1) {
                            parent.color = "transparent"
                        }
                    }
                }
            }
            
            // 黑盒子按钮
            Rectangle {
                id: blackBoxButton
                width: parent.width
                height: 64
                color: currentPage === 2 ? "#E3F2FD" : "transparent"
                
                Behavior on color {
                    ColorAnimation { duration: 150 }
                }
                
                // 选中指示器
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 3
                    color: Material.accent
                    visible: currentPage === 2
                }
                
                Column {
                    anchors.centerIn: parent
                    spacing: 4
                    
                    // 黑盒子图标（立方体）
                    Canvas {
                        id: blackBoxIcon
                        width: 24
                        height: 24
                        anchors.horizontalCenter: parent.horizontalCenter
                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.clearRect(0, 0, width, height);
                            ctx.strokeStyle = currentPage === 2 ? Material.accent : "#666666";
                            ctx.lineWidth = 2;
                            ctx.beginPath();
                            // 前面
                            ctx.rect(6, 8, 10, 10);
                            // 顶面
                            ctx.moveTo(6, 8);
                            ctx.lineTo(10, 4);
                            ctx.lineTo(20, 4);
                            ctx.lineTo(16, 8);
                            // 右侧
                            ctx.moveTo(16, 8);
                            ctx.lineTo(16, 18);
                            ctx.moveTo(20, 4);
                            ctx.lineTo(20, 14);
                            ctx.lineTo(16, 18);
                            ctx.stroke();
                        }
                        Connections {
                            target: root
                            function onCurrentPageChanged() {
                                blackBoxIcon.requestPaint();
                            }
                        }
                    }
                    
                    Text {
                        text: "黑盒"
                        font.pixelSize: 12
                        color: currentPage === 2 ? Material.accent : "#666666"
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
                
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    
                    onClicked: {
                        currentPage = 2
                    }
                    
                    onEntered: {
                        if (currentPage !== 2) {
                            parent.color = "#F5F5F5"
                        }
                    }
                    
                    onExited: {
                        if (currentPage !== 2) {
                            parent.color = "transparent"
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
        anchors.top: parent.top
        anchors.left: navigationBar.right
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

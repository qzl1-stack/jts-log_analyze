import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import Log_analyzer 1.0

VehicleReviewPage {
    id: root
    anchors.fill: parent
    
    Rectangle {
        anchors.fill: parent
        color: "#F5F5F5"
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 10
            
            
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
            
            // 信息面板
            // Rectangle {
            //     Layout.fillWidth: true
            //     Layout.preferredHeight: 80
            //     color: "#FFFFFF"
            //     border.color: "#E0E0E0"
            //     border.width: 1
            //     radius: 6
            //     visible: mapDataManager.isLoaded
                
            //     GridLayout {
            //         anchors.fill: parent
            //         anchors.margins: 15
            //         columns: 4
            //         rowSpacing: 5
            //         columnSpacing: 20
                    
            //         Text {
            //             text: "系统名称:"
            //             font.pixelSize: 12
            //             color: "#666666"
            //         }
            //         Text {
            //             text: mapDataManager.systemName || "未知"
            //             font.pixelSize: 12
            //             color: "#333333"
            //             font.bold: true
            //         }
                    
            //         Text {
            //             text: "布局名称:"
            //             font.pixelSize: 12
            //             color: "#666666"
            //         }
            //         Text {
            //             text: mapDataManager.layoutName || "未知"
            //             font.pixelSize: 12
            //             color: "#333333"
            //             font.bold: true
            //         }
                    
            //         Text {
            //             text: "路径段数:"
            //             font.pixelSize: 12
            //             color: "#666666"
            //         }
            //         Text {
            //             text: mapDataManager.segmentCount.toString()
            //             font.pixelSize: 12
            //             color: "#333333"
            //             font.bold: true
            //         }
                    
            //         Text {
            //             text: "关键点数:"
            //             font.pixelSize: 12
            //             color: "#666666"
            //         }
            //         Text {
            //             text: mapDataManager.pointCount.toString()
            //             font.pixelSize: 12
            //             color: "#333333"
            //             font.bold: true
            //         }
            //     }
            // }
        }
    }
} 
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Shapes 1.15
import QtQml 2.15
import Log_analyzer 1.0

Rectangle {
    id: mapViewer
    // 使用上下文提供的全局 mapDataManager（由 AppManager 暴露）
    property real zoomLevel: 1.0
    property real minZoom: 0.1
    property real maxZoom: 100.0
    property point panOffset: Qt.point(0, 0)
    property bool isDragging: false
    property point lastMousePos: Qt.point(0, 0)
    // 回放相关属性
    property var trackRaw: []                 // 原始车辆轨迹数据(QVariantMap数组)
    property var trackScreen: []              // 预计算的屏幕点(Qt.point)
    property var trackAngles: []              // 角度(度)
    property var trackOutOfSafe: []           // 是否越界
    property var trackTimestamps: []          // 时间戳(ms)
    // 车轮数据
    property var leftWheelSetSpeed: []        // 左轮设定速度
    property var leftWheelMeasuredSpeed: []   // 左轮测量速度
    property var leftWheelMileage: []         // 左轮里程
    property var rightWheelSetSpeed: []       // 右轮设定速度
    property var rightWheelMeasuredSpeed: []  // 右轮测量速度
    property var rightWheelMileage: []        // 右轮里程
    
    property int playIndex: 0                 // 当前帧索引
    property bool isPlaying: false            // 是否正在播放
    property real speedFactor: 1.0            // 播放倍速
    
    // 自动跟踪属性
    property bool autoFollowVehicle: false    // 是否开启自动跟踪
    property real autoFollowZoom: 100.0       // 自动跟踪时的缩放倍数
    
    // 图表缩放属性
    property real chartScaleX: 1.0             // 图表X轴缩放因子
    property real chartScaleY: 1.0             // 图表Y轴缩放因子
    property real minChartScale: 0.5           // 最小缩放倍数
    property real maxChartScale: 10.0           // 最大缩放倍数
    // 单图双轴：当前轮与分别的Y轴缩放
    property string currentWheel: "left"
    property real speedScaleY: 1.0
    property real mileageScaleY: 1.0
    // 双轴中心（数据值）与范围缓存（原始范围，含padding）
    property real speedCenterValue: NaN
    property real mileageCenterValue: NaN
    property var speedAxisCache: ({ min: 0, max: 1, span: 1 })
    property var mileageAxisCache: ({ min: 0, max: 1, span: 1 })
    
    // 条码相关数据
    property var barcodeData: []              // 存储二维码数据
    property var barcodeTimestamps: []       // 存储二维码时间戳
    
    // 监听playIndex变化，更新图表
    onPlayIndexChanged: {
        if (mapDataManager.vehicleTrackCount > 0 && !chartPanel.collapsed) {
            if (wheelChart) wheelChart.requestPaint()
        }
        
        // 自动跟踪小车：始终将其保持在视角中央
        if (autoFollowVehicle && trackScreen.length > 0 && playIndex >= 0 && playIndex < trackScreen.length) {
            updateVehicleTracking()
        }
    }
    
    
    onChartScaleYChanged: {
        if (mapDataManager.vehicleTrackCount > 0) {
            if (wheelChart) wheelChart.requestPaint()
        }
    }
    onSpeedScaleYChanged: { if (wheelChart) wheelChart.requestPaint() }
    onMileageScaleYChanged: { if (wheelChart) wheelChart.requestPaint() }
    property int trailLength: -1             // 尾迹长度(-1表示全路径)
    property int minFrameInterval: 1         // 最小帧间隔(ms)
    property int maxFrameInterval: 200        // 最大帧间隔(ms) - 已弃用，为了向后兼容保留
    property color normalColor: "#4CAF50"
    property color dangerColor: '#f41515'
    // 标记/标签显示参数（LOD）
    property int markerBaseSize: 8                 // 标记点的基准像素大小
    property int markerMinSize: 4                  // 缩小时的最小像素
    property int markerMaxSize: 12                 // 放大时的最大像素
    // 根据缩放分级抽稀标签：缩放越小，步进越大（越稀疏）
    function _labelStep() {
        var z = zoomLevel;
        if (z <= 0.4) return 50;
        if (z <= 0.6) return 30;
        if (z <= 0.8) return 20;
        if (z <= 1.0) return 12;
        if (z <= 1.5) return 8;
        if (z <= 2.0) return 5;
        if (z <= 3.0) return 3;
        return 1;
    }
    
    color: "#f5f5f5"
    clip: true
    focus: true
    
    // 快捷键处理 - 改进版本，支持焦点链传递
    Keys.onPressed: function(event) {
        // Ctrl+S: 适配视图
        if ((event.key === Qt.Key_Z) && (event.modifiers & Qt.ControlModifier)) {
            console.log("快捷键 Ctrl+Z 触发")
            fitMapToView()
            event.accepted = true
            return
        }
    }
    
    // 错误提示文本
    Text {
        id: errorText
        anchors.centerIn: parent
        text: ""
        color: "red"
        font.pixelSize: 16
        visible: false
        wrapMode: Text.WordWrap
        width: parent.width * 0.8
        horizontalAlignment: Text.AlignHCenter
    }
    
    // 加载指示器
    BusyIndicator {
        id: loadingIndicator
        anchors.centerIn: parent
        running: !mapDataManager.isLoaded && !errorText.visible
        visible: running
    }
    
    // 地图渲染区域
    Item {
        id: mapContainer
        anchors.fill: parent
        
        transform: [
            Scale {
                id: scaleTransform
                xScale: mapViewer.zoomLevel
                yScale: mapViewer.zoomLevel
                origin.x: mapViewer.width / 2
                origin.y: mapViewer.height / 2
            },
            Translate {
                id: translateTransform
                x: mapViewer.panOffset.x
                y: mapViewer.panOffset.y
            }
        ]
        
        // 主要地图形状
        Shape {
            id: mapShape
            anchors.fill: parent
            antialiasing: true
            // layer.enabled: true
            layer.samples: 4
            layer.smooth: true
            z: 1
            
            // 保留一个透明的占位路径，便于清理时保留至少一个子对象
            ShapePath {
                id: mainPath
                strokeColor: "#2196F3"
                strokeWidth: Math.max(0.05, 2.5 / mapViewer.zoomLevel)  // 逆向缩放保持恒定宽度
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg { id: svgPath; path: "" }
            }
            
            // 辅助：判断两点是否近似相等
            function _near(p, q) {
                var dx = p.x - q.x; var dy = p.y - q.y;
                return (dx*dx + dy*dy) < 0.25; // 0.5px 阈值
            }
            // 辅助：Catmull‑Rom（uniform）转多段Cubic Bezier，穿过所有点
            function catmullRomToBezier(points) {
                var out = [];
                if (!points || points.length < 2)
                    return out;
                // 端点填充
                var pts = [];
                pts.push(points[0]);
                for (var i = 0; i < points.length; i++) pts.push(points[i]);
                pts.push(points[points.length - 1]);
                for (var k = 1; k < pts.length - 2; k++) {
                    var p0 = pts[k-1];
                    var p1 = pts[k];
                    var p2 = pts[k+1];
                    var p3 = pts[k+2];
                    var c1 = Qt.point(p1.x + (p2.x - p0.x) / 6.0, p1.y + (p2.y - p0.y) / 6.0);
                    var c2 = Qt.point(p2.x - (p3.x - p1.x) / 6.0, p2.y - (p3.y - p1.y) / 6.0);
                    out.push({ c1: c1, c2: c2, p: p2 });
                }
                return out;
            }
            
            // 动态创建所有段的路径
            function createPaths() {
                console.log("createPaths called, mapDataManager:", mapDataManager);
                if (!mapDataManager || !mapDataManager.isLoaded) {
                    console.log("mapDataManager not ready or not loaded");
                    return;
                }
                if (mapViewer.width <= 0 || mapViewer.height <= 0) {
                    console.log("viewer size not ready, delaying build...");
                    Qt.callLater(function(){ mapShape.createPaths(); });
                    return;
                }
                
                var segments = mapDataManager.getSegmentPaths();
                console.log("segments count:", segments ? segments.length : "null");
                if (!segments || segments.length === 0) {
                    console.log("no segments available");
                    svgPath.path = "";
                    return;
                }
                
                var d = "";
                for (var j = 0; j < segments.length; j++) {
                    var segmentData = segments[j];
                    if (!segmentData || !segmentData.parts)
                        continue;
                    var parts = segmentData.parts;
                    var started = false;
                    var lastScreen = Qt.point(0,0);
                    for (var i = 0; i < parts.length; i++) {
                        var part = parts[i];
                        if (part.type === "Point") {
                            var p = mapDataManager.mapToScene(
                                Qt.point(part.x, part.y),
                                Qt.rect(0, 0, mapViewer.width, mapViewer.height), 1.0);
                            if (!started) {
                                d += "M " + p.x + " " + p.y + " ";
                                started = true;
                            } else if (!_near(p, lastScreen)) {
                                d += "L " + p.x + " " + p.y + " ";
                            }
                            lastScreen = p;
                        } else if (part.type === "Spline" && part.controlPoints && part.controlPoints.length >= 2) {
                            // 将控制点解释为经由点（穿点），并转换为Bezier段
                            var pts = [];
                            for (var t = 0; t < part.controlPoints.length; t++) {
                                var sp = mapDataManager.mapToScene(
                                    Qt.point(part.controlPoints[t].x, part.controlPoints[t].y),
                                    Qt.rect(0,0,mapViewer.width,mapViewer.height), 1.0);
                                pts.push(sp);
                            }
                            if (started && pts.length > 0 && !_near(pts[0], lastScreen)) {
                                // 若样条首点不等于当前端点，则补上当前端点，保证连续
                                pts.unshift(lastScreen);
                            }
                            if (!started && pts.length > 0) {
                                d += "M " + pts[0].x + " " + pts[0].y + " ";
                                started = true;
                            }
                            var beziers = catmullRomToBezier(pts);
                            for (var b = 0; b < beziers.length; b++) {
                                var seg = beziers[b];
                                d += "C " + seg.c1.x + " " + seg.c1.y + ", " + seg.c2.x + " " + seg.c2.y + ", " + seg.p.x + " " + seg.p.y + " ";
                                lastScreen = seg.p;
                            }
                        }
                    }
                }
                svgPath.path = d;
                console.log("svg total length:", d.length, " sample:", d.substring(0, 80));
            }
            
        }
        
        
        // 位置标记显示
        Repeater {
            id: markerRepeater
            model: mapDataManager.isLoaded ? mapDataManager.getPositionMarkers() : []
            
            delegate: Item {
                property var markerData: modelData
                property point scenePos: mapDataManager.mapToScene(
                    Qt.point(markerData.x, markerData.y),
                    Qt.rect(0, 0, mapViewer.width, mapViewer.height),
                    1.0
                )
                
                x: scenePos.x
                y: scenePos.y
                z: 100  // 提高z值确保不被其他元素遮挡
                
                // 位置标记圆点
                Rectangle {
                    id: markerDot
                    anchors.centerIn: parent
                    // 保持屏幕像素大小稳定：使用逆缩放
                    width: Math.min(mapViewer.markerMaxSize, Math.max(mapViewer.markerMinSize, mapViewer.markerBaseSize))
                    height: width
                    radius: width/2
                    color: mapViewer.zoomLevel < 0.9 ? "transparent" : "#804CAF50" // 低倍缩放时用空心圈，放大后半透明实心
                    border.color: "#2E7D32"
                    border.width: mapViewer.zoomLevel < 0.9 ? 1 : 2
                    opacity: 1.0
                    transform: Scale {
                        // 抵消整体地图缩放，使标记点大小不随缩放变化
                        xScale: 1.0 / scaleTransform.xScale
                        yScale: 1.0 / scaleTransform.yScale
                        origin.x: markerDot.width/2
                        origin.y: markerDot.height/2
                    }
                }
                
                // 标记ID文本
                Rectangle {
                    anchors.left: parent.right
                    anchors.leftMargin: 1
                    anchors.verticalCenter: parent.verticalCenter
                    width: labelText.width + 6
                    height: labelText.height + 4
                    color: 'transparent'
                    border.width: 0
                    // 标签分级抽稀显示，避免拥挤
                    visible: (index % mapViewer._labelStep() === 0)
                    transform: Scale {
                        // 标签也使用逆缩放，保持像素不变
                        xScale: 1.0 / scaleTransform.xScale
                        yScale: 1.0 / scaleTransform.yScale
                        origin.x: 0
                        origin.y: (labelText.height + 4)/2
                    }
                    
                    Text {
                        id: labelText
                        anchors.centerIn: parent
                        text: markerData.id
                        color: '#111111'
                        font.pixelSize: 11
                        style: Text.Outline
                        styleColor: "#ffffff"
                        // font.bold: true
                    }
                }
            }
        }
        
        // 车辆轨迹线显示
        Shape {
            id: vehicleTrackShape
            anchors.fill: parent
            antialiasing: true
            // layer.enabled: true
            layer.samples: 4
            layer.smooth: true
            z: 3
            visible: mapDataManager.vehicleTrackCount > 0
            
            // 安全段（正常）
            ShapePath {
                id: trackPathSafe
                strokeColor: Qt.rgba(1, 0.62, 0.01)   // 橙色
                strokeWidth: Math.max(0.04, 2 / mapViewer.zoomLevel)  // 逆向缩放保持恒定宽度
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg { id: trackSvgSafe; path: "" }
            }
            // 危险段（越界）
            ShapePath {
                id: trackPathDanger
                strokeColor: "#FF3B30"   // 红色
                strokeWidth: Math.max(0.04, 2 / mapViewer.zoomLevel)  // 逆向缩放保持恒定宽度
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg { id: trackSvgDanger; path: "" }
            }
            
            // 根据索引区间生成局部轨迹(用于动态回放)
            function updatePartialPath(startIdx, endIdx) {
                if (!mapViewer.trackScreen || mapViewer.trackScreen.length === 0) {
                    trackSvgSafe.path = "";
                    trackSvgDanger.path = "";
                    return;
                }
                startIdx = Math.max(0, startIdx);
                endIdx = Math.min(endIdx, mapViewer.trackScreen.length - 1);
                if (endIdx <= startIdx) {
                    trackSvgSafe.path = "";
                    trackSvgDanger.path = "";
                    return;
                }
                var dSafe = "";
                var dDanger = "";
                for (var i = startIdx + 1; i <= endIdx; i++) {
                    var p0 = mapViewer.trackScreen[i-1];
                    var p1 = mapViewer.trackScreen[i];
                    var o0 = (mapViewer.trackOutOfSafe && mapViewer.trackOutOfSafe.length > (i-1)) ? !!mapViewer.trackOutOfSafe[i-1] : false;
                    var o1 = (mapViewer.trackOutOfSafe && mapViewer.trackOutOfSafe.length > i) ? !!mapViewer.trackOutOfSafe[i] : false;
                    var danger = o0 || o1;
                    if (danger) {
                        dDanger += "M " + p0.x + " " + p0.y + " L " + p1.x + " " + p1.y + " ";
                    } else {
                        dSafe += "M " + p0.x + " " + p0.y + " L " + p1.x + " " + p1.y + " ";
                    }
                }
                trackSvgSafe.path = dSafe;
                trackSvgDanger.path = dDanger;
            }
        }
        
        // 完整轨迹预览（半透明显示整个行动路线）
        Shape {
            id: fullTrackPreviewShape
            anchors.fill: parent
            antialiasing: true
            layer.samples: 4
            layer.smooth: true
            z: 2
            visible: mapDataManager.vehicleTrackCount > 0
            
            ShapePath {
                id: fullTrackPreviewPath
                strokeColor: Qt.rgba(0.69, 0.69, 0.68, 0.7)  // 灰色，54%透明度
                strokeWidth: Math.max(0.12, 2 / mapViewer.zoomLevel)  // 逆向缩放保持恒定宽度
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg { id: fullTrackSvg; path: "" }
            }
            
            // 生成完整轨迹路径
            function generateFullTrackPath() {
                if (!mapViewer.trackScreen || mapViewer.trackScreen.length === 0) {
                    fullTrackSvg.path = "";
                    return;
                }
                
                var d = "";
                if (mapViewer.trackScreen.length > 0) {
                    d = "M " + mapViewer.trackScreen[0].x + " " + mapViewer.trackScreen[0].y + " ";
                    for (var i = 1; i < mapViewer.trackScreen.length; i++) {
                        d += "L " + mapViewer.trackScreen[i].x + " " + mapViewer.trackScreen[i].y + " ";
                    }
                }
                fullTrackSvg.path = d;
            }
        }
        

        // 当前车辆位置和状态显示（单个动态点）
        Item {
            id: currentVehicle
            visible: mapViewer.trackScreen.length > 0
            x: visible ? mapViewer.trackScreen[mapViewer.playIndex].x : 0
            y: visible ? mapViewer.trackScreen[mapViewer.playIndex].y : 0
            z: 4

            // 车辆位置点
            Rectangle {
                id: vehicleDot
                anchors.centerIn: parent
                width: 12
                height: 12
                radius: 6
                color: mapViewer.trackOutOfSafe[mapViewer.playIndex] ? mapViewer.dangerColor : mapViewer.normalColor
                border.color: "#FFFFFF"
                border.width: 2
                opacity: 0.9
                transform: Scale {
                    xScale: 1.0 / scaleTransform.xScale
                    yScale: 1.0 / scaleTransform.yScale
                    origin.x: vehicleDot.width/2
                    origin.y: vehicleDot.height/2
                }
            }

            // 小车模型（俯视图上帝视角）
            Canvas {
                id: directionArrow
                anchors.centerIn: parent
                width: 48
                height: 60
                transform: [
                    Scale {
                        xScale: 1.0 / scaleTransform.xScale
                        yScale: 1.0 / scaleTransform.yScale
                        origin.x: directionArrow.width/2
                        origin.y: directionArrow.height/2
                    },
                    Rotation {
                        angle: convertToQtAngle(mapViewer.trackAngles[mapViewer.playIndex] || 0)
                        origin.x: directionArrow.width/2
                        origin.y: directionArrow.height/2
                    }
                ]
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    
                    var isOutOfSafe = mapViewer.trackOutOfSafe[mapViewer.playIndex];
                    var mainColor = isOutOfSafe ? 'rgba(255, 59, 48, 0.5)' : 'rgba(33, 150, 243, 0.5)';
                    var accentColor = isOutOfSafe ? 'rgba(255, 59, 48, 0.8)' : 'rgba(33, 150, 243, 0.8)';
                    var lightColor = isOutOfSafe ? 'rgba(255, 59, 48, 0.3)' : 'rgba(33, 150, 243, 0.3)';
                    
                    var w = width;
                    var h = height;
                    var cx = w / 2;
                    var cy = h / 2;
                    
                    // === AGV俯视图设计 ===
                    // 1. 车身主体（圆润矩形）
                    ctx.fillStyle = lightColor;
                    ctx.strokeStyle = mainColor;
                    ctx.lineWidth = 1.2;
                    var radius = w * 0.08;
                    var rectX = cx - w * 0.25;
                    var rectY = cy - h * 0.40;
                    var rectW = w * 0.50;
                    var rectH = h * 0.55;
                    
                    ctx.beginPath();
                    ctx.moveTo(rectX + radius, rectY);
                    ctx.lineTo(rectX + rectW - radius, rectY);
                    ctx.quadraticCurveTo(rectX + rectW, rectY, rectX + rectW, rectY + radius);
                    ctx.lineTo(rectX + rectW, rectY + rectH - radius);
                    ctx.quadraticCurveTo(rectX + rectW, rectY + rectH, rectX + rectW - radius, rectY + rectH);
                    ctx.lineTo(rectX + radius, rectY + rectH);
                    ctx.quadraticCurveTo(rectX, rectY + rectH, rectX, rectY + rectH - radius);
                    ctx.lineTo(rectX, rectY + radius);
                    ctx.quadraticCurveTo(rectX, rectY, rectX + radius, rectY);
                    ctx.closePath();
                    ctx.fill();
                    ctx.stroke();
                    
                    
                    // 3. 车身中心线（分隔线，增加立体感）
                    ctx.strokeStyle = mainColor;
                    ctx.lineWidth = 0.6;
                    ctx.setLineDash([3, 3]);
                    ctx.beginPath();
                    ctx.moveTo(cx, cy - h*0.40);
                    ctx.lineTo(cx, cy + h*0.15);
                    ctx.stroke();
                    ctx.setLineDash([]);
                    
                    // 4. 左轮（矩形轮胎）
                    ctx.fillStyle = mainColor;
                    ctx.strokeStyle = accentColor;
                    ctx.lineWidth = 0.8;
                    ctx.fillRect(cx - w*0.35, cy - h*0.25, w*0.08, h*0.28);
                    ctx.strokeRect(cx - w*0.35, cy - h*0.25, w*0.08, h*0.28);
                    
                    // 5. 右轮（矩形轮胎）
                    ctx.fillRect(cx + w*0.27, cy - h*0.25, w*0.08, h*0.28);
                    ctx.strokeRect(cx + w*0.27, cy - h*0.25, w*0.08, h*0.28);
                    
                    
                    // 7. 中心位置点
                    ctx.fillStyle = accentColor;
                    ctx.beginPath();
                    ctx.arc(cx, cy, 1.5, 0, Math.PI * 2);
                    ctx.fill();
                  
                }
            }

            // 二维码扫描提示（简洁闪烁提示）
            Rectangle {
                id: barcodeNotification
                anchors.left: parent.right
                anchors.leftMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                width: barcodeText.width + 12
                height: barcodeText.height + 8
                
                // 判断当前时间戳是否扫过二维码
                readonly property int currentBarcode: {
                    if (mapViewer.playIndex < 0 || mapViewer.playIndex >= mapViewer.trackRaw.length) {
                        return 0;
                    }
                    var point = mapViewer.trackRaw[mapViewer.playIndex];
                    // 确保 barcode 属性存在且有效，否则返回 0
                    if (point && typeof point.barcode !== 'undefined' && point.barcode > 0) {
                        return point.barcode;
                    }
                    return 0;
                }
                visible: currentBarcode !== 0
                
                color: "#333333"  // 深色背景
                border.color: "#2196F3"  // 蓝色边框
                border.width: 1
                radius: 4
                
                transform: Scale {
                    xScale: 1.0 / scaleTransform.xScale
                    yScale: 1.0 / scaleTransform.yScale
                    origin.x: 0
                    origin.y: barcodeNotification.height / 2
                }
                
                Text {
                    id: barcodeText
                    anchors.centerIn: parent
                    text: "已扫码:" + barcodeNotification.currentBarcode
                    color: "#2196F3"
                    font.pixelSize: 11
                    font.bold: true
                }
            }

        }
    }
    
    // 位置标记坐标标签（全局，在mapContainer外）
    Rectangle {
        id: markerCoordLabel
        width: coordLabelText.width + 12
        height: coordLabelText.height + 8
        color: "#FFFFFF"
        border.color: "#2196F3"
        border.width: 1
        radius: 4
        opacity: 0.95
        visible: false
        z: 9999  // 最高z值，确保显示在最上面
        
        property string currentCoord: ""
        
        Text {
            id: coordLabelText
            anchors.centerIn: parent
            font.pixelSize: 10
            color: "#333333"
            text: parent.currentCoord
        }
    }
    
    // 播放定时器（逐帧调度）
    Timer {
        id: playTimer
        repeat: false
        running: false
        interval: 30
        onTriggered: nextFrame()
    }
    
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        
        // 确保焦点回传给主容器，以便接收快捷键
        onClicked: mapViewer.focus = true
        
        onPressed: function(mouse) {
            // 先检测是否点击到任一marker
            var centerX = mapViewer.width / 2
            var centerY = mapViewer.height / 2
            var markers = mapDataManager.getPositionMarkers()
            if (markers && markers.length > 0) {
                for (var i = 0; i < markers.length; i++) {
                    var marker = markers[i]
                    // 地图坐标 -> 视图坐标（未应用transform的基础坐标）
                    var base = mapDataManager.mapToScene(Qt.point(marker.x, marker.y), Qt.rect(0, 0, mapViewer.width, mapViewer.height), 1.0)
                    // 应用当前Scale(以中心为原点)与Translate
                    var screenX = centerX + mapViewer.zoomLevel * (base.x - centerX) + mapViewer.panOffset.x
                    var screenY = centerY + mapViewer.zoomLevel * (base.y - centerY) + mapViewer.panOffset.y
                    var dx = mouse.x - screenX
                    var dy = mouse.y - screenY
                    if (dx*dx + dy*dy <= 25*25) { // 25px点击半径
                        console.log("Marker clicked:", marker.x, marker.y)
                        markerCoordLabel.currentCoord = "(" + marker.x.toFixed(0) + ", " + marker.y.toFixed(0) + ")"
                        markerCoordLabel.x = screenX - markerCoordLabel.width/2
                        markerCoordLabel.y = screenY - markerCoordLabel.height - 10
                        markerCoordLabel.visible = true
                        // 不开始拖拽
                        mapViewer.isDragging = false
                        return
                    }
                }
            }
            // 未点中marker，则进入拖拽
            mapViewer.isDragging = true
            mapViewer.lastMousePos = Qt.point(mouse.x, mouse.y)
        }
        
        onReleased: function(mouse) {
            mapViewer.isDragging = false
            markerCoordLabel.visible = false
        }
        
        onPositionChanged: function(mouse) {
            if (mapViewer.isDragging && (mouse.buttons & Qt.LeftButton)) {
                var deltaX = mouse.x - mapViewer.lastMousePos.x
                var deltaY = mouse.y - mapViewer.lastMousePos.y
                mapViewer.panOffset = Qt.point(
                    mapViewer.panOffset.x + deltaX,
                    mapViewer.panOffset.y + deltaY
                )
                mapViewer.lastMousePos = Qt.point(mouse.x, mouse.y)
            }
        }
        
        onWheel: function(wheel) {
            var scaleFactor = wheel.angleDelta.y > 0 ? 1.1 : 0.9
            var oldZoom = mapViewer.zoomLevel
            var newZoom = Math.max(mapViewer.minZoom, Math.min(mapViewer.maxZoom, oldZoom * scaleFactor))
            if (newZoom === oldZoom) return
            
            var centerX = mapViewer.width / 2
            var centerY = mapViewer.height / 2
            var dx = wheel.x - centerX
            var dy = wheel.y - centerY
            var zoomRatio = newZoom / oldZoom
            // 关键：T' = T + (1 - zoomRatio) * ((mouse-center) - T)
            mapViewer.panOffset = Qt.point(
                mapViewer.panOffset.x + (1 - zoomRatio) * (dx - mapViewer.panOffset.x),
                mapViewer.panOffset.y + (1 - zoomRatio) * (dy - mapViewer.panOffset.y)
            )
            mapViewer.zoomLevel = newZoom
        }
    }
    
    // 播放控制面板
    Rectangle {
        id: playbackPanel
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 10
        width: playbackColumn.width + 20
        height: playbackColumn.height + 20
        color: "#FFFFFF"
        border.color: "#CCCCCC"
        border.width: 1
        radius: 6
        opacity: 0.9
        visible: mapDataManager.vehicleTrackCount > 0
        
        Column {
            id: playbackColumn
            anchors.centerIn: parent
            spacing: 8
            
            // 时间显示区域
            Rectangle {
                width: parent.width - 16
                height: 20
                color: "#F5F5F5"
                border.color: "#E0E0E0"
                border.width: 1
                radius: 4
                anchors.horizontalCenter: parent.horizontalCenter
                
                Column {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 2
                    
                    Text {
                        text: "当前时间: " + formatTime(mapViewer.playIndex)
                        font.pixelSize: 10
                        color: "#333333"
                    }
                
                }
            }
            
            // 播放、暂停、停止控制
            Row {
                spacing: 6
                anchors.horizontalCenter: parent.horizontalCenter
                Button { 
                    text: "▶ 播放"
                    width: 60
                    height: 28
                    onClicked: startPlayback() 
                }
                Button { 
                    text: "⏸ 暂停"
                    width: 60
                    height: 28
                    onClicked: pausePlayback() 
                }
                Button { 
                    text: "⏹ 停止"
                    width: 60
                    height: 28
                    onClicked: stopPlayback() 
                }
            }
            
            // 上帧、下帧控制
            Row {
                spacing: 6
                anchors.horizontalCenter: parent.horizontalCenter
                Button { 
                    text: "◀ 上帧"
                    width: 60
                    height: 28
                    onClicked: stepBackward() 
                }
                Button { 
                    text: "下帧 ▶"
                    width: 60
                    height: 28
                    onClicked: stepForward() 
                }
            }
            
            // 倍速控制
            Row {
                spacing: 6
                anchors.horizontalCenter: parent.horizontalCenter
                Text { 
                    text: "倍速："
                    font.pixelSize: 11
                    verticalAlignment: Text.AlignVCenter
                    width: 40
                }
                ComboBox {
                    id: speedBox
                    width: 70
                    model: [0.25, 0.5, 1.0, 2.0, 4.0]
                    currentIndex: 2
                    onActivated: mapViewer.speedFactor = parseFloat(currentText)
                }
            }
            

            
            // 自动跟踪控制
            Button {
                text: mapViewer.autoFollowVehicle ? "🎯 跟踪中" : "🎯 启用跟踪"
                width: 120
                height: 28
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: {
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
    

    
    Rectangle {
        id: statusRect
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 10
        width: statusText.width + 20
        height: statusText.height + 10
        color: "#FFFFFF"
        border.color: "#CCCCCC"
        border.width: 1
        radius: 4
        opacity: 0.9
        visible: mapDataManager.isLoaded
        
        Text {
            id: statusText
            anchors.centerIn: parent
            text: "段数: " + mapDataManager.segmentCount + 
                  " | 点数: " + mapDataManager.pointCount + 
                  " | 轨迹点: " + mapDataManager.vehicleTrackCount +
                  " | 缩放: " + (mapViewer.zoomLevel * 100).toFixed(0) + "%"
            font.pixelSize: 10
            color: "#666666"
        }
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: statusRect.right
        anchors.margins: 10
        width: statusTextRight.width + 20
        height: statusTextRight.height + 10
        color: "#FFFFFF"
        border.color: "#CCCCCC"
        border.width: 1
        radius: 4
        opacity: 0.9
        visible: mapDataManager.isLoaded
        
        Text {
            id: statusTextRight
            anchors.centerIn: parent
            text: "布局名称: " + mapDataManager.layoutName
            font.pixelSize: 10
            color: "#666666"
        }
    }
    
    // 车轮数据图表面板
    Rectangle {
        id: chartPanel
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 50
        anchors.bottomMargin: 50
        width: collapsed ? 40 : 600
        color: "#FFFFFF"
        border.color: "#CCCCCC"
        border.width: 1
        radius: 6
        opacity: 0.95
        visible: mapDataManager.vehicleTrackCount > 0
        z: 10
        
        property bool collapsed: true
        
        Behavior on width {
            NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
        }
        
        // 折叠/展开按钮
        Button {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 5
            width: 30
            height: 30
            text: chartPanel.collapsed ? "◀" : "▶"
            onClicked: chartPanel.collapsed = !chartPanel.collapsed
        }
        
        // 单图（左轮/右轮切换，双纵轴）
        Column {
            id: singleChartContainer
            anchors.fill: parent
            anchors.margins: 10
            anchors.topMargin: 45
            spacing: 10
            visible: !chartPanel.collapsed

            Text {
                text: (mapViewer.currentWheel === "left" ? "左轮" : "右轮") + " · 设定/测量/差速/里程"
                font.bold: true
                font.pixelSize: 14
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Canvas {
                id: wheelChart
                width: parent.width - 20
                height: 300
                onPaint: {
                    var active = getActiveWheelSeries()
                    var s = active.series
                    if (!s || s.length === 0 || s[0].length === 0) return
                    var labels = active.labels
                    var colors = ["#2196F3", "#4CAF50", "#FF5722", "#FF9800"]
                    var yMap = [0, 0, 0, 1]
                    drawChart(this, s, labels, colors, yMap, playIndex)
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onPositionChanged: function(mouse) {
                        var total = mapViewer.trackTimestamps.length
                        if (total === 0) return
                        var px = mouse.x - 44
                        var ww = Math.max(1, parent.width - 88)
                        var index = Math.floor(px / ww * total)
                        index = Math.max(0, Math.min(index, total - 1))
                        wheelTooltip.x = mouse.x + 10
                        wheelTooltip.y = mouse.y + 10
                        wheelTooltip.currentIndex = index
                        wheelTooltip.visible = true
                    }
                    onExited: { wheelTooltip.visible = false }
                    onClicked: function(mouse) {
                        var total = mapViewer.trackTimestamps.length
                        if (total === 0) return
                        var px = mouse.x - 44
                        var ww = Math.max(1, parent.width - 88)
                        var index = Math.floor(px / ww * total)
                        index = Math.max(0, Math.min(index, total - 1))
                        playIndex = index
                        updateTrackVisual()
                    }
                    onWheel: function(wheel) {
                        var z = wheel.angleDelta.y > 0 ? 1.1 : 0.9
                        // 计算图表绘图区
                        var padding = { left: 44, right: 44, top: 12, bottom: 22 }
                        var chartH = Math.max(1, parent.height - padding.top - padding.bottom)
                        // 鼠标相对绘图区的Y（0顶部）与归一化（0..1）
                        var mouseY = Math.max(0, Math.min(wheel.y - padding.top, chartH))
                        var normY = mouseY / chartH
                        // 速度轴（左轴）
                        var lSpanFull = mapViewer.speedAxisCache.span
                        var lOldSpan = lSpanFull / Math.max(0.1, mapViewer.speedScaleY)
                        var lOldCenter = mapViewer.speedCenterValue === mapViewer.speedCenterValue ? mapViewer.speedCenterValue : (mapViewer.speedAxisCache.min + mapViewer.speedAxisCache.max) / 2
                        var lOldMax = lOldCenter + lOldSpan / 2
                        var mouseValL = lOldMax - normY * lOldSpan
                        var lNewSpan = lOldSpan / z
                        var lNewCenter = mouseValL + (normY - 0.5) * lNewSpan
                        // 约束中心
                        var lMinCenter = mapViewer.speedAxisCache.min + lNewSpan / 2
                        var lMaxCenter = mapViewer.speedAxisCache.max - lNewSpan / 2
                        if (lMinCenter > lMaxCenter) { lMinCenter = lMaxCenter = (mapViewer.speedAxisCache.min + mapViewer.speedAxisCache.max) / 2 }
                        lNewCenter = Math.max(lMinCenter, Math.min(lMaxCenter, lNewCenter))
                        mapViewer.speedCenterValue = lNewCenter
                        mapViewer.speedScaleY = Math.max(minChartScale, Math.min(maxChartScale, lSpanFull / Math.max(1e-6, lNewSpan)))
                        // 里程轴（右轴）
                        var rSpanFull = mapViewer.mileageAxisCache.span
                        var rOldSpan = rSpanFull / Math.max(0.1, mapViewer.mileageScaleY)
                        var rOldCenter = mapViewer.mileageCenterValue === mapViewer.mileageCenterValue ? mapViewer.mileageCenterValue : (mapViewer.mileageAxisCache.min + mapViewer.mileageAxisCache.max) / 2
                        var rOldMax = rOldCenter + rOldSpan / 2
                        var mouseValR = rOldMax - normY * rOldSpan
                        var rNewSpan = rOldSpan / z
                        var rNewCenter = mouseValR + (normY - 0.5) * rNewSpan
                        var rMinCenter = mapViewer.mileageAxisCache.min + rNewSpan / 2
                        var rMaxCenter = mapViewer.mileageAxisCache.max - rNewSpan / 2
                        if (rMinCenter > rMaxCenter) { rMinCenter = rMaxCenter = (mapViewer.mileageAxisCache.min + mapViewer.mileageAxisCache.max) / 2 }
                        rNewCenter = Math.max(rMinCenter, Math.min(rMaxCenter, rNewCenter))
                        mapViewer.mileageCenterValue = rNewCenter
                        mapViewer.mileageScaleY = Math.max(minChartScale, Math.min(maxChartScale, rSpanFull / Math.max(1e-6, rNewSpan)))
                        wheel.accepted = true
                    }
                }
                Rectangle {
                    id: wheelTooltip
                    width: tooltipText.width + 12
                    height: tooltipText.height + 8
                    color: "#FFFFFF"
                    border.color: "#CCCCCC"
                    border.width: 1
                    radius: 4
                    visible: false
                    z: 100
                    property int currentIndex: 0
                    Text {
                        id: tooltipText
                        anchors.centerIn: parent
                        font.pixelSize: 9
                        text: {
                            if (!wheelTooltip.visible) return ""
                            var idx = wheelTooltip.currentIndex
                            var active = getActiveWheelSeries()
                            var s = active.series
                            if (idx >= s[0].length) return ""
                            var setV = s[0][idx]
                            var meaV = s[1][idx]
                            var deltaV = s[2][idx]
                            var milV = s[3][idx]
                            return "时间: " + formatTime(idx) + "\n" +
                                   "设定速度: " + setV.toFixed(2) + "\n" +
                                   "测量速度: " + meaV.toFixed(2) + "\n" +
                                   "差速: " + deltaV.toFixed(2) + "\n" +
                                   "里程: " + milV.toFixed(2)
                        }
                    }
                }
            }

            Row {
                spacing: 10
                anchors.horizontalCenter: parent.horizontalCenter
                Row { spacing: 3; Rectangle { width: 15; height: 10; color: "#2196F3" } Text { text: "设定速度"; font.pixelSize: 9 } }
                Row { spacing: 3; Rectangle { width: 15; height: 10; color: "#4CAF50" } Text { text: "测量速度"; font.pixelSize: 9 } }
                Row { spacing: 3; Rectangle { width: 15; height: 10; color: "#FF5722" } Text { text: "差速"; font.pixelSize: 9 } }
                Row { spacing: 3; Rectangle { width: 15; height: 10; color: "#FF9800" } Text { text: "里程"; font.pixelSize: 9 } }
            }

            Row {
                spacing: 8
                anchors.horizontalCenter: parent.horizontalCenter
                Button { text: mapViewer.currentWheel === "left" ? "左轮(当前)" : "左轮"; onClicked: { mapViewer.currentWheel = "left"; wheelChart.requestPaint() } }
                Button { text: mapViewer.currentWheel === "right" ? "右轮(当前)" : "右轮"; onClicked: { mapViewer.currentWheel = "right"; wheelChart.requestPaint() } }
            }
        }

       
        
    }
    
    function loadMap() {
        errorText.visible = false
        mapDataManager.loadMapData()
    }
    
    function loadVehicleTrack() {
        errorText.visible = false
        mapDataManager.loadVehicleTrack()
    }

    // 预计算轨迹缓存
    function buildTrackCache() {
        trackRaw = mapDataManager.getVehicleTrack()
        trackScreen = []
        trackAngles = []
        trackOutOfSafe = []
        trackTimestamps = []
        leftWheelSetSpeed = []
        leftWheelMeasuredSpeed = []
        leftWheelMileage = []
        rightWheelSetSpeed = []
        rightWheelMeasuredSpeed = []
        rightWheelMileage = []
        
        for (var i = 0; i < trackRaw.length; i++) {
            var p = mapDataManager.mapToScene(Qt.point(trackRaw[i].x, trackRaw[i].y), Qt.rect(0,0,mapViewer.width,mapViewer.height), 1.0)
            trackScreen.push(Qt.point(p.x, p.y))
            trackAngles.push(trackRaw[i].angle || 0)
            trackOutOfSafe.push(!!trackRaw[i].outOfSafeArea)
            trackTimestamps.push(trackRaw[i].timestamp || (i>0?trackTimestamps[i-1]+40:0))
            
            // 提取车轮数据
            leftWheelSetSpeed.push(trackRaw[i].leftWheel ? (trackRaw[i].leftWheel.setSpeed || 0) : 0)
            leftWheelMeasuredSpeed.push(trackRaw[i].leftWheel ? (trackRaw[i].leftWheel.measuredSpeed || 0) : 0)
            leftWheelMileage.push(trackRaw[i].leftWheel ? (trackRaw[i].leftWheel.mileage || 0) : 0)
            rightWheelSetSpeed.push(trackRaw[i].rightWheel ? (trackRaw[i].rightWheel.setSpeed || 0) : 0)
            rightWheelMeasuredSpeed.push(trackRaw[i].rightWheel ? (trackRaw[i].rightWheel.measuredSpeed || 0) : 0)
            rightWheelMileage.push(trackRaw[i].rightWheel ? (trackRaw[i].rightWheel.mileage || 0) : 0)
        }
        playIndex = 0
        isPlaying = false
        vehicleTrackShape.updatePartialPath(0, 0)
        fullTrackPreviewShape.generateFullTrackPath() // 生成完整轨迹路径
    }

    function updateTrackVisual() {
        if (trackScreen.length === 0) { return }
        var startIdx = 0
        vehicleTrackShape.updatePartialPath(startIdx, playIndex)
        fullTrackPreviewShape.generateFullTrackPath() // 更新完整轨迹预览路径
        // 触发当前车辆箭头重绘
        // 通过属性绑定已自动更新位置与颜色
        currentVehicle.visible = true
        directionArrow.requestPaint()
    }

    function scheduleNext() {
        if (!isPlaying) return
        if (playIndex + 1 >= trackScreen.length) { isPlaying = false; return }
        
        // 计算两个相邻轨迹点之间的时间差（ms）
        var rawDt = trackTimestamps[playIndex+1] - trackTimestamps[playIndex]
        
        // 考虑倍速播放：rawDt 除以倍速因子得到实际播放间隔
        // 例如：倍速2.0时，原始间隔100ms -> 实际播放50ms
        var dt = rawDt / Math.max(0.001, speedFactor)
        // 仅保留最小间隔约束，确保不会因为太快而导致帧率过高
        // 移除上限约束，以保证回放总时长准确
        dt = Math.max(minFrameInterval, dt)
        playTimer.interval = dt
        playTimer.start()
    }

    function nextFrame() {
        if (playIndex + 1 < trackScreen.length) {
            playIndex += 1
            updateTrackVisual()
            scheduleNext()
        } else {
            isPlaying = false
        }
    }

    function stepForward() {
        isPlaying = false
        playTimer.stop()
        if (playIndex + 1 < trackScreen.length) {
            playIndex += 1
            updateTrackVisual()
        }
    }

    function stepBackward() {
        isPlaying = false
        playTimer.stop()
        if (playIndex > 0) {
            playIndex -= 1
            updateTrackVisual()
        }
    }

    function startPlayback() {
        if (trackScreen.length <= 1) return
        if (playIndex >= trackScreen.length - 1) playIndex = 0
        isPlaying = true
        scheduleNext()
    }

    function pausePlayback() {
        isPlaying = false
        playTimer.stop()
    }

    function stopPlayback() {
        isPlaying = false
        playTimer.stop()
        playIndex = 0
        updateTrackVisual()
    }
    
    function fitMapToView() {
        if (!mapDataManager.isLoaded) return
            mapViewer.zoomLevel = 1.0
            mapViewer.panOffset = Qt.point(0, 0)

    }
    
    // 自动跟踪函数：始终将小车保持在视角中央
    function updateVehicleTracking() {
        if (!autoFollowVehicle || trackScreen.length === 0 || playIndex < 0 || playIndex >= trackScreen.length) {
            return
        }
        
        // 获取当前小车的屏幕坐标
        var vehicleScreenPos = trackScreen[playIndex]
        
        // 计算视口中央坐标
        var viewCenterX = mapViewer.width / 2
        var viewCenterY = mapViewer.height / 2
        
        // 计算小车相对于视口中心的偏移（未缩放时）
        var offsetX = vehicleScreenPos.x - viewCenterX
        var offsetY = vehicleScreenPos.y - viewCenterY
        
        // 根据当前缩放等级调整偏移
        var adjustedOffsetX = offsetX * mapViewer.zoomLevel
        var adjustedOffsetY = offsetY * mapViewer.zoomLevel
        
        // 更新平移偏移，使小车始终处于视角中央
        mapViewer.panOffset = Qt.point(-adjustedOffsetX, -adjustedOffsetY)
    }
    
    // 格式化时间戳为本地时间字符串（精确到ms）
    function formatTime(index) {
        if(!index){
            return "---- -- -- --:--:--.---"
        }
        
        // 时间戳已经是毫秒单位（从C++转换过来）
        var timestampMs = trackTimestamps[index]
        
        // 创建Date对象
        var date = new Date(timestampMs)
        
        // 获取本地时区偏移（分钟）
        var localTimezoneOffsetMinutes = date.getTimezoneOffset()

        var desiredTimezoneOffsetMinutes = -480  // UTC+8
        var timezoneAdjustment = localTimezoneOffsetMinutes - desiredTimezoneOffsetMinutes
        
        // 调整时间
        var adjustedDate = new Date(timestampMs + timezoneAdjustment * 60 * 1000)
        
        // 提取日期和时间信息
        var year = adjustedDate.getUTCFullYear()
        var month = adjustedDate.getUTCMonth() + 1
        var day = adjustedDate.getUTCDate()
        var hours = adjustedDate.getUTCHours()
        var minutes = adjustedDate.getUTCMinutes()
        var seconds = adjustedDate.getUTCSeconds()
        var ms = adjustedDate.getUTCMilliseconds()
        
        // 格式化每个部分
        var yearStr = year.toString()
        var monthStr = (month < 10 ? "0" : "") + month
        var dayStr = (day < 10 ? "0" : "") + day
        var hoursStr = (hours < 10 ? "0" : "") + hours
        var minutesStr = (minutes < 10 ? "0" : "") + minutes
        var secondsStr = (seconds < 10 ? "0" : "") + seconds
        var msStr = (ms < 10 ? "00" : (ms < 100 ? "0" : "")) + ms
        
        return yearStr + "-" + monthStr + "-" + dayStr + " " + hoursStr + ":" + minutesStr + ":" + secondsStr + "." + msStr
    }
    
    // 将地理学角度转换为Qt旋转角度（0度指向右方）
    function convertToQtAngle(geoAngle) {
        
        return 90 - geoAngle;
    }
    
    // 计算Y轴范围
    function calculateYRange(dataArrays) {
        var min = Infinity
        var max = -Infinity
        
        for (var i = 0; i < dataArrays.length; i++) {
            var arr = dataArrays[i]
            for (var j = 0; j < arr.length; j++) {
                if (arr[j] < min) min = arr[j]
                if (arr[j] > max) max = arr[j]
            }
        }
        
        // 添加10%的边距
        var range = max - min
        if (range === 0) range = 1
        min = min - range * 0.1
        max = max + range * 0.1
        
        return { min: min, max: max }
    }
    
    // 使用Catmull-Rom样条曲线平滑化数据点
    function smoothCurvePoints(dataPoints) {
        if (dataPoints.length < 2) return dataPoints
        
        var smoothed = []
        var segments = 3 // 每两个点之间插入的光滑点数
        
        for (var i = 0; i < dataPoints.length - 1; i++) {
            var p0 = dataPoints[Math.max(0, i - 1)]
            var p1 = dataPoints[i]
            var p2 = dataPoints[i + 1]
            var p3 = dataPoints[Math.min(dataPoints.length - 1, i + 2)]
            
            smoothed.push(p1)
            
            // 使用Catmull-Rom样条插值生成平滑过渡点
            for (var t = 1; t < segments; t++) {
                var s = t / segments
                var s2 = s * s
                var s3 = s2 * s
                
                // Catmull-Rom系数
                var c0 = -0.5 * s3 + s2 - 0.5 * s
                var c1 = 1.5 * s3 - 2.5 * s2 + 1
                var c2 = -1.5 * s3 + 2 * s2 + 0.5 * s
                var c3 = 0.5 * s3 - 0.5 * s2
                
                var value = c0 * p0 + c1 * p1 + c2 * p2 + c3 * p3
                smoothed.push(value)
            }
        }
        
        // 添加最后一个点
        smoothed.push(dataPoints[dataPoints.length - 1])
        return smoothed
    }
    
    // 绘制图表主函数（双轴：左-速度组，右-里程组）
    function drawChart(canvas, series, labels, colors, yAxisIndex, currentIndex) {
        var ctx = canvas.getContext("2d")
        ctx.clearRect(0, 0, canvas.width, canvas.height)
        if (!series || series.length === 0 || series[0].length === 0) return
        var w = canvas.width
        var h = canvas.height
        var padding = { left: 44, right: 44, top: 12, bottom: 22 }
        var chartW = w - padding.left - padding.right
        var chartH = h - padding.top - padding.bottom
        var totalDataPoints = series[0].length
        // 计算左轴(速度/差速)范围
        var leftMin = Infinity, leftMax = -Infinity
        var rightMin = Infinity, rightMax = -Infinity
        for (var si = 0; si < series.length; si++) {
            var arr = series[si]
            if (!arr || arr.length === 0) continue
            if (yAxisIndex[si] === 0) {
                for (var j = 0; j < arr.length; j++) {
                    if (arr[j] < leftMin) leftMin = arr[j]
                    if (arr[j] > leftMax) leftMax = arr[j]
                }
            } else {
                for (var k = 0; k < arr.length; k++) {
                    if (arr[k] < rightMin) rightMin = arr[k]
                    if (arr[k] > rightMax) rightMax = arr[k]
                }
            }
        }
        if (leftMin === Infinity) { leftMin = 0; leftMax = 1 }
        if (rightMin === Infinity) { rightMin = 0; rightMax = 1 }
        // 边距与最小跨度
        var leftSpan = leftMax - leftMin
        if (Math.abs(leftSpan) < 1e-6) { leftSpan = 1; leftMin = leftMax - leftSpan }
        var rightSpan = rightMax - rightMin
        if (Math.abs(rightSpan) < 1e-6) { rightSpan = 1; rightMin = rightMax - rightSpan }
        // padding 10%
        leftMin -= leftSpan * 0.1; leftMax += leftSpan * 0.1; leftSpan = leftMax - leftMin
        rightMin -= rightSpan * 0.1; rightMax += rightSpan * 0.1; rightSpan = rightMax - rightMin
        // 写入原始范围缓存（供按鼠标位置缩放使用）
        speedAxisCache = { min: leftMin, max: leftMax, span: leftSpan }
        mileageAxisCache = { min: rightMin, max: rightMax, span: rightSpan }
        // 应用轴缩放（基于可调中心）
        var leftScaledSpan = leftSpan / Math.max(0.1, speedScaleY)
        if (!(speedCenterValue === speedCenterValue)) { // NaN 检测
            speedCenterValue = (leftMin + leftMax) / 2
        }
        // 约束中心使可见范围不越界
        var minCenterL = leftMin + leftScaledSpan / 2
        var maxCenterL = leftMax - leftScaledSpan / 2
        if (minCenterL > maxCenterL) { minCenterL = maxCenterL = (leftMin + leftMax) / 2 }
        speedCenterValue = Math.max(minCenterL, Math.min(maxCenterL, speedCenterValue))
        var leftScaledMin = speedCenterValue - leftScaledSpan / 2
        var leftScaledMax = speedCenterValue + leftScaledSpan / 2
        var rightScaledSpan = rightSpan / Math.max(0.1, mileageScaleY)
        if (!(mileageCenterValue === mileageCenterValue)) { // NaN
            mileageCenterValue = (rightMin + rightMax) / 2
        }
        var minCenterR = rightMin + rightScaledSpan / 2
        var maxCenterR = rightMax - rightScaledSpan / 2
        if (minCenterR > maxCenterR) { minCenterR = maxCenterR = (rightMin + rightMax) / 2 }
        mileageCenterValue = Math.max(minCenterR, Math.min(maxCenterR, mileageCenterValue))
        var rightScaledMin = mileageCenterValue - rightScaledSpan / 2
        var rightScaledMax = mileageCenterValue + rightScaledSpan / 2
        // 背景网格
        ctx.strokeStyle = "#E0E0E0"; ctx.lineWidth = 0.5
        for (var g = 0; g <= 5; g++) {
            var gy = padding.top + (chartH / 5) * g
            ctx.beginPath(); ctx.moveTo(padding.left, gy); ctx.lineTo(padding.left + chartW, gy); ctx.stroke()
        }
        // 坐标轴
        ctx.strokeStyle = "#333333"; ctx.lineWidth = 1
        ctx.beginPath();
        ctx.moveTo(padding.left, padding.top); ctx.lineTo(padding.left, padding.top + chartH)
        ctx.moveTo(padding.left + chartW, padding.top); ctx.lineTo(padding.left + chartW, padding.top + chartH)
        ctx.moveTo(padding.left, padding.top + chartH); ctx.lineTo(padding.left + chartW, padding.top + chartH)
        ctx.stroke()
        // 轴刻度与标签
        ctx.fillStyle = "#666666"; ctx.font = "9px sans-serif"
        ctx.textAlign = "right"
        for (var t = 0; t <= 5; t++) {
            var yL = padding.top + (chartH / 5) * t
            var vL = leftScaledMax - (leftScaledSpan / 5) * t
            ctx.fillText(vL.toFixed(2), padding.left - 6, yL + 3)
        }
        ctx.textAlign = "left"
        ctx.fillStyle = "#8a8a8a"
        for (var r = 0; r <= 5; r++) {
            var yR = padding.top + (chartH / 5) * r
            var vR = rightScaledMax - (rightScaledSpan / 5) * r
            ctx.fillText(vR.toFixed(2), padding.left + chartW + 6, yR + 3)
        }
        // 曲线绘制：X轴缩放重采样 + Catmull-Rom平滑
        var displayDataPoints = Math.max(2, Math.floor(totalDataPoints / chartScaleX))
        for (var d = 0; d < series.length; d++) {
            var data = series[d]; if (!data || data.length === 0) continue
            var smoothed = smoothCurvePoints(data)
            ctx.strokeStyle = colors[d]; ctx.lineWidth = 2.0; ctx.lineCap = "round"; ctx.lineJoin = "round"
            ctx.beginPath()
            var drawn = false
            for (var pi = 0; pi < displayDataPoints; pi++) {
                var origIdx = (pi / (displayDataPoints - 1)) * (totalDataPoints - 1)
                var sIdx = (origIdx / (totalDataPoints - 1)) * (smoothed.length - 1)
                var i0 = Math.floor(sIdx), i1 = Math.ceil(sIdx), tt = sIdx - i0
                var val = (i0 === i1) ? smoothed[i0] : (smoothed[i0] * (1 - tt) + smoothed[i1] * tt)
                var x = padding.left + (pi / Math.max(1, displayDataPoints - 1)) * chartW
                var yNorm
                if (yAxisIndex[d] === 0) {
                    yNorm = leftScaledSpan > 0 ? (val - leftScaledMin) / leftScaledSpan : 0.5
                } else {
                    yNorm = rightScaledSpan > 0 ? (val - rightScaledMin) / rightScaledSpan : 0.5
                }
                if (yNorm < 0) yNorm = 0; if (yNorm > 1) yNorm = 1
                var y = padding.top + chartH - yNorm * chartH
                if (!drawn) { ctx.moveTo(x, y); drawn = true } else { ctx.lineTo(x, y) }
            }
            ctx.stroke()
        }
        // 当前位置标记（垂直线）
        if (currentIndex >= 0 && currentIndex < totalDataPoints) {
            var markerX = padding.left + (currentIndex / Math.max(1, displayDataPoints - 1)) * chartW
            ctx.strokeStyle = "#F44336"; ctx.lineWidth = 2
            ctx.beginPath(); ctx.moveTo(markerX, padding.top); ctx.lineTo(markerX, padding.top + chartH); ctx.stroke()
        }
    }

    function getActiveWheelSeries() {
        var setArr, meaArr, milArr
        if (currentWheel === "right") {
            setArr = rightWheelSetSpeed
            meaArr = rightWheelMeasuredSpeed
            milArr = rightWheelMileage
        } else {
            setArr = leftWheelSetSpeed
            meaArr = leftWheelMeasuredSpeed
            milArr = leftWheelMileage
        }
        var delta = []
        var n = Math.min(setArr.length, meaArr.length)
        for (var i = 0; i < n; i++) delta.push(meaArr[i] - setArr[i])
        return { series: [setArr, meaArr, delta, milArr], labels: ["设定速度", "测量速度", "差速", "里程"] }
    }

    // 监听地图数据加载完成后触发绘制与适配
    Connections {
        target: mapDataManager
        function onMapDataLoaded() {
            console.log("QML: mapDataLoaded, building paths...");
            if (mapViewer.width <= 0 || mapViewer.height <= 0) {
                console.log("viewer size not ready at loaded, delaying build...");
                Qt.callLater(function(){ mapShape.createPaths(); fitMapToView(); loadVehicleTrack();});
            } else {
                mapShape.createPaths();
                fitMapToView();
                loadVehicleTrack();
            }
        }
        function onVehicleTrackLoaded() {
            console.log("QML: vehicleTrackLoaded, building track paths...");
            if (mapViewer.width <= 0 || mapViewer.height <= 0) {
                console.log("viewer size not ready for track, delaying build...");
                Qt.callLater(function(){ buildTrackCache(); updateTrackVisual(); });
            } else {
                buildTrackCache();
                updateTrackVisual();
            }
        }
        function onLoadError(error) {
            console.error("QML: loadError:", error);
            errorText.text = "数据加载失败：" + error;
            errorText.visible = true;
        }
    }

    onCurrentWheelChanged: { if (wheelChart) wheelChart.requestPaint() }
    
    // ============== 窗口大小自适应机制 ==============
    // 监听窗口宽度变化
    onWidthChanged: {
        if (mapDataManager && mapDataManager.isLoaded) {
            console.log("MapViewer width changed to:", width);
            // 使用 Qt.callLater 延迟调用，确保布局已完成
            Qt.callLater(function() {
                mapShape.createPaths();
                fitMapToView();
                loadVehicleTrack();
            });
        }
    }
    
    // 监听窗口高度变化
    onHeightChanged: {
        if (mapDataManager && mapDataManager.isLoaded) {
            console.log("MapViewer height changed to:", height);
            // 使用 Qt.callLater 延迟调用，确保布局已完成
            Qt.callLater(function() {
                mapShape.createPaths();
                fitMapToView();
                loadVehicleTrack();
            });
        }
    }
}

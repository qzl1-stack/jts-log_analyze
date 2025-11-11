#include "map_xml_parser.h"
#include <QXmlStreamReader>
#include <QDebug>
#include <QtMath>

MapXmlParser::MapXmlParser(QObject *parent)
    : QObject(parent)
{
}

bool MapXmlParser::parseXmlContent(const QString& xmlContent)
{
    if (xmlContent.isEmpty()) {
        emit parseError("XML内容为空");
        return false;
    }
    
    // 清空之前的数据
    m_mapData = MapData();
    
    QXmlStreamReader reader(xmlContent);
    
    while (!reader.atEnd() && !reader.hasError()) {
        QXmlStreamReader::TokenType token = reader.readNext();
        
        if (token == QXmlStreamReader::StartElement) {
            QString elementName = reader.name().toString();
            
            if (elementName == "FileInfo") {
                parseFileInfo(reader);
            } else if (elementName == "LayoutInformation") {
                parseLayoutInformation(reader);
            } else if (elementName == "Defaults") {
                parseDefaults(reader);
            } else if (elementName == "Segments") {
                parseSegments(reader);
            } else if (elementName == "NdcPoints") {
                parsePoints(reader);
            } else if (elementName == "PositionMarkers") {
                parsePositionMarkers(reader);
            }
        }
    }
    
    if (reader.hasError()) {
        QString error = QString("XML解析错误: %1").arg(reader.errorString());
        emit parseError(error);
        return false;
    }
    
    // 计算地图边界
    m_mapData.boundingRect = calculateBoundingRect();
    
    emit parseCompleted();
    return true;
}

void MapXmlParser::parseFileInfo(QXmlStreamReader& reader)
{
    // 跳过FileInfo内容，暂时不需要处理
    reader.skipCurrentElement();
}

void MapXmlParser::parseLayoutInformation(QXmlStreamReader& reader)
{
    QXmlStreamAttributes attributes = reader.attributes();
    if (attributes.hasAttribute("LayoutName")) {
        m_mapData.layoutName = attributes.value("LayoutName").toString();
    }
    
    reader.skipCurrentElement();
}

void MapXmlParser::parseDefaults(QXmlStreamReader& reader)
{
    // 跳过Defaults内容，暂时不需要处理
    reader.skipCurrentElement();
}

void MapXmlParser::parseSegments(QXmlStreamReader& reader)
{
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.isEndElement() && reader.name().toString() == "Segments") {
            break;
        }
        
        if (reader.isStartElement() && reader.name().toString() == "Segment") {
            MapSegment segment = parseSegment(reader);
            if (segment.id > 0) {
                m_mapData.segments.append(segment);
            }
        }
    }
}

void MapXmlParser::parsePoints(QXmlStreamReader& reader)
{
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.isEndElement() && reader.name().toString() == "NdcPoints") {
            break;
        }
        
        if (reader.isStartElement() && reader.name().toString() == "NdcPoint") {
            QXmlStreamAttributes attributes = reader.attributes();
            
            MapPoint point;
            if (attributes.hasAttribute("Id")) {
                point.id = attributes.value("Id").toInt();
            }
            if (attributes.hasAttribute("CoordX")) {
                point.coordinate.setX(attributes.value("CoordX").toDouble());
            }
            if (attributes.hasAttribute("CoordY")) {
                point.coordinate.setY(attributes.value("CoordY").toDouble());
            }
            
            if (point.id > 0) {
                m_mapData.points.append(point);
            }
        }
    }
}

void MapXmlParser::parsePositionMarkers(QXmlStreamReader& reader)
{
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.isEndElement() && reader.name().toString() == "PositionMarkers") {
            break;
        }
        
        if (reader.isStartElement() && reader.name().toString() == "PositionMarker") {
            PositionMarker marker = parsePositionMarker(reader);
            if (marker.id > 0) {
                m_mapData.positionMarkers.append(marker);
            }
        }
    }
}

PositionMarker MapXmlParser::parsePositionMarker(QXmlStreamReader& reader)
{
    PositionMarker marker;
    QXmlStreamAttributes attributes = reader.attributes();
    
    if (attributes.hasAttribute("Id")) {
        marker.id = attributes.value("Id").toInt();
    }
    if (attributes.hasAttribute("CoordX")) {
        marker.coordinate.setX(attributes.value("CoordX").toDouble());
    }
    if (attributes.hasAttribute("CoordY")) {
        marker.coordinate.setY(attributes.value("CoordY").toDouble());
    }
    if (attributes.hasAttribute("Angle")) {
        marker.angle = attributes.value("Angle").toDouble();
    }
    
    reader.skipCurrentElement();
    return marker;
}

MapSegment MapXmlParser::parseSegment(QXmlStreamReader& reader)
{
    MapSegment segment;
    QXmlStreamAttributes attributes = reader.attributes();
    
    // 解析Segment属性
    if (attributes.hasAttribute("Id")) {
        segment.id = attributes.value("Id").toInt();
    }
    if (attributes.hasAttribute("StartPoint")) {
        segment.startPointId = attributes.value("StartPoint").toInt();
    }
    if (attributes.hasAttribute("EndPoint")) {
        segment.endPointId = attributes.value("EndPoint").toInt();
    }
    if (attributes.hasAttribute("Weight")) {
        segment.weight = attributes.value("Weight").toInt();
    }
    if (attributes.hasAttribute("Length")) {
        segment.length = attributes.value("Length").toInt();
    }
    
    // 解析子元素
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.isEndElement() && reader.name().toString() == "Segment") {
            break;
        }
        
        if (reader.isStartElement()) {
            QString elementName = reader.name().toString();
            
            if (elementName == "Parts") {
                // 解析Parts
                while (!reader.atEnd()) {
                    reader.readNext();
                    
                    if (reader.isEndElement() && reader.name().toString() == "Parts") {
                        break;
                    }
                    
                    if (reader.isStartElement()) {
                        MapPart part = parsePart(reader);
                        segment.parts.append(part);
                    }
                }
            } else if (elementName == "Actions") {
                // 解析Actions
                while (!reader.atEnd()) {
                    reader.readNext();
                    
                    if (reader.isEndElement() && reader.name().toString() == "Actions") {
                        break;
                    }
                    
                    if (reader.isStartElement() && reader.name().toString() == "Obstacle") {
                        QXmlStreamAttributes obstacleAttrs = reader.attributes();
                        if (obstacleAttrs.hasAttribute("Value")) {
                            segment.obstacleValue = obstacleAttrs.value("Value").toInt();
                        }
                    }
                }
            }
        }
    }
    
    return segment;
}

MapPart MapXmlParser::parsePart(QXmlStreamReader& reader)
{
    MapPart part;
    QString elementName = reader.name().toString();
    QXmlStreamAttributes attributes = reader.attributes();
    
    if (elementName == "PartPoint") {
        part.type = MapPart::Point;
        if (attributes.hasAttribute("CoordX")) {
            part.coordinate.setX(attributes.value("CoordX").toDouble());
        }
        if (attributes.hasAttribute("CoordY")) {
            part.coordinate.setY(attributes.value("CoordY").toDouble());
        }
        if (attributes.hasAttribute("Angle")) {
            part.angle = attributes.value("Angle").toDouble();
        }
        reader.skipCurrentElement();
        
    } else if (elementName == "PartLine") {
        part.type = MapPart::Line;
        if (attributes.hasAttribute("Speed")) {
            part.speed = attributes.value("Speed").toDouble();
        }
        reader.skipCurrentElement();
        
    } else if (elementName == "PartSpline") {
        part.type = MapPart::Spline;
        
        // 解析控制点
        while (!reader.atEnd()) {
            reader.readNext();
            
            if (reader.isEndElement() && reader.name().toString() == "PartSpline") {
                break;
            }
            
            if (reader.isStartElement() && reader.name().toString() == "ControlPoint") {
                ControlPoint controlPoint = parseControlPoint(reader);
                part.controlPoints.append(controlPoint);
            }
        }
        
    } else if (elementName == "PartRotation") {
        part.type = MapPart::Rotation;
        if (attributes.hasAttribute("RotationDir")) {
            part.rotationDir = attributes.value("RotationDir").toInt();
        }
        if (attributes.hasAttribute("RotationSpeed")) {
            part.rotationSpeed = attributes.value("RotationSpeed").toDouble();
        }
        reader.skipCurrentElement();
        
    } else {
        reader.skipCurrentElement();
    }
    
    return part;
}

ControlPoint MapXmlParser::parseControlPoint(QXmlStreamReader& reader)
{
    ControlPoint controlPoint;
    QXmlStreamAttributes attributes = reader.attributes();
    
    if (attributes.hasAttribute("CoordX")) {
        controlPoint.coordinate.setX(attributes.value("CoordX").toDouble());
    }
    if (attributes.hasAttribute("CoordY")) {
        controlPoint.coordinate.setY(attributes.value("CoordY").toDouble());
    }
    if (attributes.hasAttribute("Speed")) {
        controlPoint.speed = attributes.value("Speed").toDouble();
    }
    
    reader.skipCurrentElement();
    return controlPoint;
}

QPainterPath MapXmlParser::generateSegmentPath(const MapSegment& segment) const
{
    QPainterPath path;
    QPointF currentPos;
    bool hasStartPoint = false;
    
    for (const MapPart& part : segment.parts) {
        switch (part.type) {
        case MapPart::Point:
            currentPos = part.coordinate;
            if (!hasStartPoint) {
                path.moveTo(currentPos);
                hasStartPoint = true;
            }
            break;
            
        case MapPart::Line:
            // PartLine通常跟在两个PartPoint之间，连接前后两个点
            break;
            
        case MapPart::Spline:
            if (part.controlPoints.size() >= 4) {
                // 使用贝塞尔曲线连接控制点
                // 这里简化处理，使用三次贝塞尔曲线
                if (part.controlPoints.size() >= 6) {
                    // 六点样条，分段处理
                    QPointF p0 = part.controlPoints[1].coordinate; // 起点
                    QPointF p1 = part.controlPoints[0].coordinate; // 控制点1
                    QPointF p2 = part.controlPoints[2].coordinate; // 控制点2
                    QPointF p3 = part.controlPoints[4].coordinate; // 终点
                    
                    if (!hasStartPoint) {
                        path.moveTo(p0);
                        hasStartPoint = true;
                    } else {
                        path.lineTo(p0);
                    }
                    path.cubicTo(p1, p2, p3);
                    currentPos = p3;
                }
            }
            break;
            
        case MapPart::Rotation:
            // 旋转部分暂时不绘制路径，只是状态改变
            break;
        }
    }
    
    return path;
}

QPointF MapXmlParser::mapToScene(const QPointF& mapCoord, const QRectF& sceneRect, double scale) const
{
    if (m_mapData.boundingRect.isEmpty()) {
        return mapCoord;
    }
    
    // 获取地图的边界矩形
    const QRectF& mapBounds = m_mapData.boundingRect;
    
    // 计算地图的宽度和高度
    double mapWidth = mapBounds.width();
    double mapHeight = mapBounds.height();
    
    // 计算scene可用区域的宽度和高度
    double sceneWidth = sceneRect.width();
    double sceneHeight = sceneRect.height();
    
    // 计算在x和y方向上的基础缩放比例
    double scaleX = sceneWidth / mapWidth;
    double scaleY = sceneHeight / mapHeight;
    
    // 使用较小的缩放比例，保持地图的宽高比
    // 这确保了地图中的距离关系能够100%还原
    double uniformScale = qMin(scaleX, scaleY) * scale;
    
    // 将地图坐标相对于地图原点进行归一化
    double relativeX = mapCoord.x() - mapBounds.left();
    double relativeY = mapCoord.y() - mapBounds.top();
    
    // 应用统一的缩放
    double scaledX = relativeX * uniformScale;
    // Y轴翻转：地图坐标系中Y向上，而场景坐标系中Y向下
    double scaledY = (mapHeight - relativeY) * uniformScale;
    
    // 计算缩放后地图的实际大小
    double scaledMapWidth = mapWidth * uniformScale;
    double scaledMapHeight = mapHeight * uniformScale;
    
    // 居中显示缩放后的地图
    double offsetX = (sceneWidth - scaledMapWidth) / 2.0;
    double offsetY = (sceneHeight - scaledMapHeight) / 2.0;
    
    // 返回最终的场景坐标
    return QPointF(scaledX + offsetX + sceneRect.left(), 
                   scaledY + offsetY + sceneRect.top());
}

QPointF MapXmlParser::sceneToMap(const QPointF& sceneCoord, const QRectF& sceneRect, double scale) const
{
    if (m_mapData.boundingRect.isEmpty()) {
        return sceneCoord;
    }

    const QRectF& mapBounds = m_mapData.boundingRect;

    double mapWidth = mapBounds.width();
    double mapHeight = mapBounds.height();

    double sceneWidth = sceneRect.width();
    double sceneHeight = sceneRect.height();

    double scaleX = sceneWidth / mapWidth;
    double scaleY = sceneHeight / mapHeight;
    double uniformScale = qMin(scaleX, scaleY) * scale;

    double scaledMapWidth = mapWidth * uniformScale;
    double scaledMapHeight = mapHeight * uniformScale;

    double offsetX = (sceneWidth - scaledMapWidth) / 2.0 + sceneRect.left();
    double offsetY = (sceneHeight - scaledMapHeight) / 2.0 + sceneRect.top();

    // 把 sceneCoord 减去 offset 得到相对缩放后的scene坐标
    double sx = sceneCoord.x() - offsetX;
    double sy = sceneCoord.y() - offsetY;

    // 反向缩放，得到地图坐标的归一化坐标
    double relativeX = sx / uniformScale;
    double relativeY = sy / uniformScale;

    // Y轴翻转（对应 mapToScene 中 mapHeight - relativeY）
    double mapX = relativeX + mapBounds.left();
    double mapY = mapHeight - relativeY + mapBounds.top();

    return QPointF(mapX, mapY);
}

QRectF MapXmlParser::calculateBoundingRect() const
{
    if (m_mapData.segments.isEmpty()) {
        return QRectF();
    }
    
    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    
    // 遍历所有segment的所有parts
    for (const MapSegment& segment : m_mapData.segments) {
        for (const MapPart& part : segment.parts) {
            if (part.type == MapPart::Point) {
                minX = qMin(minX, part.coordinate.x());
                maxX = qMax(maxX, part.coordinate.x());
                minY = qMin(minY, part.coordinate.y());
                maxY = qMax(maxY, part.coordinate.y());
            } else if (part.type == MapPart::Spline) {
                for (const ControlPoint& cp : part.controlPoints) {
                    minX = qMin(minX, cp.coordinate.x());
                    maxX = qMax(maxX, cp.coordinate.x());
                    minY = qMin(minY, cp.coordinate.y());
                    maxY = qMax(maxY, cp.coordinate.y());
                }
            }
        }
    }
    
    // 遍历所有PositionMarker
    for (const PositionMarker& marker : m_mapData.positionMarkers) {
        minX = qMin(minX, marker.coordinate.x());
        maxX = qMax(maxX, marker.coordinate.x());
        minY = qMin(minY, marker.coordinate.y());
        maxY = qMax(maxY, marker.coordinate.y());
    }
    
    // 添加一些边距
    double margin = qMax((maxX - minX), (maxY - minY)) * 0.1;
    return QRectF(minX - margin, minY - margin, 
                  (maxX - minX) + 2 * margin, 
                  (maxY - minY) + 2 * margin);
}

double MapXmlParser::angleToRadians(double angle) const
{
    return angle * M_PI / 18000.0; // NDC角度单位转弧度
}

QPointF MapXmlParser::calculateSplinePoint(const QList<ControlPoint>& controlPoints, double t) const
{
    if (controlPoints.size() < 4) {
        return QPointF();
    }
    
    // 简化的贝塞尔曲线计算
    double u = 1.0 - t;
    double tt = t * t;
    double uu = u * u;
    double uuu = uu * u;
    double ttt = tt * t;
    
    QPointF p = controlPoints[0].coordinate * uuu;
    p += controlPoints[1].coordinate * 3 * uu * t;
    p += controlPoints[2].coordinate * 3 * u * tt;
    p += controlPoints[3].coordinate * ttt;
    
    return p;
}

bool MapXmlParser::parseVehicleData(const QString& vehicleText)
{
    if (vehicleText.isEmpty()) {
        qDebug() << "Vehicle text is empty";
        return false;
    }
    
    // 清空之前的轨迹数据
    m_mapData.vehicleTrack.clear();
    
    // 按行分割文本
    QStringList lines = vehicleText.split('\n', Qt::SkipEmptyParts);
    
    VehicleTrackPoint currentPoint;
    bool hasCurrentPoint = false;
    
    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        
        // 跳过空行
        if (trimmedLine.isEmpty()) {
            continue;
        }
        
        // 解析各种数据行
        if (trimmedLine.startsWith("now ")) {
            // 如果有之前的点数据，先保存
            if (hasCurrentPoint) {
                m_mapData.vehicleTrack.append(currentPoint);
                currentPoint = VehicleTrackPoint();
                hasCurrentPoint = false;
            }
        }
        else if (trimmedLine.startsWith("position ")) {
            // position 1753195393.192 4 0 1 49393 70590 269.743
            QStringList parts = trimmedLine.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 8) {
                // 获取时间戳（转换为毫秒）
                currentPoint.timestamp = static_cast<qint64>(parts[1].toDouble() * 1000);
                
                // 获取坐标和角度（最后三个数据）
                int size = parts.size();
                if (size >= 3) {
                    currentPoint.position.setX(parts[size - 3].toDouble());
                    currentPoint.position.setY(parts[size - 2].toDouble());
                    currentPoint.angle = parts[size - 1].toDouble();
                    hasCurrentPoint = true;
                }
            }
        }
        else if (trimmedLine.startsWith("state ")) {
            // state 1753195393.192 1 0 1 0 0 0 0 0 1
            QStringList parts = trimmedLine.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 6) {
                currentPoint.outOfSafeArea = (parts[5].toInt() == 1);
                currentPoint.isAutoDriving = (parts[3].toInt() == 0);
                currentPoint.isRetard = (parts[6].toInt() == 1);
                currentPoint.isStop = (parts[7].toInt() == 1);
                currentPoint.isQuickStop = (parts[8].toInt() == 1);
                currentPoint.isEmergencyStop = (parts[9].toInt() == 1);
            }
        }
        else if (trimmedLine.startsWith("guidance ")) {
            QStringList parts = trimmedLine.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 6) {
                currentPoint.distance = parts[5].toInt();
            }
        }
        else if (trimmedLine.startsWith("LeftWheel ")) {
            // LeftWheel 1753195393.194 0.000 0.000 498.000 498.000 26721.000
            QStringList parts = trimmedLine.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 6) {
                // 获取最后三个数据：设定速度、测量速度、里程
                int size = parts.size();
                if (size >= 3) {
                    currentPoint.leftWheel.setSpeed = parts[size - 3].toDouble();
                    currentPoint.leftWheel.measuredSpeed = parts[size - 2].toDouble();
                    currentPoint.leftWheel.mileage = parts[size - 1].toDouble();
                }
            }
        }
        else if (trimmedLine.startsWith("RightWheel ")) {
            // RightWheel 1753195393.194 0.000 0.000 501.000 501.000 25720.000
            QStringList parts = trimmedLine.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 6) {
                // 获取最后三个数据：设定速度、测量速度、里程
                int size = parts.size();
                if (size >= 3) {
                    currentPoint.rightWheel.setSpeed = parts[size - 3].toDouble();
                    currentPoint.rightWheel.measuredSpeed = parts[size - 2].toDouble();
                    currentPoint.rightWheel.mileage = parts[size - 1].toDouble();
                }
            }
        }
        else if (trimmedLine.startsWith("barcode ")) {
            QStringList parts = trimmedLine.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 3) {
                currentPoint.barcode = parts[2].toInt();
            }
        }
    }
    
    // 保存最后一个点数据
    if (hasCurrentPoint) {
        m_mapData.vehicleTrack.append(currentPoint);
    }
    
    qDebug() << "Parsed" << m_mapData.vehicleTrack.size() << "vehicle track points";
    
    return !m_mapData.vehicleTrack.isEmpty();
}
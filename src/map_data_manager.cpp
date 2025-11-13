#include "map_data_manager.h"
#include "sqlite_text_handler.h"
#include <QDebug>

MapDataManager::MapDataManager(QObject *parent)
    : QObject(parent)
    , m_dbManager(nullptr)
    , m_mapParser(new MapXmlParser(this))
    , m_isLoaded(false)
    , m_vehicleTrackLoaded(false)
{
    // 连接解析器信号
    connect(m_mapParser, &MapXmlParser::parseCompleted,
            this, &MapDataManager::onParseCompleted);
    connect(m_mapParser, &MapXmlParser::parseError,
            this, &MapDataManager::onParseError);
}

void MapDataManager::setDatabaseManager(SqliteDbManager* dbManager)
{
    m_dbManager = dbManager;
}

bool MapDataManager::loadMapData()
{
    if (!m_dbManager) {
        emit loadError("数据库管理器未设置");
        return false;
    }
    
    QString xmlContent = getXmlContentFromDatabase();
    if (xmlContent.isEmpty()) {
        emit loadError("无法从数据库获取地图XML数据");
        return false;
    }
    
    qDebug() << "开始解析地图XML数据，内容长度：" << xmlContent.length();
    
    return m_mapParser->parseXmlContent(xmlContent);
}

bool MapDataManager::loadVehicleTrack()
{
    if (!m_dbManager) {
        emit loadError("数据库管理器未设置");
        return false;
    }
    
    QString vehicleData = getVehicleDataFromDatabase();
    if (vehicleData.isEmpty()) {
        emit loadError("无法从数据库获取vehicle轨迹数据");
        return false;
    }
    
    qDebug() << "开始解析车辆轨迹数据，内容长度：" << vehicleData.length();
    
    bool success = m_mapParser->parseVehicleData(vehicleData);
    if (success) {
        m_vehicleTrackLoaded = true;
        emit vehicleTrackCountChanged();
        emit vehicleTrackLoaded();
    }
    
    return success;
}

void MapDataManager::clearMapData()
{
    m_isLoaded = false;
    m_vehicleTrackLoaded = false;
    emit isLoadedChanged();
    emit layoutNameChanged();
    emit boundingRectChanged();
    emit segmentCountChanged();
    emit pointCountChanged();
    emit vehicleTrackCountChanged();
}

QString MapDataManager::getXmlContentFromDatabase()
{
    if (!m_dbManager) {
        return QString();
    }
    
    // 通过关键字"map"获取文件内容
    QString content = m_dbManager->GetMergedContentByKeyword("map");
    QString version_str = m_dbManager->GetMergedContentByKeyword("version");
    
    // 从version字符串中提取版本号
    if (!version_str.isEmpty()) {
        QStringList lines = version_str.split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            QString trimmed_line = line.trimmed();
            if (trimmed_line.startsWith("VERSION=")) {
                QString version = trimmed_line.mid(8).trimmed(); // 跳过 "VERSION="
                if (version != m_version) {
                    m_version = version;
                    emit versionChanged();
                }
                break;
            }
        }
    }
    
    if (content.isEmpty()) {
        // 尝试直接查询file_name为"map.wef"的记录
        QList<DbFileRecord> allFiles = m_dbManager->GetAllFiles();
        for (const DbFileRecord& record : allFiles) {
            if (record.file_name.toLower() == "map.wef" || 
                record.file_name.toLower().contains("map")) {
                content = record.content;
                break;
            }
        }
    }
    
    return content;
}

QString MapDataManager::getVehicleDataFromDatabase()
{
    if (!m_dbManager) {
        return QString();
    }
    
    // 通过关键字"vehicle"获取文件内容
    QString content = m_dbManager->GetMergedContentByKeyword("vehicle");
    
    if (content.isEmpty()) {
        // 如果通过关键字找不到，尝试查找包含"vehicle"的文件名
        QList<DbFileRecord> allFiles = m_dbManager->GetAllFiles();
        for (const DbFileRecord& record : allFiles) {
            if (record.file_name.toLower().contains("vehicle")) {
                content = record.content;
                break;
            }
        }
    }
    
    return content;
}

QVariantList MapDataManager::getSegmentPaths() const
{
    if (!m_isLoaded) {
        return QVariantList();
    }
    
    QVariantList result;
    const MapData& mapData = m_mapParser->getMapData();
    
    for (const MapSegment& segment : mapData.segments) {
        QVariantMap segmentData = segmentToVariantMap(segment);
        result.append(segmentData);
    }
    
    return result;
}


QVariantList MapDataManager::getPositionMarkers() const
{
    if (!m_isLoaded) {
        return QVariantList();
    }
    
    QVariantList result;
    const MapData& mapData = m_mapParser->getMapData();
    
    for (const PositionMarker& marker : mapData.positionMarkers) {
        QVariantMap markerData = positionMarkerToVariantMap(marker);
        result.append(markerData);
    }
    
    return result;
}

QVariantList MapDataManager::getVehicleTrack() const
{
    if (!m_vehicleTrackLoaded) {
        return QVariantList();
    }
    
    QVariantList result;
    const MapData& mapData = m_mapParser->getMapData();
    
    for (const VehicleTrackPoint& point : mapData.vehicleTrack) {
        QVariantMap pointData = vehicleTrackPointToVariantMap(point);
        result.append(pointData);
    }
    
    return result;
}

QVariantMap MapDataManager::getSegmentInfo(int segmentId) const
{
    if (!m_isLoaded) {
        return QVariantMap();
    }
    
    const MapData& mapData = m_mapParser->getMapData();
    for (const MapSegment& segment : mapData.segments) {
        if (segment.id == segmentId) {
            return segmentToVariantMap(segment);
        }
    }
    
    return QVariantMap();
}

QPointF MapDataManager::mapToScene(const QPointF& mapCoord, const QRectF& sceneRect, double scale) const
{
    if (!m_isLoaded) {
        return mapCoord;
    }
    
    return m_mapParser->mapToScene(mapCoord, sceneRect, scale);
}

QPointF MapDataManager::sceneToMap(const QPointF& sceneCoord, const QRectF& sceneRect, double scale) const
{
    if (!m_isLoaded) {
        return sceneCoord;
    }
    return m_mapParser->sceneToMap(sceneCoord, sceneRect, scale);
}

QRectF MapDataManager::getOptimalViewRect(const QRectF& viewSize) const
{
    if (!m_isLoaded) {
        return QRectF();
    }
    
    QRectF mapBounds = m_mapParser->getMapData().boundingRect;
    if (mapBounds.isEmpty()) {
        return QRectF();
    }
    
    // 计算最佳缩放比例
    double scaleX = viewSize.width() / mapBounds.width();
    double scaleY = viewSize.height() / mapBounds.height();
    double scale = qMin(scaleX, scaleY) * 0.9; // 留10%边距
    
    double scaledWidth = mapBounds.width() * scale;
    double scaledHeight = mapBounds.height() * scale;
    
    // 居中
    double x = (viewSize.width() - scaledWidth) / 2.0;
    double y = (viewSize.height() - scaledHeight) / 2.0;
    
    return QRectF(x, y, scaledWidth, scaledHeight);
}

void MapDataManager::onParseCompleted()
{
    m_isLoaded = true;
    
    qDebug() << "地图数据解析完成";
    qDebug() << "布局名称：" << layoutName();
    qDebug() << "段数量：" << segmentCount();
    qDebug() << "点数量：" << pointCount();
    qDebug() << "边界矩形：" << boundingRect();
    
    emit isLoadedChanged();
    emit layoutNameChanged();
    emit boundingRectChanged();
    emit segmentCountChanged();
    emit pointCountChanged();
    emit mapDataLoaded();
}

void MapDataManager::onParseError(const QString& error)
{
    qCritical() << "地图数据解析失败：" << error;
    m_isLoaded = false;
    emit isLoadedChanged();
    emit loadError(error);
}

QVariantMap MapDataManager::segmentToVariantMap(const MapSegment& segment) const
{
    QVariantMap result;
    result["id"] = segment.id;
    result["startPointId"] = segment.startPointId;
    result["endPointId"] = segment.endPointId;
    result["weight"] = segment.weight;
    result["length"] = segment.length;
    result["obstacleValue"] = segment.obstacleValue;
    
    // 转换Parts
    QVariantList parts;
    for (const MapPart& part : segment.parts) {
        parts.append(partToVariantMap(part));
    }
    result["parts"] = parts;
    
    return result;
}

QVariantMap MapDataManager::partToVariantMap(const MapPart& part) const
{
    QVariantMap result;
    
    switch (part.type) {
    case MapPart::Point:
        result["type"] = "Point";
        result["x"] = part.coordinate.x();
        result["y"] = part.coordinate.y();
        result["angle"] = part.angle;
        break;
        
    case MapPart::Line:
        result["type"] = "Line";
        result["speed"] = part.speed;
        break;
        
    case MapPart::Spline:
        result["type"] = "Spline";
        result["controlPoints"] = controlPointsToVariantList(part.controlPoints);
        break;
        
    case MapPart::Rotation:
        result["type"] = "Rotation";
        result["rotationDir"] = part.rotationDir;
        result["rotationSpeed"] = part.rotationSpeed;
        break;
    }
    
    return result;
}

QVariantList MapDataManager::controlPointsToVariantList(const QList<ControlPoint>& controlPoints) const
{
    QVariantList result;
    
    for (const ControlPoint& cp : controlPoints) {
        QVariantMap point;
        point["x"] = cp.coordinate.x();
        point["y"] = cp.coordinate.y();
        point["speed"] = cp.speed;
        result.append(point);
    }
    
    return result;
}

QVariantMap MapDataManager::positionMarkerToVariantMap(const PositionMarker& marker) const
{
    QVariantMap result;
    result["id"] = marker.id;
    result["x"] = marker.coordinate.x();
    result["y"] = marker.coordinate.y();
    result["angle"] = marker.angle;
    return result;
}

QVariantMap MapDataManager::vehicleTrackPointToVariantMap(const VehicleTrackPoint& point) const
{
    QVariantMap result;
    result["timestamp"] = point.timestamp;
    result["x"] = point.position.x();
    result["y"] = point.position.y();
    result["angle"] = point.angle;
    result["outOfSafeArea"] = point.outOfSafeArea;
    result["barcode"] = point.barcode; 
    result["isAutoDriving"] = point.isAutoDriving;
    result["isRetard"] = point.isRetard;
    result["isStop"] = point.isStop;
    result["isQuickStop"] = point.isQuickStop;
    result["isEmergencyStop"] = point.isEmergencyStop;
    result["distance"] = point.distance;
    
    // 左轮数据
    QVariantMap leftWheel;
    leftWheel["setSpeed"] = point.leftWheel.setSpeed;
    leftWheel["measuredSpeed"] = point.leftWheel.measuredSpeed;
    leftWheel["mileage"] = point.leftWheel.mileage;
    result["leftWheel"] = leftWheel;
    
    // 右轮数据
    QVariantMap rightWheel;
    rightWheel["setSpeed"] = point.rightWheel.setSpeed;
    rightWheel["measuredSpeed"] = point.rightWheel.measuredSpeed;
    rightWheel["mileage"] = point.rightWheel.mileage;
    result["rightWheel"] = rightWheel;
    
    return result;
}

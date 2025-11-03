#ifndef MAP_DATA_MANAGER_H
#define MAP_DATA_MANAGER_H

#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>
#include <QRectF>
#include <QPointF>
#include <QPainterPath>
#include "map_xml_parser.h"

class SqliteDbManager;
Q_DECLARE_OPAQUE_POINTER(SqliteDbManager*)

class MapDataManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    
    // QML可访问的属性
    Q_PROPERTY(bool isLoaded READ isLoaded NOTIFY isLoadedChanged)
    Q_PROPERTY(QString layoutName READ layoutName NOTIFY layoutNameChanged)
    Q_PROPERTY(QRectF boundingRect READ boundingRect NOTIFY boundingRectChanged)
    Q_PROPERTY(int segmentCount READ segmentCount NOTIFY segmentCountChanged)
    Q_PROPERTY(int pointCount READ pointCount NOTIFY pointCountChanged)
    Q_PROPERTY(int positionMarkerCount READ positionMarkerCount NOTIFY positionMarkerCountChanged)
    Q_PROPERTY(int vehicleTrackCount READ vehicleTrackCount NOTIFY vehicleTrackCountChanged)

public:
    explicit MapDataManager(QObject *parent = nullptr);
    
    // 设置数据库管理器
    void setDatabaseManager(SqliteDbManager* dbManager);
    
    // QML可调用的方法
    Q_INVOKABLE bool loadMapData();
    Q_INVOKABLE void clearMapData();
    Q_INVOKABLE bool loadVehicleTrack();
    
    // 获取地图数据用于渲染
    Q_INVOKABLE QVariantList getSegmentPaths() const;
    Q_INVOKABLE QVariantList getPositionMarkers() const;
    Q_INVOKABLE QVariantList getVehicleTrack() const;
    Q_INVOKABLE QVariantMap getSegmentInfo(int segmentId) const;
    
    // 坐标转换
    Q_INVOKABLE QPointF mapToScene(const QPointF& mapCoord, const QRectF& sceneRect, double scale = 1.0) const;
    Q_INVOKABLE QRectF getOptimalViewRect(const QRectF& viewSize) const;
    
    // 属性访问器
    bool isLoaded() const { return m_isLoaded; }
    QString layoutName() const { return m_mapParser->getMapData().layoutName; }
    QRectF boundingRect() const { return m_mapParser->getMapData().boundingRect; }
    int segmentCount() const { return m_mapParser->getMapData().segments.size(); }
    int pointCount() const { return m_mapParser->getMapData().points.size(); }
    int positionMarkerCount() const { return m_mapParser->getMapData().positionMarkers.size(); }
    int vehicleTrackCount() const { return m_mapParser->getMapData().vehicleTrack.size(); }

signals:
    void isLoadedChanged();
    void layoutNameChanged();
    void boundingRectChanged();
    void segmentCountChanged();
    void pointCountChanged();
    void positionMarkerCountChanged();
    void vehicleTrackCountChanged();
    void mapDataLoaded();
    void vehicleTrackLoaded();
    void loadError(const QString& error);

private slots:
    void onParseCompleted();
    void onParseError(const QString& error);

private:
    SqliteDbManager* m_dbManager;
    MapXmlParser* m_mapParser;
    bool m_isLoaded;
    bool m_vehicleTrackLoaded;
    
    // 内部辅助方法
    QString getXmlContentFromDatabase();
    QString getVehicleDataFromDatabase();
    QVariantMap segmentToVariantMap(const MapSegment& segment) const;
    QVariantMap partToVariantMap(const MapPart& part) const;
    QVariantList controlPointsToVariantList(const QList<ControlPoint>& controlPoints) const;
    QVariantMap positionMarkerToVariantMap(const PositionMarker& marker) const;
    QVariantMap vehicleTrackPointToVariantMap(const VehicleTrackPoint& point) const;
};

#endif // MAP_DATA_MANAGER_H

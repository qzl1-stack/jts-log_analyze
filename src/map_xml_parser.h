#ifndef MAP_XML_PARSER_H
#define MAP_XML_PARSER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QPainterPath>
#include <QXmlStreamReader>

// 地图数据结构定义
struct MapPoint {
    int id;
    QPointF coordinate;
    double angle;
    
    MapPoint() : id(0), angle(0.0) {}
    MapPoint(int id, double x, double y, double angle = 0.0) 
        : id(id), coordinate(x, y), angle(angle) {}
};

struct ControlPoint {
    QPointF coordinate;
    double speed;
    
    ControlPoint() : speed(0.0) {}
    ControlPoint(double x, double y, double speed = 0.0)
        : coordinate(x, y), speed(speed) {}
};

struct PositionMarker {
    int id;
    QPointF coordinate;
    double angle;
    
    PositionMarker() : id(0), angle(0.0) {}
    PositionMarker(int id, double x, double y, double angle = 0.0)
        : id(id), coordinate(x, y), angle(angle) {}
};

struct MapPart {
    enum PartType {
        Point,
        Line, 
        Spline,
        Rotation
    };
    
    PartType type;
    QPointF coordinate;
    double angle;
    double speed;
    int rotationDir;
    double rotationSpeed;
    QList<ControlPoint> controlPoints;
    
    MapPart() : type(Point), angle(0.0), speed(0.0), rotationDir(0), rotationSpeed(0.0) {}
};

struct MapSegment {
    int id;
    int startPointId;
    int endPointId;
    int weight;
    int length;
    QList<MapPart> parts;
    int obstacleValue;
    
    MapSegment() : id(0), startPointId(0), endPointId(0), weight(0), length(0), obstacleValue(0) {}
};

// 车辆轨迹相关数据结构
struct WheelData {
    double setSpeed;        // 设定速度
    double measuredSpeed;   // 测量速度
    double mileage;        // 里程
    
    WheelData() : setSpeed(0.0), measuredSpeed(0.0), mileage(0.0) {}
    WheelData(double set, double measured, double mile)
        : setSpeed(set), measuredSpeed(measured), mileage(mile) {}
};

struct VehicleTrackPoint {
    qint64 timestamp;      // 时间戳（毫秒）
    QPointF position;      // 位置坐标
    double angle;          // 车头角度
    bool outOfSafeArea;    // 是否超出安全区
    WheelData leftWheel;   // 左轮数据
    WheelData rightWheel;  // 右轮数据
    qint32 barcode;        // 条码
    bool isAutoDriving;    // 是否自动驾驶
    bool isRetard;
    bool isStop;
    bool isQuickStop;
    bool isEmergencyStop;
    qint32 distance;
    
    VehicleTrackPoint() : timestamp(0), angle(0.0), outOfSafeArea(false), barcode(0), isAutoDriving(false), distance(0) {}
};

struct MapData {
    QString layoutName;
    QList<MapPoint> points;
    QList<MapSegment> segments;
    QList<PositionMarker> positionMarkers;
    QList<VehicleTrackPoint> vehicleTrack;  // 车辆轨迹数据
    QRectF boundingRect;
    
    MapData() {}
};

class MapXmlParser : public QObject
{
    Q_OBJECT

public:
    explicit MapXmlParser(QObject *parent = nullptr);
    
    // 解析XML内容
    bool parseXmlContent(const QString& xmlContent);
    
    // 获取解析后的地图数据
    const MapData& getMapData() const { return m_mapData; }
    
    // 解析vehicle文本内容
    bool parseVehicleData(const QString& vehicleText);
    
    // 获取车辆轨迹数据
    const QList<VehicleTrackPoint>& getVehicleTrack() const { return m_mapData.vehicleTrack; }
    
    // 生成QPainterPath用于渲染
    QPainterPath generateSegmentPath(const MapSegment& segment) const;
    
    // 坐标转换相关
    QPointF mapToScene(const QPointF& mapCoord, const QRectF& sceneRect, double scale = 1.0) const;
    QPointF sceneToMap(const QPointF& sceneCoord, const QRectF& sceneRect, double scale = 1.0) const;
    QRectF calculateBoundingRect() const;

signals:
    void parseCompleted();
    void parseError(const QString& error);

private:
    MapData m_mapData;
    
    // XML解析辅助方法
    void parseFileInfo(QXmlStreamReader& reader);
    void parseLayoutInformation(QXmlStreamReader& reader);
    void parseDefaults(QXmlStreamReader& reader);
    void parseSegments(QXmlStreamReader& reader);
    void parsePoints(QXmlStreamReader& reader);
    void parsePositionMarkers(QXmlStreamReader& reader);
    
    MapSegment parseSegment(QXmlStreamReader& reader);
    MapPart parsePart(QXmlStreamReader& reader);
    ControlPoint parseControlPoint(QXmlStreamReader& reader);
    PositionMarker parsePositionMarker(QXmlStreamReader& reader);
    
    // 辅助方法
    double angleToRadians(double angle) const;
    QPointF calculateSplinePoint(const QList<ControlPoint>& controlPoints, double t) const;
};

#endif // MAP_XML_PARSER_H

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QDBusReply>
#include <QDBusMetaType>
#include <QtMath>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <QDateTime>
#include <librdkafka/rdkafka.h>

// ── Kafka Producer ────────────────────────────────────────────────────
class KafkaProducer {
public:
    bool init() {
        QByteArray brokers = qgetenv("KAFKA_BROKERS");
        QByteArray topic   = qgetenv("KAFKA_TOPIC");
        QByteArray apiKey  = qgetenv("KAFKA_API_KEY");
        QByteArray secret  = qgetenv("KAFKA_API_SECRET");
        if (brokers.isEmpty() || topic.isEmpty()) {
            fprintf(stderr, "Kafka: KAFKA_BROKERS/KAFKA_TOPIC not set, producing disabled\n");
            return false;
        }
        m_topicName = topic;

        char errstr[512];
        rd_kafka_conf_t *conf = rd_kafka_conf_new();
        rd_kafka_conf_set(conf, "bootstrap.servers", brokers.constData(), errstr, sizeof(errstr));
        rd_kafka_conf_set(conf, "security.protocol", "SASL_SSL", errstr, sizeof(errstr));
        rd_kafka_conf_set(conf, "sasl.mechanisms", "PLAIN", errstr, sizeof(errstr));
        rd_kafka_conf_set(conf, "sasl.username", apiKey.constData(), errstr, sizeof(errstr));
        rd_kafka_conf_set(conf, "sasl.password", secret.constData(), errstr, sizeof(errstr));
        // Low-latency settings
        rd_kafka_conf_set(conf, "linger.ms", "0", errstr, sizeof(errstr));
        rd_kafka_conf_set(conf, "batch.num.messages", "1", errstr, sizeof(errstr));
        rd_kafka_conf_set(conf, "acks", "1", errstr, sizeof(errstr));
        rd_kafka_conf_set(conf, "compression.type", "none", errstr, sizeof(errstr));
        rd_kafka_conf_set(conf, "socket.nagle.disable", "true", errstr, sizeof(errstr));
        rd_kafka_conf_set(conf, "queue.buffering.max.messages", "100", errstr, sizeof(errstr));
        rd_kafka_conf_set(conf, "queue.buffering.max.kbytes", "64", errstr, sizeof(errstr));

        m_rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
        if (!m_rk) {
            fprintf(stderr, "Kafka: failed to create producer: %s\n", errstr);
            return false;
        }

        m_rkt = rd_kafka_topic_new(m_rk, m_topicName.constData(), nullptr);
        if (!m_rkt) {
            fprintf(stderr, "Kafka: failed to create topic handle\n");
            return false;
        }
        fprintf(stderr, "Kafka: producer initialized, topic=%s\n", m_topicName.constData());
        return true;
    }

    void produce(double pitch, double roll, double yaw,
                 double ax, double ay, double az,
                 double gx, double gy, double gz) {
        if (!m_rkt) return;
        char buf[256];
        int len = snprintf(buf, sizeof(buf),
            "{\"ts\":%" PRId64 ",\"p\":%.2f,\"r\":%.2f,\"w\":%.2f,"
            "\"ax\":%.3f,\"ay\":%.3f,\"az\":%.3f,"
            "\"gx\":%.2f,\"gy\":%.2f,\"gz\":%.2f}",
            (int64_t)QDateTime::currentMSecsSinceEpoch(),
            pitch, roll, yaw, ax, ay, az, gx, gy, gz);
        rd_kafka_produce(m_rkt, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY,
                         buf, len, nullptr, 0, nullptr);
        rd_kafka_poll(m_rk, 0);
    }

    ~KafkaProducer() {
        if (m_rk) {
            rd_kafka_flush(m_rk, 2000);
            if (m_rkt) rd_kafka_topic_destroy(m_rkt);
            rd_kafka_destroy(m_rk);
        }
    }

private:
    rd_kafka_t *m_rk = nullptr;
    rd_kafka_topic_t *m_rkt = nullptr;
    QByteArray m_topicName;
};

static KafkaProducer g_kafka;

// BlueZ GetManagedObjects return type: a{oa{sa{sv}}}
typedef QMap<QString, QDBusVariant>          BluezPropMap;
typedef QMap<QString, BluezPropMap>           BluezIfaceMap;
typedef QMap<QDBusObjectPath, BluezIfaceMap>  BluezManagedObjects;

Q_DECLARE_METATYPE(BluezPropMap)
Q_DECLARE_METATYPE(BluezIfaceMap)
Q_DECLARE_METATYPE(BluezManagedObjects)

class SensorTagManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool scanning  READ scanning  NOTIFY scanningChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool resetting READ resetting NOTIFY resetDone)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(double pitch  READ pitch  NOTIFY orientationChanged)
    Q_PROPERTY(double roll   READ roll   NOTIFY orientationChanged)
    Q_PROPERTY(double yaw    READ yaw    NOTIFY orientationChanged)
    Q_PROPERTY(double rawGx  READ rawGx  NOTIFY orientationChanged)
    Q_PROPERTY(double rawGy  READ rawGy  NOTIFY orientationChanged)
    Q_PROPERTY(double rawGz  READ rawGz  NOTIFY orientationChanged)
    Q_PROPERTY(double rawAx  READ rawAx  NOTIFY orientationChanged)
    Q_PROPERTY(double rawAy  READ rawAy  NOTIFY orientationChanged)
    Q_PROPERTY(double rawAz    READ rawAz    NOTIFY orientationChanged)
    Q_PROPERTY(bool calibrating READ calibrating NOTIFY calibratingChanged)
    Q_PROPERTY(QString status  READ status   NOTIFY statusChanged)

public:
    explicit SensorTagManager(QObject *parent = nullptr) : QObject(parent) {
        qDBusRegisterMetaType<BluezPropMap>();
        qDBusRegisterMetaType<BluezIfaceMap>();
        qDBusRegisterMetaType<BluezManagedObjects>();

        // On startup, reset BLE stack to drop any stale connections from a previous crash
        QTimer::singleShot(0, this, &SensorTagManager::resetStack);
    }

    bool scanning()  const { return m_scanning; }
    bool connected() const { return m_connected; }
    bool resetting() const { return m_resetting; }
    QVariantList devices() const { return m_devices; }
    double pitch()   const { return m_pitch; }
    double roll()    const { return m_roll; }
    double yaw()     const { return m_yaw; }
    double rawGx()   const { return m_rawGx; }
    double rawGy()   const { return m_rawGy; }
    double rawGz()   const { return m_rawGz; }
    double rawAx()   const { return m_rawAx; }
    double rawAy()   const { return m_rawAy; }
    double rawAz()      const { return m_rawAz; }
    bool calibrating()  const { return m_calibCount < CALIB_SAMPLES; }
    QString status()    const { return m_status; }

    Q_INVOKABLE void startScan() {
        if (m_resetting) {
            QTimer::singleShot(600, this, &SensorTagManager::startScan);
            return;
        }
        m_devices.clear();
        m_found.clear();
        emit devicesChanged();

        // Remove cached BlueZ devices so only currently-advertising tags appear
        removeCachedDevices();

        if (m_agent) {
            if (m_agent->isActive()) m_agent->stop();
            delete m_agent;
            m_agent = nullptr;
        }
        m_agent = new QBluetoothDeviceDiscoveryAgent(this);
        m_agent->setLowEnergyDiscoveryTimeout(5000);
        connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
                this, &SensorTagManager::onDeviceDiscovered);
        connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished,
                this, [this]() {
                    m_scanning = false;
                    emit scanningChanged();
                    // Reset BLE stack when scan finds nothing, so next scan starts clean
                    if (m_devices.isEmpty()) resetStack();
                });
        connect(m_agent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
                this, [this](QBluetoothDeviceDiscoveryAgent::Error error) {
                    Q_UNUSED(error);
                    m_scanning = false;
                    emit scanningChanged();
                    setStatus("Scan error — resetting BLE...");
                    resetStack();
                });
        m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
        m_scanning = true;
        emit scanningChanged();
    }

    Q_INVOKABLE void stopScan() {
        if (m_agent && m_agent->isActive()) {
            m_agent->stop();
        }
        m_scanning = false;
        emit scanningChanged();
    }

    Q_INVOKABLE void connectToDevice(const QString &address) {
        if (m_agent && m_agent->isActive()) m_agent->stop();

        // Build BlueZ D-Bus path: /org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF
        m_devicePath = "/org/bluez/hci0/dev_" + QString(address).toUpper().replace(':', '_');
        m_dataPath.clear(); m_cfgPath.clear(); m_perPath.clear();

        // Subscribe to device property changes (Connected, ServicesResolved)
        QDBusConnection::systemBus().connect(
            "org.bluez", m_devicePath,
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            this, SLOT(onDevicePropsChanged(QString, QVariantMap, QStringList)));

        // Kick off async BLE connection — BlueZ uses its GATT cache if available
        QDBusInterface dev("org.bluez", m_devicePath,
                           "org.bluez.Device1", QDBusConnection::systemBus());
        dev.asyncCall("Connect");

        setStatus("Connecting...");
    }

    Q_INVOKABLE void zeroOrientation() {
        // Reset orientation and restart gyro bias calibration
        m_pitch = 0; m_roll = 0; m_yaw = 0;
        m_biasGx = 0; m_biasGy = 0; m_biasGz = 0;
        m_sumGx  = 0; m_sumGy  = 0; m_sumGz  = 0;
        m_calibCount = 0;
        m_orientationInitialized = false;
        emit calibratingChanged();
    }

    Q_INVOKABLE void disconnectDevice() {
        if (m_agent && m_agent->isActive()) {
            m_agent->stop();
            m_scanning = false;
            emit scanningChanged();
        }
        m_connected = false;
        emit connectedChanged();
        cleanupConnection();
        resetStack();
    }

signals:
    void scanningChanged();
    void connectedChanged();
    void devicesChanged();
    void orientationChanged();
    void calibratingChanged();
    void statusChanged();
    void resetDone();

private slots:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &dev) {
        if (!dev.name().contains("CC2650 SensorTag", Qt::CaseInsensitive)) return;
        for (const auto &d : m_found) if (d.address() == dev.address()) return;
        m_found.append(dev);
        QVariantMap e;
        e["name"]    = dev.name();
        e["address"] = dev.address().toString();
        m_devices.append(e);
        emit devicesChanged();
    }

    void onDevicePropsChanged(const QString &iface, const QVariantMap &changed, const QStringList &) {
        if (iface != "org.bluez.Device1") return;

        // Device dropped connection
        if (changed.contains("Connected") && !changed["Connected"].toBool()) {
            bool was = m_connected;
            m_connected = false;
            emit connectedChanged();
            if (was) resetStack();
            return;
        }

        // BlueZ finished resolving GATT services (uses cache on repeat connections)
        if (changed.contains("ServicesResolved") && changed["ServicesResolved"].toBool()) {
            setStatus("Finding movement service...");
            findAndConfigureCharacteristics();
        }
    }

    void onDataChanged(const QString &iface, const QVariantMap &changed, const QStringList &) {
        if (iface != "org.bluez.GattCharacteristic1") return;
        if (!changed.contains("Value")) return;

        QByteArray data;
        QVariant raw = changed["Value"];
        while (raw.userType() == qMetaTypeId<QDBusVariant>())
            raw = raw.value<QDBusVariant>().variant();
        if (raw.userType() == QMetaType::QByteArray) {
            data = raw.toByteArray();
        } else if (raw.userType() == qMetaTypeId<QDBusArgument>()) {
            raw.value<QDBusArgument>() >> data;
        }
        if (data.size() < 18) return;

        auto i16 = [&](int i) -> int16_t {
            return int16_t(uint8_t(data[i]) | (uint16_t(uint8_t(data[i+1])) << 8));
        };

        // Gyro: bytes 0-5, scale = 1/128 deg/s
        const double dt = 0.1;
        double gx = i16(0) / 128.0;
        double gy = i16(2) / 128.0;
        double gz = i16(4) / 128.0;

        // Accel: bytes 6-11, scale = 1/4096 g  (±8g range)
        double ax = i16(6)  / 4096.0;
        double ay = i16(8)  / 4096.0;
        double az = i16(10) / 4096.0;

        // Store raw values for debug overlay
        m_rawGx = gx; m_rawGy = gy; m_rawGz = gz;
        m_rawAx = ax; m_rawAy = ay; m_rawAz = az;

        // Gyro bias calibration: average first CALIB_SAMPLES readings while stationary
        if (m_calibCount < CALIB_SAMPLES) {
            m_sumGx += gx; m_sumGy += gy; m_sumGz += gz;
            m_calibCount++;
            if (m_calibCount == CALIB_SAMPLES) {
                m_biasGx = m_sumGx / CALIB_SAMPLES;
                m_biasGy = m_sumGy / CALIB_SAMPLES;
                m_biasGz = m_sumGz / CALIB_SAMPLES;
                emit calibratingChanged();
            }
            return;  // Hold orientation still during calibration
        }

        // Adaptive gyro bias: when sensor is nearly still, slowly update bias
        // to track thermal drift. Uses raw (pre-subtraction) values.
        double gyroMag = qSqrt(gx*gx + gy*gy + gz*gz);
        if (gyroMag < 3.0) {  // nearly stationary (< 3 deg/s total)
            const double biasAlpha = 0.01;  // slow adaptation
            m_biasGx += biasAlpha * (gx - m_biasGx);
            m_biasGy += biasAlpha * (gy - m_biasGy);
            m_biasGz += biasAlpha * (gz - m_biasGz);
        }

        // Subtract adaptive bias so stationary tag reads ~0 deg/s
        gx -= m_biasGx; gy -= m_biasGy; gz -= m_biasGz;

        // Accelerometer-based absolute tilt reference (valid when mostly static)
        double acc_pitch = -qRadiansToDegrees(qAtan2(ay, qSqrt(ax*ax + az*az)));
        double acc_roll  =  qRadiansToDegrees(qAtan2(-ax, qSqrt(ay*ay + az*az)));

        // On the very first frame after calibration, snap to actual device position
        // so the model immediately matches wherever the tag is sitting
        if (!m_orientationInitialized) {
            m_pitch = acc_pitch;
            m_roll  = acc_roll;
            m_yaw   = 0;
            m_orientationInitialized = true;
        }

        // Complementary filter:
        //   96% gyro integration (fast response, correct axis mapping)
        //    4% accel correction (gravity reference, prevents slow drift)
        // Yaw: no magnetometer, gyro-only with slow decay toward zero
        const double alpha = 0.96;
        m_pitch = alpha * (m_pitch + gx * dt) + (1.0 - alpha) * acc_pitch;
        m_roll  = alpha * (m_roll  + gz * dt) + (1.0 - alpha) * acc_roll;
        m_yaw   = 0.998 * (m_yaw + gy * dt);

        // Stream to Kafka
        g_kafka.produce(m_pitch, m_roll, m_yaw, ax, ay, az, gx, gy, gz);

        emit orientationChanged();
    }

private:
    void setStatus(const QString &s) { m_status = s; emit statusChanged(); }

    void findAndConfigureCharacteristics() {
        // GetManagedObjects — fast local D-Bus call, returns BlueZ GATT object tree
        QDBusInterface objMgr("org.bluez", "/",
                              "org.freedesktop.DBus.ObjectManager",
                              QDBusConnection::systemBus());
        QDBusReply<BluezManagedObjects> reply = objMgr.call("GetManagedObjects");
        if (!reply.isValid()) {
            setStatus("Service lookup failed: " + reply.error().message());
            return;
        }

        const QString DATA = "f000aa81-0451-4000-b000-000000000000";
        const QString CFG  = "f000aa82-0451-4000-b000-000000000000";
        const QString PER  = "f000aa83-0451-4000-b000-000000000000";

        for (auto it = reply.value().constBegin(); it != reply.value().constEnd(); ++it) {
            const QString path = it.key().path();
            if (!path.startsWith(m_devicePath)) continue;
            const auto &ifaces = it.value();
            if (!ifaces.contains("org.bluez.GattCharacteristic1")) continue;

            QString uuid = ifaces["org.bluez.GattCharacteristic1"]["UUID"]
                               .variant().toString().toLower();
            if      (uuid == DATA) m_dataPath = path;
            else if (uuid == CFG)  m_cfgPath  = path;
            else if (uuid == PER)  m_perPath  = path;
        }

        if (m_dataPath.isEmpty()) {
            setStatus("Movement service not found");
            return;
        }

        // Reset orientation and gyro bias calibration for fresh connection
        m_pitch = 0; m_roll = 0; m_yaw = 0;
        m_biasGx = 0; m_biasGy = 0; m_biasGz = 0;
        m_sumGx  = 0; m_sumGy  = 0; m_sumGz  = 0;
        m_calibCount = 0;
        m_orientationInitialized = false;

        setStatus("Enabling sensor...");

        // Enable all 9-axis IMU at ±8g
        writeChar(m_cfgPath, QByteArray::fromHex("7F08"));
        // 100 ms sample period
        writeChar(m_perPath, QByteArray::fromHex("0A"));

        // Subscribe to notifications
        QDBusInterface dataChar("org.bluez", m_dataPath,
                                "org.bluez.GattCharacteristic1",
                                QDBusConnection::systemBus());
        dataChar.asyncCall("StartNotify");

        // Wire up data signal
        QDBusConnection::systemBus().connect(
            "org.bluez", m_dataPath,
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            this, SLOT(onDataChanged(QString, QVariantMap, QStringList)));

        m_connected = true;
        emit connectedChanged();
    }

    void writeChar(const QString &path, const QByteArray &value) {
        if (path.isEmpty()) return;
        QDBusInterface ch("org.bluez", path,
                          "org.bluez.GattCharacteristic1",
                          QDBusConnection::systemBus());
        QList<QVariant> args;
        args << QVariant::fromValue(value) << QVariant::fromValue(QVariantMap());
        ch.callWithArgumentList(QDBus::NoBlock, "WriteValue", args);
    }

    void cleanupConnection() {
        if (!m_dataPath.isEmpty()) {
            QDBusConnection::systemBus().disconnect(
                "org.bluez", m_dataPath,
                "org.freedesktop.DBus.Properties", "PropertiesChanged",
                this, SLOT(onDataChanged(QString, QVariantMap, QStringList)));
            QDBusInterface ch("org.bluez", m_dataPath,
                              "org.bluez.GattCharacteristic1",
                              QDBusConnection::systemBus());
            ch.asyncCall("StopNotify");
        }
        if (!m_devicePath.isEmpty()) {
            QDBusConnection::systemBus().disconnect(
                "org.bluez", m_devicePath,
                "org.freedesktop.DBus.Properties", "PropertiesChanged",
                this, SLOT(onDevicePropsChanged(QString, QVariantMap, QStringList)));
            QDBusInterface dev("org.bluez", m_devicePath,
                               "org.bluez.Device1", QDBusConnection::systemBus());
            dev.asyncCall("Disconnect");
        }
        m_dataPath.clear(); m_cfgPath.clear(); m_perPath.clear();
    }

    void removeCachedDevices() {
        // Ask BlueZ ObjectManager for all known devices, then remove them
        // so the next scan only reports currently-advertising devices
        QDBusInterface om("org.bluez", "/",
                          "org.freedesktop.DBus.ObjectManager",
                          QDBusConnection::systemBus());
        QDBusReply<BluezManagedObjects> reply = om.call("GetManagedObjects");
        if (!reply.isValid()) return;

        QDBusInterface adapter("org.bluez", "/org/bluez/hci0",
                               "org.bluez.Adapter1",
                               QDBusConnection::systemBus());
        for (auto it = reply.value().constBegin(); it != reply.value().constEnd(); ++it) {
            const QString &path = it.key().path();
            if (path.startsWith("/org/bluez/hci0/dev_") && it.value().contains("org.bluez.Device1")) {
                adapter.call("RemoveDevice", QVariant::fromValue(it.key()));
            }
        }
    }

    void resetStack() {
        m_resetting = true;
        // Power-cycle via BlueZ D-Bus — BlueZ manages state properly.
        // Raw hciconfig reset bypasses BlueZ and breaks subsequent scans.
        QDBusInterface adapter("org.bluez", "/org/bluez/hci0",
                               "org.bluez.Adapter1",
                               QDBusConnection::systemBus());
        adapter.setProperty("Powered", false);
        QTimer::singleShot(400, this, [this]() {
            QDBusInterface adapter("org.bluez", "/org/bluez/hci0",
                                   "org.bluez.Adapter1",
                                   QDBusConnection::systemBus());
            adapter.setProperty("Powered", true);
            QTimer::singleShot(300, this, [this]() {
                m_resetting = false;
                emit resetDone();
            });
        });
    }

    QBluetoothDeviceDiscoveryAgent *m_agent = nullptr;
    QVariantList m_devices;
    QList<QBluetoothDeviceInfo> m_found;
    bool m_scanning  = false;
    bool m_connected = false;
    bool m_resetting = false;
    double m_pitch = 0, m_roll = 0, m_yaw = 0;
    double m_rawGx = 0, m_rawGy = 0, m_rawGz = 0;
    double m_rawAx = 0, m_rawAy = 0, m_rawAz = 0;
    double m_biasGx = 0, m_biasGy = 0, m_biasGz = 0;
    double m_sumGx  = 0, m_sumGy  = 0, m_sumGz  = 0;
    int m_calibCount = 0;
    bool m_orientationInitialized = false;
    static const int CALIB_SAMPLES = 20;  // 2 seconds at 100ms period
    QString m_status;
    QString m_devicePath;
    QString m_dataPath;
    QString m_cfgPath;
    QString m_perPath;
};

class Calibrator : public QObject {
    Q_OBJECT
public:
    explicit Calibrator(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE void save(double mx, double bx, double my, double by) {
        long long a0=20697, a1=0, a2=0, a3=0, a4=12775, a5=0, a6=65536;
        QFile rf("/etc/pointercal");
        if (rf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&rf);
            in >> a0 >> a1 >> a2 >> a3 >> a4 >> a5 >> a6;
            rf.close();
        }
        long long na0 = (long long)(mx * a0);
        long long na2 = (long long)(mx * a2 + bx * a6);
        long long na4 = (long long)(my * a4);
        long long na5 = (long long)(my * a5 + by * a6);
        QString cal = QString("%1 0 %2 0 %3 %4 %5\n")
                          .arg(na0).arg(na2).arg(na4).arg(na5).arg(a6);
        QFile f("/etc/pointercal");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream(&f) << cal;
            f.close();
        }
        QCoreApplication::quit();
    }
};

#include "main.moc"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    g_kafka.init();
    Calibrator cal;
    SensorTagManager sensorTag;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("calibrator", &cal);
    engine.rootContext()->setContextProperty("sensorTag", &sensorTag);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()) return -1;
    return app.exec();
}

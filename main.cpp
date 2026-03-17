#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QObject>
#include <QFile>
#include <QTextStream>

class Calibrator : public QObject {
    Q_OBJECT
public:
    explicit Calibrator(QObject *parent = nullptr) : QObject(parent) {}

    // Called from QML with least-squares affine correction coefficients:
    // corrected_x = mx * old_x + bx
    // corrected_y = my * old_y + by
    Q_INVOKABLE void save(double mx, double bx, double my, double by) {
        // Current pointercal: a0 a1 a2 a3 a4 a5 a6
        // x_screen = (a0*ADS + a2) / a6
        // After correction: x_new = mx*(a0*ADS+a2)/a6 + bx
        //                         = (mx*a0*ADS + mx*a2 + bx*a6) / a6

        // Read current pointercal
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

        // Quit — systemd Restart=always will relaunch with new calibration
        QCoreApplication::quit();
    }
};

#include "main.moc"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    Calibrator cal;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("calibrator", &cal);
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty()) return -1;
    return app.exec();
}

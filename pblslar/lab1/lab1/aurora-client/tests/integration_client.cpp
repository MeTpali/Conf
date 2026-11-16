#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

#include "temperaturecontroller.h"

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    TemperatureController controller;
    const QString endpoint = argc > 1
        ? QString::fromLocal8Bit(argv[1])
        : QStringLiteral("ws://10.0.2.2:8080/ws");

    QObject::connect(&controller, &TemperatureController::connectedChanged, [&]() {
        if (controller.connected())
            controller.convert(QStringLiteral("36.6"), QStringLiteral("C"));
    });
    QObject::connect(&controller, &TemperatureController::resultChanged, [&]() {
        const QString result = controller.result();
        if (result.contains(QStringLiteral("36.6 °C"))
                && result.contains(QStringLiteral("97.88 °F"))) {
            qInfo().noquote() << "OK:" << result;
            application.exit(0);
        } else {
            qCritical().noquote() << "Unexpected result:" << result;
            application.exit(2);
        }
    });

    QTimer::singleShot(10000, [&]() {
        qCritical().noquote() << "Timeout:" << controller.status() << controller.error();
        application.exit(3);
    });

    controller.connectToServer(endpoint);
    return application.exec();
}


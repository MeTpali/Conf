#include <auroraapp.h>
#include <QtQuick>

#include "temperaturecontroller.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> application(Aurora::Application::application(argc, argv));
    application->setOrganizationName(QStringLiteral("ru.mai"));
    application->setApplicationName(QStringLiteral("temperature_converter"));

    TemperatureController controller;

    QScopedPointer<QQuickView> view(Aurora::Application::createView());
    view->rootContext()->setContextProperty(QStringLiteral("temperatureController"), &controller);
    view->setSource(Aurora::Application::pathTo(
        QStringLiteral("qml/ru.mai.temperature_converter.qml")));
    view->show();

    return application->exec();
}


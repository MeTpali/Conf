#include <auroraapp.h>
#include <QtQuick>

#include "resilientcontroller.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> application(Aurora::Application::application(argc, argv));
    application->setOrganizationName(QStringLiteral("ru.mai"));
    application->setApplicationName(QStringLiteral("resilient_music"));
    ResilientController controller;
    QScopedPointer<QQuickView> view(Aurora::Application::createView());
    view->rootContext()->setContextProperty(QStringLiteral("resilientController"), &controller);
    view->setSource(Aurora::Application::pathTo(QStringLiteral("qml/ru.mai.resilient_music.qml")));
    view->show();
    return application->exec();
}

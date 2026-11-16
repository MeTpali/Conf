#include <auroraapp.h>
#include <QtQuick>

#include "albumcontroller.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> application(Aurora::Application::application(argc, argv));
    application->setOrganizationName(QStringLiteral("ru.mai"));
    application->setApplicationName(QStringLiteral("music_collection"));

    AlbumController controller;
    QScopedPointer<QQuickView> view(Aurora::Application::createView());
    view->rootContext()->setContextProperty(QStringLiteral("albumController"), &controller);
    view->setSource(Aurora::Application::pathTo(QStringLiteral("qml/ru.mai.music_collection.qml")));
    view->show();
    return application->exec();
}

#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QStandardPaths>
#include <QString>

// Песочница Авроры открывает приложению только его каталог ~/.config/ru.mai/music_collection,
// поэтому настройки и cookie хранятся внутри него, а не в ~/.config/ru.mai/music_collection.conf.
inline QString appSettingsFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/music_collection.conf");
}

#endif

#include "persistentcookiejar.h"

#include "appsettings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkCookie>

PersistentCookieJar::PersistentCookieJar(QObject *parent)
    : QNetworkCookieJar(parent)
    , m_settings(appSettingsFile(), QSettings::IniFormat)
{
    load();
}

bool PersistentCookieJar::setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url)
{
    const bool accepted = QNetworkCookieJar::setCookiesFromUrl(cookieList, url);
    if (accepted)
        save();
    return accepted;
}

void PersistentCookieJar::clearStoredCookies()
{
    setAllCookies(QList<QNetworkCookie>());
    save();
}

void PersistentCookieJar::load()
{
    const QJsonDocument document = QJsonDocument::fromJson(m_settings.value(QStringLiteral("cookies")).toByteArray());
    QList<QNetworkCookie> cookies;
    for (const QJsonValue &value : document.array()) {
        const QByteArray raw = QByteArray::fromBase64(value.toString().toLatin1());
        cookies.append(QNetworkCookie::parseCookies(raw));
    }
    setAllCookies(cookies);
}

void PersistentCookieJar::save() const
{
    QJsonArray array;
    for (const QNetworkCookie &cookie : allCookies())
        array.append(QString::fromLatin1(cookie.toRawForm(QNetworkCookie::Full).toBase64()));
    m_settings.setValue(QStringLiteral("cookies"), QJsonDocument(array).toJson(QJsonDocument::Compact));
}

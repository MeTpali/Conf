#ifndef PERSISTENTCOOKIEJAR_H
#define PERSISTENTCOOKIEJAR_H

#include <QNetworkCookieJar>
#include <QSettings>

class PersistentCookieJar : public QNetworkCookieJar
{
    Q_OBJECT

public:
    explicit PersistentCookieJar(QObject *parent = nullptr);
    bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url) override;
    void clearStoredCookies();

private:
    void load();
    void save() const;
    mutable QSettings m_settings;
};

#endif

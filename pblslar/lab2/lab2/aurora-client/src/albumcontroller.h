#ifndef ALBUMCONTROLLER_H
#define ALBUMCONTROLLER_H

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QSqlDatabase>
#include <QVariantList>

class PersistentCookieJar;
class QNetworkReply;

class AlbumController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList albums READ albums NOTIFY albumsChanged)
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool online READ online WRITE setOnline NOTIFY onlineChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool authenticated READ authenticated NOTIFY authenticatedChanged)
    Q_PROPERTY(bool allowInsecureTls READ allowInsecureTls WRITE setAllowInsecureTls NOTIFY allowInsecureTlsChanged)
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY pendingCountChanged)

public:
    explicit AlbumController(QObject *parent = nullptr);
    ~AlbumController();

    QVariantList albums() const { return m_albums; }
    QString baseUrl() const { return m_baseUrl; }
    QString status() const { return m_status; }
    QString error() const { return m_error; }
    bool online() const { return m_online; }
    bool busy() const { return m_busy; }
    bool authenticated() const { return m_authenticated; }
    bool allowInsecureTls() const { return m_allowInsecureTls; }
    int pendingCount() const;

    void setBaseUrl(const QString &value);
    void setOnline(bool value);
    void setAllowInsecureTls(bool value);

    Q_INVOKABLE void login(const QString &username, const QString &password);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void createAlbum(const QString &performer, const QString &album,
                                 const QString &year, const QString &genre);
    Q_INVOKABLE void updateAlbum(int id, const QString &performer, const QString &album,
                                 const QString &year, const QString &genre);
    Q_INVOKABLE void deleteAlbum(int id);
    Q_INVOKABLE void synchronize();

signals:
    void albumsChanged();
    void baseUrlChanged();
    void statusChanged();
    void errorChanged();
    void onlineChanged();
    void busyChanged();
    void authenticatedChanged();
    void allowInsecureTlsChanged();
    void pendingCountChanged();

private:
    enum RequestKind { LoginRequest, RefreshRequest, SyncRequest, LogoutRequest };

    void initializeDatabase();
    void loadCache();
    void replaceCache(const QJsonArray &items);
    void queueOperation(const QString &method, const QString &path, const QJsonObject &body, int localId = 0);
    QJsonObject normalizedAlbum(const QString &performer, const QString &album,
                                const QString &year, const QString &genre, bool *ok);
    QNetworkReply *send(const QString &method, const QString &path, const QJsonObject &body,
                        RequestKind kind);
    void handleFinished(QNetworkReply *reply);
    void syncNext();
    void setStatus(const QString &value);
    void setError(const QString &value);
    void setBusy(bool value);
    void setAuthenticated(bool value);
    static QString networkMessage(QNetworkReply *reply, const QByteArray &payload);

    QVariantList m_albums;
    QString m_baseUrl;
    QString m_status;
    QString m_error;
    bool m_online;
    bool m_busy;
    bool m_authenticated;
    bool m_allowInsecureTls;
    bool m_syncing;
    QNetworkAccessManager m_network;
    PersistentCookieJar *m_cookieJar;
    QSettings m_settings;
    QSqlDatabase m_database;
};

#endif

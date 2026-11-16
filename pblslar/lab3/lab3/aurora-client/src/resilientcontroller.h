#ifndef RESILIENTCONTROLLER_H
#define RESILIENTCONTROLLER_H

#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

#include <functional>

class QNetworkRequest;

class ResilientController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString apiUrl READ apiUrl WRITE setApiUrl NOTIFY apiUrlChanged)
    Q_PROPERTY(QString webSocketUrl READ webSocketUrl WRITE setWebSocketUrl NOTIFY webSocketUrlChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString instance READ instance NOTIFY healthChanged)
    Q_PROPERTY(QString version READ version NOTIFY healthChanged)
    Q_PROPERTY(bool available READ available NOTIFY healthChanged)
    Q_PROPERTY(bool pushConnected READ pushConnected NOTIFY pushConnectedChanged)
    Q_PROPERTY(int retryCount READ retryCount NOTIFY retryCountChanged)
    Q_PROPERTY(QStringList notifications READ notifications NOTIFY notificationsChanged)
    Q_PROPERTY(QStringList albums READ albums NOTIFY albumsChanged)
    Q_PROPERTY(QString albumsInstance READ albumsInstance NOTIFY albumsChanged)

public:
    explicit ResilientController(QObject *parent = nullptr);
    QString apiUrl() const { return m_apiUrl; }
    QString webSocketUrl() const { return m_webSocketUrl; }
    QString status() const { return m_status; }
    QString instance() const { return m_instance; }
    QString version() const { return m_version; }
    bool available() const { return m_available; }
    bool pushConnected() const { return m_pushConnected; }
    int retryCount() const { return m_retryCount; }
    QStringList notifications() const { return m_notifications; }
    QStringList albums() const { return m_albums; }
    QString albumsInstance() const { return m_albumsInstance; }

    void setApiUrl(const QString &value);
    void setWebSocketUrl(const QString &value);
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void checkNow();
    Q_INVOKABLE void loadAlbums();
    Q_INVOKABLE void addTestAlbum();

signals:
    void apiUrlChanged();
    void webSocketUrlChanged();
    void statusChanged();
    void healthChanged();
    void pushConnectedChanged();
    void retryCountChanged();
    void notificationsChanged();
    void albumsChanged();

private:
    QNetworkRequest apiRequest(const QString &path) const;
    void login(const std::function<void()> &next);
    void checkHealth();
    void scheduleRetry();
    void connectPush();
    void sendHandshake();
    void processHandshake();
    void consumeFrames();
    void addNotification(const QString &message);
    void setStatus(const QString &value);
    void setAvailable(bool value);
    void setPushConnected(bool value);

    QNetworkAccessManager m_network;
    QTcpSocket m_socket;
    QTimer m_healthTimer;
    QTimer m_retryTimer;
    QSettings m_settings;
    QString m_apiUrl;
    QString m_webSocketUrl;
    QString m_status;
    QString m_instance;
    QString m_version;
    QStringList m_notifications;
    QStringList m_albums;
    QString m_albumsInstance;
    QByteArray m_buffer;
    QByteArray m_clientKey;
    QUrl m_ws;
    bool m_available;
    bool m_pushConnected;
    bool m_handshakeComplete;
    bool m_running;
    int m_retryCount;
};

#endif

#include "resilientcontroller.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QtGlobal>

namespace {
const QByteArray websocketGuid("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

// Песочница Авроры открывает приложению только его каталог ~/.config/ru.mai/resilient_music,
// поэтому файл настроек хранится внутри него, а не в ~/.config/ru.mai/resilient_music.conf.
QString settingsFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/resilient_music.conf");
}
}

ResilientController::ResilientController(QObject *parent)
    : QObject(parent)
    , m_settings(settingsFile(), QSettings::IniFormat)
    , m_apiUrl(m_settings.value(QStringLiteral("apiUrl"), QStringLiteral("http://10.0.2.2:8080")).toString())
    , m_webSocketUrl(m_settings.value(QStringLiteral("wsUrl"), QStringLiteral("ws://10.0.2.2:8080/ws/notifications")).toString())
    , m_status(QStringLiteral("Мониторинг остановлен"))
    , m_available(false)
    , m_pushConnected(false)
    , m_handshakeComplete(false)
    , m_running(false)
    , m_retryCount(0)
{
    m_healthTimer.setInterval(5000);
    connect(&m_healthTimer, &QTimer::timeout, this, &ResilientController::checkHealth);
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, [this]() { checkHealth(); loadAlbums(); connectPush(); });
    connect(&m_socket, &QTcpSocket::connected, this, &ResilientController::sendHandshake);
    connect(&m_socket, &QTcpSocket::readyRead, this, [this]() {
        m_buffer += m_socket.readAll();
        if (!m_handshakeComplete) processHandshake();
        if (m_handshakeComplete) consumeFrames();
    });
    connect(&m_socket, &QTcpSocket::disconnected, this, [this]() {
        m_handshakeComplete = false;
        setPushConnected(false);
        if (m_running) scheduleRetry();
    });
}

void ResilientController::setApiUrl(const QString &value)
{
    QString normalized = value.trimmed(); while (normalized.endsWith('/')) normalized.chop(1);
    if (normalized == m_apiUrl) return;
    m_apiUrl = normalized; m_settings.setValue(QStringLiteral("apiUrl"), normalized); emit apiUrlChanged();
}

void ResilientController::setWebSocketUrl(const QString &value)
{
    if (value.trimmed() == m_webSocketUrl) return;
    m_webSocketUrl = value.trimmed(); m_settings.setValue(QStringLiteral("wsUrl"), m_webSocketUrl); emit webSocketUrlChanged();
}

void ResilientController::start()
{
    if (m_running) return;
    m_running = true; m_retryCount = 0; emit retryCountChanged();
    m_healthTimer.start(); checkHealth(); connectPush();
}

void ResilientController::stop()
{
    m_running = false; m_healthTimer.stop(); m_retryTimer.stop(); m_socket.abort();
    setAvailable(false); setPushConnected(false); setStatus(QStringLiteral("Мониторинг остановлен"));
}

void ResilientController::checkNow() { checkHealth(); loadAlbums(); }

QNetworkRequest ResilientController::apiRequest(const QString &path) const
{
    QNetworkRequest request(QUrl(m_apiUrl + path));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "AuroraResilientMusic/1.0");
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    return request;
}

void ResilientController::login(const std::function<void()> &next)
{
    QJsonObject body;
    body.insert(QStringLiteral("username"), m_settings.value(QStringLiteral("username"), QStringLiteral("demo")).toString());
    body.insert(QStringLiteral("password"), m_settings.value(QStringLiteral("password"), QStringLiteral("demo")).toString());
    QNetworkReply *reply = m_network.post(apiRequest(QStringLiteral("/api/v1/auth/login")), QJsonDocument(body).toJson(QJsonDocument::Compact));
    QTimer::singleShot(4000, reply, [reply]() { if (reply->isRunning()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, next]() {
        if (reply->error() == QNetworkReply::NoError) next();
        else { setStatus(QStringLiteral("Вход не выполнен: %1").arg(reply->errorString())); scheduleRetry(); }
        reply->deleteLater();
    });
}

void ResilientController::loadAlbums()
{
    if (!m_running) return;
    QNetworkReply *reply = m_network.get(apiRequest(QStringLiteral("/api/v1/albums")));
    QTimer::singleShot(4000, reply, [reply]() { if (reply->isRunning()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code == 401) {
            login([this]() { loadAlbums(); });
        } else if (reply->error() == QNetworkReply::NoError) {
            QStringList items;
            const QJsonArray array = QJsonDocument::fromJson(reply->readAll()).array();
            for (const QJsonValue &value : array) {
                const QJsonObject album = value.toObject();
                items << QStringLiteral("%1 — %2 (%3, %4)").arg(
                    album.value(QStringLiteral("performer")).toString(), album.value(QStringLiteral("album")).toString(),
                    QString::number(album.value(QStringLiteral("year")).toInt()), album.value(QStringLiteral("genre")).toString());
            }
            m_albums = items;
            m_albumsInstance = QString::fromUtf8(reply->rawHeader("X-Instance-Id"));
            emit albumsChanged();
        } else {
            // Последний загруженный список остаётся на экране, запрос будет повторён.
            setStatus(QStringLiteral("Альбомы не загружены: %1").arg(reply->errorString()));
            scheduleRetry();
        }
        reply->deleteLater();
    });
}

void ResilientController::addTestAlbum()
{
    if (!m_running) return;
    QJsonObject body;
    body.insert(QStringLiteral("performer"), QStringLiteral("Aurora Demo"));
    body.insert(QStringLiteral("album"), QStringLiteral("Push %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
    body.insert(QStringLiteral("year"), QDate::currentDate().year());
    body.insert(QStringLiteral("genre"), QStringLiteral("Test"));
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_network.post(apiRequest(QStringLiteral("/api/v1/albums")), payload);
    QTimer::singleShot(4000, reply, [reply]() { if (reply->isRunning()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code == 401) login([this]() { addTestAlbum(); });
        else if (code == 201) setStatus(QStringLiteral("Альбом добавлен, ожидается push"));
        else setStatus(QStringLiteral("Ошибка добавления: %1").arg(reply->errorString()));
        reply->deleteLater();
    });
}

void ResilientController::checkHealth()
{
    if (!m_running) return;
    QNetworkReply *reply = m_network.get(apiRequest(QStringLiteral("/api/v1/health")));
    QTimer::singleShot(4000, reply, [reply]() { if (reply->isRunning()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QJsonObject object = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() == QNetworkReply::NoError && object.value(QStringLiteral("status")).toString() == QStringLiteral("ok")) {
            m_instance = object.value(QStringLiteral("instance")).toString();
            m_version = object.value(QStringLiteral("version")).toString();
            const bool recovered = !m_available || m_albums.isEmpty();
            setAvailable(true); m_retryCount = 0; emit retryCountChanged(); emit healthChanged();
            setStatus(QStringLiteral("Балансировщик доступен"));
            if (recovered) loadAlbums();
        } else {
            setAvailable(false); setStatus(QStringLiteral("Backend недоступен: %1").arg(reply->errorString())); scheduleRetry();
        }
        reply->deleteLater();
    });
}

void ResilientController::scheduleRetry()
{
    if (!m_running || m_retryTimer.isActive()) return;
    ++m_retryCount; emit retryCountChanged();
    const int delay = qMin(30000, 1000 * (1 << qMin(m_retryCount - 1, 5)));
    m_retryTimer.start(delay);
    setStatus(QStringLiteral("Повтор через %1 с").arg(delay / 1000));
}

void ResilientController::connectPush()
{
    if (!m_running || m_socket.state() != QAbstractSocket::UnconnectedState) return;
    m_ws = QUrl(m_webSocketUrl);
    if (m_ws.scheme() != QStringLiteral("ws") || m_ws.host().isEmpty()) {
        setStatus(QStringLiteral("Укажите ws:// адрес push-канала")); return;
    }
    m_buffer.clear(); m_handshakeComplete = false;
    m_socket.connectToHost(m_ws.host(), static_cast<quint16>(m_ws.port(80)));
}

void ResilientController::sendHandshake()
{
    QByteArray random(16, Qt::Uninitialized);
    for (int i = 0; i < random.size(); ++i) random[i] = static_cast<char>(qrand() & 0xff);
    m_clientKey = random.toBase64();
    QByteArray path = m_ws.path(QUrl::FullyEncoded).toUtf8(); if (path.isEmpty()) path = "/ws/notifications";
    QByteArray request = "GET " + path + " HTTP/1.1\r\nHost: " + m_ws.host().toUtf8() + ':' + QByteArray::number(m_ws.port(80))
        + "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + m_clientKey
        + "\r\nSec-WebSocket-Version: 13\r\n\r\n";
    m_socket.write(request);
}

void ResilientController::processHandshake()
{
    const int end = m_buffer.indexOf("\r\n\r\n"); if (end < 0) return;
    const QByteArray headers = m_buffer.left(end + 4); m_buffer.remove(0, end + 4);
    const QByteArray expected = QCryptographicHash::hash(m_clientKey + websocketGuid, QCryptographicHash::Sha1).toBase64();
    if (!headers.startsWith("HTTP/1.1 101 ") || !headers.toLower().contains("sec-websocket-accept: " + expected.toLower())) {
        m_socket.abort(); scheduleRetry(); return;
    }
    m_handshakeComplete = true; setPushConnected(true); setStatus(QStringLiteral("Push подключён"));
}

void ResilientController::consumeFrames()
{
    while (m_buffer.size() >= 2) {
        const quint8 first = static_cast<quint8>(m_buffer.at(0));
        quint64 length = static_cast<quint8>(m_buffer.at(1)) & 0x7f; int offset = 2;
        if (length == 126) { if (m_buffer.size() < 4) return; length = (static_cast<quint8>(m_buffer.at(2)) << 8) | static_cast<quint8>(m_buffer.at(3)); offset = 4; }
        else if (length == 127) { if (m_buffer.size() < 10) return; length = 0; for (int i = 2; i < 10; ++i) length = (length << 8) | static_cast<quint8>(m_buffer.at(i)); offset = 10; }
        if (length > 65536 || m_buffer.size() < offset + static_cast<int>(length)) return;
        const QByteArray payload = m_buffer.mid(offset, static_cast<int>(length)); m_buffer.remove(0, offset + static_cast<int>(length));
        if ((first & 0x0f) == 0x1) {
            const QJsonObject object = QJsonDocument::fromJson(payload).object();
            if (object.value(QStringLiteral("type")).toString() == QStringLiteral("albums")) loadAlbums();
            addNotification(QStringLiteral("%1 · %2 · %3").arg(object.value(QStringLiteral("message")).toString(), object.value(QStringLiteral("instance")).toString(), object.value(QStringLiteral("version")).toString()));
        } else if ((first & 0x0f) == 0x8) { m_socket.disconnectFromHost(); return; }
    }
}

void ResilientController::addNotification(const QString &message)
{
    m_notifications.prepend(message); while (m_notifications.size() > 20) m_notifications.removeLast(); emit notificationsChanged();
}
void ResilientController::setStatus(const QString &value) { if (m_status != value) { m_status = value; emit statusChanged(); } }
void ResilientController::setAvailable(bool value) { if (m_available != value) { m_available = value; emit healthChanged(); } }
void ResilientController::setPushConnected(bool value) { if (m_pushConnected != value) { m_pushConnected = value; emit pushConnectedChanged(); } }

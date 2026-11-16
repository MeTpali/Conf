#include "albumcontroller.h"
#include "appsettings.h"
#include "persistentcookiejar.h"

#include <QBuffer>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSqlError>
#include <QSqlQuery>
#include <QSslError>
#include <QStandardPaths>
#include <QTimer>

AlbumController::AlbumController(QObject *parent)
    : QObject(parent)
    , m_baseUrl(QStringLiteral("https://10.0.2.2:8443"))
    , m_status(QStringLiteral("Данные загружены из локального кэша"))
    , m_online(true)
    , m_busy(false)
    , m_authenticated(false)
    , m_allowInsecureTls(true)
    , m_syncing(false)
    , m_cookieJar(new PersistentCookieJar(this))
    , m_settings(appSettingsFile(), QSettings::IniFormat)
{
    // m_settings объявлен в классе после этих полей и создаётся позже них,
    // поэтому сохранённые значения читаются только здесь.
    m_online = m_settings.value(QStringLiteral("online"), true).toBool();
    m_allowInsecureTls = m_settings.value(QStringLiteral("allowInsecureTls"), true).toBool();
    m_baseUrl = m_settings.value(QStringLiteral("baseUrl"), m_baseUrl).toString();
    m_authenticated = m_settings.value(QStringLiteral("authenticated"), false).toBool();
    m_network.setCookieJar(m_cookieJar);
    m_cookieJar->setParent(&m_network);
    initializeDatabase();
    loadCache();
}

AlbumController::~AlbumController()
{
    const QString connectionName = m_database.connectionName();
    m_database.close();
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

void AlbumController::initializeDatabase()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(directory);
    const QString connectionName = QStringLiteral("music-cache-%1").arg(reinterpret_cast<quintptr>(this));
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    m_database.setDatabaseName(directory + QStringLiteral("/music.sqlite"));
    if (!m_database.open()) {
        setError(QStringLiteral("SQLite: %1").arg(m_database.lastError().text()));
        return;
    }
    QSqlQuery query(m_database);
    query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS albums (id INTEGER PRIMARY KEY, performer TEXT NOT NULL, album TEXT NOT NULL, year INTEGER NOT NULL, genre TEXT NOT NULL, dirty INTEGER NOT NULL DEFAULT 0, deleted INTEGER NOT NULL DEFAULT 0)"));
    query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS pending (id INTEGER PRIMARY KEY AUTOINCREMENT, method TEXT NOT NULL, path TEXT NOT NULL, body TEXT NOT NULL, local_id INTEGER NOT NULL DEFAULT 0)"));
}

int AlbumController::pendingCount() const
{
    if (!m_database.isOpen()) return 0;
    QSqlQuery query(m_database);
    return query.exec(QStringLiteral("SELECT COUNT(*) FROM pending")) && query.next() ? query.value(0).toInt() : 0;
}

void AlbumController::loadCache()
{
    QVariantList result;
    if (m_database.isOpen()) {
        QSqlQuery query(QStringLiteral("SELECT id, performer, album, year, genre, dirty FROM albums WHERE deleted=0 ORDER BY performer, album"), m_database);
        while (query.next()) {
            QVariantMap item;
            item.insert(QStringLiteral("id"), query.value(0));
            item.insert(QStringLiteral("performer"), query.value(1));
            item.insert(QStringLiteral("album"), query.value(2));
            item.insert(QStringLiteral("year"), query.value(3));
            item.insert(QStringLiteral("genre"), query.value(4));
            item.insert(QStringLiteral("dirty"), query.value(5).toBool());
            result.append(item);
        }
    }
    m_albums = result;
    emit albumsChanged();
    emit pendingCountChanged();
}

void AlbumController::replaceCache(const QJsonArray &items)
{
    QSqlQuery query(m_database);
    m_database.transaction();
    query.exec(QStringLiteral("DELETE FROM albums WHERE dirty=0"));
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO albums(id, performer, album, year, genre, dirty, deleted) VALUES(?,?,?,?,?,0,0)"));
    for (const QJsonValue &value : items) {
        const QJsonObject item = value.toObject();
        query.addBindValue(item.value(QStringLiteral("id")).toInt());
        query.addBindValue(item.value(QStringLiteral("performer")).toString());
        query.addBindValue(item.value(QStringLiteral("album")).toString());
        query.addBindValue(item.value(QStringLiteral("year")).toInt());
        query.addBindValue(item.value(QStringLiteral("genre")).toString());
        query.exec();
        query.finish();
    }
    m_database.commit();
    loadCache();
}

void AlbumController::setBaseUrl(const QString &value)
{
    QString normalized = value.trimmed();
    while (normalized.endsWith('/')) normalized.chop(1);
    if (normalized == m_baseUrl) return;
    m_baseUrl = normalized;
    m_settings.setValue(QStringLiteral("baseUrl"), normalized);
    emit baseUrlChanged();
}

void AlbumController::setOnline(bool value)
{
    if (value == m_online) return;
    m_online = value;
    m_settings.setValue(QStringLiteral("online"), value);
    emit onlineChanged();
    setStatus(value ? QStringLiteral("Сеть включена") : QStringLiteral("Offline-режим: изменения сохраняются в очередь"));
    if (value && m_authenticated) synchronize();
}

void AlbumController::setAllowInsecureTls(bool value)
{
    if (value == m_allowInsecureTls) return;
    m_allowInsecureTls = value;
    m_settings.setValue(QStringLiteral("allowInsecureTls"), value);
    emit allowInsecureTlsChanged();
}

QJsonObject AlbumController::normalizedAlbum(const QString &performer, const QString &album,
                                              const QString &year, const QString &genre, bool *ok)
{
    bool yearOk = false;
    const int parsedYear = year.toInt(&yearOk);
    const bool valid = !performer.trimmed().isEmpty() && !album.trimmed().isEmpty()
        && !genre.trimmed().isEmpty() && yearOk && parsedYear >= 1800 && parsedYear <= 2100;
    if (ok) *ok = valid;
    return QJsonObject{{QStringLiteral("performer"), performer.trimmed()},
                       {QStringLiteral("album"), album.trimmed()},
                       {QStringLiteral("year"), parsedYear},
                       {QStringLiteral("genre"), genre.trimmed()}};
}

void AlbumController::queueOperation(const QString &method, const QString &path,
                                     const QJsonObject &body, int localId)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT INTO pending(method,path,body,local_id) VALUES(?,?,?,?)"));
    query.addBindValue(method);
    query.addBindValue(path);
    query.addBindValue(QString::fromUtf8(QJsonDocument(body).toJson(QJsonDocument::Compact)));
    query.addBindValue(localId);
    query.exec();
    emit pendingCountChanged();
}

void AlbumController::createAlbum(const QString &performer, const QString &album,
                                  const QString &year, const QString &genre)
{
    bool ok = false;
    const QJsonObject body = normalizedAlbum(performer, album, year, genre, &ok);
    if (!ok) { setError(QStringLiteral("Заполните все поля; год — от 1800 до 2100")); return; }
    QSqlQuery idQuery(QStringLiteral("SELECT COALESCE(MIN(id),0)-1 FROM albums"), m_database);
    idQuery.next();
    const int localId = qMin(-1, idQuery.value(0).toInt());
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT INTO albums(id,performer,album,year,genre,dirty,deleted) VALUES(?,?,?,?,?,1,0)"));
    query.addBindValue(localId);
    query.addBindValue(body.value(QStringLiteral("performer")).toString());
    query.addBindValue(body.value(QStringLiteral("album")).toString());
    query.addBindValue(body.value(QStringLiteral("year")).toInt());
    query.addBindValue(body.value(QStringLiteral("genre")).toString());
    query.exec();
    queueOperation(QStringLiteral("POST"), QStringLiteral("/api/v1/albums"), body, localId);
    loadCache();
    setStatus(QStringLiteral("Запись сохранена локально"));
    if (m_online && m_authenticated) synchronize();
}

void AlbumController::updateAlbum(int id, const QString &performer, const QString &album,
                                  const QString &year, const QString &genre)
{
    bool ok = false;
    const QJsonObject body = normalizedAlbum(performer, album, year, genre, &ok);
    if (!ok) { setError(QStringLiteral("Проверьте поля альбома")); return; }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE albums SET performer=?,album=?,year=?,genre=?,dirty=1 WHERE id=?"));
    query.addBindValue(body.value(QStringLiteral("performer")).toString());
    query.addBindValue(body.value(QStringLiteral("album")).toString());
    query.addBindValue(body.value(QStringLiteral("year")).toInt());
    query.addBindValue(body.value(QStringLiteral("genre")).toString());
    query.addBindValue(id);
    query.exec();
    if (id > 0) {
        queueOperation(QStringLiteral("PUT"), QStringLiteral("/api/v1/albums/%1").arg(id), body);
    } else {
        query.prepare(QStringLiteral("UPDATE pending SET body=? WHERE local_id=? AND method='POST'"));
        query.addBindValue(QString::fromUtf8(QJsonDocument(body).toJson(QJsonDocument::Compact)));
        query.addBindValue(id);
        query.exec();
    }
    loadCache();
    if (m_online && m_authenticated) synchronize();
}

void AlbumController::deleteAlbum(int id)
{
    QSqlQuery query(m_database);
    if (id < 0) {
        query.prepare(QStringLiteral("DELETE FROM albums WHERE id=?"));
        query.addBindValue(id);
        query.exec();
        query.prepare(QStringLiteral("DELETE FROM pending WHERE local_id=?"));
        query.addBindValue(id);
        query.exec();
    } else {
        query.prepare(QStringLiteral("UPDATE albums SET deleted=1,dirty=1 WHERE id=?"));
        query.addBindValue(id);
        query.exec();
        queueOperation(QStringLiteral("DELETE"), QStringLiteral("/api/v1/albums/%1").arg(id), QJsonObject());
    }
    loadCache();
    if (m_online && m_authenticated) synchronize();
}

QNetworkReply *AlbumController::send(const QString &method, const QString &path,
                                     const QJsonObject &body, RequestKind kind)
{
    QNetworkRequest request(QUrl(m_baseUrl + path));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "AuroraMusicCollection/1.0");
    if (method == QStringLiteral("GET") && path == QStringLiteral("/api/v1/albums")) {
        const QByteArray etag = m_settings.value(QStringLiteral("etag")).toByteArray();
        if (!etag.isEmpty()) request.setRawHeader("If-None-Match", etag);
    }
    QByteArray payload;
    if (!body.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    }
    QBuffer *device = nullptr;
    if (!payload.isEmpty()) {
        device = new QBuffer;
        device->setData(payload);
        device->open(QIODevice::ReadOnly);
    }
    QNetworkReply *reply = m_network.sendCustomRequest(request, method.toLatin1(), device);
    if (device)
        device->setParent(reply);
    reply->setProperty("kind", static_cast<int>(kind));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { handleFinished(reply); });
    connect(reply, &QNetworkReply::sslErrors, this, [this, reply](const QList<QSslError> &) {
        if (m_allowInsecureTls) reply->ignoreSslErrors();
    });
    QTimer *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    timer->start(10000);
    return reply;
}

void AlbumController::login(const QString &username, const QString &password)
{
    if (!m_online) { setError(QStringLiteral("Вход недоступен в offline-режиме")); return; }
    setBusy(true); setError(QString()); setStatus(QStringLiteral("Выполняется вход…"));
    send(QStringLiteral("POST"), QStringLiteral("/api/v1/auth/login"),
         QJsonObject{{QStringLiteral("username"), username}, {QStringLiteral("password"), password}}, LoginRequest);
}

void AlbumController::logout()
{
    m_cookieJar->clearStoredCookies();
    setAuthenticated(false);
    setStatus(QStringLiteral("Выполнен выход"));
}

void AlbumController::refresh()
{
    if (!m_online) { loadCache(); setStatus(QStringLiteral("Показан offline-кэш")); return; }
    if (!m_authenticated) { setError(QStringLiteral("Сначала выполните вход")); return; }
    if (pendingCount() > 0) { synchronize(); return; }
    setBusy(true); setError(QString()); setStatus(QStringLiteral("Загрузка с сервера…"));
    send(QStringLiteral("GET"), QStringLiteral("/api/v1/albums"), QJsonObject(), RefreshRequest);
}

void AlbumController::synchronize()
{
    if (!m_online || !m_authenticated || m_syncing) return;
    m_syncing = true;
    setBusy(true); setError(QString()); setStatus(QStringLiteral("Синхронизация…"));
    syncNext();
}

void AlbumController::syncNext()
{
    QSqlQuery query(QStringLiteral("SELECT id,method,path,body,local_id FROM pending ORDER BY id LIMIT 1"), m_database);
    if (!query.next()) {
        m_syncing = false;
        setBusy(false);
        emit pendingCountChanged();
        refresh();
        return;
    }
    const QJsonObject body = QJsonDocument::fromJson(query.value(3).toString().toUtf8()).object();
    QNetworkReply *reply = send(query.value(1).toString(), query.value(2).toString(), body, SyncRequest);
    reply->setProperty("queueId", query.value(0));
    reply->setProperty("localId", query.value(4));
    reply->setProperty("method", query.value(1));
}

QString AlbumController::networkMessage(QNetworkReply *reply, const QByteArray &payload)
{
    const QJsonObject errorObject = QJsonDocument::fromJson(payload).object();
    const QString serverMessage = errorObject.value(QStringLiteral("error")).toString();
    if (!serverMessage.isEmpty()) return serverMessage;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return status ? QStringLiteral("HTTP %1").arg(status) : reply->errorString();
}

void AlbumController::handleFinished(QNetworkReply *reply)
{
    const RequestKind kind = static_cast<RequestKind>(reply->property("kind").toInt());
    const QByteArray payload = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool success = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;

    // Сессия на сервере истекла или недействительна: без сброса флага кнопка
    // осталась бы «Обновить», и войти заново было бы нельзя.
    if (status == 401 && kind != LoginRequest)
        setAuthenticated(false);

    if (kind == LoginRequest) {
        setBusy(false);
        if (success) {
            setAuthenticated(true);
            setStatus(QStringLiteral("Вход выполнен"));
            synchronize();
        } else setError(networkMessage(reply, payload));
    } else if (kind == RefreshRequest) {
        setBusy(false);
        if (success) {
            const QJsonDocument document = QJsonDocument::fromJson(payload);
            if (document.isArray()) replaceCache(document.array());
            m_settings.setValue(QStringLiteral("etag"), reply->rawHeader("ETag"));
            setStatus(QStringLiteral("Данные обновлены"));
        } else if (status == 304) {
            loadCache(); setStatus(QStringLiteral("Кэш актуален"));
        } else {
            loadCache(); setStatus(QStringLiteral("Показан локальный кэш")); setError(networkMessage(reply, payload));
        }
    } else if (kind == SyncRequest) {
        if (success) {
            const int queueId = reply->property("queueId").toInt();
            const int localId = reply->property("localId").toInt();
            QSqlQuery query(m_database);
            if (reply->property("method").toString() == QStringLiteral("POST") && localId < 0) {
                const QJsonObject created = QJsonDocument::fromJson(payload).object();
                query.prepare(QStringLiteral("DELETE FROM albums WHERE id=?"));
                query.addBindValue(localId); query.exec();
                query.prepare(QStringLiteral("INSERT OR REPLACE INTO albums(id,performer,album,year,genre,dirty,deleted) VALUES(?,?,?,?,?,0,0)"));
                query.addBindValue(created.value(QStringLiteral("id")).toInt());
                query.addBindValue(created.value(QStringLiteral("performer")).toString());
                query.addBindValue(created.value(QStringLiteral("album")).toString());
                query.addBindValue(created.value(QStringLiteral("year")).toInt());
                query.addBindValue(created.value(QStringLiteral("genre")).toString());
                query.exec();
            }
            query.prepare(QStringLiteral("DELETE FROM pending WHERE id=?"));
            query.addBindValue(queueId); query.exec();
            emit pendingCountChanged();
            syncNext();
        } else {
            m_syncing = false; setBusy(false); loadCache();
            setStatus(QStringLiteral("Изменения остались в очереди")); setError(networkMessage(reply, payload));
        }
    }
    reply->deleteLater();
}

void AlbumController::setStatus(const QString &value) { if (m_status != value) { m_status = value; emit statusChanged(); } }
void AlbumController::setError(const QString &value) { if (m_error != value) { m_error = value; emit errorChanged(); } }
void AlbumController::setBusy(bool value) { if (m_busy != value) { m_busy = value; emit busyChanged(); } }
void AlbumController::setAuthenticated(bool value) { if (m_authenticated != value) { m_authenticated = value; m_settings.setValue(QStringLiteral("authenticated"), value); emit authenticatedChanged(); } }

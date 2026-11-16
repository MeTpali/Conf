#include "temperaturecontroller.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegExp>

namespace {
const char websocketGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const quint64 maximumFrameSize = 65536;
}

TemperatureController::TemperatureController(QObject *parent)
    : QObject(parent)
    , m_status(QStringLiteral("Нет соединения"))
    , m_result(QStringLiteral("Результат появится после преобразования"))
    , m_handshakeComplete(false)
    , m_connected(false)
    , m_connecting(false)
{
    qsrand(static_cast<uint>(QDateTime::currentMSecsSinceEpoch()));
    connect(&m_socket, SIGNAL(connected()), this, SLOT(onSocketConnected()));
    connect(&m_socket, SIGNAL(disconnected()), this, SLOT(onSocketDisconnected()));
    connect(&m_socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(&m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onSocketError(QAbstractSocket::SocketError)));
}

QString TemperatureController::status() const { return m_status; }
QString TemperatureController::result() const { return m_result; }
QString TemperatureController::error() const { return m_error; }
QStringList TemperatureController::history() const { return m_history; }
bool TemperatureController::connected() const { return m_connected; }
bool TemperatureController::connecting() const { return m_connecting; }

void TemperatureController::connectToServer(const QString &address)
{
    QString text = address.trimmed();
    if (!text.startsWith(QStringLiteral("ws://"), Qt::CaseInsensitive))
        text.prepend(QStringLiteral("ws://"));

    const QUrl url(text);
    if (!url.isValid() || url.scheme().toLower() != QStringLiteral("ws")
            || url.host().isEmpty() || url.port() <= 0) {
        setError(QStringLiteral("Адрес должен иметь вид ws://192.168.1.10:8080/ws"));
        return;
    }

    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.abort();

    m_url = url;
    if (m_url.path().isEmpty() || m_url.path() == QStringLiteral("/"))
        m_url.setPath(QStringLiteral("/ws"));
    m_clientKey = makeClientKey();
    m_buffer.clear();
    m_handshakeComplete = false;
    setConnected(false);
    setConnecting(true);
    setStatus(QStringLiteral("Подключение..."));
    setError(QString());
    m_socket.connectToHost(m_url.host(), static_cast<quint16>(m_url.port()));
}

void TemperatureController::disconnectFromServer()
{
    if (m_handshakeComplete)
        sendFrame(QByteArray(), 0x8);
    if (m_socket.state() == QAbstractSocket::ConnectedState) {
        m_socket.disconnectFromHost();
        return;
    }
    // Пока соединение устанавливается, сигнал disconnected() не приходит,
    // поэтому отмену попытки подключения обрабатываем сами.
    m_socket.abort();
    onSocketDisconnected();
}

void TemperatureController::convert(const QString &value, const QString &fromUnit)
{
    if (!m_connected) {
        setError(QStringLiteral("Сначала подключитесь к серверу"));
        return;
    }

    bool ok = false;
    QString normalized = value.trimmed();
    normalized.replace(QLatin1Char(','), QLatin1Char('.'));
    const double number = normalized.toDouble(&ok);
    const QString unit = fromUnit.trimmed().toUpper();

    if (!ok) {
        setError(QStringLiteral("Введите корректное число"));
        return;
    }
    if (unit != QStringLiteral("C") && unit != QStringLiteral("F")) {
        setError(QStringLiteral("Выберите исходную единицу измерения"));
        return;
    }
    if ((unit == QStringLiteral("C") && number < -273.15)
            || (unit == QStringLiteral("F") && number < -459.67)) {
        setError(QStringLiteral("Температура ниже абсолютного нуля невозможна"));
        return;
    }

    QJsonObject request;
    request.insert(QStringLiteral("type"), QStringLiteral("convert"));
    request.insert(QStringLiteral("value"), number);
    request.insert(QStringLiteral("from"), unit);
    setError(QString());
    sendJson(request);
}

void TemperatureController::clearHistory()
{
    if (!m_connected) {
        setError(QStringLiteral("Сначала подключитесь к серверу"));
        return;
    }
    QJsonObject request;
    request.insert(QStringLiteral("type"), QStringLiteral("clearHistory"));
    sendJson(request);
}

void TemperatureController::onSocketConnected()
{
    QByteArray path = m_url.path(QUrl::FullyEncoded).toUtf8();
    if (path.isEmpty())
        path = "/ws";
    if (m_url.hasQuery())
        path += '?' + m_url.query(QUrl::FullyEncoded).toUtf8();

    QByteArray host = m_url.host().toUtf8();
    host += ':' + QByteArray::number(m_url.port());
    QByteArray request;
    request += "GET " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    request += "Upgrade: websocket\r\n";
    request += "Connection: Upgrade\r\n";
    request += "Sec-WebSocket-Key: " + m_clientKey + "\r\n";
    request += "Sec-WebSocket-Version: 13\r\n\r\n";
    m_socket.write(request);
}

void TemperatureController::onSocketDisconnected()
{
    m_handshakeComplete = false;
    setConnected(false);
    setConnecting(false);
    setStatus(QStringLiteral("Нет соединения"));
}

void TemperatureController::onReadyRead()
{
    m_buffer += m_socket.readAll();
    if (!m_handshakeComplete)
        processHandshake();
    if (m_handshakeComplete)
        consumeFrames();
}

void TemperatureController::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    failConnection(m_socket.errorString());
}

void TemperatureController::processHandshake()
{
    const int end = m_buffer.indexOf("\r\n\r\n");
    if (end < 0)
        return;

    const QByteArray headers = m_buffer.left(end + 4);
    m_buffer.remove(0, end + 4);
    const QByteArray expected = QCryptographicHash::hash(
        m_clientKey + websocketGuid, QCryptographicHash::Sha1).toBase64();

    const bool switched = headers.startsWith("HTTP/1.1 101 ")
        || headers.startsWith("HTTP/1.0 101 ");
    const bool upgrade = headers.toLower().contains("\r\nupgrade: websocket\r\n");
    const bool accepted = headers.toLower().contains(
        QByteArray("\r\nsec-websocket-accept: ") + expected.toLower() + "\r\n");

    if (!switched || !upgrade || !accepted) {
        failConnection(QStringLiteral("Сервер отклонил WebSocket-рукопожатие"));
        return;
    }

    m_handshakeComplete = true;
    setConnecting(false);
    setConnected(true);
    setStatus(QStringLiteral("Соединение установлено"));
    setError(QString());

    QJsonObject request;
    request.insert(QStringLiteral("type"), QStringLiteral("history"));
    sendJson(request);
}

void TemperatureController::consumeFrames()
{
    while (m_buffer.size() >= 2) {
        const quint8 first = static_cast<quint8>(m_buffer.at(0));
        const quint8 second = static_cast<quint8>(m_buffer.at(1));
        const bool finalFrame = first & 0x80;
        const quint8 opcode = first & 0x0f;
        const bool masked = second & 0x80;
        quint64 length = second & 0x7f;
        int offset = 2;

        if (!finalFrame || masked) {
            failConnection(QStringLiteral("Получен неподдерживаемый WebSocket-кадр"));
            return;
        }
        if (length == 126) {
            if (m_buffer.size() < 4)
                return;
            length = (static_cast<quint8>(m_buffer.at(2)) << 8)
                | static_cast<quint8>(m_buffer.at(3));
            offset = 4;
        } else if (length == 127) {
            if (m_buffer.size() < 10)
                return;
            length = 0;
            for (int i = 2; i < 10; ++i)
                length = (length << 8) | static_cast<quint8>(m_buffer.at(i));
            offset = 10;
        }
        if (length > maximumFrameSize) {
            failConnection(QStringLiteral("Ответ сервера слишком большой"));
            return;
        }
        if (m_buffer.size() < offset + static_cast<int>(length))
            return;

        const QByteArray payload = m_buffer.mid(offset, static_cast<int>(length));
        m_buffer.remove(0, offset + static_cast<int>(length));

        if (opcode == 0x1)
            processMessage(payload);
        else if (opcode == 0x8) {
            m_socket.disconnectFromHost();
            return;
        } else if (opcode == 0x9) {
            sendFrame(payload, 0xA);
        }
    }
}

void TemperatureController::processMessage(const QByteArray &message)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(message, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(QStringLiteral("Сервер прислал некорректный JSON"));
        return;
    }

    const QJsonObject object = document.object();
    const QString type = object.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("connected")) {
        setStatus(object.value(QStringLiteral("message")).toString(
            QStringLiteral("Соединение установлено")));
        return;
    }
    if (type == QStringLiteral("result")) {
        setResult(formatRecord(object.value(QStringLiteral("record")).toObject()));
        setError(QString());
        return;
    }
    if (type == QStringLiteral("history")) {
        QStringList items;
        const QJsonArray array = object.value(QStringLiteral("items")).toArray();
        for (const QJsonValue &value : array) {
            if (value.isObject())
                items.append(formatRecord(value.toObject()));
        }
        if (items != m_history) {
            m_history = items;
            emit historyChanged();
        }
        return;
    }
    if (type == QStringLiteral("error")) {
        setError(object.value(QStringLiteral("message")).toString(
            QStringLiteral("Ошибка сервера")));
    }
}

void TemperatureController::sendJson(const QJsonObject &object)
{
    sendFrame(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

void TemperatureController::sendFrame(const QByteArray &payload, quint8 opcode)
{
    if (m_socket.state() != QAbstractSocket::ConnectedState)
        return;

    QByteArray frame;
    frame.append(static_cast<char>(0x80 | opcode));
    const quint64 length = static_cast<quint64>(payload.size());
    if (length < 126) {
        frame.append(static_cast<char>(0x80 | length));
    } else if (length <= 0xffff) {
        frame.append(static_cast<char>(0x80 | 126));
        frame.append(static_cast<char>((length >> 8) & 0xff));
        frame.append(static_cast<char>(length & 0xff));
    } else {
        frame.append(static_cast<char>(0x80 | 127));
        for (int shift = 56; shift >= 0; shift -= 8)
            frame.append(static_cast<char>((length >> shift) & 0xff));
    }

    QByteArray mask(4, Qt::Uninitialized);
    for (int i = 0; i < mask.size(); ++i)
        mask[i] = static_cast<char>(qrand() & 0xff);
    frame.append(mask);

    QByteArray maskedPayload = payload;
    for (int i = 0; i < maskedPayload.size(); ++i)
        maskedPayload[i] = maskedPayload.at(i) ^ mask.at(i % 4);
    frame.append(maskedPayload);
    m_socket.write(frame);
}

void TemperatureController::failConnection(const QString &message)
{
    setError(message.isEmpty()
        ? QStringLiteral("Не удалось подключиться к серверу")
        : message);
    setStatus(QStringLiteral("Ошибка соединения"));
    setConnecting(false);
    setConnected(false);
    m_socket.abort();
}

QByteArray TemperatureController::makeClientKey()
{
    QByteArray bytes(16, Qt::Uninitialized);
    for (int i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<char>(qrand() & 0xff);
    return bytes.toBase64();
}

QString TemperatureController::formatNumber(double value)
{
    return QString::number(value, 'f', 2)
        .remove(QRegExp(QStringLiteral("0+$")))
        .remove(QRegExp(QStringLiteral("\\.$")));
}

QString TemperatureController::formatRecord(const QJsonObject &record)
{
    return QStringLiteral("%1 °%2  =  %3 °%4")
        .arg(formatNumber(record.value(QStringLiteral("input")).toDouble()),
             record.value(QStringLiteral("from")).toString(),
             formatNumber(record.value(QStringLiteral("output")).toDouble()),
             record.value(QStringLiteral("to")).toString());
}

void TemperatureController::setStatus(const QString &value)
{
    if (m_status == value)
        return;
    m_status = value;
    emit statusChanged();
}

void TemperatureController::setResult(const QString &value)
{
    if (m_result == value)
        return;
    m_result = value;
    emit resultChanged();
}

void TemperatureController::setError(const QString &value)
{
    if (m_error == value)
        return;
    m_error = value;
    emit errorChanged();
}

void TemperatureController::setConnected(bool value)
{
    if (m_connected == value)
        return;
    m_connected = value;
    emit connectedChanged();
}

void TemperatureController::setConnecting(bool value)
{
    if (m_connecting == value)
        return;
    m_connecting = value;
    emit connectingChanged();
}


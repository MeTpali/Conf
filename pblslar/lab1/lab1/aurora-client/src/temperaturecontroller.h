#ifndef TEMPERATURECONTROLLER_H
#define TEMPERATURECONTROLLER_H

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QTcpSocket>
#include <QUrl>

class TemperatureController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString result READ result NOTIFY resultChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QStringList history READ history NOTIFY historyChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY connectingChanged)

public:
    explicit TemperatureController(QObject *parent = nullptr);

    QString status() const;
    QString result() const;
    QString error() const;
    QStringList history() const;
    bool connected() const;
    bool connecting() const;

    Q_INVOKABLE void connectToServer(const QString &address);
    Q_INVOKABLE void disconnectFromServer();
    Q_INVOKABLE void convert(const QString &value, const QString &fromUnit);
    Q_INVOKABLE void clearHistory();

signals:
    void statusChanged();
    void resultChanged();
    void errorChanged();
    void historyChanged();
    void connectedChanged();
    void connectingChanged();

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    static QString formatNumber(double value);
    static QString formatRecord(const QJsonObject &record);
    static QByteArray makeClientKey();
    void setStatus(const QString &value);
    void setResult(const QString &value);
    void setError(const QString &value);
    void setConnected(bool value);
    void setConnecting(bool value);
    void processHandshake();
    void consumeFrames();
    void processMessage(const QByteArray &message);
    void sendJson(const QJsonObject &object);
    void sendFrame(const QByteArray &payload, quint8 opcode = 0x1);
    void failConnection(const QString &message);

    QTcpSocket m_socket;
    QUrl m_url;
    QByteArray m_clientKey;
    QByteArray m_buffer;
    QString m_status;
    QString m_result;
    QString m_error;
    QStringList m_history;
    bool m_handshakeComplete;
    bool m_connected;
    bool m_connecting;
};

#endif


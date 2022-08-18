#pragma once

#include <QAbstractTableModel>
#include <qurlinfo.h>
#include <qftp.h>

class FtpModelPrivate;

class FtpModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum FtpModelRole {
        FtpBegin = Qt::UserRole,

        FtpIsDir = FtpBegin,
        FtpNameRole,
        FtpSizeRole,

        FtpEnd
    };
    static constexpr int FTP_ROLE_COUNT = FtpEnd - FtpBegin;

    FtpModel(QObject *parent = nullptr);
    FtpModel(bool isTable, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    int setProxy(const QString &host, quint16 port);
    int connectToHost(const QString &host, quint16 port=21);
    int login(const QString &user = QString(), const QString &password = QString());
    int close();
    int setTransferMode(QFtp::TransferMode mode);
    int list(const QString &dir = QString());
    int cd(const QString &dir);
    int get(const QString &file, QIODevice *dev=0, QFtp::TransferType type = QFtp::Binary);
    int put(const QByteArray &data, const QString &file, QFtp::TransferType type = QFtp::Binary);
    int put(QIODevice *dev, const QString &file, QFtp::TransferType type = QFtp::Binary);
    int remove(const QString &file);
    int mkdir(const QString &dir);
    int rmdir(const QString &dir);
    int rename(const QString &oldname, const QString &newname);
    int rawCommand(const QString &command);
    QIODevice* currentDevice() const;
    QFtp::Command currentCommand() const;
    bool hasPendingCommands() const;
    void clearPendingCommands();

    QFtp::State state() const;
    QFtp::Error error() const;
    QString errorString() const;

public slots:
    void abort();

private slots:
    void stateChangedSlot(QFtp::State state);
    void listInfoSlot(const QUrlInfo& info);
    void readyReadSlot();
    void dataTransferProgressSlot(qint64 done, qint64 total);
    void rawCommandReplySlot(int replyCode, const QString& detail);
    void commandStartedSlot(int id);
    void commandFinishedSlot(int id, bool error);
    void doneSlot(bool error);

signals:
    void stateChanged(QFtp::State);
    void listInfo(const QUrlInfo&);
    void readyRead();
    void dataTransferProgress(qint64, qint64);
    void rawCommandReply(int, const QString&);
    void commandStarted(int);
    void commandFinished(int, bool);
    void done(bool);

private:
    bool _isTable = false;
    QFtp::Command _lastCommand;
    QList<QUrlInfo> _rows;
    QMap<int, QFtp::Command> _commandsQueue;
    QFtp *_ftp;
};

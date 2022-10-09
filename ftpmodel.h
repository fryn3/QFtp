#pragma once

#include <QAbstractTableModel>
#include <qurlinfo.h>
#include <qftp.h>

class FtpModelPrivate;

class FtpModel : public QAbstractTableModel
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path NOTIFY pathChanged FINAL)
    Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged FINAL)
public:

    enum class FileState {
        None,
        Downloading,
        Downloaded,
        Failed
    };

    enum FtpModelRole {
        FtpRoleBegin = Qt::UserRole + 1,

        FtpIsDir = FtpRoleBegin,
        FtpNameRole,
        FtpSizeRole,
        FtpFileStateRole,
        FtpProgressDoneRole,
        FtpProgressTotalRole,

        FtpRoleEnd
    };

    static constexpr int FTP_ROLE_COUNT = FtpRoleEnd - FtpRoleBegin;
    static const std::array<QString, FTP_ROLE_COUNT> FTP_ROLE_STR;

    struct RowStruct {
        QUrlInfo info;
        FileState state = FileState::None;
        qint64 done = 0;
        qint64 total = 1;
    };

    FtpModel(QObject *parent = nullptr);
    FtpModel(bool isTable, QObject *parent = nullptr);

    int findName(QString name) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

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
    bool isDone() const;
    QString path() const;

    const QVector<RowStruct> &files() const;

    bool freeze() const;

public slots:
    void abort();

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    void setFreeze(bool newFreeze);

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
    void rowCountChanged();
    void stateChanged(QFtp::State);
    void listInfo(const QUrlInfo&);
    void readyRead();
    void dataTransferProgress(qint64, qint64);
    void rawCommandReply(int, const QString&);
    void commandStarted(int);
    void commandFinished(int, bool);
    void done(bool);

    void errorChanged();
    void pathChanged();

    void freezeChanged();

private:
    struct CommandQueue {
        CommandQueue(QFtp::Command c) : command(c) {}
        CommandQueue(QFtp::Command c, QStringList argParams) : command(c), params(argParams) {}
        CommandQueue() = default;
        // Команда, в очереди.
        QFtp::Command command;
        // Для команды cd сохраняем путь.
        QStringList params;
    };

    bool _isTable = false;
    CommandQueue _lastCommand {};
    QVector<RowStruct> _rows;
    QMap<int, CommandQueue> _commandsQueue;
    QFtp *_ftp;
    QStringList _path;
    int _getRowIndex = -1;
    int _getTimerId = 0;
    // QFTP не отрабатывает abort (не вызываются сигналы окончания).
    // Добавил костыль, если срабатывает сторожевой таймер, freeze
    // выставляется true.
    bool _freeze = false;

    // Время сторожевого таймера на скачивание.
    static constexpr int _GET_TIMEOUT = 30000;

};

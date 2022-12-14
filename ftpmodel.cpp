#include "ftpmodel.h"

#include <array>
#include <QDir>
#include <QDebug>
#include <QTimerEvent>

const std::array<QString, FtpModel::FTP_ROLE_COUNT>
        FtpModel::FTP_ROLE_STR {
    "isDir",
    "name",
    "size",
    "fileState",
    "progressDone",
    "progressTotal"
};

FtpModel::FtpModel(QObject *parent)
    : FtpModel(false, parent) { }

FtpModel::FtpModel(bool isTable, QObject *parent)
    : QAbstractTableModel(parent), _isTable(isTable), _ftp(new QFtp(this))
{
    connect(_ftp, &QFtp::stateChanged, this, &FtpModel::stateChangedSlot);
    connect(_ftp, &QFtp::listInfo, this, &FtpModel::listInfoSlot);
    connect(_ftp, &QFtp::readyRead, this, &FtpModel::readyReadSlot);
    connect(_ftp, &QFtp::dataTransferProgress, this, &FtpModel::dataTransferProgressSlot);
    connect(_ftp, &QFtp::rawCommandReply, this, &FtpModel::rawCommandReplySlot);
    connect(_ftp, &QFtp::commandStarted, this, &FtpModel::commandStartedSlot);
    connect(_ftp, &QFtp::commandFinished, this, &FtpModel::commandFinishedSlot);
    connect(_ftp, &QFtp::done, this, &FtpModel::doneSlot);
    connect(this, &QAbstractItemModel::rowsInserted, this, &FtpModel::rowCountChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &FtpModel::rowCountChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &FtpModel::rowCountChanged);
}

int FtpModel::findName(QString name) const
{
    for (int i = 0; i < _rows.size(); ++i) {
        if (_rows.at(i).info.name() == name) {
            return i;
        }
    }
    return -1;
}

int FtpModel::rowCount(const QModelIndex &) const
{
    return _rows.size();
}

int FtpModel::columnCount(const QModelIndex &) const
{
    return _isTable ? FTP_ROLE_COUNT : 1;
}

QVariant FtpModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()
            || index.row() >= rowCount()
            || index.column() >= columnCount()
            || (role != Qt::DisplayRole && role <= Qt::UserRole)) {
        return QVariant();
    }
    if (_isTable && role < FtpRoleBegin) {
        qDebug() << role;
        Q_ASSERT(role == Qt::DisplayRole);
        return data(this->index(index.row(), 0), index.column() + FtpRoleBegin);
    }
    Q_ASSERT(index.column() == 0);
    switch (role) {
    case Qt::DisplayRole:
    case FtpNameRole:
        return _rows.at(index.row()).info.name();
    case FtpIsDir:
        return _rows.at(index.row()).info.isDir();
    case FtpSizeRole:
        return _rows.at(index.row()).info.size();
    case FtpProgressDoneRole:
        return _rows.at(index.row()).done;
    case FtpProgressTotalRole:
        return _rows.at(index.row()).total;
    default:
        break;
    }
    return {};
}

QHash<int, QByteArray> FtpModel::roleNames() const {
    QHash<int, QByteArray> r;
    for (int i = FtpRoleBegin; i < FtpRoleEnd; ++i) {
        r.insert(i, FTP_ROLE_STR.at(i - FtpRoleBegin).toLatin1());
    }
    return r;
}

int FtpModel::setProxy(const QString &host, quint16 port)
{
    auto c = _ftp->setProxy(host, port);
    _commandsQueue[c] = {QFtp::Command::SetProxy, {host}};
    return c;
}

int FtpModel::connectToHost(const QString &host, quint16 port)
{
    auto c = _ftp->connectToHost(host, port);
    _commandsQueue[c] = {QFtp::Command::ConnectToHost, {host}};
    return c;
}

int FtpModel::login(const QString &user, const QString &password)
{
    auto c = _ftp->login(user, password);
    _commandsQueue[c] = {QFtp::Command::Login, {user, password}};
    return c;
}

int FtpModel::close()
{
    auto c = _ftp->close();
    _commandsQueue[c] = QFtp::Command::Close;
    return c;

}

int FtpModel::setTransferMode(QFtp::TransferMode mode)
{
    auto c = _ftp->setTransferMode(mode);
    _commandsQueue[c] = QFtp::Command::SetTransferMode;
    return c;
}

int FtpModel::list(const QString &dir)
{
    auto c = _ftp->list(dir);
    _commandsQueue[c] = {QFtp::Command::List, {dir}};
    return c;

}

int FtpModel::cd(const QString &dir)
{
    auto c = _ftp->cd(dir);
    _commandsQueue[c] = {QFtp::Command::Cd, {dir}};
    return c;
}

int FtpModel::get(const QString &file, QIODevice *dev, QFtp::TransferType type)
{
    auto c = _ftp->get(file, dev, type);
    _commandsQueue[c] = {QFtp::Command::Get, {file}};
    return c;
}

int FtpModel::put(const QByteArray &data, const QString &file, QFtp::TransferType type)
{
    auto c = _ftp->put(data, file, type);
    _commandsQueue[c] = {QFtp::Command::Put, {file}};
    return c;
}

int FtpModel::put(QIODevice *dev, const QString &file, QFtp::TransferType type)
{
    auto c = _ftp->put(dev, file, type);
    _commandsQueue[c] = {QFtp::Command::Put, {file}};
    return c;
}

int FtpModel::remove(const QString &file)
{
    auto c = _ftp->remove(file);
    _commandsQueue[c] = {QFtp::Command::Remove, {file}};
    return c;
}

int FtpModel::mkdir(const QString &dir)
{
    auto c = _ftp->mkdir(dir);
    _commandsQueue[c] = {QFtp::Command::Mkdir, {dir}};
    return c;
}

int FtpModel::rmdir(const QString &dir)
{
    auto c = _ftp->rmdir(dir);
    _commandsQueue[c] = {QFtp::Command::Rmdir, {dir}};
    return c;
}

int FtpModel::rename(const QString &oldname, const QString &newname)
{
    auto c = _ftp->rename(oldname, newname);
    _commandsQueue[c] = {QFtp::Command::Rename, {oldname, newname}};
    return c;
}

int FtpModel::rawCommand(const QString &command)
{
    auto c = _ftp->rawCommand(command);
    _commandsQueue[c] = {QFtp::Command::RawCommand, {command}};
    return c;
}

QIODevice *FtpModel::currentDevice() const
{
    return _ftp->currentDevice();
}

QFtp::Command FtpModel::currentCommand() const
{
    return _ftp->currentCommand();
}

bool FtpModel::hasPendingCommands() const
{
    return _ftp->hasPendingCommands();
}

void FtpModel::clearPendingCommands()
{
    return _ftp->clearPendingCommands();
}

QFtp::State FtpModel::state() const
{
    return _ftp->state();
}

QFtp::Error FtpModel::error() const
{
    return _ftp->error();
}

QString FtpModel::errorString() const
{
    return _ftp->errorString();
}

bool FtpModel::isDone() const
{
    return _lastCommand.command == QFtp::None
            && _commandsQueue.isEmpty();
}

QString FtpModel::path() const
{
    static const auto SEP = QDir::separator();
    return QDir::cleanPath(_path.join(SEP));
}

void FtpModel::abort()
{
    _commandsQueue.clear();
    return _ftp->abort();
}

void FtpModel::timerEvent(QTimerEvent *event)
{
    qDebug() << __FILE__ << __LINE__;
    Q_ASSERT(event->timerId() == _getTimerId);
    // ?????????????????? ????????????????. ?????????????? ????????????????!
    killTimer(_getTimerId);
    _getTimerId = -1;
    abort();
    setFreeze(true);
//    commandFinishedSlot(-1, true);
//    doneSlot(true);
}

void FtpModel::stateChangedSlot(QFtp::State state)
{
    switch(state) {
    case QFtp::Unconnected:
    case QFtp::HostLookup:
    case QFtp::Connecting:
    case QFtp::Connected:
    case QFtp::LoggedIn:
    case QFtp::Closing:
        emit stateChanged(state);
    }
}

void FtpModel::listInfoSlot(const QUrlInfo &info)
{
    beginInsertRows(QModelIndex(), _rows.size(), _rows.size());
    _rows.push_back({info});
    endInsertRows();
    emit listInfo(info);
}

void FtpModel::readyReadSlot()
{
    emit readyRead();
}

void FtpModel::dataTransferProgressSlot(qint64 done, qint64 total)
{
    if (_getTimerId == -1) {
        qDebug() << __LINE__ << "wow";
        return;
    }
    killTimer(_getTimerId);
    _getTimerId = startTimer(_GET_TIMEOUT);
    _rows[_getRowIndex].done = done;
    _rows[_getRowIndex].total = total;
    emit dataChanged(index(_getRowIndex, 0), index(_getRowIndex, 0),
                     {FtpProgressDoneRole, FtpProgressTotalRole});

    emit dataTransferProgress(done, total);
}

void FtpModel::rawCommandReplySlot(int replyCode, const QString &detail)
{
    emit rawCommandReply(replyCode, detail);
}

void FtpModel::commandStartedSlot(int id)
{
    qDebug() << __FILE__ << __LINE__ << this << _commandsQueue.firstKey() << _commandsQueue[id].command << id;
    Q_ASSERT(_commandsQueue.firstKey() == id);

    _lastCommand = _commandsQueue[id];
    switch(_lastCommand.command) {
    case QFtp::List: {
        beginResetModel();
        _rows.clear();
        endResetModel();
        break;
    }
    case QFtp::Get: {
        _getRowIndex = findName(_lastCommand.params.at(0));
        Q_ASSERT(_getRowIndex >= 0);
        _rows[_getRowIndex].state = FileState::Downloading;
        if (_getTimerId == -1) {
            qDebug() << __LINE__ << "wow";
            return;
        }
        _getTimerId = startTimer(_GET_TIMEOUT);
        emit dataChanged(index(_getRowIndex, 0), index(_getRowIndex, 0), {FtpFileStateRole});
        break;
    }
    case QFtp::None:
    case QFtp::SetTransferMode:
    case QFtp::SetProxy:
    case QFtp::ConnectToHost:
    case QFtp::Login:
    case QFtp::Close:
    case QFtp::Cd:
    case QFtp::Put:
    case QFtp::Remove:
    case QFtp::Mkdir:
    case QFtp::Rmdir:
    case QFtp::Rename:
    case QFtp::RawCommand:
        break;
    }

    _commandsQueue.remove(id);
    emit commandStarted(id);
}

void FtpModel::commandFinishedSlot(int id, bool error)
{
    qDebug() << __FILE__ << __LINE__ << this << _lastCommand.command << id << error;
    if (error) {
        qDebug() << "lastCommand = " << _lastCommand.command << ": " << _ftp->errorString();
        _commandsQueue.clear();
        emit errorChanged();
    }
    switch(_lastCommand.command) {
    case QFtp::ConnectToHost: {
        _path.clear();
        _path.append("");
        emit pathChanged();
        break;
    }
    case QFtp::Close: {
        _path.clear();
        emit pathChanged();
        beginResetModel();
        _rows.clear();
        endResetModel();
        break;
    }
    case QFtp::Cd: {
        _path.append(_lastCommand.params.at(0));
        emit pathChanged();
        break;
    }
    case QFtp::Get: {
        killTimer(_getTimerId);
        if (_getTimerId == -1) {
            qDebug() << __LINE__ << "wow";
        }
        _getTimerId = 0;
        _getRowIndex = findName(_lastCommand.params.at(0));
        _rows[_getRowIndex].state = error ? FileState::Failed : FileState::Downloaded;
        emit dataChanged(index(_getRowIndex, 0), index(_getRowIndex, 0), {FtpFileStateRole});
        break;
    }
    case QFtp::List:
    case QFtp::None:
    case QFtp::SetTransferMode:
    case QFtp::SetProxy:
    case QFtp::Login:
    case QFtp::Put:
    case QFtp::Remove:
    case QFtp::Mkdir:
    case QFtp::Rmdir:
    case QFtp::Rename:
    case QFtp::RawCommand:
        break;

    }
    _lastCommand = QFtp::None;
    emit commandFinished(id, error);
}

void FtpModel::doneSlot(bool error)
{
    qDebug() << __FILE__ << __LINE__ << this << "error = " << error;
    emit done(error);
}

bool FtpModel::freeze() const
{
    return _freeze;
}

void FtpModel::setFreeze(bool newFreeze)
{
    if (_freeze == newFreeze)
        return;
    _freeze = newFreeze;
    emit freezeChanged();
}

const QVector<FtpModel::RowStruct> &FtpModel::files() const
{
    return _rows;
}

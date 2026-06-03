#include "transfer.h"
#include <QFileInfo>
#include <QDataStream>
#include <QDateTime>
#include <QHostInfo>

// 辅助：将 std::vector<std::string> 转换为 QStringList
static QStringList toQStringList(const std::vector<std::string>& vec) {
    QStringList list;
    for (const auto& s : vec) {
        list.append(QString::fromStdString(s));
    }
    return list;
}

TransferEngine::TransferEngine(Role role, QObject* parent)
    : QObject(parent)
    , m_role(role)
    , m_state(State::Idle)
    , m_server(nullptr)
    , m_socket(nullptr)
    , m_has_current_item(false)
    , m_block_size(DEFAULT_BLOCK_SIZE)
    , m_current_block(0)
    , m_total_blocks(0)
    , m_total_bytes(0)
    , m_transferred_bytes(0)
    , m_last_bytes(0)
    , m_current_speed(0.0)
    , m_expected_seq(0)
    , m_heartbeat_timer(nullptr)
    , m_heartbeat_check_timer(nullptr)
    , m_heartbeat_pending(false)
{
    // 创建临时目录名
    m_temp_dir = QDir::tempPath() + "/ftransfer_" + QString::number(
                    reinterpret_cast<quintptr>(this), 16);
}

TransferEngine::~TransferEngine() {
    cancelTransfer();
    disconnectServer();
    stopServer();
    FileUtil::cleanupTempDir(m_temp_dir.toStdString());
}

// ============================================================
// 服务端接口
// ============================================================

bool TransferEngine::startServer(uint16_t port) {
    if (m_server) {
        stopServer();
    }

    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection,
            this, &TransferEngine::onNewConnection);

    if (!m_server->listen(QHostAddress::Any, port)) {
        emit logMessage(tr("无法启动服务端: %1").arg(m_server->errorString()));
        return false;
    }

    emit logMessage(tr("服务端启动，监听端口: %1").arg(port));
    setState(State::Idle);
    return true;
}

void TransferEngine::stopServer() {
    if (m_socket) {
        m_socket->disconnect();
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    setState(State::Idle);
}

// ============================================================
// 客户端接口
// ============================================================

bool TransferEngine::connectToServer(const QString& host, uint16_t port) {
    if (m_socket) {
        disconnectServer();
    }

    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::readyRead,
            this, &TransferEngine::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &TransferEngine::onDisconnected);
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this, &TransferEngine::onError);

    m_socket->connectToHost(host, port);
    if (!m_socket->waitForConnected(5000)) {
        emit logMessage(tr("连接到 %1:%2 失败: %3")
                        .arg(host).arg(port).arg(m_socket->errorString()));
        return false;
    }

    emit logMessage(tr("已连接到服务端 %1:%2").arg(host).arg(port));
    emit connected();

    // 发送握手请求
    HandshakeBody handshake;
    strncpy(handshake.hostname, QHostInfo::localHostName().toStdString().c_str(), sizeof(handshake.hostname) - 1);
    sendMessage(MessageType::HANDSHAKE_REQ, &handshake, sizeof(handshake));
    setState(State::Handshaking);

    return true;
}

void TransferEngine::disconnectServer() {
    stopHeartbeat();
    if (m_socket) {
        m_socket->disconnect();
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    setState(State::Idle);
    emit disconnected();
}

bool TransferEngine::isConnected() const {
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

QString TransferEngine::peerAddress() const {
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        return m_socket->peerAddress().toString();
    }
    return QString();
}

// ============================================================
// 传输接口
// ============================================================

void TransferEngine::sendFiles(const QStringList& file_list) {
    if (!isConnected()) {
        emit logMessage(tr("错误：未连接到服务端"));
        return;
    }

    // 如果正在传输，将文件追加到队列末尾
    if (m_state == State::Transferring || m_state == State::Paused) {
        for (const QString& path : file_list) {
            QFileInfo fi(path);
            TransferItem item;
            item.local_path = fi.absoluteFilePath();
            item.remote_path = fi.fileName();
            item.file_size = fi.size();
            item.is_directory = false;
            m_transfer_queue.push(item);
            m_total_bytes += fi.size();

            emit logMessage(tr("添加文件到传输队列: %1 (%2)")
                            .arg(fi.fileName()).arg(fi.size()));
        }
        return;
    }

    // 清空队列并添加文件
    while (!m_transfer_queue.empty()) m_transfer_queue.pop();
    m_has_current_item = false;
    m_total_bytes = 0;

    for (const QString& path : file_list) {
        QFileInfo fi(path);
        TransferItem item;
        item.local_path = fi.absoluteFilePath();
        item.remote_path = fi.fileName();
        item.file_size = fi.size();
        item.is_directory = false;
        m_transfer_queue.push(item);
        m_total_bytes += fi.size();

        emit logMessage(tr("添加文件到传输队列: %1 (%2)")
                        .arg(fi.fileName()).arg(fi.size()));
    }

    m_transferred_bytes = 0;
    startNextTransfer();
}

void TransferEngine::sendDirectory(const QString& dir_path) {
    if (!isConnected()) {
        emit logMessage(tr("错误：未连接到服务端"));
        return;
    }

    QDir dir(dir_path);
    if (!dir.exists()) {
        emit logMessage(tr("目录不存在: %1").arg(dir_path));
        return;
    }

    QDir abs_dir(dir.absolutePath());
    QString base_name = abs_dir.dirName();

    // 获取目录下所有文件
    std::vector<std::string> files_vec;
    FileUtil::listDirectory(abs_dir.absolutePath().toStdString(), files_vec);
    QStringList files = toQStringList(files_vec);

    // 计算总大小
    uint64_t total = 0;
    for (const QString& f : files) {
        total += QFileInfo(abs_dir.absoluteFilePath(f)).size();
    }

    // 如果正在传输，将目录项追加到队列末尾
    if (m_state == State::Transferring || m_state == State::Paused) {
        TransferItem item;
        item.local_path = abs_dir.absolutePath();
        item.remote_path = base_name;
        item.is_directory = true;
        item.file_size = total;
        item.sub_files = files;
        m_transfer_queue.push(item);
        m_total_bytes += total;
        emit logMessage(tr("添加目录到传输队列: %1 (%2 个文件, 共 %3 字节)")
                        .arg(base_name).arg(files.size()).arg(total));
        return;
    }

    // 清空队列
    while (!m_transfer_queue.empty()) m_transfer_queue.pop();
    m_has_current_item = false;
    m_total_bytes = 0;

    TransferItem item;
    item.local_path = abs_dir.absolutePath();
    item.remote_path = base_name;
    item.is_directory = true;
    item.file_size = total;
    item.sub_files = files;
    m_total_bytes = total;

    m_transfer_queue.push(item);
    emit logMessage(tr("添加目录到传输队列: %1 (%2 个文件, 共 %3 字节)")
                    .arg(base_name).arg(files.size()).arg(total));

    m_transferred_bytes = 0;
    startNextTransfer();
}

void TransferEngine::cancelTransfer() {
    if (m_state == State::Transferring || m_state == State::Paused) {
        sendMessage(MessageType::CANCEL_TRANSFER, nullptr, 0);
        // 清空队列
        while (!m_transfer_queue.empty()) m_transfer_queue.pop();
        m_has_current_item = false;
        FileUtil::cleanupTempDir(m_temp_dir.toStdString());
        setState(State::Idle);
        emit logMessage(tr("传输已取消"));
    }
}

void TransferEngine::pauseTransfer() {
    if (m_state == State::Transferring) {
        sendMessage(MessageType::PAUSE_TRANSFER, nullptr, 0);
        setState(State::Paused);
        emit logMessage(tr("传输已暂停"));
    }
}

void TransferEngine::resumeTransfer() {
    if (m_state == State::Paused) {
        sendMessage(MessageType::RESUME_TRANSFER, nullptr, 0);
        setState(State::Transferring);
        emit logMessage(tr("传输已恢复"));
        // 继续发送当前文件的下一个块
        sendNextBlock();
    }
}

void TransferEngine::requestUpload(const QStringList& file_list) {
    // 客户端请求上传：实际上是客户端把自己作为发送端
    // 等同于 sendFiles
    sendFiles(file_list);
}

// ============================================================
// 内部方法 - 消息发送
// ============================================================

bool TransferEngine::sendMessage(MessageType type, const void* body, uint32_t body_length) {
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        return false;
    }

    MessageHeader header;
    header.type = static_cast<uint32_t>(type);
    header.body_length = body_length;
    header.sequence = m_expected_seq++;
    header.calcChecksum();

    // 发送头部
    qint64 sent = m_socket->write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (sent != sizeof(header)) return false;

    // 发送消息体
    if (body && body_length > 0) {
        sent = m_socket->write(static_cast<const char*>(body), body_length);
        if (sent != body_length) return false;
    }

    m_socket->flush();
    return true;
}

// ============================================================
// 内部方法 - 文件发送
// ============================================================

void TransferEngine::sendFile(const QString& filepath, const QString& remote_name) {
    QFileInfo fi(filepath);

    FileInfoBody info;
    strncpy(info.file_name, remote_name.toStdString().c_str(), MAX_FILENAME_LEN - 1);
    info.file_size = fi.size();
    info.block_size = m_block_size;
    info.file_modify_time = fi.lastModified().toSecsSinceEpoch();
    info.file_checksum = FileUtil::fileCrc32(filepath.toStdString());

    // 计算分块数
    m_total_blocks = (fi.size() + m_block_size - 1) / m_block_size;
    info.block_count = m_total_blocks;
    m_current_block = 0;

    emit logMessage(tr("开始发送文件: %1 (大小: %2, 分块: %3)")
                    .arg(remote_name).arg(fi.size()).arg(m_total_blocks));

    m_last_bytes = 0;
    m_speed_timer.start();

    sendMessage(MessageType::FILE_INFO, &info, sizeof(info));
    setState(State::Transferring);
}

void TransferEngine::sendNextBlock() {
    // 暂停状态检查：如果已暂停，不发送新的数据块
    if (m_state == State::Paused) {
        return;
    }

    if (m_current_block >= m_total_blocks) {
        // 所有块发送完毕
        sendMessage(MessageType::FILE_END, nullptr, 0);
        emit logMessage(tr("文件发送完成: %1").arg(m_current_item.remote_path));
        emit fileTransferFinished(m_current_item.remote_path, true);
        m_has_current_item = false;
        startNextTransfer();
        return;
    }

    QString filepath = m_current_item.local_path;
    uint64_t offset = static_cast<uint64_t>(m_current_block) * m_block_size;
    uint32_t remaining = static_cast<uint32_t>(
        qMin(static_cast<uint64_t>(m_block_size),
             QFileInfo(filepath).size() - offset));

    // 读取块数据
    std::vector<char> data;
    uint32_t read_bytes = FileUtil::readBlock(filepath.toStdString(), offset, remaining, data);
    if (read_bytes == 0) {
        notifyError(tr("读取文件失败: %1").arg(filepath));
        return;
    }

    // 准备块消息体
    uint32_t msg_size = sizeof(FileBlockBody) + read_bytes;
    QByteArray buffer;
    buffer.resize(static_cast<int>(msg_size));

    FileBlockBody* block_body = reinterpret_cast<FileBlockBody*>(buffer.data());
    block_body->block_index = m_current_block;
    block_body->data_size = read_bytes;
    block_body->file_offset = offset;
    block_body->block_checksum = FileUtil::crc32(data.data(), data.size());

    // 复制数据
    memcpy(block_body->data, data.data(), read_bytes);

    sendMessage(MessageType::FILE_BLOCK, buffer.data(), msg_size);

    // 更新进度
    m_transferred_bytes += read_bytes;
    double speed = 0.0;
    if (m_speed_timer.elapsed() > 0) {
        double elapsed_sec = m_speed_timer.elapsed() / 1000.0;
        speed = (m_transferred_bytes - m_last_bytes) / elapsed_sec / 1024.0;
    }

    int percent = m_total_bytes > 0
        ? static_cast<int>(m_transferred_bytes * 100 / m_total_bytes)
        : 0;

    emit progressUpdated(m_current_item.remote_path, percent, speed);

    // 更新速度统计
    m_last_bytes = m_transferred_bytes;
    m_speed_timer.restart();

    m_current_block++;
}

void TransferEngine::sendDirectoryInternal(const QString& dir_path, const QString& base_dir) {
    QDir dir(dir_path);
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                                              QDir::Name);

    for (const QFileInfo& entry : entries) {
        QString rel_path = base_dir.isEmpty()
            ? entry.fileName()
            : base_dir + "/" + entry.fileName();

        if (entry.isDir()) {
            // 发送目录条目
            DirEntryBody dir_entry;
            dir_entry.is_directory = 1;
            strncpy(dir_entry.entry_name, rel_path.toStdString().c_str(), MAX_FILENAME_LEN - 1);
            sendMessage(MessageType::DIR_ENTRY, &dir_entry, sizeof(dir_entry));

            // 递归子目录
            sendDirectoryInternal(entry.absoluteFilePath(), rel_path);
        } else {
            // 发送文件条目
            DirEntryBody file_entry;
            file_entry.is_directory = 0;
            file_entry.entry_size = entry.size();
            strncpy(file_entry.entry_name, rel_path.toStdString().c_str(), MAX_FILENAME_LEN - 1);
            sendMessage(MessageType::DIR_ENTRY, &file_entry, sizeof(file_entry));

            // 添加文件到传输队列
            TransferItem item;
            item.local_path = entry.absoluteFilePath();
            item.remote_path = rel_path;
            item.file_size = entry.size();
            item.is_directory = false;
            m_transfer_queue.push(item);
        }
    }
}

void TransferEngine::startNextTransfer() {
    if (m_transfer_queue.empty()) {
        if (m_has_current_item) {
            emit fileTransferFinished(m_current_item.remote_path, true);
        }
        m_has_current_item = false;
        emit logMessage(tr("所有传输任务已完成"));
        emit allTransfersFinished();
        setState(State::Finished);
        m_current_speed = 0.0;
        return;
    }

    // 取出下一个传输项
    m_current_item = m_transfer_queue.front();
    m_transfer_queue.pop();
    m_has_current_item = true;

    if (m_current_item.is_directory) {
        // 发送目录开始标志
        DirEntryBody dir_start;
        dir_start.is_directory = 1;
        strncpy(dir_start.entry_name, m_current_item.remote_path.toStdString().c_str(),
                MAX_FILENAME_LEN - 1);
        sendMessage(MessageType::DIR_START, &dir_start, sizeof(dir_start));
        emit logMessage(tr("开始传输目录: %1").arg(m_current_item.remote_path));

        // 递归发送目录内容
        sendDirectoryInternal(m_current_item.local_path, m_current_item.remote_path);

        // 发送目录结束标志
        sendMessage(MessageType::DIR_END, nullptr, 0);

        // 开始传输目录内的文件
        startNextTransfer();
    } else {
        // 发送单个文件
        sendFile(m_current_item.local_path, m_current_item.remote_path);
    }
}

// ============================================================
// 服务端槽 - 新连接
// ============================================================

void TransferEngine::onNewConnection() {
    if (m_socket) {
        // 已有连接，拒绝新的
        QTcpSocket* new_socket = m_server->nextPendingConnection();
        new_socket->disconnectFromHost();
        new_socket->deleteLater();
        emit logMessage(tr("拒绝新连接：已有客户端连接"));
        return;
    }

    m_socket = m_server->nextPendingConnection();
    connect(m_socket, &QTcpSocket::readyRead,
            this, &TransferEngine::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &TransferEngine::onDisconnected);
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this, &TransferEngine::onError);

    emit logMessage(tr("新客户端连接: %1").arg(m_socket->peerAddress().toString()));
    emit connected();

    setState(State::Handshaking);
}

// ============================================================
// 数据接收
// ============================================================

void TransferEngine::onReadyRead() {
    if (!m_socket) return;

    // 将数据追加到接收缓冲区
    m_recv_buffer.append(m_socket->readAll());

    // 尝试解析消息
    while (static_cast<size_t>(m_recv_buffer.size()) >= sizeof(MessageHeader)) {
        MessageHeader header;
        memcpy(&header, m_recv_buffer.constData(), sizeof(header));

        // 验证魔数
        if (header.magic != PROTOCOL_MAGIC) {
            // 魔数不匹配，丢弃第一个字节
            m_recv_buffer.remove(0, 1);
            continue;
        }

        // 验证头部校验
        if (!header.verifyChecksum()) {
            m_recv_buffer.remove(0, 1);
            continue;
        }

        // 检查数据是否足够
        size_t total_msg_size = sizeof(header) + header.body_length;
        if (static_cast<size_t>(m_recv_buffer.size()) < total_msg_size) {
            break;  // 数据还不完整，等待更多数据
        }

        // 提取消息体
        QByteArray body;
        if (header.body_length > 0) {
            body = m_recv_buffer.mid(sizeof(header), header.body_length);
        }

        // 移除已处理的数据
        m_recv_buffer.remove(0, static_cast<int>(total_msg_size));

        // 处理消息
        processMessage(header, body);
    }
}

void TransferEngine::onDisconnected() {
    stopHeartbeat();
    emit logMessage(tr("连接已断开"));
    emit disconnected();
    setState(State::Idle);
}

void TransferEngine::onError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    if (m_socket) {
        emit logMessage(tr("网络错误: %1").arg(m_socket->errorString()));
    }
}

// ============================================================
// 消息处理分发
// ============================================================

void TransferEngine::processMessage(const MessageHeader& header, const QByteArray& body) {
    MessageType type = static_cast<MessageType>(header.type);

    emit logMessage(tr("收到消息: %1 (seq=%2, len=%3)")
                    .arg(QString::fromStdString(messageTypeName(type)))
                    .arg(header.sequence)
                    .arg(header.body_length));

    switch (type) {
        case MessageType::HANDSHAKE_REQ:
            handleHandshakeReq(header, body);
            break;
        case MessageType::HANDSHAKE_ACK:
            handleHandshakeAck(header, body);
            break;
        case MessageType::HANDSHAKE_NAK:
            notifyError(tr("握手被拒绝：版本不兼容"));
            break;
        case MessageType::FILE_INFO:
            handleFileInfo(header, body);
            break;
        case MessageType::FILE_BLOCK:
            handleFileBlock(header, body);
            break;
        case MessageType::FILE_END:
            handleFileEnd(header, body);
            break;
        case MessageType::FILE_ACK:
            handleFileAck(header, body);
            break;
        case MessageType::FILE_NAK:
            handleFileNak(header, body);
            break;
        case MessageType::DIR_START:
            handleDirStart(header, body);
            break;
        case MessageType::DIR_ENTRY:
            handleDirEntry(header, body);
            break;
        case MessageType::DIR_END:
            handleDirEnd(header, body);
            break;
        case MessageType::CANCEL_TRANSFER:
            handleCancel(header, body);
            break;
        case MessageType::PAUSE_TRANSFER:
            handlePauseResume(type);
            break;
        case MessageType::RESUME_TRANSFER:
            handlePauseResume(type);
            break;
        case MessageType::ERROR_MSG:
            handleError(header, body);
            break;
        case MessageType::HEARTBEAT:
            handleHeartbeat();
            break;
        case MessageType::HEARTBEAT_ACK:
            handleHeartbeatAck();
            break;
        default:
            emit logMessage(tr("未知消息类型: %1").arg(header.type));
            break;
    }
}

// ============================================================
// 握手处理
// ============================================================

void TransferEngine::handleHandshakeReq(const MessageHeader& header, const QByteArray& body) {
    Q_UNUSED(header);
    if (body.size() < static_cast<int>(sizeof(HandshakeBody))) {
        notifyError(tr("握手消息不完整"));
        return;
    }

    const HandshakeBody* handshake = reinterpret_cast<const HandshakeBody*>(body.constData());
    emit logMessage(tr("收到握手请求: 版本=%1, 主机=%2")
                    .arg(handshake->version, 8, 16)
                    .arg(handshake->hostname));

    // 检查版本兼容性
    if (handshake->version != PROTOCOL_VERSION) {
        sendMessage(MessageType::HANDSHAKE_NAK, nullptr, 0);
        notifyError(tr("版本不兼容: 期望 %1, 收到 %2")
                    .arg(PROTOCOL_VERSION, 8, 16).arg(handshake->version, 8, 16));
        return;
    }

    // 发送握手应答
    HandshakeBody ack;
    strncpy(ack.hostname, QHostInfo::localHostName().toStdString().c_str(), sizeof(ack.hostname) - 1);
    sendMessage(MessageType::HANDSHAKE_ACK, &ack, sizeof(ack));

    emit logMessage(tr("握手成功，客户端: %1").arg(handshake->hostname));
    setState(State::Idle);
    startHeartbeat();
}

void TransferEngine::handleHandshakeAck(const MessageHeader& header, const QByteArray& body) {
    Q_UNUSED(header);
    if (body.size() < static_cast<int>(sizeof(HandshakeBody))) {
        notifyError(tr("握手应答不完整"));
        return;
    }

    const HandshakeBody* handshake = reinterpret_cast<const HandshakeBody*>(body.constData());
    emit logMessage(tr("握手成功，服务端: %1").arg(handshake->hostname));
    setState(State::Idle);
    startHeartbeat();
}

// ============================================================
// 文件接收处理（接收端）
// ============================================================

void TransferEngine::handleFileInfo(const MessageHeader& header, const QByteArray& body) {
    Q_UNUSED(header);
    if (body.size() < static_cast<int>(sizeof(FileInfoBody))) {
        notifyError(tr("文件信息不完整"));
        return;
    }

    memcpy(&m_current_file_info, body.constData(), sizeof(FileInfoBody));

    emit logMessage(tr("准备接收文件: %1 (大小: %2, 分块: %3)")
                    .arg(m_current_file_info.file_name)
                    .arg(m_current_file_info.file_size)
                    .arg(m_current_file_info.block_count));

    // 准备接收分块
    m_received_blocks.clear();
    m_current_block = 0;
    // 发送确认
    FileAckBody ack;
    ack.block_index = m_current_block;
    sendMessage(MessageType::FILE_ACK, &ack, sizeof(ack));

    setState(State::Transferring);
}

void TransferEngine::handleFileBlock(const MessageHeader& header, const QByteArray& body) {
    Q_UNUSED(header);
    if (body.size() < static_cast<int>(sizeof(FileBlockBody))) {
        notifyError(tr("数据块消息不完整"));
        return;
    }

    const FileBlockBody* block = reinterpret_cast<const FileBlockBody*>(body.constData());

    // 提取数据
    uint32_t data_size = block->data_size;
    const char* data_ptr = block->data;

    // 验证 CRC32
    uint32_t calc_crc = FileUtil::crc32(data_ptr, data_size);
    if (calc_crc != block->block_checksum) {
        // 校验失败，请求重传
        emit logMessage(tr("块 %1 校验失败，请求重传").arg(block->block_index));
        sendMessage(MessageType::FILE_NAK, nullptr, 0);
        return;
    }

    // 保存块信息
    FileUtil::BlockInfo block_info;
    block_info.index = m_current_block;
    block_info.size = data_size;
    block_info.offset = block->file_offset;
    block_info.checksum = block->block_checksum;
    m_received_blocks.push_back(block_info);

    // 将块数据写入临时文件
    std::vector<char> data_vec(data_ptr, data_ptr + data_size);
    FileUtil::writeBlockFile(block_info, data_vec, m_temp_dir.toStdString());

    // 发送确认
    FileAckBody ack;
    ack.block_index = block->block_index;
    sendMessage(MessageType::FILE_ACK, &ack, sizeof(ack));

    // 更新进度
    m_transferred_bytes += data_size;
    int percent = (m_current_file_info.file_size > 0)
        ? static_cast<int>(m_transferred_bytes * 100 / m_current_file_info.file_size)
        : 0;

    double speed = 0.0;
    if (m_speed_timer.elapsed() > 0) {
        double elapsed_sec = m_speed_timer.elapsed() / 1000.0;
        speed = (m_transferred_bytes - m_last_bytes) / elapsed_sec / 1024.0;
    }
    m_last_bytes = m_transferred_bytes;
    m_speed_timer.restart();

    emit progressUpdated(QString::fromUtf8(m_current_file_info.file_name), percent, speed);

    m_current_block++;
}

void TransferEngine::handleFileEnd(const MessageHeader& header, const QByteArray& body) {
    Q_UNUSED(header);
    Q_UNUSED(body);

    // 构建输出路径
    QString output_path;
    if (m_recv_base_dir.isEmpty()) {
        output_path = QDir::currentPath() + "/" +
                      QString::fromUtf8(m_current_file_info.file_name);
    } else {
        output_path = m_recv_base_dir + "/" +
                      QString::fromUtf8(m_current_file_info.file_name);
    }

    emit logMessage(tr("文件接收完毕，正在合并: %1").arg(output_path));

    // 合并分块为完整文件
    bool success = FileUtil::mergeFile(
        output_path.toStdString(),
        m_received_blocks,
        m_temp_dir.toStdString());

    if (success) {
        // 验证文件整体校验
        uint32_t file_crc = FileUtil::fileCrc32(output_path.toStdString());
        if (file_crc == m_current_file_info.file_checksum) {
            emit logMessage(tr("文件保存成功: %1 (CRC32: %2)")
                            .arg(output_path).arg(file_crc, 8, 16));
            emit fileTransferFinished(
                QString::fromUtf8(m_current_file_info.file_name), true);
        } else {
            emit logMessage(tr("文件校验失败: %1 (期望: %2, 实际: %3)")
                            .arg(output_path)
                            .arg(m_current_file_info.file_checksum, 8, 16)
                            .arg(file_crc, 8, 16));
            emit fileTransferFinished(
                QString::fromUtf8(m_current_file_info.file_name), false);
        }
    } else {
        emit logMessage(tr("文件合并失败: %1").arg(output_path));
        emit fileTransferFinished(
            QString::fromUtf8(m_current_file_info.file_name), false);
    }

    // 清理临时文件
    FileUtil::cleanupTempDir(m_temp_dir.toStdString());

    setState(State::Idle);
}

void TransferEngine::handleFileAck(const MessageHeader& header, const QByteArray& body) {
    Q_UNUSED(header);
    Q_UNUSED(body);

    // 发送端收到确认后，发送下一个块
    sendNextBlock();
}

void TransferEngine::handleFileNak(const MessageHeader& header, const QByteArray& body) {
    Q_UNUSED(header);
    Q_UNUSED(body);

    // 接收端校验失败，重发当前块
    emit logMessage(tr("接收端请求重发块 %1").arg(m_current_block - 1));
    if (m_current_block > 0) {
        m_current_block--;  // 回退一个块
        sendNextBlock();
    }
}

// ============================================================
// 目录传输处理
// ============================================================

void TransferEngine::handleDirStart(const MessageHeader& header, const QByteArray& body) {
    Q_UNUSED(header);
    if (body.size() < static_cast<int>(sizeof(DirEntryBody))) {
        notifyError(tr("目录开始消息不完整"));
        return;
    }

    const DirEntryBody* entry = reinterpret_cast<const DirEntryBody*>(body.constData());
    m_recv_base_dir = QDir::currentPath() + "/" +
                      QString::fromUtf8(entry->entry_name);

    QDir().mkpath(m_recv_base_dir);
    emit logMessage(tr("开始接收目录: %1").arg(m_recv_base_dir));
}

void TransferEngine::handleDirEntry(const MessageHeader& header, const QByteArray& body) {
    Q_UNUSED(header);
    if (body.size() < static_cast<int>(sizeof(DirEntryBody))) {
        notifyError(tr("目录条目消息不完整"));
        return;
    }

    const DirEntryBody* entry = reinterpret_cast<const DirEntryBody*>(body.constData());
    QString entry_path = m_recv_base_dir + "/" +
                         QString::fromUtf8(entry->entry_name);

    if (entry->is_directory) {
        QDir().mkpath(entry_path);
        emit logMessage(tr("创建目录: %1").arg(entry_path));
    }
    // 文件条目在收到 FILE_INFO 时处理
}

void TransferEngine::handleDirEnd(const MessageHeader& header, const QByteArray& body) {
    Q_UNUSED(header);
    Q_UNUSED(body);

    emit logMessage(tr("目录接收完成"));
    emit fileTransferFinished(QString::fromUtf8(m_current_file_info.file_name), true);
    m_recv_base_dir.clear();
    setState(State::Idle);
}

// ============================================================
// 控制消息处理
// ============================================================

void TransferEngine::handleCancel(const MessageHeader& header, const QByteArray& body) {
    Q_UNUSED(header);
    Q_UNUSED(body);

    emit logMessage(tr("对方取消了传输"));
    while (!m_transfer_queue.empty()) m_transfer_queue.pop();
    FileUtil::cleanupTempDir(m_temp_dir.toStdString());
    setState(State::Idle);
}

void TransferEngine::handlePauseResume(MessageType type) {
    if (type == MessageType::PAUSE_TRANSFER) {
        emit logMessage(tr("对方暂停了传输"));
        setState(State::Paused);
    } else {
        emit logMessage(tr("对方恢复了传输"));
        setState(State::Transferring);
    }
}

void TransferEngine::handleError(const MessageHeader& header, const QByteArray& body) {
    if (body.size() < static_cast<int>(sizeof(ErrorBody))) {
        notifyError(tr("收到未知错误"));
        return;
    }

    const ErrorBody* error = reinterpret_cast<const ErrorBody*>(body.constData());
    emit logMessage(tr("对方错误: [%1] %2")
                    .arg(error->error_code).arg(error->error_msg));
}

// ============================================================
// 心跳处理
// ============================================================

void TransferEngine::handleHeartbeat() {
    sendMessage(MessageType::HEARTBEAT_ACK, nullptr, 0);
}

void TransferEngine::handleHeartbeatAck() {
    m_heartbeat_pending = false;
}

void TransferEngine::onHeartbeatTimeout() {
    if (m_heartbeat_pending) {
        // 对方没有响应心跳
        emit logMessage(tr("心跳超时，连接可能已断开"));
        disconnectServer();
        return;
    }

    m_heartbeat_pending = true;
    sendMessage(MessageType::HEARTBEAT, nullptr, 0);
}

void TransferEngine::startHeartbeat() {
    stopHeartbeat();

    m_heartbeat_timer = new QTimer(this);
    connect(m_heartbeat_timer, &QTimer::timeout,
            this, &TransferEngine::onHeartbeatTimeout);
    m_heartbeat_timer->start(10000);  // 每 10 秒发送一次心跳

    m_heartbeat_check_timer = new QTimer(this);
    connect(m_heartbeat_check_timer, &QTimer::timeout,
            this, &TransferEngine::onHeartbeatTimeout);
    m_heartbeat_pending = false;
}

void TransferEngine::stopHeartbeat() {
    if (m_heartbeat_timer) {
        m_heartbeat_timer->stop();
        m_heartbeat_timer->deleteLater();
        m_heartbeat_timer = nullptr;
    }
    if (m_heartbeat_check_timer) {
        m_heartbeat_check_timer->stop();
        m_heartbeat_check_timer->deleteLater();
        m_heartbeat_check_timer = nullptr;
    }
    m_heartbeat_pending = false;
}

// ============================================================
// 辅助方法
// ============================================================

void TransferEngine::notifyError(const QString& msg) {
    emit logMessage(tr("错误: %1").arg(msg));

    ErrorBody error_body;
    error_body.error_code = 1;
    strncpy(error_body.error_msg, msg.toStdString().c_str(), sizeof(error_body.error_msg) - 1);
    sendMessage(MessageType::ERROR_MSG, &error_body, sizeof(error_body));

    setState(State::Error);
}

void TransferEngine::setState(State new_state) {
    if (m_state != new_state) {
        m_state = new_state;
        emit stateChanged(new_state);
    }
}

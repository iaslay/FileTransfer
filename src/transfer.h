#ifndef TRANSFER_H
#define TRANSFER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QElapsedTimer>
#include <memory>
#include <vector>
#include <string>
#include <queue>

#include "protocol.h"
#include "fileutil.h"

/**
 * 传输引擎类
 * 同时支持服务器模式和客户端模式
 * 通过 Role 枚举区分：Server 监听连接，Client 主动连接
 */
class TransferEngine : public QObject {
    Q_OBJECT

public:
    // 模式枚举
    enum class Role {
        Server,  // 服务端（监听模式，通常运行在 Windows）
        Client   // 客户端（主动连接，通常运行在树莓派）
    };

    // 传输状态
    enum class State {
        Idle,           // 空闲
        Handshaking,    // 握手
        Transferring,   // 传输中
        Paused,         // 已暂停
        Finished,       // 完成
        Error           // 错误
    };

    // 传输队列中的文件项
    struct TransferItem {
        QString local_path;      // 本地路径
        QString remote_path;     // 远端路径（文件名/相对路径）
        uint64_t file_size;      // 文件大小
        bool     is_directory;   // 是否为目录
        QStringList sub_files;   // 如果是目录，包含的子文件列表
    };

    explicit TransferEngine(Role role, QObject* parent = nullptr);
    ~TransferEngine() override;

    // ---- 服务端接口 ----
    bool startServer(uint16_t port = DEFAULT_PORT);
    void stopServer();

    // ---- 客户端接口 ----
    bool connectToServer(const QString& host, uint16_t port = DEFAULT_PORT);
    void disconnectServer();

    // ---- 传输接口 ----
    void sendFiles(const QStringList& file_list);
    void sendDirectory(const QString& dir_path);
    void cancelTransfer();
    void pauseTransfer();
    void resumeTransfer();

    // ---- 客户端请求上传 ----
    void requestUpload(const QStringList& file_list);

    // ---- 状态查询 ----
    Role role() const { return m_role; }
    State state() const { return m_state; }
    bool isConnected() const;
    QString peerAddress() const;

    // 设置分块大小
    void setBlockSize(uint32_t block_size) { m_block_size = block_size; }

    // 获取传输统计
    uint64_t totalBytes() const { return m_total_bytes; }
    uint64_t transferredBytes() const { return m_transferred_bytes; }

signals:
    // 传输进度信号（文件名，已传输百分比，传输速度 KB/s）
    void progressUpdated(const QString& file_name, int percent, double speed_kbps);

    // 单个文件传输完成信号
    void fileTransferFinished(const QString& file_name, bool success);

    // 所有传输完成信号
    void allTransfersFinished();

    // 日志消息信号
    void logMessage(const QString& msg);

    // 状态变化信号
    void stateChanged(TransferEngine::State new_state);

    // 连接状态信号
    void connected();
    void disconnected();

private slots:
    // 服务端槽函数
    void onNewConnection();

    // 客户端/服务端 数据接收槽
    void onReadyRead();

    // 连接断开处理
    void onDisconnected();

    // 错误处理
    void onError(QAbstractSocket::SocketError error);

    // 心跳超时
    void onHeartbeatTimeout();

private:
    // ---- 内部数据结构 ----
    Role              m_role;
    State             m_state;

    // 网络组件
    QTcpServer*       m_server;
    QTcpSocket*       m_socket;

    // 传输队列
    std::queue<TransferItem> m_transfer_queue;
    TransferItem      m_current_item;
    bool              m_has_current_item;

    // 分块传输
    uint32_t          m_block_size;
    uint32_t          m_current_block;
    uint32_t          m_total_blocks;

    // 统计信息
    uint64_t          m_total_bytes;
    uint64_t          m_transferred_bytes;
    QElapsedTimer     m_speed_timer;
    uint64_t          m_last_bytes;
    double            m_current_speed;

    // 接收端缓冲区
    QByteArray        m_recv_buffer;
    uint32_t          m_expected_seq;

    // 心跳
    QTimer*           m_heartbeat_timer;
    QTimer*           m_heartbeat_check_timer;
    bool              m_heartbeat_pending;

    // 临时目录（用于分块传输时的临时文件存储）
    QString           m_temp_dir;
    // 接收到的分块信息
    std::vector<FileUtil::BlockInfo> m_received_blocks;
    // 当前接收的文件信息
    FileInfoBody      m_current_file_info;
    // 接收目录时的目标路径
    QString           m_recv_base_dir;

    // ---- 内部方法 ----

    // 发送消息
    bool sendMessage(MessageType type, const void* body, uint32_t body_length);

    // 发送文件
    void sendFile(const QString& filepath, const QString& remote_name);

    // 发送下一个文件块
    void sendNextBlock();

    // 发送目录
    void sendDirectoryInternal(const QString& dir_path, const QString& base_dir);

    // 启动下一个传输项
    void startNextTransfer();

    // 处理接收到的消息
    void processMessage(const MessageHeader& header, const QByteArray& body);

    // 处理各种消息类型
    void handleHandshakeReq(const MessageHeader& header, const QByteArray& body);
    void handleHandshakeAck(const MessageHeader& header, const QByteArray& body);
    void handleFileInfo(const MessageHeader& header, const QByteArray& body);
    void handleFileBlock(const MessageHeader& header, const QByteArray& body);
    void handleFileEnd(const MessageHeader& header, const QByteArray& body);
    void handleFileAck(const MessageHeader& header, const QByteArray& body);
    void handleFileNak(const MessageHeader& header, const QByteArray& body);
    void handleDirStart(const MessageHeader& header, const QByteArray& body);
    void handleDirEntry(const MessageHeader& header, const QByteArray& body);
    void handleDirEnd(const MessageHeader& header, const QByteArray& body);
    void handleCancel(const MessageHeader& header, const QByteArray& body);
    void handlePauseResume(MessageType type);
    void handleError(const MessageHeader& header, const QByteArray& body);
    void handleHeartbeat();
    void handleHeartbeatAck();

    // 通知错误
    void notifyError(const QString& msg);

    // 更新状态
    void setState(State new_state);

    // 开始心跳
    void startHeartbeat();
    void stopHeartbeat();
};

#endif // TRANSFER_H
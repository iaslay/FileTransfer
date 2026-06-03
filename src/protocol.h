#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// ============================================================
// 文件传输协议定义
// 协议基于 TCP 流式传输，消息格式：固定头 + 可变体
// 魔数：0x46544652 ("FTFR" - File Transfer)
// ============================================================

#pragma pack(push, 1)

// 协议魔数
constexpr uint32_t PROTOCOL_MAGIC = 0x46544652;  // "FTFR"

// 协议版本
constexpr uint32_t PROTOCOL_VERSION = 0x00010000; // v1.0.0

// 默认端口
constexpr uint16_t DEFAULT_PORT = 8888;

// 默认分块大小：4MB
constexpr uint32_t DEFAULT_BLOCK_SIZE = 4 * 1024 * 1024;

// 最大文件名长度
constexpr uint32_t MAX_FILENAME_LEN = 512;

// ============================================================
// 消息类型枚举
// ============================================================
enum class MessageType : uint32_t {
    // ---- 握手阶段 ----
    HANDSHAKE_REQ     = 0x00000001,  // 握手请求
    HANDSHAKE_ACK     = 0x00000002,  // 握手应答
    HANDSHAKE_NAK     = 0x00000003,  // 握手拒绝（版本不兼容）

    // ---- 文件传输 ----
    FILE_INFO         = 0x00000100,  // 文件元信息
    FILE_BLOCK        = 0x00000101,  // 文件数据块
    FILE_END          = 0x00000102,  // 文件传输结束
    FILE_ACK          = 0x00000103,  // 文件接收确认
    FILE_NAK          = 0x00000104,  // 文件接收失败

    // ---- 目录传输 ----
    DIR_START         = 0x00000200,  // 目录传输开始
    DIR_ENTRY         = 0x00000201,  // 目录条目（子文件/子目录）
    DIR_END           = 0x00000202,  // 目录传输结束

    // ---- 控制指令 ----
    CANCEL_TRANSFER   = 0x00000300,  // 取消传输
    PAUSE_TRANSFER    = 0x00000301,  // 暂停传输
    RESUME_TRANSFER   = 0x00000302,  // 恢复传输
    ERROR_MSG         = 0x00000303,  // 错误消息
    HEARTBEAT         = 0x00000304,  // 心跳检测
    HEARTBEAT_ACK     = 0x00000305,  // 心跳应答
};

// ============================================================
// 消息头部（固定 20 字节）
// ============================================================
struct MessageHeader {
    uint32_t magic;          // 魔数：0x46544652
    uint32_t type;           // 消息类型 (MessageType)
    uint32_t body_length;    // 消息体长度（不含头部）
    uint32_t sequence;       // 序列号（用于匹配请求和应答）
    uint32_t checksum;       // 消息头校验（magic + type + body_length + sequence 的 XOR 和）

    MessageHeader()
        : magic(PROTOCOL_MAGIC)
        , type(0)
        , body_length(0)
        , sequence(0)
        , checksum(0)
    {}

    // 计算头部校验
    void calcChecksum() {
        checksum = magic ^ type ^ body_length ^ sequence;
    }

    // 验证头部校验
    bool verifyChecksum() const {
        return checksum == (magic ^ type ^ body_length ^ sequence);
    }
};

// ============================================================
// 握手消息体
// ============================================================
struct HandshakeBody {
    uint32_t version;        // 协议版本
    uint32_t capabilities;   // 能力标志位
    char     hostname[64];   // 主机名

    // 能力标志
    enum Capability {
        CAP_BLOCK_TRANSFER  = 0x00000001,  // 支持分块传输
        CAP_DIR_TRANSFER    = 0x00000002,  // 支持目录传输
        CAP_RESUME          = 0x00000004,  // 支持断点续传
        CAP_COMPRESS        = 0x00000008,  // 支持压缩传输
        CAP_ENCRYPT         = 0x00000010,  // 支持加密传输
    };

    HandshakeBody()
        : version(PROTOCOL_VERSION)
        , capabilities(CAP_BLOCK_TRANSFER | CAP_DIR_TRANSFER)
    {
        hostname[0] = '\0';
    }
};

// ============================================================
// 文件信息消息体
// ============================================================
struct FileInfoBody {
    char     file_name[MAX_FILENAME_LEN];  // 文件名（含相对路径）
    uint64_t file_size;                    // 文件总大小
    uint32_t block_size;                   // 分块大小
    uint32_t block_count;                  // 总分块数
    uint64_t file_modify_time;             // 文件修改时间戳
    uint32_t file_checksum;                // 文件整体 CRC32 校验
    uint32_t reserved;                     // 保留

    FileInfoBody()
        : file_size(0)
        , block_size(DEFAULT_BLOCK_SIZE)
        , block_count(0)
        , file_modify_time(0)
        , file_checksum(0)
        , reserved(0)
    {
        file_name[0] = '\0';
    }
};

// ============================================================
// 文件数据块消息体
// ============================================================
struct FileBlockBody {
    uint32_t block_index;     // 块序号（从 0 开始）
    uint32_t data_size;       // 本块实际数据大小
    uint64_t file_offset;     // 本块在文件中的偏移量
    uint32_t block_checksum;  // 本块 CRC32 校验
    // 数据紧随其后（柔性数组模拟）
    char     data[0];

    // 计算包含数据的总消息体大小
    static uint32_t totalSize(uint32_t data_len) {
        return static_cast<uint32_t>(sizeof(FileBlockBody) + data_len);
    }
};

// ============================================================
// 文件确认消息体
// ============================================================
struct FileAckBody {
    uint32_t block_index;     // 确认的块序号（0xFFFFFFFF 表示全部完成）
    uint32_t status;          // 状态码：0=成功, 其他=错误码
    uint32_t reserved;

    FileAckBody() : block_index(0), status(0), reserved(0) {}
};

// ============================================================
// 目录条目消息体
// ============================================================
struct DirEntryBody {
    uint8_t  is_directory;    // 是否为目录：0=文件, 1=目录
    uint32_t entry_size;      // 如果是文件则为文件大小，目录则为 0
    char     entry_name[MAX_FILENAME_LEN];  // 相对路径名

    DirEntryBody() : is_directory(0), entry_size(0) {
        entry_name[0] = '\0';
    }
};

// ============================================================
// 错误消息体
// ============================================================
struct ErrorBody {
    uint32_t error_code;      // 错误码
    char     error_msg[256];  // 错误描述

    ErrorBody() : error_code(0) {
        error_msg[0] = '\0';
    }
};

#pragma pack(pop)

// ============================================================
// 辅助函数
// ============================================================

// 获取消息类型名称（用于日志输出）
inline std::string messageTypeName(MessageType type) {
    switch (type) {
        case MessageType::HANDSHAKE_REQ:    return "HANDSHAKE_REQ";
        case MessageType::HANDSHAKE_ACK:    return "HANDSHAKE_ACK";
        case MessageType::HANDSHAKE_NAK:    return "HANDSHAKE_NAK";
        case MessageType::FILE_INFO:        return "FILE_INFO";
        case MessageType::FILE_BLOCK:       return "FILE_BLOCK";
        case MessageType::FILE_END:         return "FILE_END";
        case MessageType::FILE_ACK:         return "FILE_ACK";
        case MessageType::FILE_NAK:         return "FILE_NAK";
        case MessageType::DIR_START:        return "DIR_START";
        case MessageType::DIR_ENTRY:        return "DIR_ENTRY";
        case MessageType::DIR_END:          return "DIR_END";
        case MessageType::CANCEL_TRANSFER:  return "CANCEL_TRANSFER";
        case MessageType::PAUSE_TRANSFER:   return "PAUSE_TRANSFER";
        case MessageType::RESUME_TRANSFER:  return "RESUME_TRANSFER";
        case MessageType::ERROR_MSG:        return "ERROR_MSG";
        case MessageType::HEARTBEAT:        return "HEARTBEAT";
        case MessageType::HEARTBEAT_ACK:    return "HEARTBEAT_ACK";
        default:                            return "UNKNOWN";
    }
}

#endif // PROTOCOL_H
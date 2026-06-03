#ifndef FILEUTIL_H
#define FILEUTIL_H

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

/**
 * 文件工具类
 * 提供文件分块、合并、校验和目录遍历功能
 */
class FileUtil {
public:
    // 分块信息结构
    struct BlockInfo {
        uint32_t index;       // 块序号
        uint32_t size;        // 块实际数据大小
        uint64_t offset;      // 在文件中的偏移
        uint32_t checksum;    // 块 CRC32 校验值
    };

    /**
     * 将文件按指定块大小切割，返回分块信息列表
     * @param filepath 文件路径
     * @param block_size 块大小（字节）
     * @param blocks [输出] 分块信息列表
     * @return 成功返回 true
     */
    static bool splitFile(const std::string& filepath,
                          uint32_t block_size,
                          std::vector<BlockInfo>& blocks);

    /**
     * 从分块重建合并文件
     * @param output_path 输出文件路径
     * @param blocks 分块信息列表
     * @param temp_dir 临时目录（存放分块数据文件）
     * @return 成功返回 true
     */
    static bool mergeFile(const std::string& output_path,
                          const std::vector<BlockInfo>& blocks,
                          const std::string& temp_dir);

    /**
     * 读取文件中的一个分块到内存
     * @param filepath 文件路径
     * @param offset 偏移量
     * @param size 读取大小
     * @param data [输出] 读取的数据
     * @return 实际读取的字节数
     */
    static uint32_t readBlock(const std::string& filepath,
                              uint64_t offset,
                              uint32_t size,
                              std::vector<char>& data);

    /**
     * 将分块数据写入临时文件
     * @param block 分块信息
     * @param data 分块数据
     * @param temp_dir 临时目录
     * @return 成功返回 true
     */
    static bool writeBlockFile(const BlockInfo& block,
                               const std::vector<char>& data,
                               const std::string& temp_dir);

    /**
     * 读取临时目录中的分块数据
     * @param block 分块信息
     * @param temp_dir 临时目录
     * @param data [输出] 读取的数据
     * @return 成功返回 true
     */
    static bool readBlockFile(const BlockInfo& block,
                              const std::string& temp_dir,
                              std::vector<char>& data);

    /**
     * 计算数据块的 CRC32 校验值
     * @param data 数据指针
     * @param len 数据长度
     * @return CRC32 值
     */
    static uint32_t crc32(const char* data, size_t len);

    /**
     * 计算文件的 CRC32 校验值
     * @param filepath 文件路径
     * @return CRC32 值，失败返回 0
     */
    static uint32_t fileCrc32(const std::string& filepath);

    /**
     * 遍历目录获取所有文件列表（递归）
     * @param dir_path 目录路径
     * @param files [输出] 文件路径列表（相对路径）
     * @param base_path 基础路径（用于递归构建相对路径）
     * @return 成功返回 true
     */
    static bool listDirectory(const std::string& dir_path,
                              std::vector<std::string>& files,
                              const std::string& base_path = "");

    /**
     * 获取文件大小
     */
    static uint64_t fileSize(const std::string& filepath);

    /**
     * 确保目录存在，不存在则创建
     */
    static bool ensureDir(const std::string& dir_path);

    /**
     * 获取文件名的父目录路径
     */
    static std::string parentDir(const std::string& filepath);

    /**
     * 清空并移除临时目录
     */
    static bool cleanupTempDir(const std::string& temp_dir);

private:
    // CRC32 查表法相关静态数据
    static uint32_t CRC32_TABLE[256];
    static void buildCrc32Table();
    static bool s_crc_table_ready;
};

#endif // FILEUTIL_H
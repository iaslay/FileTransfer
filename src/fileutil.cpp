#include "fileutil.h"
#include <cstring>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <QFile>
#include <QDir>
#include <QFileInfo>

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#define access _access
#define mkdir _mkdir
#else
#include <unistd.h>
#endif

// 初始化静态 CRC32 表和标志
bool FileUtil::s_crc_table_ready = false;
uint32_t FileUtil::CRC32_TABLE[256] = {0};

void FileUtil::buildCrc32Table() {
    if (s_crc_table_ready) return;

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (uint32_t j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc = crc >> 1;
        }
        CRC32_TABLE[i] = crc;
    }
    s_crc_table_ready = true;
}

uint32_t FileUtil::crc32(const char* data, size_t len) {
    buildCrc32Table();

    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        uint8_t index = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ CRC32_TABLE[index];
    }
    return crc ^ 0xFFFFFFFF;
}

uint32_t FileUtil::fileCrc32(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) return 0;

    buildCrc32Table();

    uint32_t crc = 0xFFFFFFFF;
    char buffer[8192];

    while (file.good() && !file.eof()) {
        file.read(buffer, sizeof(buffer));
        std::streamsize bytes = file.gcount();
        if (bytes > 0) {
            for (std::streamsize i = 0; i < bytes; i++) {
                uint8_t index = (crc ^ buffer[i]) & 0xFF;
                crc = (crc >> 8) ^ CRC32_TABLE[index];
            }
        }
    }

    file.close();
    return crc ^ 0xFFFFFFFF;
}

bool FileUtil::splitFile(const std::string& filepath,
                          uint32_t block_size,
                          std::vector<BlockInfo>& blocks) {
    uint64_t size = fileSize(filepath);
    if (size == 0) return false;

    blocks.clear();

    uint64_t remaining = size;
    uint32_t index = 0;

    while (remaining > 0) {
        BlockInfo block;
        block.index = index;
        block.offset = static_cast<uint64_t>(index) * block_size;
        block.size = (remaining >= block_size) ? block_size : static_cast<uint32_t>(remaining);

        // 读取块数据计算校验
        std::vector<char> data;
        readBlock(filepath, block.offset, block.size, data);
        block.checksum = crc32(data.data(), data.size());

        blocks.push_back(block);

        remaining -= block.size;
        index++;
    }

    return !blocks.empty();
}

uint32_t FileUtil::readBlock(const std::string& filepath,
                              uint64_t offset,
                              uint32_t size,
                              std::vector<char>& data) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) return 0;

    data.resize(size);
    file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    file.read(data.data(), size);
    uint32_t read_bytes = static_cast<uint32_t>(file.gcount());

    if (read_bytes < size) {
        data.resize(read_bytes);
    }

    file.close();
    return read_bytes;
}

bool FileUtil::writeBlockFile(const BlockInfo& block,
                               const std::vector<char>& data,
                               const std::string& temp_dir) {
    // 确保临时目录存在
    if (!ensureDir(temp_dir)) return false;

    std::string block_file = temp_dir + "/block_" + std::to_string(block.index) + ".tmp";
    std::ofstream file(block_file, std::ios::binary);
    if (!file.is_open()) return false;

    file.write(data.data(), data.size());
    file.close();

    return true;
}

bool FileUtil::readBlockFile(const BlockInfo& block,
                              const std::string& temp_dir,
                              std::vector<char>& data) {
    std::string block_file = temp_dir + "/block_" + std::to_string(block.index) + ".tmp";
    std::ifstream file(block_file, std::ios::binary);
    if (!file.is_open()) return false;

    data.resize(block.size);
    file.read(data.data(), block.size);
    uint32_t read_bytes = static_cast<uint32_t>(file.gcount());

    if (read_bytes < block.size) {
        data.resize(read_bytes);
    }

    file.close();
    return read_bytes > 0;
}

bool FileUtil::mergeFile(const std::string& output_path,
                          const std::vector<BlockInfo>& blocks,
                          const std::string& temp_dir) {
    // 确保输出目录存在
    std::string parent = parentDir(output_path);
    if (!parent.empty() && !ensureDir(parent)) return false;

    std::ofstream file(output_path, std::ios::binary);
    if (!file.is_open()) return false;

    for (const auto& block : blocks) {
        std::vector<char> data;
        if (!readBlockFile(block, temp_dir, data)) {
            file.close();
            return false;
        }

        // 验证块校验
        uint32_t calc_checksum = crc32(data.data(), data.size());
        if (calc_checksum != block.checksum) {
            // 校验失败，说明传输损坏
            file.close();
            return false;
        }

        file.write(data.data(), data.size());
    }

    file.close();
    return true;
}

bool FileUtil::listDirectory(const std::string& dir_path,
                              std::vector<std::string>& files,
                              const std::string& base_path) {
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) return false;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);

        // 跳过 '.' 和 '..' 目录
        if (name == "." || name == "..") continue;

        std::string full_path = dir_path + "/" + name;
        std::string rel_path;
        if (base_path.empty()) {
            rel_path = name;
        } else {
            rel_path = base_path + "/" + name;
        }

        struct stat st;
        if (stat(full_path.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            // 递归遍历子目录
            listDirectory(full_path, files, rel_path);
        } else if (S_ISREG(st.st_mode)) {
            files.push_back(rel_path);
        }
    }

    closedir(dir);
    return true;
}

uint64_t FileUtil::fileSize(const std::string& filepath) {
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) return 0;
    return static_cast<uint64_t>(st.st_size);
}

bool FileUtil::ensureDir(const std::string& dir_path) {
    struct stat st;
    if (stat(dir_path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }

#ifdef _WIN32
    return mkdir(dir_path.c_str()) == 0;
#else
    return mkdir(dir_path.c_str(), 0755) == 0;
#endif
}

std::string FileUtil::parentDir(const std::string& filepath) {
    size_t pos = filepath.find_last_of("/\\");
    if (pos == std::string::npos) return "";
    return filepath.substr(0, pos);
}

bool FileUtil::cleanupTempDir(const std::string& temp_dir) {
    // 删除临时目录中的所有 .tmp 文件
    DIR* dir = opendir(temp_dir.c_str());
    if (!dir) return true; // 目录不存在也算成功

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        if (name.size() > 4 && name.substr(name.size() - 4) == ".tmp") {
            std::string full_path = temp_dir + "/" + name;
            unlink(full_path.c_str());
        }
    }

    closedir(dir);
    return true;
}

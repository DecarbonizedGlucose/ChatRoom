#pragma once

#include <string>
#include <memory>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <google/protobuf/message.h>
#include "../abstract/datatypes.hpp"

// 文件分片大小 (64KB)
constexpr size_t CHUNK_SIZE = 4 * 1024 * 1024;  // 4MB

// 文件状态枚举
enum class FileStatus {
    PENDING,        // 等待处理
    UPLOADING,      // 上传中
    DOWNLOADING,    // 下载中
    COMPLETED,      // 完成
    FAILED,         // 失败
    CANCELLED       // 取消
};

// 基础文件类
class File {
public:
    std::string file_name;      // 文件名
    std::string file_hash;      // 文件hash (64字符)
    std::string file_id;        // 服务端分配的文件ID (File_xxx)
    size_t file_size = 0;       // 文件大小
    FileStatus status = FileStatus::PENDING;

    File() = default;
    File(const std::string& name, const std::string& hash, size_t size);
    virtual ~File() = default;

    // 计算文件hash
    static std::string calculate_hash(const std::string& file_path);

    // 获取总分片数
    size_t get_total_chunks() const;
};

class ClientFile : public File {
private:
    std::string local_path;     // 本地文件路径
    std::ifstream input_stream; // 读取文件流
    std::ofstream output_stream;// 写入文件流
    size_t current_chunk = 0;   // 当前分片索引

public:
    ClientFile(const std::string& path);  // 用于上传
    ClientFile(const std::string& name, const std::string& save_path,
               const std::string& hash, size_t size);  // 用于下载

    ~ClientFile();

    // 上传相关
    bool open_for_read();
    std::vector<char> read_next_chunk();
    bool has_more_chunks() const;

    // 下载相关
    bool open_for_write();
    bool write_chunk(const std::vector<char>& data, size_t chunk_index);
    bool finalize_download();

    const std::string& get_local_path() const { return local_path; }
    double get_progress() const;
    size_t get_current_chunk() const { return current_chunk; }
};

class ServerFile : public File {
private:
    std::string storage_path;   // 服务端存储路径
    std::ofstream output_stream;// 写入文件流
    std::ifstream input_stream; // 读取文件流
    std::vector<char> received_chunks; // 记录已接收的分片 (使用char避免vector<bool>问题)
    size_t received_count = 0;  // 已接收分片数

public:
    ServerFile(const std::string& hash, const std::string& file_id,
               const std::string& name, size_t size, const std::string& storage);

    ~ServerFile();

    // 接收文件相关
    bool open_for_write();
    bool write_chunk(const std::vector<char>& data, size_t chunk_index);
    bool is_complete() const;
    bool finalize_upload();

    // 发送文件相关
    bool open_for_read();
    std::vector<char> read_chunk(size_t chunk_index);

    // 获取存储路径
    const std::string& get_storage_path() const { return storage_path; }

    // 获取接收进度
    double get_receive_progress() const;

    // 检查分片是否已接收
    bool is_chunk_received(size_t chunk_index) const;
};

using ServerFilePtr = std::shared_ptr<ServerFile>;
using ClientFilePtr = std::shared_ptr<ClientFile>;
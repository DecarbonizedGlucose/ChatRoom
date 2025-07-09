#ifndef FILE_HPP
#define FILE_HPP

#include <string>
#include <memory>
#include <google/protobuf/message.h>
#include "../abstract/file.pb.h"

using ChatFilePtr = std::shared_ptr<ChatFile>;

class File {
public:
    ChatFilePtr chat_file = nullptr; // data通道的ChatFile(protobuf)
    std::string file_name;      // 文件名
    std::string local_path;     // 本地路径, 发送时忽略这项
    std::string file_hash;
    off_t file_size;            // using off_t = long;
    int fd = -1;

    File() = delete;
    File(const std::string& local_path);
    File(const std::string& file_name, const std::string& local_path);

    bool calc_hash();
};

using FilePtr = std::shared_ptr<File>;

// 获取文件指针 发送文件时使用
// 获取成功时, 返回一个shared ptr, 失败时返回nullptr
FilePtr get_fileptr(const std::string& local_path);

// 创建文件指针
// 用于指定下载文件
FilePtr make_fileptr(const std::string& local_path);

ChatFilePtr getChatFilePtr(const FilePtr& file);

#endif
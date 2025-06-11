#ifndef FILE_HPP
#define FILE_HPP

#include <string>
#include <memory>

class File {
public:
    File() = delete;
    File(const std::string& local_path);
    File(const std::string& file_name, const std::string& local_path);

    std::string file_name;      // 文件名
    std::string local_path;     // 本地路径, 发送时忽略这项
    off_t file_size;            // using off_t = long;
    int fd = -1;
};

using FilePtr = std::shared_ptr<File>;

// 获取文件指针
// 获取成功时, 返回一个shared ptr, 失败时返回nullptr
FilePtr get_fileptr(const std::string& local_path);

// 创建文件指针
// 用于指定下载文件
FilePtr make_fileptr(const std::string& local_path);

#endif
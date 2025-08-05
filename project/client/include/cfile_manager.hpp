# pragma once
#include "../../global/include/file.hpp"
#include <string>
#include <mutex>
#include <condition_variable>

class thread_pool;
class CommManager;

enum class OperationType {
    Free,
    Uploading,
    Downloading,
    Error
};

class CFileManager {
public:
    thread_pool* pool = nullptr;
    CommManager* comm = nullptr;

    OperationType r_status = OperationType::Free;
    OperationType w_status = OperationType::Free;

    CFileManager(thread_pool* pool, CommManager* comm);
    ~CFileManager();

    // 上传文件
    void upload_file(const std::string& file_path);
    // 下载文件
    void download_file(
        const std::string& file_name,
        const std::string& save_path,
        const std::string& file_hash,
        size_t file_size
    );


private:
    mutable std::mutex read_Mutex;
    mutable std::mutex write_Mutex;
    std::condition_variable upload_cv;    // 上传完成通知
    std::condition_variable download_cv;  // 下载完成通知

    void process_upload_task(const ClientFilePtr& file);
    void process_download_task(const ClientFilePtr& file);
};


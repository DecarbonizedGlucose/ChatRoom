#pragma once

#include <utility>
#include <string>
#include <functional>
#include "../../global/include/safe_queue.hpp"
#include "../../global/include/file.hpp"

class Dispatcher;
class thread_pool;

struct FileDownloadTask {
    std::string user_id;
    std::string file_id;
    std::string file_hash;
    std::string file_name;
    size_t file_size;
};

struct FileUploadTask {
    std::string user_id;
    ServerFilePtr server_file;
};

class SFileManager {
public:
    Dispatcher* disp = nullptr;
    thread_pool* pool = nullptr;

    SFileManager(Dispatcher* dispatcher);
    ~SFileManager();

    void set_thread_pool(thread_pool* thread_pool_ptr);
    void add_download_task(const std::string& user_id, const std::string& file_id,
                           const std::string& file_hash, const std::string& file_name, size_t file_size);
    void add_upload_task(const std::string& user_id, ServerFilePtr server_file);

private:
    void send_file_chunk(const std::string& user_id, const std::string& file_id,
                        const std::vector<char>& chunk_data, size_t chunk_index,
                        size_t total_chunks, bool is_last_chunk);
    void process_single_download_task(const FileDownloadTask& task);
    void process_single_upload_task(const FileUploadTask& task);
};
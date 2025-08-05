#include "../include/sfile_manager.hpp"
#include "../include/dispatcher.hpp"
#include "../include/connection_manager.hpp"
#include "../../global/include/logging.hpp"
#include "../../global/include/threadpool.hpp"
#include "../../global/abstract/datatypes.hpp"
#include <chrono>

extern void try_send(ConnectionManager* conn_manager, TcpServerConnection* conn,
                    const std::string& proto, DataType type = DataType::Command);

SFileManager::SFileManager(Dispatcher* dispatcher) : disp(dispatcher) {}

SFileManager::~SFileManager() {}

void SFileManager::set_thread_pool(thread_pool* thread_pool_ptr) {
    pool = thread_pool_ptr;
}

void SFileManager::add_download_task(const std::string& user_id, const std::string& file_id,
                                     const std::string& file_hash, const std::string& file_name, size_t file_size) {
    FileDownloadTask task;
    task.user_id = user_id;
    task.file_id = file_id;
    task.file_hash = file_hash;
    task.file_name = file_name;
    task.file_size = file_size;
    log_debug("Adding download task for user: {}, file_id: {}", user_id, file_id);
    // 直接提交单个任务到线程池处理
    if (pool) {
        pool->submit([this, task]() {
            this->process_single_download_task(task);
        });
    } else {
        log_error("Thread pool not set for SFileManager");
    }
}

void SFileManager::add_upload_task(const std::string& user_id, ServerFilePtr server_file) {
    FileUploadTask task;
    task.user_id = user_id;
    task.server_file = server_file;
    log_debug("Adding upload task for user: {}, file: {}", user_id, server_file->file_name);
    // 直接提交单个任务到线程池处理
    if (pool) {
        pool->submit([this, task]() {
            this->process_single_upload_task(task);
        });
    } else {
        log_error("Thread pool not set for SFileManager");
    }
}

void SFileManager::process_single_download_task(const FileDownloadTask& task) {
    log_debug("Processing download task for user: {}, file_id: {}", task.user_id, task.file_id);
    // 创建服务端文件对象用于读取
    auto server_file = std::make_shared<ServerFile>(task.file_hash, task.file_id, task.file_name, task.file_size);
    // 打开文件进行读取
    if (!server_file->open_for_read()) {
        log_error("Failed to open file for reading: {}", task.file_id);
        return;
    }
    // 获取总分片数
    size_t total_chunks = server_file->get_total_chunks();
    log_info("Starting file download: {} ({} chunks)", task.file_name, total_chunks);
    // 分片发送文件
    for (size_t chunk_index = 0; chunk_index < total_chunks; ++chunk_index) {
        try {
            // 读取分片数据
            std::vector<char> chunk_data = server_file->read_chunk(chunk_index);
            if (chunk_data.empty()) {
                log_error("Failed to read chunk {} for file: {}", chunk_index, task.file_id);
                break;
            }
            bool is_last_chunk = (chunk_index == total_chunks - 1);
            // 发送分片
            send_file_chunk(task.user_id, task.file_id, chunk_data, chunk_index, total_chunks, is_last_chunk);
            log_debug("Sent chunk {}/{} for file: {} (size: {} bytes)",
                     chunk_index + 1, total_chunks, task.file_name, chunk_data.size());
            // 添加小延迟避免网络拥塞
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        } catch (const std::exception& e) {
            log_error("Error sending chunk {} for file {}: {}", chunk_index, task.file_id, e.what());
            break;
        }
    }

    log_info("File download completed for user: {}, file: {}", task.user_id, task.file_name);
}

void SFileManager::process_single_upload_task(const FileUploadTask& task) {
    log_debug("Processing upload task for user: {}, file: {}", task.user_id, task.server_file->file_name);

    // 处理上传完成后的逻辑
    if (task.server_file->is_complete()) {
        log_info("File upload completed for user: {}, file: {}", task.user_id, task.server_file->file_name);

        // 这里可以添加更多上传完成后的处理逻辑
        // 比如通知客户端、更新数据库状态、清理临时文件等

        // 验证文件完整性
        if (task.server_file->finalize_upload()) {
            log_info("File upload verification successful: {}", task.server_file->file_name);
        } else {
            log_error("File upload verification failed: {}", task.server_file->file_name);
        }
    }
}

void SFileManager::send_file_chunk(const std::string& user_id, const std::string& file_id,
                                  const std::vector<char>& chunk_data, size_t chunk_index,
                                  size_t total_chunks, bool is_last_chunk) {
    auto chunk = create_file_chunk_string(
        file_id, chunk_data, chunk_index,
        total_chunks, is_last_chunk);
    if (chunk.empty()) {
        log_error("Failed to create file chunk string for file_id: {}, chunk_index: {}", file_id, chunk_index);
        return;
    }
    auto conn = disp->conn_manager->get_connection(user_id, 2);
    if (!conn) {
        log_error("Data connection not found for user: {}", user_id);
        return;
    }
    try_send(disp->conn_manager, conn, chunk, DataType::FileChunk);
}


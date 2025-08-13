#include "../include/cfile_manager.hpp"
#include "../../global/include/threadpool.hpp"
#include "../../global/include/logging.hpp"
#include "../include/CommManager.hpp"

CFileManager::CFileManager(thread_pool* pool, CommManager* comm)
    : pool(pool), comm(comm) {}

CFileManager::~CFileManager() {}

void CFileManager::upload_file(const std::string& file_path) {
    std::unique_lock<std::mutex> lock(write_Mutex);
    upload_cv.wait(lock, [this]() {
        return w_status == OperationType::Free;
    });
    auto file = std::make_shared<ClientFile>(file_path);
    FileOpenStatus status = file->open_for_read();
    if (status != FileOpenStatus::SUCCESS) {
        log_error("Failed to open file for upload: {}, reason: {}", file_path, static_cast<int>(status));
        return;
    }
    pool->submit([this, file]() {
        w_status = OperationType::Uploading;
        process_upload_task(file);
        w_status = OperationType::Free;
        upload_cv.notify_one();
    });
}

void CFileManager::upload_file(const ClientFilePtr& file) {
    std::unique_lock<std::mutex> lock(write_Mutex);
    upload_cv.wait(lock, [this]() {
        return w_status == OperationType::Free;
    });
    FileOpenStatus status = file->open_for_read();
    if (status != FileOpenStatus::SUCCESS) {
        log_error("Failed to open file for upload: {}, reason: {}", file->get_local_path(), static_cast<int>(status));
        return;
    }
    pool->submit([this, file]() {
        w_status = OperationType::Uploading;
        process_upload_task(file);
        w_status = OperationType::Free;
        upload_cv.notify_one();
    });
}

void CFileManager::download_file(
    const std::string& file_name,
    const std::string& save_path, // 路径是带有文件名的完整路径
    const std::string& file_hash,
    size_t file_size
) {
    std::unique_lock<std::mutex> lock(read_Mutex);
    download_cv.wait(lock, [this]() {
        return r_status == OperationType::Free;
    });
    auto file = std::make_shared<ClientFile>(file_name, save_path, file_hash, file_size);
    FileOpenStatus status = file->open_for_write();
    if (status != FileOpenStatus::SUCCESS) {
        log_error("Failed to open file for download: {}, reason: {}", save_path, static_cast<int>(status));
        return;
    }
    pool->submit([this, file]() {
        r_status = OperationType::Downloading;
        process_download_task(file);
        r_status = OperationType::Free;
        download_cv.notify_one();
    });
}

void CFileManager::process_upload_task(const ClientFilePtr& file) {
    // 客户端上传
    while (file->has_more_chunks()) {
        size_t chunk_index = file->get_current_chunk(); // 在读取前获取索引
        auto chunk_data = file->read_next_chunk();
        if (chunk_data.empty()) {
            log_error("Failed to read next chunk from file: {}", file->get_local_path());
            return;
        }
        comm->handle_send_file_chunk(
            file->file_id,
            chunk_data,
            chunk_index,
            file->get_total_chunks(),
            !file->has_more_chunks()
        );
    }
    comm->print_wfile_notice();
    log_info("File upload completed: {}", file->get_local_path());
}

void CFileManager::process_download_task(const ClientFilePtr& file) {
    // 客户端下载
    while (file->has_more_chunks()) {
        auto chunk = comm->handle_receive_file_chunk();
        std::vector<char> data(chunk.data().begin(), chunk.data().end());
        file->write_chunk(data, chunk.chunk_index());
        log_debug("Receive file chunk: {}, index: {}", file->file_id, chunk.chunk_index());
    }
    file->finalize_download();
    comm->print_rfile_notice();
    log_info("File download completed: {}", file->get_local_path());
}
#include "../include/file.hpp"
#include "../include/logging.hpp"
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <cmath>

// ============ File 基类实现 ============

File::File(const std::string& name, const std::string& hash, size_t size)
    : file_name(name), file_hash(hash), file_size(size) {}

std::string File::calculate_hash(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        log_error("Failed to open file for hash calculation: {}", file_path);
        return "";
    }

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context) {
        log_error("Failed to create EVP context");
        return "";
    }

    if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1) {
        log_error("Failed to initialize SHA256");
        EVP_MD_CTX_free(context);
        return "";
    }

    char buffer[8192];
    while (file.read(buffer, sizeof(buffer))) {
        if (EVP_DigestUpdate(context, buffer, file.gcount()) != 1) {
            log_error("Failed to update hash");
            EVP_MD_CTX_free(context);
            return "";
        }
    }

    // 处理最后一次读取的数据
    if (file.gcount() > 0) {
        if (EVP_DigestUpdate(context, buffer, file.gcount()) != 1) {
            log_error("Failed to update hash");
            EVP_MD_CTX_free(context);
            return "";
        }
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    if (EVP_DigestFinal_ex(context, hash, &hash_len) != 1) {
        log_error("Failed to finalize hash");
        EVP_MD_CTX_free(context);
        return "";
    }

    EVP_MD_CTX_free(context);

    // 转换为十六进制字符串
    std::stringstream ss;
    for (unsigned int i = 0; i < hash_len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return ss.str();
}

size_t File::get_total_chunks() const {
    return (file_size + CHUNK_SIZE - 1) / CHUNK_SIZE; // 向上取整
}

// ============ ClientFile 实现 ============

ClientFile::ClientFile(const std::string& path) : local_path(path) {
    if (!std::filesystem::exists(path)) {
        log_error("File does not exist: {}", path);
        status = FileStatus::FAILED;
        return;
    }

    // 获取文件信息
    file_name = std::filesystem::path(path).filename().string();
    file_size = std::filesystem::file_size(path);
    file_hash = calculate_hash(path);

    if (file_hash.empty()) {
        status = FileStatus::FAILED;
        return;
    }

    status = FileStatus::PENDING;
    log_info("Created ClientFile for upload: {} (size: {}, hash: {})",
             file_name, file_size, file_hash);
}

ClientFile::ClientFile(const std::string& name, const std::string& save_path,
                       const std::string& hash, size_t size)
    : File(name, hash, size), local_path(save_path) {
    status = FileStatus::PENDING;
    log_info("Created ClientFile for download: {} -> {}", name, save_path);
}

ClientFile::~ClientFile() {
    if (input_stream.is_open()) {
        input_stream.close();
    }
    if (output_stream.is_open()) {
        output_stream.close();
    }
}

bool ClientFile::open_for_read() {
    if (input_stream.is_open()) {
        input_stream.close();
    }

    input_stream.open(local_path, std::ios::binary);
    if (!input_stream.is_open()) {
        log_error("Failed to open file for reading: {}", local_path);
        status = FileStatus::FAILED;
        return false;
    }

    status = FileStatus::UPLOADING;
    current_chunk = 0;
    return true;
}

std::vector<char> ClientFile::read_next_chunk() {
    std::vector<char> chunk(CHUNK_SIZE);

    if (!input_stream.is_open() || !has_more_chunks()) {
        return {};
    }

    input_stream.read(chunk.data(), CHUNK_SIZE);
    std::streamsize bytes_read = input_stream.gcount();

    if (bytes_read > 0) {
        chunk.resize(bytes_read);
        log_debug("Read chunk {}/{} ({} bytes) from {}",
                  current_chunk, get_total_chunks(), bytes_read, file_name);
        current_chunk++; // 在返回分片数据后才递增索引
        return chunk;
    }

    return {};
}

bool ClientFile::has_more_chunks() const {
    return current_chunk < get_total_chunks();
}

bool ClientFile::open_for_write() {
    // 确保目录存在
    std::filesystem::path file_path(local_path);
    std::filesystem::create_directories(file_path.parent_path());

    if (output_stream.is_open()) {
        output_stream.close();
    }

    output_stream.open(local_path, std::ios::binary);
    if (!output_stream.is_open()) {
        log_error("Failed to open file for writing: {}", local_path);
        status = FileStatus::FAILED;
        return false;
    }

    status = FileStatus::DOWNLOADING;
    return true;
}

bool ClientFile::write_chunk(const std::vector<char>& data, size_t chunk_index) {
    if (!output_stream.is_open()) {
        log_error("Output stream not open for file: {}", file_name);
        return false;
    }

    // 计算应该写入的位置
    std::streampos pos = static_cast<std::streampos>(chunk_index * CHUNK_SIZE);
    output_stream.seekp(pos);

    output_stream.write(data.data(), data.size());
    if (output_stream.fail()) {
        log_error("Failed to write chunk {} to file: {}", chunk_index, file_name);
        status = FileStatus::FAILED;
        return false;
    }

    log_debug("Wrote chunk {} ({} bytes) to {}", chunk_index, data.size(), file_name);
    return true;
}

bool ClientFile::finalize_download() {
    if (output_stream.is_open()) {
        output_stream.close();
    }

    // 验证文件hash
    std::string actual_hash = calculate_hash(local_path);
    if (actual_hash != file_hash) {
        log_error("Hash mismatch for downloaded file: {} (expected: {}, actual: {})",
                  file_name, file_hash, actual_hash);
        status = FileStatus::FAILED;
        return false;
    }

    status = FileStatus::COMPLETED;
    log_info("Successfully downloaded file: {} -> {}", file_name, local_path);
    return true;
}

double ClientFile::get_progress() const {
    if (get_total_chunks() == 0) return 0.0;
    return static_cast<double>(current_chunk) / get_total_chunks();
}

// ============ ServerFile 实现 ============

ServerFile::ServerFile(const std::string& hash, const std::string& file_id,
                       const std::string& name, size_t size, const std::string& storage)
    : File(name, hash, size) {
    this->file_id = file_id;

    // 设置服务端存储路径
    storage_path = storage;
    storage_path += (storage.back() == '/' ? "" : "/") + file_id;

    // 初始化分片接收状态
    size_t total_chunks = get_total_chunks();
    received_chunks.resize(total_chunks, 0); // 使用0表示未接收, 1表示已接收

    log_info("Created ServerFile: {} (id: {}, chunks: {})", name, file_id, total_chunks);
}

ServerFile::~ServerFile() {
    if (input_stream.is_open()) {
        input_stream.close();
    }
    if (output_stream.is_open()) {
        output_stream.close();
    }
}

bool ServerFile::open_for_write() {
    // 确保存储目录存在
    std::filesystem::path file_path(storage_path);
    std::filesystem::create_directories(file_path.parent_path());

    if (output_stream.is_open()) {
        output_stream.close();
    }

    output_stream.open(storage_path, std::ios::binary);
    if (!output_stream.is_open()) {
        log_error("Failed to open file for writing: {}", storage_path);
        status = FileStatus::FAILED;
        return false;
    }

    status = FileStatus::UPLOADING;
    return true;
}

bool ServerFile::write_chunk(const std::vector<char>& data, size_t chunk_index) {
    if (!output_stream.is_open()) {
        log_error("Output stream not open for server file: {}", file_name);
        return false;
    }

    if (chunk_index >= received_chunks.size()) {
        log_error("Invalid chunk index: {} (max: {})", chunk_index, received_chunks.size());
        return false;
    }

    // 如果这个分片已经接收过了，跳过
    if (received_chunks[chunk_index]) {
        log_debug("Chunk {} already received for file: {}", chunk_index, file_name);
        return true;
    }

    // 计算写入位置
    std::streampos pos = static_cast<std::streampos>(chunk_index * CHUNK_SIZE);
    output_stream.seekp(pos);

    output_stream.write(data.data(), data.size());
    if (output_stream.fail()) {
        log_error("Failed to write chunk {} to server file: {}", chunk_index, file_name);
        status = FileStatus::FAILED;
        return false;
    }

    // 标记分片已接收
    received_chunks[chunk_index] = 1; // 使用1表示已接收
    received_count++;

    log_debug("Received chunk {}/{} for file: {}",
              chunk_index + 1, received_chunks.size(), file_name);
    return true;
}

bool ServerFile::is_complete() const {
    return received_count == received_chunks.size();
}

bool ServerFile::finalize_upload() {
    if (output_stream.is_open()) {
        output_stream.close();
    }

    if (!is_complete()) {
        log_error("File upload incomplete: {}/{} chunks received",
                  received_count, received_chunks.size());
        status = FileStatus::FAILED;
        return false;
    }

    // 验证文件hash
    std::string actual_hash = calculate_hash(storage_path);
    if (actual_hash != file_hash) {
        log_error("Hash mismatch for uploaded file: {} (expected: {}, actual: {})",
                  file_name, file_hash, actual_hash);
        status = FileStatus::FAILED;
        return false;
    }

    status = FileStatus::COMPLETED;
    log_info("Successfully received file: {} (id: {})", file_name, file_id);
    return true;
}

bool ServerFile::open_for_read() {
    if (input_stream.is_open()) {
        input_stream.close();
    }

    input_stream.open(storage_path, std::ios::binary);
    if (!input_stream.is_open()) {
        log_error("Failed to open file for reading: {}", storage_path);
        return false;
    }

    return true;
}

std::vector<char> ServerFile::read_chunk(size_t chunk_index) {
    if (!input_stream.is_open()) {
        log_error("Input stream not open for server file: {}", file_name);
        return {};
    }

    // 计算读取位置和大小
    std::streampos pos = static_cast<std::streampos>(chunk_index * CHUNK_SIZE);
    size_t remaining_size = file_size - (chunk_index * CHUNK_SIZE);
    size_t chunk_size = std::min(CHUNK_SIZE, remaining_size);

    std::vector<char> chunk(chunk_size);

    input_stream.seekg(pos);
    input_stream.read(chunk.data(), chunk_size);

    if (input_stream.gcount() != static_cast<std::streamsize>(chunk_size)) {
        log_error("Failed to read chunk {} from server file: {}", chunk_index, file_name);
        return {};
    }

    log_debug("Read chunk {} ({} bytes) from server file: {}",
              chunk_index, chunk_size, file_name);
    return chunk;
}

double ServerFile::get_receive_progress() const {
    if (received_chunks.empty()) return 0.0;
    return static_cast<double>(received_count) / received_chunks.size();
}

bool ServerFile::is_chunk_received(size_t chunk_index) const {
    return chunk_index < received_chunks.size() && received_chunks[chunk_index] != 0;
}

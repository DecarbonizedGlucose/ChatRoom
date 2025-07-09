#include "../include/file.hpp"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>

File::File(const std::string& local_path)
    : local_path(local_path), file_size(0) {}

File::File(const std::string& file_name, const std::string& local_path)
    : file_name(file_name), local_path(local_path), file_size(0) {}

FilePtr get_fileptr(const std::string& local_path) {
    if (local_path.empty()) {
        return nullptr;
    }
    int fd = open(local_path.c_str(), O_RDONLY);
    if (fd < 0) {
        return nullptr;
    }
    struct stat file_stat;
    off_t file_size = 0;
    if (fstat(fd, &file_stat) == 0) {
        file_size = file_stat.st_size;
    } else {
        close(fd);
        return nullptr;
    }
    close(fd);
    FilePtr file_ptr = std::make_shared<File>(local_path);
    file_ptr->fd = fd;
    file_ptr->file_size = file_size;
    file_ptr->file_name = local_path.substr(local_path.find_last_of('/') + 1);
    if (!file_ptr->calc_hash() || file_ptr->file_hash.empty()) {
        file_ptr.reset();
        return nullptr; // 计算哈希失败
    }
    return file_ptr;
}

FilePtr make_fileptr(const std::string& local_path) {
    if (local_path.empty()) {
        return nullptr;
    }
    int fd = open(local_path.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd < 0) {
        return nullptr;
    }
    FilePtr file_ptr = std::make_shared<File>(local_path);
    file_ptr->fd = fd;
    file_ptr->file_size = 0;
    file_ptr->file_name = local_path.substr(local_path.find_last_of('/') + 1);
    return file_ptr;
}

bool File::calc_hash() {
    std::ifstream _file(local_path, std::ios::binary);
    if (!_file) return false;

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    const size_t buf_size = 4096;
    char buffer[buf_size];
    while (_file.read(buffer, buf_size)) {
        SHA256_Update(&ctx, buffer, _file.gcount());
    }
    SHA256_Update(&ctx, buffer, _file.gcount()); // 处理最后一块

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &ctx);

    std::ostringstream out;
    for (unsigned char byte : hash)
        out << std::hex << std::setw(2) << std::setfill('0') << (int)byte;

    file_hash = out.str(); // 64位
    return true;
}

ChatFilePtr getChatFilePtr(const FilePtr& file) {
    ChatFilePtr chat_file = std::make_shared<ChatFile>();
    chat_file->set_type("file");
    chat_file->set_file_name(file->file_name);
    chat_file->set_file_hash(file->file_hash);
    return chat_file;
}
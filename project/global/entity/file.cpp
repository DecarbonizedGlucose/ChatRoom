#include "../include/file.hpp"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>

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
}

FilePtr make_fileptr(const std::string& local_path) {
    if (local_path.empty()) {
        return nullptr;
    }
    int fd = open(local_path.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd < 0) {
        return nullptr;
    }
    FilePtr ptr = std::make_shared<File>(local_path);
    ptr->fd = fd;
    ptr->file_size = 0;
    ptr->file_name = local_path.substr(local_path.find_last_of('/') + 1);
    return ptr;
}
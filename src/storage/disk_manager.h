//===----------------------------------------------------------------------===//
//
//                         Rucbase
//
// disk_manager.h
//
// Identification: src/storage/disk_manager.h
//
// Copyright (c) 2022, RUC Deke Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <fcntl.h>     // for open/close
#include <sys/stat.h>  // for S_ISREG
#include <unistd.h>    // for open/close

#include <atomic>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

#include "common/config.h"
#include "errors.h"  // for throw Exception

/**
 * @brief DiskManager takes care of the allocation and deallocation of pages within a database. It performs the reading
 * and writing of pages to and from disk, providing a logical file layer within the context of a database management
 * system.
 */
class DiskManager {
   public:
    explicit DiskManager();

    ~DiskManager() = default;

    /**
     * @brief 将buffer中的页面数据写回diskFile中
     */
    void write_page(int fd, page_id_t page_no, const char *offset, int num_bytes);

    /**
     * @brief 读取指定编号的页面部分字节到buffer中
     *
     * @param fd 页面所在文件开启后的文件描述符
     * @param page_no 指定页面编号
     * @param offset 读取的内容写入buffer
     * @param num_bytes 读取的字节数
     */
    void read_page(int fd, page_id_t page_no, char *offset, int num_bytes);

    /**
     * @brief Allocate a page on disk.
     * @return the page_no of the allocated page
     */
    page_id_t AllocatePage(int fd);

    /**
     * @brief Deallocate a page on disk.
     * @param page_id id of the page to deallocate
     */
    void DeallocatePage(page_id_t page_id);

    // 目录操作
    bool is_dir(const std::string &path);

    void create_dir(const std::string &path);

    void destroy_dir(const std::string &path);

    // 文件操作
    bool is_file(const std::string &path);

    void create_file(const std::string &path);

    void destroy_file(const std::string &path);

    int open_file(const std::string &path);

    void close_file(int fd);

    int GetFileSize(const std::string &file_name);

    std::string GetFileName(int fd);

    int GetFileFd(const std::string &file_name);

    // LOG操作
    bool ReadLog(char *log_data, int size, int offset, int prev_log_end);

    void WriteLog(char *log_data, int size);

    void SetLogFd(int log_fd) { log_fd_ = log_fd; }

    int GetLogFd() { return log_fd_; }

    // 在fd对应文件中，从start_page_no开始分配page_no
    void set_fd2pageno(int fd, int start_page_no) { fd2pageno_[fd] = start_page_no; }

    page_id_t get_fd2pageno(int fd) { return fd2pageno_[fd]; }

    static constexpr int MAX_FD = 8192;

   private:
    // 文件打开列表，用于记录文件是否被打开
    std::unordered_map<std::string, int> path2fd_;  //<Page文件磁盘路径,Page fd>哈希表
    std::unordered_map<int, std::string> fd2path_;  //<Page fd,Page文件磁盘路径>哈希表

    int log_fd_ = -1;                             // log file
    std::atomic<page_id_t> fd2pageno_[MAX_FD]{};  // 在文件fd中分配的page no个数
};

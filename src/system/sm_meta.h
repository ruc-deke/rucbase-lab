#pragma once

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "errors.h"
#include "sm_defs.h"

struct ColMeta {
    std::string tab_name;  // 字段所属表名称
    std::string name;      // 字段名称
    ColType type;          // 字段类型
    int len;               // 字段长度
    int offset;            // 字段位于记录中的偏移量
    bool index;            // 该字段上是否建立索引

    friend std::ostream &operator<<(std::ostream &os, const ColMeta &col) {
        // ColMeta中有各个基本类型的变量，然后调用重载的这些变量的操作符<<（具体实现逻辑在defs.h）
        return os << col.tab_name << ' ' << col.name << ' ' << col.type << ' ' << col.len << ' ' << col.offset << ' '
                  << col.index;
    }

    friend std::istream &operator>>(std::istream &is, ColMeta &col) {
        return is >> col.tab_name >> col.name >> col.type >> col.len >> col.offset >> col.index;
    }
};

struct TabMeta {
    std::string name;
    std::vector<ColMeta> cols;

    /**
     * @brief 根据列名在本表元数据结构体中查找是否有该名字的列
     *
     * @param col_name 目标列名
     * @return true
     * @return false
     */
    bool is_col(const std::string &col_name) const {
        // lab3 task1 Todo
        return false;
        // lab3 task1 Todo End
    }
    /**
     * @brief 根据列名获得列元数据ColMeta
     *
     * @param col_name 目标列名
     * @return std::vector<ColMeta>::iterator
     */
    std::vector<ColMeta>::iterator get_col(const std::string &col_name) {
        // lab3 task1 Todo
        std::vector<ColMeta>::iterator it;
        return it;
        // lab3 task1 Todo End
    }

    friend std::ostream &operator<<(std::ostream &os, const TabMeta &tab) {
        os << tab.name << '\n' << tab.cols.size() << '\n';
        for (auto &col : tab.cols) {
            os << col << '\n';  // col是ColMeta类型，然后调用重载的ColMeta的操作符<<
        }
        return os;
    }

    friend std::istream &operator>>(std::istream &is, TabMeta &tab) {
        size_t n;
        is >> tab.name >> n;
        for (size_t i = 0; i < n; i++) {
            ColMeta col;
            is >> col;
            tab.cols.push_back(col);
        }
        return is;
    }
};

// 注意重载了操作符 << 和 >>，这需要更底层同样重载TabMeta、ColMeta的操作符 << 和 >>
class DbMeta {
    friend class SmManager;

   private:
    std::string name_;                     // 数据库名称
    std::map<std::string, TabMeta> tabs_;  // 数据库内的表名称和元数据的映射

   public:
    // DbMeta(std::string name) : name_(name) {}

    bool is_table(const std::string &tab_name) const { return tabs_.find(tab_name) != tabs_.end(); }

    TabMeta &get_table(const std::string &tab_name) {
        auto pos = tabs_.find(tab_name);
        if (pos == tabs_.end()) {
            throw TableNotFoundError(tab_name);
        }
        return pos->second;
    }

    // 重载操作符 <<
    friend std::ostream &operator<<(std::ostream &os, const DbMeta &db_meta) {
        os << db_meta.name_ << '\n' << db_meta.tabs_.size() << '\n';
        for (auto &entry : db_meta.tabs_) {
            os << entry.second << '\n';  // entry.second是TabMeta类型，然后调用重载的TabMeta的操作符<<
        }
        return os;
    }

    friend std::istream &operator>>(std::istream &is, DbMeta &db_meta) {
        size_t n;
        is >> db_meta.name_ >> n;
        for (size_t i = 0; i < n; i++) {
            TabMeta tab;
            is >> tab;
            db_meta.tabs_[tab.name] = tab;
        }
        return is;
    }
};

#pragma once

#include <cassert>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include "common/context.h"

class RecordPrinter {
    static constexpr size_t COL_WIDTH = 16;
    size_t num_cols;
public:
    RecordPrinter(size_t num_cols_) : num_cols(num_cols_) {
        assert(num_cols_ > 0);
    }

    void print_separator(Context *context) const {
        for (size_t i = 0; i < num_cols; i++) {
            // std::cout << '+' << std::string(COL_WIDTH + 2, '-');
            std::string str = "+" + std::string(COL_WIDTH + 2, '-');
            memcpy(context->data_send_ + *(context->offset_), str.c_str(), str.length());
            *(context->offset_) = *(context->offset_) + str.length();
        }
        std::string str = "+\n";
        memcpy(context->data_send_ + *(context->offset_), str.c_str(), str.length());
        *(context->offset_) = *(context->offset_) + str.length();
        // std::cout << "+\n";
    }

    void print_record(const std::vector<std::string> &rec_str, Context *context) const {
        assert(rec_str.size() == num_cols);
        for (auto col: rec_str) {
            if (col.size() > COL_WIDTH) {
                col = col.substr(0, COL_WIDTH - 3) + "...";
            }
            // std::cout << "| " << std::setw(COL_WIDTH) << col << ' ';
            std::stringstream ss;
            ss << "| " << std::setw(COL_WIDTH) << col << " ";
            memcpy(context->data_send_ + *(context->offset_), ss.str().c_str(), ss.str().length());
            *(context->offset_) = *(context->offset_) + ss.str().length();
        }
        // std::cout << "|\n";
        std::string str = "|\n";
        memcpy(context->data_send_ + *(context->offset_), str.c_str(), str.length());
        *(context->offset_) = *(context->offset_) + str.length();
    }

    static void print_record_count(size_t num_rec, Context *context) {
        // std::cout << "Total record(s): " << num_rec << '\n';
        std::string str = "Total record(s): " + std::to_string(num_rec) + '\n';
        memcpy(context->data_send_ + *(context->offset_), str.c_str(), str.length());
        *(context->offset_) = *(context->offset_) + str.length();
    }
};

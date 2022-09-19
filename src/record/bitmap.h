#pragma once

#include <cinttypes>
#include <cstring>

static constexpr int BITMAP_WIDTH = 8;
static constexpr unsigned BITMAP_HIGHEST_BIT = 0x80u;  // 128 (2^7)

class Bitmap {
   public:
    // 从地址bm开始的size个字节全部置0
    static void init(char *bm, int size) { memset(bm, 0, size); }

    // pos位 置1
    static void set(char *bm, int pos) { bm[get_bucket(pos)] |= get_bit(pos); }

    // pos位 置0
    static void reset(char *bm, int pos) { bm[get_bucket(pos)] &= static_cast<char>(~get_bit(pos)); }

    // 如果pos位是1，则返回true
    static bool is_set(const char *bm, int pos) { return (bm[get_bucket(pos)] & get_bit(pos)) != 0; }

    /**
     * @brief 找下一个为0 or 1的位
     * @param bit false表示要找下一个为0的位，true表示要找下一个为1的位
     * @param bm 要找的起始地址为bm
     * @param max_n 要找的从起始地址开始的偏移为[curr+1,max_n)
     * @param curr 要找的从起始地址开始的偏移为[curr+1,max_n)
     * @return 找到了就返回偏移位置，没找到就返回max_n
     */
    static int next_bit(bool bit, const char *bm, int max_n, int curr) {
        for (int i = curr + 1; i < max_n; i++) {
            if (is_set(bm, i) == bit) {
                return i;
            }
        }
        return max_n;
    }

    // 找第一个为0 or 1的位
    static int first_bit(bool bit, const char *bm, int max_n) { return next_bit(bit, bm, max_n, -1); }

    // for example:
    // rid_.slot_no = Bitmap::next_bit(true, page_handle.bitmap, file_handle_->file_hdr_.num_records_per_page,
    // rid_.slot_no); int slot_no = Bitmap::first_bit(false, page_handle.bitmap, file_hdr_.num_records_per_page);

   private:
    static int get_bucket(int pos) { return pos / BITMAP_WIDTH; }

    static char get_bit(int pos) { return BITMAP_HIGHEST_BIT >> static_cast<char>(pos % BITMAP_WIDTH); }
};

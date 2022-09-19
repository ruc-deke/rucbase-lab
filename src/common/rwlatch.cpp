#include "rwlatch.h"

/**
 * Acquire a write latch.
 */
void ReaderWriterLatch::WLock() {
  std::unique_lock<mutex_t> latch(mutex_);
  while (writer_entered_) {
    reader_.wait(latch);
  }
  writer_entered_ = true;
  while (reader_count_ > 0) {
    writer_.wait(latch);
  }
}

/**
 * Release a write latch.
 */
void ReaderWriterLatch::WUnlock() {
  std::lock_guard<mutex_t> guard(mutex_);
  writer_entered_ = false;
  reader_.notify_all();
}

/**
 * Acquire a read latch.
 */
void ReaderWriterLatch::RLock() {
  std::unique_lock<mutex_t> latch(mutex_);
  while (writer_entered_ || reader_count_ == MAX_READERS) {
    reader_.wait(latch);
  }
  reader_count_++;
}

/**
 * Release a read latch.
 */
void ReaderWriterLatch::RUnlock() {
  std::lock_guard<mutex_t> guard(mutex_);
  reader_count_--;
  if (writer_entered_) {
    if (reader_count_ == 0) {
      writer_.notify_one();
    }
  } else {
    if (reader_count_ == MAX_READERS - 1) {
      reader_.notify_one();
    }
  }
}
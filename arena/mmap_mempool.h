#ifndef BASE_MMAP_MEMPOOL_H_
#define BASE_MMAP_MEMPOOL_H_

#include "arena/mempool.h"
#include <stdint.h>
#include <string>

namespace base {

enum MMapOpenMode {
  MFILE_MODE_IGNORE = 0,
  MFILE_MODE_READ,
  MFILE_MODE_WRITE,
  MFILE_MODE_WRITE_NODUMP
};

struct MMapFileHeader {
  int64_t max_size;
  int64_t used_size;
};

class MMapMempool : public Mempool {
 public:
  MMapMempool();

  ~MMapMempool();

  virtual int32_t init(const char* file_name, uint32_t mode);

  virtual void close();

  virtual int32_t dump();

  virtual int32_t reset();

  virtual int64_t alloc(const int64_t& size);

  virtual inline char* getAddress(const int64_t& offset);

  virtual char* getAddress(const int64_t& offset, const int64_t& length);

  virtual char* getAddressSafe(const int64_t& offset);

  virtual inline char* getBase();

  virtual inline int64_t getUsedSize();

  virtual void setExpandSize(const int64_t& size) {
    expand_size_ = size;
  }

 protected:
  virtual int32_t loadFile();

  virtual int32_t createFile();

  virtual int32_t expand(const int64_t& size);

 public:
  static const int64_t _NULL;

 protected:
  int32_t fd_;
  int32_t fd_header_;
  char* file_;
  MMapFileHeader* header_file_;
  char* base_;
  bool read_only_;
  int64_t expand_size_;

  static const int64_t kMmapSize_;
  static const int64_t kMaxMempoolSize_;
};

inline char* MMapMempool::getAddress(const int64_t& offset) {
  return base_ + offset;
}

inline char* MMapMempool::getBase() {
  return base_;
}

inline int64_t MMapMempool::getUsedSize() {
  return header_file_->used_size;
}

inline void MMapMempool::close() {
  return;
}

}  // namespace base

#endif  // BASE_MMAP_MEMPOOL_H_

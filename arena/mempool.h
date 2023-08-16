
#ifndef BASE_MEMPOOL_H_
#define BASE_MEMPOOL_H_

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

namespace base {

class Mempool {
 public:
  Mempool() {}

  virtual ~Mempool() {}

  virtual void close() = 0;

  virtual int32_t dump() = 0;

  virtual int32_t reset() = 0;

  virtual int64_t alloc(const int64_t& size) = 0;

  virtual char* getAddress(const int64_t& offset) = 0;

  virtual char* getAddress(const int64_t& offset, const int64_t& length) = 0;

  virtual char* getAddressSafe(const int64_t& offset) = 0;

  virtual char* getBase() = 0;

  virtual int64_t getUsedSize() = 0;

  virtual void setExpandSize(const int64_t&) {
    return;
  }

  const char* getFileName() {
      return file_name_;
  }
  const char* getHeaderFileName() {
      return header_file_name_;
  }

  virtual int32_t init(const char* file_name, uint32_t mode = 0) {
    if (!file_name) {
      return -1;
    }
    int32_t ret = snprintf(file_name_, PATH_MAX, "%s", file_name);
    if (ret >= PATH_MAX) {
      return -1;
    }
    ret = snprintf(header_file_name_, PATH_MAX, "%s.header", file_name);
    if (ret >= PATH_MAX) {
      return -1;
    }
    return 0;
  }

 protected:
  char file_name_[PATH_MAX] = {0};
  char header_file_name_[PATH_MAX] = {0};
};

}  // namespace base

#endif  // BASE_MEMPOOL_H_

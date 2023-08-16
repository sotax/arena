
#ifndef BASE_ARENA_H_
#define BASE_ARENA_H_

#include <stdlib.h>
#include <stdint.h>
#include "arena/delay_queue.h"
#include "arena/mempool.h"

namespace base {

class Arena {
 public:
  Arena();
  ~Arena();

  int32_t init(Mempool* pool,
           uint32_t minMemSize = 32,
           uint32_t maxMemSize = 1U << 31,
           float rate = 1.05,
           uint32_t delayTime = 10);

  int32_t dump();

  void close();

  int64_t alloc(uint32_t size);

  int64_t realloc(int64_t key, uint32_t new_size);

  int32_t free(int64_t key);

  inline uint32_t getSize(int64_t key);

  inline char* getAddress(int64_t key);

  int32_t reset();

  Mempool* getMempool() {
    return pool_;
  }

  int64_t append(Arena* pSrc);

  int64_t getDataSize();

  int64_t getHeaderSize();


  void set_expand_factor(double expand_factor) {
    expand_factor_ = expand_factor;
  }

  // keep 64-bit for user define.
  uint64_t* GetUserDefine();
  bool SetUserDefine(const uint64_t* user_define);

 private:
  int32_t create(uint32_t minMemSize, uint32_t maxMemSize, float rate,
    uint32_t delayTime);

  int32_t load();

  uint32_t getLevel(uint32_t &size);

  void freeDelayQueue();

  void expandDelayQueue();

 private:
  Mempool* pool_;

  uint32_t min_mem_size_;
  uint32_t max_mem_size_;
  float rate_;
  uint32_t level_;
  int64_t free_list_offset_;

  uint32_t delay_time_;
  int64_t delay_queue_offset_;

  int64_t header_size_;

  bool use_free_list_;
  double expand_factor_;

  int64_t user_define_offset_;  // offset, keep 64-bit for user define.
};

uint32_t Arena::getSize(int64_t key) {
  return *(reinterpret_cast<uint32_t*>((pool_->getAddress(key))));
}

inline char* Arena::getAddress(const int64_t key) {
  uint32_t* pLength = reinterpret_cast<uint32_t*>
    (pool_->getAddress(key, sizeof(uint32_t)));
  if (!pLength) {
    return NULL;
  }
  return pool_->getAddress(key + sizeof(uint32_t), *pLength);
}

}  // namespace base

#endif  // BASE_ARENA_H_

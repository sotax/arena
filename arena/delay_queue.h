
#include "arena/mempool.h"

#ifndef BASE_DELAY_QUEUE_H_
#define BASE_DELAY_QUEUE_H_

namespace base {

struct DelayNode {
  int64_t key;
  uint32_t level;
  int64_t time;
};

class DelayQueue {
 public:
  DelayQueue(uint32_t size, int64_t array_offset) {
    size_ = size > 2 ? size : 2;
    used_ = 0;
    front_ = 0;
    rear_ = 1;
    array_offset_ = array_offset;
  }

  ~DelayQueue() {}

  int32_t push(const DelayNode &node, Mempool* pool) {
    DelayNode* pArray =
      reinterpret_cast<DelayNode*>(pool->getAddress(array_offset_));
    if (!full()) {
      pArray[rear_] = node;
      rear_ = (rear_ + 1) % size_;
      used_++;
      return 0;
    }
    return -1;
  }

  int32_t pop() {
    if (!empty()) {
      front_ = (front_ + 1) % size_;
      used_--;
      return 0;
    }
    return -1;
  }

  DelayNode* front(Mempool* pool) {
    if (!empty()) {
      DelayNode* pArray =
        reinterpret_cast<DelayNode*>(pool->getAddress(array_offset_));
      return pArray + ((front_ + 1) % size_);
    }
    return NULL;
  }

  bool empty() {
    return used_ == 0;
  }

  bool full() {
    return used_ == size_;
  }

  void setArrayOffset(int64_t array_offset) {
    array_offset_ = array_offset;
  }

  uint32_t size() {
    return size_;
  }

  uint32_t usedSize() {
    return used_;
  }

 private:
  uint32_t front_;
  uint32_t rear_;
  uint32_t used_;
  uint32_t size_;
  int64_t array_offset_;
};

}  // namespace base

#endif  // BASE_DELAY_QUEUE_H_

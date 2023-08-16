#include <time.h>
#include <string.h>
#include <iostream>

#include "arena/arena.h"

namespace base {

const uint32_t kDefaultDelayQueueSize = 100000;
bool use_delay_queue = true;

Arena::Arena()
    : pool_(NULL),
      min_mem_size_(0),
      max_mem_size_(0),
      rate_(0.0),
      level_(0),
      free_list_offset_(0),
      delay_time_(0),
      delay_queue_offset_(0),
      header_size_(0),
      use_free_list_(false),
      expand_factor_(2.0) {
}

Arena::~Arena() {
}

void Arena::close() {
    pool_->close();
}

int32_t Arena::init(Mempool* pool,
                    uint32_t    minMemSize,
                    uint32_t    maxMemSize,
                    float       rate,
                    uint32_t    delayTime) {
    if (!pool) {
        return -1;
    }
    pool_ = pool;

    if (rate <= 1.0) {
        rate = 1.05;
    }

    if (minMemSize < 16) {  // 安全分配下限
        minMemSize = 16;
    }

    if (pool_->getUsedSize() == 0) {
        return create(minMemSize, maxMemSize, rate, delayTime);
    } else {
        return load();
    }
}

int32_t Arena::dump() {
    return pool_->dump();
}

int64_t Arena::alloc(uint32_t size) {
    if (size == 0 || size > max_mem_size_) {
        return -1;
    }

    int64_t key      = -1;
    uint32_t level    = 0;
    uint32_t realSize = size;

    level = getLevel(realSize);
    int64_t* freeList = NULL;
    if (use_free_list_) {
        freeList = (int64_t*)pool_->getAddress(free_list_offset_);
    }

/*
    if (freeList != NULL && freeList[level] != -1) {
        key = freeList[level];
        freeList[level] = *(int64_t*)getAddress(key);
    } else {
        key = pool_->alloc(realSize + sizeof(uint32_t));
        if (key == -1) {
            return -1;
        }
        *(uint32_t*)(pool_->getAddress(key)) = realSize;
    }
*/
    uint32_t realSize_end = (uint32_t)(realSize * expand_factor_);
    if (realSize_end > max_mem_size_) {
        realSize_end = max_mem_size_;
    }
    uint32_t level_end = getLevel(realSize_end);
    if (freeList != NULL) {
        for (uint32_t i = level; i <= level_end; i++) {
            if (freeList[i] != -1 && getAddress(freeList[i]) != NULL) {
                key = freeList[i];
                freeList[i] = *(int64_t*)getAddress(key);
                break;
            }
        }
    }
    if (key == -1) {
        key = pool_->alloc(realSize + sizeof(uint32_t));
        if (key == -1) {
            return -1;
        }
        *(uint32_t*)(pool_->getAddress(key)) = realSize;
    }
    return key;
}

int64_t Arena::realloc(int64_t key, uint32_t new_size) {
    if (key == -1) {
        return -1;
    }

    uint32_t size   = getSize(key);
    if (new_size <= size) {
        return -1;
    }
    int64_t new_key = alloc(new_size);
    if (new_key == -1) {
        return -1;
    }
    memcpy(getAddress(new_key), getAddress(key), size);

    freeDelayQueue();

    DelayNode node = {key, getLevel(size), time(NULL)};
    DelayQueue *delayQueue =
      (DelayQueue*) pool_->getAddress(delay_queue_offset_);
    if (!delayQueue->full()) {
        delayQueue->push(node, pool_);
    } else {
        expandDelayQueue();
        delayQueue = (DelayQueue*) pool_->getAddress(delay_queue_offset_);
        delayQueue->push(node, pool_);
    }

    return new_key;
}

int32_t Arena::free(int64_t key) {
    if (key == -1) {
        return -1;
    }
    uint32_t size   = getSize(key);
    if (use_delay_queue) {
        freeDelayQueue();
        DelayNode node = {key, getLevel(size), time(NULL)};
        DelayQueue *delayQueue =
          (DelayQueue*) pool_->getAddress(delay_queue_offset_);
        if (!delayQueue->full()) {
            delayQueue->push(node, pool_);
        } else {
            expandDelayQueue();
            delayQueue = (DelayQueue*) pool_->getAddress(delay_queue_offset_);
            delayQueue->push(node, pool_);
        }
    } else {  // Safe update mode, not use delay queue
        uint32_t level = getLevel(size);
        int64_t* freeList = reinterpret_cast<int64_t*>(
            pool_->getAddress(free_list_offset_));
        int64_t* next_key = reinterpret_cast<int64_t*>(getAddress(key));
        *next_key = freeList[level];
        freeList[level] = key;
    }

    use_free_list_ = true;
    return 0;
}

int Arena::create(uint32_t minMemSize,
                                   uint32_t maxMemSize,
                                   float    rate,
                                   uint32_t delayTime) {
    do {
        pool_->reset();

        int64_t key = -1;
        char* pData  = NULL;

        key = pool_->alloc(sizeof(min_mem_size_));
        if (key == -1) {
            break;
        }
        pData = pool_->getAddress(key);
        *(uint32_t*)pData = minMemSize;
        min_mem_size_ = minMemSize;

        key = pool_->alloc(sizeof(max_mem_size_));
        if (key == -1) {
            break;
        }
        pData = pool_->getAddress(key);
        *(uint32_t*)pData = maxMemSize;
        max_mem_size_ = maxMemSize;

        key = pool_->alloc(sizeof(rate_));
        if (key == -1) {
            break;
        }
        pData = pool_->getAddress(key);
        *(float*)pData = rate;
        rate_ = rate;

        key = pool_->alloc(sizeof(level_));
        if (key == -1) {
            break;
        }
        pData = pool_->getAddress(key);
        uint32_t level = getLevel(max_mem_size_) + 1;
        *(uint32_t*)pData = level;
        level_ = level;

        key = pool_->alloc(sizeof(int64_t) * level_);
        if (key == -1) {
            break;
        }
        free_list_offset_ = key;
        int64_t *freeList = reinterpret_cast<int64_t*>(pool_->getAddress(key));
        for (uint32_t i = 0; i < level_; i++) {
            freeList[i] = -1;
        }

        key = pool_->alloc(sizeof(delay_time_));
        if (key == -1) {
            break;
        }
        pData = pool_->getAddress(key);
        *(uint32_t*)pData = delayTime;
        delay_time_ = delayTime;

        key = pool_->alloc(sizeof(DelayQueue));
        if (key == -1) {
            break;
        }
        delay_queue_offset_ = key;
        DelayQueue *delayQueue = reinterpret_cast<DelayQueue*>
          (pool_->getAddress(key));

        key = pool_->alloc(sizeof(DelayNode) * kDefaultDelayQueueSize);
        if (key == -1) {
            break;
        }
        delayQueue = new (delayQueue) DelayQueue(kDefaultDelayQueueSize, key);

        // user_define, kept for user extension.
        user_define_offset_ = pool_->alloc(sizeof(uint64_t));
        if (user_define_offset_ == -1) {
            break;
        }
        // header_size
        header_size_ = pool_->getUsedSize();

        return 0;
    } while (0);

    pool_->reset();
    min_mem_size_ = 0;
    max_mem_size_ = 0;
    rate_       = 0.0;
    level_      = 0;
    free_list_offset_   = 0;
    delay_time_  = 0;
    delay_queue_offset_ = 0;
    user_define_offset_ = 0;

    return -1;
}

uint64_t* Arena::GetUserDefine() {
    char* data = 
        pool_->getAddress(user_define_offset_, sizeof(uint64_t));
    return (uint64_t*)data;
}

bool Arena::SetUserDefine(const uint64_t* user_define) {
    if (user_define == NULL) {
        return false;
    }
    char* data = 
        pool_->getAddress(user_define_offset_, sizeof(uint64_t));
    if (data == NULL) {
        return false;
    }
    memcpy(data, user_define, sizeof(uint64_t));
    return true;
}

int32_t Arena::load() {
    char *pBase = pool_->getBase();
    char *pData = pBase;

    min_mem_size_ = *(reinterpret_cast<uint32_t*>(pData));
    pData += sizeof(min_mem_size_);

    max_mem_size_ = *(reinterpret_cast<uint32_t*>(pData));
    pData += sizeof(max_mem_size_);

    rate_ = *(reinterpret_cast<float*>(pData));
    pData += sizeof(rate_);

    level_ = *(reinterpret_cast<uint32_t*>(pData));
    pData += sizeof(level_);

//    _freeList = (int64_t*)pData;
    free_list_offset_ = pData - pool_->getBase();
    pData += level_ * sizeof(int64_t);

    delay_time_ = *(reinterpret_cast<uint32_t*>(pData));
    pData += sizeof(delay_time_);

//  _delayQueue = (DelayQueue*)pData;
    delay_queue_offset_ = pData - pool_->getBase();
    pData += sizeof(DelayQueue);
    pData += sizeof(DelayNode) * kDefaultDelayQueueSize;

    // user_define, kept for user extension.
    user_define_offset_ = pData - pool_->getBase();
    pData += sizeof(uint64_t);

    header_size_ = pData - pBase;
    use_free_list_ = true;
    return 0;
}

uint32_t Arena::getLevel(uint32_t& size) {
    uint32_t level    = 0;
    uint32_t realSize = min_mem_size_;
    while (size > realSize) {
        uint32_t t = static_cast<uint32_t>(realSize * rate_);
        if (t <= realSize) {
            t = realSize + 1;
        }
        realSize = t;
        level++;
    }
    size = realSize;
    return level;
}

void Arena::freeDelayQueue() {
    use_free_list_ = true;
    uint32_t nowTime = time(NULL);
    int64_t *freeList = reinterpret_cast<int64_t*>
      (pool_->getAddress(free_list_offset_));

    DelayQueue *delayQueue = reinterpret_cast<DelayQueue*>
      (pool_->getAddress(delay_queue_offset_));
    while (!delayQueue->empty()) {
        DelayNode *pNode = delayQueue->front(pool_);
        if (pNode->time + delay_time_ < nowTime) {
            int64_t* pNextKey = reinterpret_cast<int64_t*>
              (getAddress(pNode->key));
            *pNextKey = freeList[pNode->level];
            freeList[pNode->level] = pNode->key;

            delayQueue->pop();
        } else {
            return;
        }
    }
}

void Arena::expandDelayQueue() {
    DelayQueue* delayQueue = reinterpret_cast<DelayQueue*>
      (pool_->getAddress(delay_queue_offset_));

    uint32_t newSize   = delayQueue->size() + kDefaultDelayQueueSize;
    int64_t key       = pool_->alloc(newSize * sizeof(DelayNode));
    DelayQueue newQueue(newSize, key);

    delayQueue = reinterpret_cast<DelayQueue*>
      (pool_->getAddress(delay_queue_offset_));
    while (!delayQueue->empty()) {
       newQueue.push(*(delayQueue->front(pool_)), pool_);
       delayQueue->pop();
    }

    memcpy(delayQueue, &newQueue, sizeof(DelayQueue));
}

int32_t Arena::reset() {
    return create(min_mem_size_, max_mem_size_, rate_, delay_time_);
}

int64_t Arena::getDataSize() {
    return pool_->getUsedSize() - header_size_;
}

int64_t Arena::getHeaderSize() {
    return header_size_;
}

int64_t Arena::append(Arena *pSrc) {
    int64_t nDataSize = pSrc->getDataSize();
    int64_t nHeaderSize = pSrc->getHeaderSize();

    const int64_t blockSize = (1024*1024);

    int64_t copySize = 0;
    int64_t offset = nHeaderSize;
    int64_t remain = 0;
    int64_t dstKey = 0;
    char *pSrcBuf = NULL;
    char *pDstBuf = NULL;
    while (1) {
        if (offset >= nHeaderSize + nDataSize) {
            break;
        }
        remain = (nHeaderSize + nDataSize) - offset;
        copySize = (remain > blockSize) ? blockSize : remain;
        pSrcBuf = pSrc->pool_->getAddress(offset, copySize);
        if (!pSrcBuf) {
            return -1;
        }
        dstKey = pool_->alloc(copySize);
        if (dstKey == -1) {
            return -1;
        }
        pDstBuf = pool_->getAddress(dstKey);
        if (!pDstBuf) {
            return -1;
        }
        memcpy(pDstBuf, pSrcBuf, copySize);
        offset += copySize;
    }
    return nDataSize;
}

}  // namespace base

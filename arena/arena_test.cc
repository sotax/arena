
#define private public
#define protected public

#include <time.h>
#include <gtest/gtest.h>

#include "arena/mmap_mempool.h"
#include "arena/mempool.h"
#include "arena/arena.h"

using namespace base;

const uint32_t kDefaultDelayQueueSize = 100000;

class ArenaTest : public testing::Test {
 public:
  Arena* pool_;

  virtual void SetUp() {
    pool_ = new Arena();
    char name[128] = "testArena.mmap";
    MMapMempool* pool64 = new MMapMempool;
    pool64->init(name, MFILE_MODE_READ);
    pool64->reset();
    pool_->init((MempoolInterface*)pool64);
  }
  virtual void TearDown() {
    delete pool_->pool_;
    delete pool_;
    pool_ = NULL;
  }
};

class ArenaTest2 : public testing::Test {
public:
  Arena *poolSrc_, *poolDst_;

  virtual void SetUp() {
    poolSrc_ = new Arena();
    poolDst_ = new Arena();
    char name[128] = "testArena.mmap";
    if (g_use_mmap) {
      MMapMempool *pool64 = new MMapMempool;
      pool64->init(name, MFILE_MODE_READ);
      pool64->reset();
      poolSrc_->init((MempoolInterface*)pool64);

      MMapMempool *pool64Dst = new MMapMempool;
      pool64Dst->init(name, MFILE_MODE_READ);
      pool64Dst->reset();
      poolDst_->init((MempoolInterface*)pool64Dst);
    }
    else {
      FixFileMempool *pool64 = new FixFileMempool();
      pool64->init(name, MFILE_MODE_WRITE);
      pool64->reset();
      poolSrc_->init((MempoolInterface*)pool64);
      FixFileMempool *pool64Dst = new FixFileMempool();
      pool64Dst->init(name, MFILE_MODE_WRITE);
      pool64Dst->reset();
      poolDst_->init((MempoolInterface*)pool64Dst);
    }
  }
  virtual void TearDown() {
    delete poolSrc_->pool_;
    delete poolSrc_;
    poolSrc_ = NULL;
    delete poolDst_->pool_;
    delete poolDst_;
    poolDst_ = NULL;
  }
};

TEST_F(ArenaTest, init) {
  Arena* pool = NULL;
  pool = new Arena();
  char name[128] = "testArena.mmap";
  MMapMempool *pool64 = new MMapMempool;
  pool64->init(name, MFILE_MODE_WRITE);
  pool64->reset();
  int ret = pool->init((MempoolInterface*)pool64);
  EXPECT_EQ(ret, 0);
  pool->pool_->alloc(1);
  MMapMempool *pool64_1 = new MMapMempool;

  pool64_1->init(name, MFILE_MODE_WRITE);
  ret = pool->init(pool64_1);
  EXPECT_EQ(ret, 0);
  pool->close();
  delete pool64;
  delete pool64_1;
  delete pool;
}

TEST_F(ArenaTest, create) {
  int ret = pool_->create(10, 20, 2.0, 10);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(pool_->min_mem_size_, static_cast<uint32_t>(10));
  char *base = pool_->pool_->getBase();
  uint32_t min = *(uint32_t*)base;
  EXPECT_EQ(min, static_cast<uint32_t>(10));
  EXPECT_EQ(pool_->max_mem_size_, static_cast<uint32_t>(20));
  uint32_t max = *((uint32_t*)base+1);
  EXPECT_EQ(max, static_cast<uint32_t>(20));
  EXPECT_EQ(pool_->rate_, 2.0);
  float rate = *(float*)(base + 2 * sizeof(uint32_t));
  EXPECT_EQ(rate, 2.0);
  uint32_t level = pool_->getLevel(pool_->max_mem_size_) + 1;
  uint32_t level_real = *(uint32_t*)(base + 2 * sizeof(uint32_t) +
    sizeof(float));
  EXPECT_EQ(level_real, level);
  EXPECT_EQ(pool_->level_, level);
  EXPECT_EQ(pool_->delay_time_, 10U);
  uint32_t delayTime = *(uint32_t*)(base + 2 * sizeof(uint32_t) +
    sizeof(float) + sizeof(uint32_t) + sizeof(uint64_t) * pool_->level_);
  EXPECT_EQ(delayTime, static_cast<uint32_t>(10));
}

TEST_F(ArenaTest, load) {
  pool_->create(10, 20, 2.0, 10);
  int ret = pool_->load();
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(pool_->min_mem_size_, static_cast<uint32_t>(10));
  EXPECT_EQ(pool_->max_mem_size_, static_cast<uint32_t>(20));
  EXPECT_EQ(pool_->delay_time_, static_cast<uint32_t>(10));
  EXPECT_EQ(pool_->rate_, 2.0);
  pool_->dump();
}

TEST_F(ArenaTest, getLevel) {
  pool_->rate_ = 2.0;
  pool_->min_mem_size_ = 10;
  uint32_t size = 1;
  uint32_t l = pool_->getLevel(size);
  EXPECT_EQ(l, 0u);
  size = 20;
  l = pool_->getLevel(size);
  EXPECT_EQ(l, 1u);
  size = 30;
  l = pool_->getLevel(size);
  EXPECT_EQ(l, static_cast<uint32_t>(2));
  EXPECT_EQ(size, static_cast<uint32_t>(40));
}

TEST_F(ArenaTest, freeDelayQueue) {
  DelayNode node;
  node.key = 1;
  node.level = 0;
  uint32_t nowTime = time(NULL);
  node.time = nowTime + 100;
  DelayQueue *delayQueue = NULL;
  delayQueue = (DelayQueue*)pool_->pool_->getAddress(pool_->delay_queue_offset_);
  delayQueue->push(node, pool_->pool_);
  pool_->freeDelayQueue();
  delayQueue = (DelayQueue*)pool_->pool_->getAddress(pool_->delay_queue_offset_);
  DelayNode *node1 = delayQueue->front(pool_->pool_);
  EXPECT_EQ(node1->key, node.key);
  EXPECT_EQ(node1->time, node.time);
  delayQueue = (DelayQueue*)pool_->pool_->getAddress(pool_->delay_queue_offset_);
  delayQueue->pop();
  node.time = nowTime - 100;
  delayQueue = (DelayQueue*)pool_->pool_->getAddress(pool_->delay_queue_offset_);
  delayQueue->push(node, pool_->pool_);
  node.key = 2;
  node.time = nowTime + 100;
  delayQueue = (DelayQueue*)pool_->pool_->getAddress(pool_->delay_queue_offset_);
  delayQueue->push(node, pool_->pool_);
  pool_->freeDelayQueue();
  delayQueue = (DelayQueue*)pool_->pool_->getAddress(pool_->delay_queue_offset_);
  node1 = delayQueue->front(pool_->pool_);
  EXPECT_EQ(node1->key, node.key);
  EXPECT_EQ(node1->time, node.time);
}

TEST_F(ArenaTest, expandDelayQueue) {
  DelayNode node;
  node.key = 1;
  node.level = 0;
  uint32_t nowTime = time(NULL);
  node.time = nowTime;
  DelayQueue *delayQueue = NULL;
  delayQueue = (DelayQueue*)pool_->pool_->getAddress(pool_->delay_queue_offset_);
  delayQueue->push(node, pool_->pool_);
  delayQueue = (DelayQueue*)pool_->pool_->getAddress(pool_->delay_queue_offset_);
  uint32_t oldSize = delayQueue->size();
  pool_->expandDelayQueue();
  delayQueue = (DelayQueue*)pool_->pool_->getAddress(pool_->delay_queue_offset_);
  uint32_t newSize = delayQueue->size();
  EXPECT_EQ(newSize, oldSize+kDefaultDelayQueueSize);
  delayQueue = (DelayQueue*)pool_->pool_->getAddress(pool_->delay_queue_offset_);
  DelayNode node1 = *(delayQueue->front(pool_->pool_));
  EXPECT_EQ(node1.key, node.key);
  EXPECT_EQ(node1.time, node.time);
}

TEST_F(ArenaTest, alloc) {
  uint32_t size = 0;
  int64_t key = pool_->alloc(size);
  EXPECT_EQ(key, -1);
  size = pool_->max_mem_size_ + 1;
  key = pool_->alloc(size);
  EXPECT_EQ(key, -1);
  size = 10;
  uint32_t level = pool_->getLevel(size);

  int64_t *freeList = NULL;
  freeList = (int64_t*)pool_->pool_->getAddress(pool_->free_list_offset_);
  int64_t keyOld = freeList[level];

  EXPECT_EQ(keyOld, MMapMempool::_NULL);
  key = pool_->alloc(size);
  uint32_t realSize = *(uint32_t*)(pool_->pool_->getAddress(key));
  EXPECT_EQ(realSize, size);
  freeList[level] = 1234;
  pool_->use_free_list_ = true;
  key = pool_->alloc(size);
  // EXPECT_EQ(key, 1234);
}

TEST_F(ArenaTest, realloc) {
  int64_t key = -1;
  uint32_t size = 0;
  int64_t keyNew = pool_->realloc(key, size);
  EXPECT_EQ(keyNew, -1);
  key = 1;
  keyNew = pool_->realloc(key, size);
  EXPECT_EQ(keyNew, -1);
  size = pool_->max_mem_size_ + 1;
  keyNew = pool_->realloc(key, size);
  EXPECT_EQ(keyNew, -1);
  size = pool_->getSize(key) + 1;
  keyNew = pool_->realloc(key, size);
  EXPECT_EQ((keyNew != -1), true);
  DelayQueue *delayQueue = NULL;
  delayQueue =
    (DelayQueue*)pool_->pool_->getAddress(pool_->delay_queue_offset_);
  DelayNode node = *(delayQueue->front(pool_->pool_));
  EXPECT_EQ(node.key, static_cast<uint64_t>(key));
}

TEST_F(ArenaTest, reset) {
  uint32_t minMemSizeOld = pool_->min_mem_size_;
  uint32_t maxMemSizeOld = pool_->max_mem_size_;
  float rateOld = pool_->rate_;
  int ret = pool_->reset();
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(pool_->min_mem_size_, minMemSizeOld);
  EXPECT_EQ(pool_->max_mem_size_, maxMemSizeOld);
  EXPECT_EQ(pool_->rate_, rateOld);
}

TEST_F(ArenaTest, getDataSize) {
  EXPECT_EQ(0, pool_->getDataSize());
  uint32_t size = 10;
  pool_->alloc(size);
  pool_->getLevel(size);
  EXPECT_EQ(size + sizeof(uint32_t),
            static_cast<uint32_t>(pool_->getDataSize()));
}

TEST_F(ArenaTest, getHeaderSize) {
  EXPECT_EQ(1603092, pool_->getHeaderSize());
  pool_->alloc(100);
  EXPECT_EQ(1603092, pool_->getHeaderSize());
}

TEST_F(ArenaTest2, append) {
  uint32_t srcSize = 10;
  uint32_t dstSize = 50;
  char *pSrcBuf = NULL;
  char *pDstBuf = NULL;
  int64_t srcKey = poolSrc_->alloc(srcSize);
  int64_t dstKey = poolDst_->alloc(dstSize);
  const char *srcString = "Hello!!!";
  const char *dstString = "Hello1234!";

  ASSERT_TRUE(srcKey != -1);
  ASSERT_TRUE(dstKey != -1);

  pSrcBuf = poolSrc_->getAddress(srcKey);
  pDstBuf = poolDst_->getAddress(dstKey);

  ASSERT_TRUE(pSrcBuf != NULL);
  ASSERT_TRUE(pDstBuf != NULL);

  snprintf(pSrcBuf, 10, "%s", srcString);
  snprintf(pDstBuf, 50, "%s", dstString);


  srcKey = poolDst_->getHeaderSize() + poolDst_->getDataSize();

  int64_t srcDataSize = poolSrc_->getDataSize();
  EXPECT_EQ(srcDataSize, poolDst_->append(poolSrc_));
  pSrcBuf = poolDst_->getAddress(srcKey);
  EXPECT_EQ(0, strcmp(pSrcBuf, srcString));

  srcKey = poolDst_->getHeaderSize() + poolDst_->getDataSize();
  srcDataSize = poolSrc_->getDataSize();
  EXPECT_EQ(srcDataSize, poolDst_->append(poolSrc_));

  pSrcBuf = poolDst_->getAddress(srcKey);
  EXPECT_EQ(0, strcmp(pSrcBuf, srcString));

  pDstBuf = poolDst_->getAddress(dstKey);
  EXPECT_EQ(0, strcmp(pDstBuf, dstString));
}

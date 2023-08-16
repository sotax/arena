
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <iostream>

#include "arena/mmap_mempool.h"

using namespace std;

namespace base {

const int64_t MMapMempool::_NULL = -1L;
const int64_t MMapMempool::kMmapSize_ = (64L * 1024 * 1024 * 1024);  // 64G
const int64_t MMapMempool::kMaxMempoolSize_ = MMapMempool::kMmapSize_;

MMapMempool::MMapMempool()
    : fd_(-1),
      fd_header_(-1),
      file_(NULL),
      header_file_(NULL),
      base_(NULL),
      read_only_(false),
      expand_size_(1*1024*1024*1024) {
}

MMapMempool::~MMapMempool() {
  fd_ = -1;
  fd_header_ = -1;
  file_ = NULL;
  header_file_ = NULL;
  base_ = NULL;
}

int32_t MMapMempool::init(const char* file_name, uint32_t mode) {
  int32_t ret = Mempool::init(file_name);
  if (0 != ret) {
    return -1;
  }

  read_only_ = (MFILE_MODE_READ == mode);

  do {
    ret = access(file_name_, F_OK);  // check for existence
    if (ret == 0) {  // file exists
      ret = access(header_file_name_, F_OK);  // check header file
      if (ret != 0) {
        return -1;
      }
      if (loadFile() < 0) {
        break;
      }
    } else if (ret == -1 && errno == ENOENT) {
      if (createFile() < 0) {  // create if not exist 
        break;
      }
    } else {
      break;
    }
    return 0;
  } while (0);

  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
  if (file_) {
    munmap(file_, kMmapSize_);
    file_ = NULL;
  }
  base_ = NULL;
  return -1;
}

int32_t MMapMempool::dump() {
  if (read_only_) {
    return -1;
  }
  if (header_file_) {
    header_file_->max_size = header_file_->used_size;
  }
  msync(file_, header_file_->used_size, MS_SYNC);
  msync(header_file_, sizeof(MMapFileHeader), MS_SYNC);
  return 0;
}

int32_t MMapMempool::reset() {
  if (read_only_) {
    return -1;
  }
  if (base_) {
    header_file_->used_size = 0;
    return 0;
  }
  return -1;
}

int64_t MMapMempool::alloc(const int64_t& size) {
  if (size == 0) {
    return _NULL;
  }
  if (read_only_) {
    return _NULL;
  }

  if (header_file_->used_size + size > header_file_->max_size) {
    if (expand(size) < 0) {
      return _NULL;
    }
  }

  int64_t ret = header_file_->used_size;
  header_file_->used_size += size;
  return ret;
}

char* MMapMempool::getAddressSafe(const int64_t& offset) {
  if (base_ != NULL
      && offset != _NULL
      && offset < header_file_->used_size) {
    return base_ + offset;
  }
  return NULL;
}

char* MMapMempool::getAddress(const int64_t& offset, const int64_t& length) {
  if (base_ != NULL
      && offset != _NULL
      && offset < header_file_->used_size
      && offset + length <= header_file_->used_size
      ) {
    return base_ + offset;
  }
  return NULL;
}

int32_t MMapMempool::expand(const int64_t& size) {
  if (header_file_->max_size + size > kMaxMempoolSize_) {
    return -1;
  }
  if (read_only_) {
    return -1;
  }

  int64_t expand_size = expand_size_;
  if (expand_size < size) {
    expand_size = size;
  } else if (expand_size + header_file_->max_size > kMaxMempoolSize_) {
    expand_size = kMaxMempoolSize_ - header_file_->max_size;
  }

  int32_t ret = 0;
  ret = lseek(fd_, header_file_->max_size + expand_size - sizeof(expand_size),
    SEEK_SET);
  if (ret == -1) {
    return -1;
  }
  ret = write(fd_, &expand_size, sizeof(expand_size));
  if (ret == -1) {
    return -1;
  }

  header_file_->max_size += expand_size;
  return 0;
}

int32_t MMapMempool::loadFile() {
  int openFlags = 0;
  int mmapProt = 0;
  if (read_only_) {
    openFlags = O_RDONLY;
    mmapProt = PROT_READ;
  } else {
    openFlags = O_RDWR;
    mmapProt = PROT_READ | PROT_WRITE;
  }
  fd_ = open(file_name_, openFlags);
  if (fd_ < 0) {
    return -1;
  }
  fd_header_ = open(header_file_name_, openFlags);
  if (fd_header_ < 0) {
    ::close(fd_);
    return -1;
  }

  file_ = reinterpret_cast<char*>(mmap(NULL, kMmapSize_, mmapProt,
    MAP_SHARED, fd_, 0));
  if (MAP_FAILED == file_) {
    return -1;
  }

  header_file_ = reinterpret_cast<MMapFileHeader *>(mmap(NULL,
    sizeof(MMapFileHeader), mmapProt, MAP_SHARED, fd_header_, 0));
  if (MAP_FAILED == header_file_) {
    return -1;
  }

  struct stat stHeader;
  if (fstat(fd_header_, &stHeader) != 0) {
    return -1;
  }
  if (stHeader.st_size < (off_t)sizeof(MMapFileHeader)) {
    return -1;
  }

  struct stat st;
  fstat(fd_, &st);

  base_ = file_;

  if ((int64_t) st.st_size > kMmapSize_
      || stHeader.st_size != sizeof(MMapFileHeader)
      || header_file_->max_size > (int64_t) st.st_size
      || header_file_->used_size > header_file_->max_size) {
    return -1;
  }
  return 0;
}

int32_t MMapMempool::createFile() {
  if (read_only_) {
    return -1;
  }
  fd_ = open(file_name_, O_RDWR | O_CREAT, S_IRWXU | S_IRGRP | S_IROTH);
  if (fd_ < 0) {
    return -1;
  }

  fd_header_ = open(header_file_name_, O_RDWR | O_CREAT,
    S_IRWXU | S_IRGRP | S_IROTH);
  if (fd_header_ < 0) {
    return -1;
  }

  file_ = reinterpret_cast<char*>(mmap(NULL, kMmapSize_,
    PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0));
  if (MAP_FAILED == file_) {
    return -1;
  }

  header_file_ = reinterpret_cast<MMapFileHeader*> (mmap(NULL,
    sizeof(MMapFileHeader), PROT_READ | PROT_WRITE, MAP_SHARED, fd_header_, 0));

  if (MAP_FAILED == header_file_) {
    return -1;
  }

  int32_t ret = 0;
  char buf[sizeof(MMapFileHeader)] = {0};
  ret = lseek(fd_header_, 0, SEEK_SET);
  if (ret < 0) {
    return -1;
  }
  ret = write(fd_header_, buf, sizeof(MMapFileHeader));
  if (ret < 0) {
    return -1;
  }

  base_ = file_;
  return 0;
}

}  // namespace base

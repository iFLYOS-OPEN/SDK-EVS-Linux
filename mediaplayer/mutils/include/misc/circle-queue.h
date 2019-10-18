#pragma once

#include <stdint.h>
#include <string>

#define MIN_MEM_SIZE sizeof(ControlData)

namespace rokid {

typedef struct {
  uint32_t front;
  uint32_t back;
  uint32_t space_size;
  uint32_t unused;
} ControlData;

class CircleQueue {
public:
  bool create(void* mem, uint32_t size) {
    // already initialized
    if (_control)
      return false;
    // invalid args
    if (mem == nullptr || size < MIN_MEM_SIZE)
      return false;
    _control = reinterpret_cast<ControlData*>(mem);
    _remain_size = size - sizeof(ControlData);
    _begin = reinterpret_cast<uint8_t*>(mem) + sizeof(ControlData);
    _end = _begin + _remain_size;
    _control->space_size = _remain_size;
    _control->front = 0;
    _control->back = 0;
    return true;
  }

  void close() {
    _remain_size = 0;
    _control = nullptr;
  }

  inline uint32_t free_space() const {
    return _remain_size;
  }

  inline uint32_t capacity() const {
    return _control->space_size;
  }

  uint32_t size() const {
    return _control == nullptr ? 0 : _control->space_size - _remain_size;
  }

  // write a data record to queue
  int32_t write(const void* data, uint32_t length) {
    if (_control == nullptr || _remain_size < length)
      return -1;
    if (length == 0)
      return 0;
    // circle queue, if write reach memory pointer end,
    // jump to memory pointer begin, write remain data.
    uint8_t *bp = _begin + _control->back;
    uint32_t csz = _end - bp;
    if (csz >= length) {
      memcpy(bp, data, length);
      if (csz == length)
        _control->back = 0;
      else
        _control->back += length;
    } else {
      uint32_t sz1 = csz;
      uint32_t sz2 = length - sz1;
      memcpy(bp, data, sz1);
      memcpy(_begin, reinterpret_cast<const uint8_t*>(data) + sz1, sz2);
      _control->back = sz2;
    }
    _remain_size -= length;
    return length;
  }

  // return actual length of the data
  // or zero if no data available
  uint32_t read(void *buf, uint32_t bufsize) {
    if (_control == nullptr || _control->space_size == _remain_size)
      return 0;
    uint32_t total_size = _control->space_size - _remain_size;
    uint32_t outsz = total_size <= bufsize ? total_size : bufsize;
    char *bp = reinterpret_cast<char *>(_begin + _control->front);
    uint32_t csz = _end - reinterpret_cast<uint8_t*>(bp);
    if (buf) {
      if (outsz <= csz) {
        memcpy(buf, bp, outsz);
      } else {
        // 不是连续内存块，把两部分数据拼接起来
        uint32_t sz1 = csz;
        uint32_t sz2 = outsz - csz;
        memcpy(buf, bp, sz1);
        memcpy(reinterpret_cast<char *>(buf) + sz1, _begin, sz2);
      }
    }
    _remain_size += outsz;
    if (_remain_size == _control->space_size) {
      _control->front = 0;
      _control->back = 0;
    } else {
      if (outsz < csz)
        _control->front += outsz;
      else
        _control->front = outsz - csz;
    }
    return outsz;
  }

  // return erased data length
  uint32_t erase(uint32_t size) {
    return read(nullptr, size);
  }

  void clear() {
    if (_control) {
      _control->front = 0;
      _control->back = 0;
      _remain_size = _control->space_size;
    }
  }

  bool empty() const {
    return _control == nullptr || _control->space_size == _remain_size;
  }

private:
  // memory adress of begin of queue data space
  uint8_t *_begin = nullptr;
  uint8_t *_end = nullptr;
  ControlData *_control = nullptr;
  uint32_t _remain_size = 0;
};

} // namespace rokid

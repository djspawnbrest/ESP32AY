#include "esp_heap_caps.h"

extern "C" {
  //real functions to wrap
  void* __real_malloc(size_t size);
  void* __real_calloc(size_t num, size_t size);
  //malloc wrapper
  void* __wrap_malloc(size_t size) {
    return heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
  //calloc wrapper
  void* __wrap_calloc(size_t num, size_t size) {
    // Выделяем очищенную память строго во внутреннем DRAM
    return heap_caps_calloc(num, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
}
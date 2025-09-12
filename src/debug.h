#include "Arduino.h"

#define LOG_ERROR(fmt, ...) printf("\n\033[31m[ERROR] " fmt "\033[0m\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  printf("\033[33m[WARN] " fmt "\033[0m\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  printf("\033[32m[INFO] " fmt "\033[0m\n", ##__VA_ARGS__)
#define LOG_HEAP(fmt, ...)  printf("\033[32m" fmt "\033[0m\n", ##__VA_ARGS__)

void checkHeap(){
  LOG_HEAP("\n\tTotal heap: %d bytes", ESP.getHeapSize());
  LOG_HEAP("\tMinimum free heap: %d bytes",ESP.getMinFreeHeap());
  LOG_HEAP("\tMaximum allocatable block: %d bytes",ESP.getMaxAllocHeap());
  LOG_HEAP("\tFree heap: %d bytes", ESP.getFreeHeap());
  LOG_HEAP("\n\tTotal PSRAM: %d bytes", ESP.getPsramSize());
  LOG_HEAP("\tUsed PSRAM: %d bytes", ESP.getPsramSize()-ESP.getFreePsram());
  LOG_HEAP("\tFree PSRAM: %d bytes\n", ESP.getFreePsram());
}

int mem_info(){
  printf("Heap free/total:\n");
  printf(" Default:\t%u/%u\n",    heap_caps_get_free_size(MALLOC_CAP_DEFAULT), heap_caps_get_total_size(MALLOC_CAP_DEFAULT));
  printf(" 32-bit:\t%u/%u\n",     heap_caps_get_free_size(MALLOC_CAP_32BIT), heap_caps_get_total_size(MALLOC_CAP_32BIT));
  printf(" 8-bit:\t\t%u/%u\n",    heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_total_size(MALLOC_CAP_8BIT));
  printf(" SPI RAM:\t%u/%u\n",    heap_caps_get_free_size(MALLOC_CAP_SPIRAM), heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
  printf(" SRAM:\t\t%u/%u\n",     heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
  printf(" DMA:\t\t%u/%u\n",      heap_caps_get_free_size(MALLOC_CAP_DMA), heap_caps_get_total_size(MALLOC_CAP_DMA));
  printf(" RTC fast:\t%u/%u\n",   heap_caps_get_free_size(MALLOC_CAP_RTCRAM), heap_caps_get_total_size(MALLOC_CAP_RTCRAM));
  printf(" Retention:\t%u/%u\n",  heap_caps_get_free_size(MALLOC_CAP_RETENTION), heap_caps_get_total_size(MALLOC_CAP_RETENTION));
  printf(" IRAM 8-bit:\t%u/%u\n", heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT), heap_caps_get_total_size(MALLOC_CAP_IRAM_8BIT));
  printf(" Exec:\t\t%u/%u\n",     heap_caps_get_free_size(MALLOC_CAP_EXEC), heap_caps_get_total_size(MALLOC_CAP_EXEC));
  printf("\nMin heap:\t%u\n", heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
  return 0;
}
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cached = 0; // keeps track fo cache create and destory (1 = created)

int cache_create(int num_entries) {
  if (num_entries < 2 || num_entries > 4096){
    return -1;
  }
  if (cached == 0){
    cache = calloc(num_entries, sizeof(cache_entry_t));  // dynamically allocate space for cache
    cache_size = num_entries;
    cached = 1;
    return 1;
  }
  return -1;
}
int inserted = 0;
int cache_destroy(void) {
  if (cached == 1){
    free(cache);
    cache = NULL;
    cache_size = 0;
    cached = 0;
    inserted = 0;
    clock = 0;
    return 1;
  }
  return -1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  if (inserted == 0){ // checks if anything has been inserted
    return -1;
  }
  num_queries++;
  for (int i = 0; i < cache_size; i++){
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num && buf != NULL){
      num_hits++;
      clock++;
      cache[i].access_time = clock;
      memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
      return 1;     
    }
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  for (int i = 0; i < cache_size; i++){
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num ){
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE); // updates data in cache
      clock++;
      cache[i].access_time = clock;
    }
  }
}
int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  int location = -1;
  int smallest;
  // check invalid parameters
  if (cached == 0 || buf == NULL || cache_size == 0){
    return -1;
  }
  if (disk_num > 16 || disk_num < 0 || block_num > 256 || block_num < 0){
    return -1;
  }
  inserted = 1;
  // linear search
  for (int i = 0; i < cache_size; i++){
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num && block_num != 0 && disk_num != 0){ // inserting existing entry fails; also checks zeros because elements in calloc are 0
    return -1;
    }
    if (cache[i].valid == 0){
      location = i;
      break;
    }
  }
  // no room in cache
  if (location == -1){
    smallest = cache[0].access_time;
    location = 0;
    for (int i = 1; i < cache_size; i++){
      if (cache[i].access_time < smallest){
        smallest = cache[i].access_time;
        location = i;
      }
    }
  }
  memcpy(cache[location].block, buf, JBOD_BLOCK_SIZE); // copy buf into block of corresponding entry
  // update disk number and block number
  cache[location].disk_num = disk_num; 
  cache[location].block_num = block_num;
  cache[location].valid = 1; // block has valid data in it
  clock++;
  cache[location].access_time = clock;
  return 1;
}

bool cache_enabled(void) {
  return false;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}

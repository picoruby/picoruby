/*
  Original source code from https://github.com/thi-ng/tinyalloc
    Copyright (C) 2016 - 2017 Karsten Schmidt
  Modified source code for picoruby/picoruby:
    Copyright (C) 2025 HASUMI Hitoshi.

  This file is distributed under the Apache Software License 2.0.
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

bool ta_init(const void *base, const void *limit, const size_t heap_blocks, const size_t split_thresh, const size_t alignment);
void *ta_alloc(size_t num);
void *ta_calloc(size_t num, size_t size);
bool ta_free(void *ptr);

size_t ta_num_free();
size_t ta_num_used();
size_t ta_num_fresh();
bool ta_check();

struct walker_data {
  size_t total;
  size_t used;
  size_t free;
  size_t fresh_blocks;
};

void *ta_realloc(void *p, size_t size);
void ta_walk_pool(struct walker_data *data);

#ifdef __cplusplus
}
#endif

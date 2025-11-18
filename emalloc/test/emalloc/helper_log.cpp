// /////////////////////////////////////////////////////////////////////////////
/// @file benchmark_tests.cpp
/// @brief ./build/emalloc/test/RunAllTests -g BenchmarkTests
///
/// @par  Plataform Target: Tests
///
/// @copyright (C) 2025 Mario Luzeiro All rights reserved.
/// @author Mario Luzeiro <mluzeiro@ua.pt>
///
/// @par  License: Distributed under the 3-Clause BSD License. See accompanying
/// file LICENSE or a copy at https://opensource.org/licenses/BSD-3-Clause
/// SPDX-License-Identifier: BSD-3-Clause
///
// /////////////////////////////////////////////////////////////////////////////

// Includes
// /////////////////////////////////////////////////////////////////////////////
#include "helper_log.hpp"
#include <cstdio>

typedef struct s_emalloc_node {
  /// offset address on the external memory.
  /// Offset will be aligned based on EMALLOC_MIN_ALLOC_SIZE (16 bytes),
  /// so it gives is 4 bits for allocation info meaning.
  uint32_t offset;

  /// size or bitmask
  uint32_t alloc_info;
} sEMALLOC_node;

#define EMALLOC_ALLOC_INFO_MASK (0x0000000F)

void debug_header(const sEMALLOC_ctx* a_ctx) {
  printf("nodes_poll_length:%u\n", a_ctx->nodes_poll_length);
  printf("external_memory_size_bytes:%u\n", a_ctx->external_memory_size_bytes);
  printf("external_allocated_bytes:%u\n", a_ctx->external_allocated_bytes);
  printf("node_count:%u\n", a_ctx->node_count);
  printf("node_free_count:%u\n", a_ctx->node_free_count);
  printf("low_free_node_idx:%u\n", a_ctx->low_free_node_idx);
  printf("hi_free_node_idx:%u\n", a_ctx->hi_free_node_idx);
}

void debug_all_nodes_poll(const sEMALLOC_ctx* a_ctx) {
  for (uint32_t i = 0; i < a_ctx->nodes_poll_length; i++) {
    const sEMALLOC_node* nodes =
        reinterpret_cast<const sEMALLOC_node*>(a_ctx->nodes_poll);
    const sEMALLOC_node* node = &nodes[i];

    printf("%u\t(0x%08X)\t(0x%08X)\t%u\t%u\n", i, node->offset,
           node->alloc_info, node->offset & ~EMALLOC_ALLOC_INFO_MASK,
           node->alloc_info);

    if (i == (a_ctx->node_count - 1)) {
      printf("--\n");
    }
  }
}

// EOF
// /////////////////////////////////////////////////////////////////////////////

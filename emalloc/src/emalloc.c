// /////////////////////////////////////////////////////////////////////////////
/// @file emalloc.c
///
/// @par  Plataform Target: Any
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
#include "../include/emalloc/emalloc.h"
#include <stdbool.h>

#if (EMALLOC_STATISTICS == 1)
#include <string.h>
#endif

// Definitions
// /////////////////////////////////////////////////////////////////////////////

#define EMALLOC_MIN_ALLOC_SHIFTS (4)
#define EMALLOC_MIN_ALLOC_SIZE (1 << (EMALLOC_MIN_ALLOC_SHIFTS))  // 16 bytes
#define EMALLOC_ALLOC_INFO_MASK (0x0000000F)

typedef enum e_emalloc_alloc_info {
  /// Node is free, alloc_info has size info
  EMALLOC_NODE_FREE = 0x00,

  /// Node is allocated, alloc_info has size info
  EMALLOC_NODE_VARIABLE_SIZE = 0x0F,
} eEMALLOC_alloc_info;

typedef struct s_emalloc_node {
  /// offset address on the external memory.
  /// Offset will be aligned based on EMALLOC_MIN_ALLOC_SIZE (16 bytes),
  /// so it gives is 4 bits for allocation info meaning.
  uint32_t offset;

  /// size or bitmask
  uint32_t alloc_info;
} sEMALLOC_node;

#if (EMALLOC_STATISTICS == 1)
static sEMALLOC_statistics s_stats;
static sEMALLOC_operation_stats* s_pOpStats = NULL;

#define EMALLOC_STATS_INC_IF(n)                               \
  if (s_pOpStats->n_ifs < (0xFFFFFFFFFFFFFFFF - ((n) - 1))) { \
    s_pOpStats->n_ifs += (n);                                 \
  }

#define EMALLOC_STATS_INC_LOOPS(n)                              \
  if (s_pOpStats->n_loops < (0xFFFFFFFFFFFFFFFF - ((n) - 1))) { \
    s_pOpStats->n_loops += (n);                                 \
  }

#define EMALLOC_STATS_INC_RDWR(n)                                     \
  if (s_pOpStats->n_nodes_rd_wr < (0xFFFFFFFFFFFFFFFF - ((n) - 1))) { \
    s_pOpStats->n_nodes_rd_wr += (n);                                 \
  }

#define EMALLOC_STATS_INC_CALLS(n)                      \
  if (s_pOpStats->n_calls < (0xFFFFFFFF - ((n) - 1))) { \
    s_pOpStats->n_calls += (n);                         \
  }
#else
#define EMALLOC_STATS_INC_IF(n)
#define EMALLOC_STATS_INC_LOOPS(n)
#define EMALLOC_STATS_INC_RDWR(n)
#define EMALLOC_STATS_INC_CALLS(n)
#endif

uint32_t emalloc_init(sEMALLOC_ctx* a_emalloc_ctx,
                      const sEMALLOC_cfg* a_emalloc_configuration) {
  // Parameter validation
  if ((!a_emalloc_ctx) || (!a_emalloc_configuration)) {
    return EMALLOC_ERR_INVALID_PARAMETER;
  }

  // Configuration validation
  if ((!a_emalloc_configuration->nodes_poll) ||
      (a_emalloc_configuration->nodes_poll_length == 0) ||
      (a_emalloc_configuration->external_memory_size_bytes <
       EMALLOC_MIN_ALLOC_SIZE)) {
    return EMALLOC_ERR_INVALID_PARAMETER;
  }

  // Save config
  a_emalloc_ctx->nodes_poll = a_emalloc_configuration->nodes_poll;
  a_emalloc_ctx->nodes_poll_length = a_emalloc_configuration->nodes_poll_length;
  a_emalloc_ctx->external_memory_size_bytes =
      a_emalloc_configuration->external_memory_size_bytes;
  a_emalloc_ctx->external_allocated_bytes = 0;
  a_emalloc_ctx->node_count = 1;

  // Initialize first node, covering entire external memory
  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_configuration->nodes_poll;

  nodes[0].offset = EMALLOC_NODE_FREE;
  nodes[0].alloc_info = a_emalloc_configuration->external_memory_size_bytes;

  return EMALLOC_OK;
}

static bool is_node_free(const sEMALLOC_node* a_node) {
  EMALLOC_STATS_INC_RDWR(1);
  return (a_node->offset & EMALLOC_ALLOC_INFO_MASK) == EMALLOC_NODE_FREE;
}

static int32_t find_free_node(sEMALLOC_ctx* a_emalloc_ctx, uint32_t a_size) {
  sEMALLOC_node* node = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;

  uint32_t i = 0;

  for (; i < a_emalloc_ctx->node_count; ++i) {
    EMALLOC_STATS_INC_LOOPS(1);

    const bool isNodeFree = is_node_free(node);

    EMALLOC_STATS_INC_IF(1);
    if (isNodeFree) {
      EMALLOC_STATS_INC_RDWR(1);
      EMALLOC_STATS_INC_IF(1);
      if (node->alloc_info >= a_size) {
        return (int32_t)i;
      }
    }
    node++;
  }

  EMALLOC_STATS_INC_IF(1);

  if (i == a_emalloc_ctx->nodes_poll_length) {
    return -2;
  }

  return -1;
}
#if (EMALLOC_USE_BUBBLE_SORT == 1)
// Sort nodes by offset (simple bubble sort for clarity)
static void sort_nodes(sEMALLOC_ctx* a_emalloc_ctx) {
  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;

  for (uint32_t i = 0; i < (a_emalloc_ctx->node_count - 1); ++i) {
    EMALLOC_STATS_INC_LOOPS(1);

    for (uint32_t j = 0; j < (a_emalloc_ctx->node_count - i - 1); ++j) {
      EMALLOC_STATS_INC_LOOPS(1);
      EMALLOC_STATS_INC_RDWR(2);
      EMALLOC_STATS_INC_IF(1);
      if (nodes[j].offset > nodes[j + 1].offset) {
        sEMALLOC_node temp = nodes[j];
        nodes[j] = nodes[j + 1];
        nodes[j + 1] = temp;

        EMALLOC_STATS_INC_RDWR(4 * 2);
      }
    }
  }
}
#endif

#if (EMALLOC_USE_SHELL_SORT == 1)
// Shell sort
static void sort_nodes(sEMALLOC_ctx* a_emalloc_ctx) {
  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;
  const uint32_t n = a_emalloc_ctx->node_count;

  // Rearrange elements at each n/2, n/4, n/8, ... intervals
  for (uint32_t interval = n / 2; interval > 0; interval /= 2) {
    EMALLOC_STATS_INC_LOOPS(1);

    for (uint32_t i = interval; i < n; i += 1) {
      EMALLOC_STATS_INC_LOOPS(1);
      EMALLOC_STATS_INC_RDWR(2);
      const sEMALLOC_node temp = nodes[i];
      uint32_t j = i;
      for (; (j >= interval); j -= interval) {
        EMALLOC_STATS_INC_LOOPS(1);

        EMALLOC_STATS_INC_RDWR(2);
        const sEMALLOC_node temp_j = nodes[j - interval];

        EMALLOC_STATS_INC_IF(1);
        if (temp_j.offset <= temp.offset) {
          break;
        }

        nodes[j] = temp_j;
        EMALLOC_STATS_INC_RDWR(2 * 2);
      }

      nodes[j] = temp;
      EMALLOC_STATS_INC_RDWR(2);
    }
  }
}
#endif

#if (EMALLOC_USE_SEARCH_INTERPOLATION == 1)
static uint32_t interpolationSearch(const sEMALLOC_ctx* a_emalloc_ctx,
                                    uint32_t a_offset_to_search,
                                    uint32_t a_mask) {
  uint32_t lowIdx = 0;
  uint32_t highIdx = a_emalloc_ctx->node_count - 1;

  const sEMALLOC_node* nodes = (const sEMALLOC_node*)a_emalloc_ctx->nodes_poll;

  // Assume values inside range only,
  // they will return EMALLOC_ERR_OFFSET_NOT_FOUND anyway, but will take longer.

  EMALLOC_STATS_INC_IF(1);
  while (lowIdx <= highIdx) {
    EMALLOC_STATS_INC_LOOPS(1);

    EMALLOC_STATS_INC_RDWR(1);
    const uint32_t lowOffset = nodes[lowIdx].offset & a_mask;

    // If array has only one (or is the last search) element
    EMALLOC_STATS_INC_IF(1);
    if (lowIdx == highIdx) {
      EMALLOC_STATS_INC_IF(1);
      if (lowOffset == a_offset_to_search) {
        return lowIdx;
      }

      return EMALLOC_ERR_OFFSET_NOT_FOUND;
    }

    EMALLOC_STATS_INC_RDWR(1);
    const uint32_t highOffset = nodes[highIdx].offset & a_mask;

    // Estimate position using interpolation formula
    const uint32_t estimatedIdx =
        lowIdx + (((highIdx - lowIdx) * (a_offset_to_search - lowOffset)) /
                  (highOffset - lowOffset));

    EMALLOC_STATS_INC_RDWR(1);
    const uint32_t offsetFromEstimatedIdx = nodes[estimatedIdx].offset & a_mask;

    EMALLOC_STATS_INC_IF(1);
    if (offsetFromEstimatedIdx == a_offset_to_search) {
      return estimatedIdx;
    }

    // Target is in right subarray
    EMALLOC_STATS_INC_IF(1);
    if (offsetFromEstimatedIdx < a_offset_to_search) {
      lowIdx = estimatedIdx + 1;
    } else {
      // Target is in left subarray
      highIdx = estimatedIdx - 1;
    }
  }

  return EMALLOC_ERR_OFFSET_NOT_FOUND;
}
#endif

// Coalesce adjacent free nodes
static void coalesce(sEMALLOC_ctx* a_emalloc_ctx) {
  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;

  for (uint32_t i = 0; i < a_emalloc_ctx->node_count - 1; i++) {
    EMALLOC_STATS_INC_LOOPS(1);
    EMALLOC_STATS_INC_IF(2);

    if (is_node_free(&nodes[i]) && is_node_free(&nodes[i + 1])) {
      EMALLOC_STATS_INC_IF(1);
      EMALLOC_STATS_INC_RDWR(3);

      if ((nodes[i].offset + nodes[i].alloc_info) == nodes[i + 1].offset) {
        // Merge nodes
        nodes[i].alloc_info += nodes[i + 1].alloc_info;
        EMALLOC_STATS_INC_RDWR(2);

        // Shift remaining nodes
        for (uint32_t j = i + 1; j < (a_emalloc_ctx->node_count - 1); ++j) {
          EMALLOC_STATS_INC_LOOPS(1);

          nodes[j] = nodes[j + 1];
          EMALLOC_STATS_INC_RDWR(2 * 2);
        }

        a_emalloc_ctx->node_count--;
        EMALLOC_STATS_INC_RDWR(1);

        i--;  // Check again in case of multiple adjacent free nodes
      }
    }
  }
}

uint32_t emalloc_alloc(sEMALLOC_ctx* a_emalloc_ctx, uint32_t a_alloc_size) {
#if (EMALLOC_STATISTICS == 1)
  s_pOpStats = &s_stats.alloc;
  EMALLOC_STATS_INC_CALLS(1);
#endif

  EMALLOC_STATS_INC_IF(1);
  if (a_alloc_size == 0) {
    return EMALLOC_ERR_ZERO_REQUESTED;
  }

  EMALLOC_STATS_INC_IF(1);
  if (a_alloc_size > a_emalloc_ctx->external_memory_size_bytes) {
    return EMALLOC_ERR_INVALID_PARAMETER;  // Error value
  }

  EMALLOC_STATS_INC_IF(1);
  if ((a_alloc_size & ((1 << EMALLOC_MIN_ALLOC_SHIFTS) - 1)) != 0) {
    a_alloc_size = ((a_alloc_size >> EMALLOC_MIN_ALLOC_SHIFTS) + 1)
                   << EMALLOC_MIN_ALLOC_SHIFTS;
    EMALLOC_STATS_INC_RDWR(1);
  }

  const int32_t idx = find_free_node(a_emalloc_ctx, a_alloc_size);

  EMALLOC_STATS_INC_IF(1);
  if (idx == -1) {
    return EMALLOC_ERR_NO_EXTERNAL_MEMORY;
  }

  EMALLOC_STATS_INC_IF(1);
  if (idx == -2) {
    return EMALLOC_ERR_NO_MORE_FREE_NODES;
  }

  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;

  const uint32_t offset = nodes[idx].offset & ~EMALLOC_ALLOC_INFO_MASK;
  EMALLOC_STATS_INC_RDWR(1);

  // Split node if there's leftover space
  const uint32_t node_count = a_emalloc_ctx->node_count;

  EMALLOC_STATS_INC_RDWR(3);
  EMALLOC_STATS_INC_IF(2);

  if ((node_count < a_emalloc_ctx->nodes_poll_length) &&
      (nodes[idx].alloc_info > a_alloc_size)) {
    // Create new node for remainder
    nodes[node_count].offset = (offset + a_alloc_size) | EMALLOC_NODE_FREE;
    nodes[node_count].alloc_info = nodes[idx].alloc_info - a_alloc_size;

    EMALLOC_STATS_INC_RDWR(3 + 2);

    a_emalloc_ctx->node_count++;

    nodes[idx].alloc_info = a_alloc_size;
  }

  nodes[idx].offset |= EMALLOC_NODE_VARIABLE_SIZE;

  a_emalloc_ctx->external_allocated_bytes += a_alloc_size;

  EMALLOC_STATS_INC_RDWR(2);

  return offset;
}

uint32_t emalloc_free(sEMALLOC_ctx* a_emalloc_ctx,
                      uint32_t a_allocated_offset) {
#if (EMALLOC_STATISTICS == 1)
  s_pOpStats = &s_stats.free;
  EMALLOC_STATS_INC_CALLS(1);
#endif
#if 1
  sort_nodes(a_emalloc_ctx);

  const uint32_t idx =
      interpolationSearch(a_emalloc_ctx, a_allocated_offset, ~EMALLOC_ERR_MASK);

  if (idx == EMALLOC_ERR_OFFSET_NOT_FOUND) {
    return EMALLOC_ERR_OFFSET_NOT_FOUND;
  }

  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;
  sEMALLOC_node* node = &nodes[idx];

  EMALLOC_STATS_INC_IF(1);

  if (!is_node_free(node)) {
    // Free node
    node->offset = a_allocated_offset;
    node->offset |= EMALLOC_NODE_FREE;

    a_emalloc_ctx->external_allocated_bytes -= node->alloc_info;

    EMALLOC_STATS_INC_RDWR(3);

    coalesce(a_emalloc_ctx);

    return EMALLOC_OK;
  }

  return EMALLOC_ERR_OFFSET_NOT_FOUND;
#endif
#if 0
  sEMALLOC_node* node = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;

  for (uint32_t i = 0; i < a_emalloc_ctx->node_count; ++i) {
    EMALLOC_STATS_INC_LOOPS(1);

    const uint32_t nodeOffset = node->offset & ~EMALLOC_ALLOC_INFO_MASK;

    EMALLOC_STATS_INC_RDWR(1);
    EMALLOC_STATS_INC_IF(2);
    if ((nodeOffset == a_allocated_offset) && !is_node_free(node)) {
      // Free node
      node->offset = nodeOffset;
      node->offset |= EMALLOC_NODE_FREE;

      a_emalloc_ctx->external_allocated_bytes -= node->alloc_info;

      EMALLOC_STATS_INC_RDWR(3);

      sort_nodes(a_emalloc_ctx);
      coalesce(a_emalloc_ctx);

      return EMALLOC_OK;
    }

    node++;
  }

  return EMALLOC_ERR_OFFSET_NOT_FOUND;
#endif
}

#if (EMALLOC_STATISTICS == 1)
void emalloc_reset_statistics() {
  memset(&s_stats, 0, sizeof(s_stats));
}

void emalloc_get_statistics(sEMALLOC_statistics* a_out_statistics) {
  if (a_out_statistics) {
    *a_out_statistics = s_stats;
  }
}
#endif

// EOF
// /////////////////////////////////////////////////////////////////////////////

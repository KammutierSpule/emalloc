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
#include <assert.h>
#include <stdbool.h>

// Definitions
// /////////////////////////////////////////////////////////////////////////////

#define EMALLOC_MIN_ALLOC_SHIFTS (4)
#define EMALLOC_MIN_ALLOC_SIZE (1 << (EMALLOC_MIN_ALLOC_SHIFTS))  // 16 bytes
#define EMALLOC_ALLOC_INFO_MASK (0x0000000F)
#define EMALLOC_IS_SORTED (0xFFFFFFFF)
#define EMALLOC_HIGHEST_IDX (0xFFFFFFFF)
#define EMALLOC_NO_DANGLING (0xFFFFFFFF)

extern void debug_header(const sEMALLOC_ctx* a_ctx);
extern void debug_all_nodes_poll(const sEMALLOC_ctx* a_ctx);

typedef enum e_emalloc_alloc_info {
  /// Node is free, alloc_info has size info
  EMALLOC_NODE_FREE = 0x00,

  /// Node is allocated, alloc_info has size info
  EMALLOC_NODE_VARIABLE_SIZE = 0x0E,
  EMALLOC_NODE_ALLOCATED_BUT_NOT_USED = 0x0F
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
#include <string.h>

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

#if (EMALLOC_INTERNAL_CHECKS == 1)
static bool emalloc_total_memory_is_valid(sEMALLOC_ctx* a_emalloc_ctx) {
  sEMALLOC_node* node = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;

  uint32_t total_size = 0;

  for (uint32_t i = 0; i < a_emalloc_ctx->node_count; i++) {
    const uint32_t alloc_info = node[i].offset & EMALLOC_ALLOC_INFO_MASK;
    if (alloc_info != EMALLOC_NODE_ALLOCATED_BUT_NOT_USED) {
      total_size += node[i].alloc_info;
    }
  }

  return total_size == a_emalloc_ctx->external_memory_size_bytes;
}
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
  a_emalloc_ctx->node_free_count = 1;
  a_emalloc_ctx->low_free_node_idx = EMALLOC_HIGHEST_IDX;
  a_emalloc_ctx->hi_free_node_idx = 0;
  a_emalloc_ctx->start_idx_of_unsorted_node = EMALLOC_IS_SORTED;
  a_emalloc_ctx->allocated_but_not_used_count = 0;

  // Initialize first node, covering entire external memory
  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_configuration->nodes_poll;

  nodes[0].offset = EMALLOC_NODE_FREE;
  nodes[0].alloc_info = a_emalloc_configuration->external_memory_size_bytes;

#if (EMALLOC_INTERNAL_CHECKS == 1)
  EMALLOC_ASSERT(emalloc_total_memory_is_valid(a_emalloc_ctx));
#endif

  return EMALLOC_OK;
}

static bool is_node_free(const sEMALLOC_node* a_node) {
  EMALLOC_STATS_INC_RDWR(1);
  return (a_node->offset & EMALLOC_ALLOC_INFO_MASK) == EMALLOC_NODE_FREE;
}

static int32_t find_free_node(sEMALLOC_ctx* a_emalloc_ctx, uint32_t a_size) {
  sEMALLOC_node* node = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;

  // Start from the end and work backwards
  int32_t best_fit_idx = -1;
#if (EMALLOC_FIND_FREE_NODES_USE_BEST_FIT == 1)
  uint32_t best_fit_size = 0xFFFFFFFF;
#endif

  for (int32_t i = a_emalloc_ctx->node_count - 1; i >= 0; --i) {
    EMALLOC_STATS_INC_LOOPS(1);

    const bool isNodeFree = is_node_free(&node[i]);

    EMALLOC_STATS_INC_IF(2);
    if (isNodeFree) {
      uint32_t total_size = node[i].alloc_info;
      uint32_t merge_count = 0;
      EMALLOC_STATS_INC_RDWR(1);

      // Check adjacent free nodes backwards and calculate total size
      int32_t j = i - 1;
      while (j >= 0) {
        EMALLOC_STATS_INC_LOOPS(1);

        EMALLOC_STATS_INC_IF(1);
        if (!is_node_free(&node[j])) {
          break;
        }

        EMALLOC_STATS_INC_RDWR(3);
        EMALLOC_STATS_INC_IF(1);
        // Check if nodes are adjacent (offset + size == next offset)
        if ((node[j].offset + node[j].alloc_info) !=
            (node[j + 1].offset & ~EMALLOC_ALLOC_INFO_MASK)) {
          break;
        }

        node[j + 1].offset |= EMALLOC_NODE_ALLOCATED_BUT_NOT_USED;
        EMALLOC_STATS_INC_RDWR(1);

        EMALLOC_STATS_INC_RDWR(2);
        total_size += node[j].alloc_info;
        merge_count++;
        j--;
      }

      // debug_header(a_emalloc_ctx);
      // debug_all_nodes_poll(a_emalloc_ctx);

      // Merge adjacent free nodes if any were found
      EMALLOC_STATS_INC_IF(1);
      if (merge_count > 0) {
        int32_t merge_start_idx = j + 1;
        node[merge_start_idx].alloc_info = total_size;
        EMALLOC_STATS_INC_RDWR(1);

        a_emalloc_ctx->allocated_but_not_used_count += merge_count;

        EMALLOC_ASSERT(a_emalloc_ctx->node_free_count >= merge_count);
        a_emalloc_ctx->node_free_count -= merge_count;
        EMALLOC_STATS_INC_RDWR(2);

        // Update i to skip merged nodes
        i = merge_start_idx - 1;
      }

      // Check if this is a better fit
#if (EMALLOC_FIND_FREE_NODES_USE_BEST_FIT == 1)
      EMALLOC_STATS_INC_IF(2);
      if ((total_size >= a_size) && (total_size < best_fit_size)) {
        best_fit_size = total_size;
        best_fit_idx = j + 1;
      }
#endif
#if (EMALLOC_FIND_FREE_NODES_USE_FIRST_FIT == 1)
      EMALLOC_STATS_INC_IF(2);
      if (total_size >= a_size) {
        best_fit_idx = j + 1;

        return best_fit_idx;
      }
#endif
    }

#if (EMALLOC_INTERNAL_CHECKS == 1)
    EMALLOC_ASSERT(emalloc_total_memory_is_valid(a_emalloc_ctx));
#endif
  }

  uint32_t i = a_emalloc_ctx->node_count;

  EMALLOC_STATS_INC_IF(1);

  if (best_fit_idx != -1) {
    return best_fit_idx;
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
        EMALLOC_STATS_INC_RDWR(2);
      }

      nodes[j] = temp;
      EMALLOC_STATS_INC_RDWR(2);
    }
  }
}
#endif

#if (EMALLOC_USE_SHELL_SORT_TOKUDA == 1)
// Shell sort
static void sort_nodes(sEMALLOC_ctx* a_emalloc_ctx) {
  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;
  const uint32_t n = a_emalloc_ctx->node_count;

  // Generate Tokuda sequence: h[k] = ceil((9^k - 4^k)/(5*4^(k-1)))
  int32_t gaps[20];
  int32_t gap_count = 0;
  int32_t h = 1;

  while (h < n) {
    EMALLOC_STATS_INC_LOOPS(1);
    EMALLOC_STATS_INC_RDWR(1);
    gaps[gap_count++] = h;
    h = (h * 9 + 4) / 5;  // Approximation of Tokuda formula
  }

  // Sort using gaps in reverse order
  for (int32_t g = gap_count - 1; g >= 0; g--) {
    EMALLOC_STATS_INC_LOOPS(1);
    int32_t gap = gaps[g];
    EMALLOC_STATS_INC_RDWR(1);

    for (int i = gap; i < n; i++) {
      EMALLOC_STATS_INC_LOOPS(1);

      const sEMALLOC_node temp = nodes[i];
      EMALLOC_STATS_INC_RDWR(2);

      int j = i;

      while ((j >= gap) && (nodes[j - gap].offset > temp.offset)) {
        EMALLOC_STATS_INC_LOOPS(1);

        nodes[j] = nodes[j - gap];
        j -= gap;
        EMALLOC_STATS_INC_RDWR(2 * 2);
      }
      nodes[j] = temp;
      EMALLOC_STATS_INC_RDWR(2);
    }
  }
}
#endif

#if (EMALLOC_USE_INSERTION_SORT == 1)
static void sort_nodes(sEMALLOC_ctx* a_emalloc_ctx) {
  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;
  const uint32_t n = a_emalloc_ctx->node_count;

  for (uint32_t i = (a_emalloc_ctx->start_idx_of_unsorted_node + 1); i < n;
       i++) {
    EMALLOC_STATS_INC_LOOPS(1);

    sEMALLOC_node key;

    EMALLOC_STATS_INC_RDWR(1);
    key.offset = nodes[i].offset;

    EMALLOC_STATS_INC_RDWR(1);
    key.alloc_info = nodes[i].alloc_info;

    int32_t j = i - 1;

    while ((j >= 0) && (nodes[j].offset > key.offset)) {
      EMALLOC_STATS_INC_LOOPS(1);
      EMALLOC_STATS_INC_IF(1);
      EMALLOC_STATS_INC_RDWR(1);

      nodes[j + 1] = nodes[j];
      EMALLOC_STATS_INC_RDWR(2 + 2);

      j--;
    }

    EMALLOC_STATS_INC_RDWR(2);
    nodes[j + 1] = key;
  }
}
#endif

#if (EMALLOC_USE_INSERTION_SORT_BINARY == 1)
static void sort_nodes(sEMALLOC_ctx* a_emalloc_ctx) {
  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;
  const uint32_t n = a_emalloc_ctx->node_count;

  for (int i = (a_emalloc_ctx->start_idx_of_unsorted_node + 1); i < n; i++) {
    EMALLOC_STATS_INC_LOOPS(1);

    EMALLOC_STATS_INC_RDWR(2);
    const sEMALLOC_node key = nodes[i];

    int left = 0;
    int right = i - 1;

    // Binary search for correct position
    while (left <= right) {
      EMALLOC_STATS_INC_LOOPS(1);

      const int mid = left + ((right - left) / 2);

      EMALLOC_STATS_INC_RDWR(1);
      EMALLOC_STATS_INC_IF(1);
      if (nodes[mid].offset > key.offset) {
        right = mid - 1;
      } else {
        left = mid + 1;
      }
    }

    // Shift elements to make room
    int pos = left;
    for (int j = i - 1; j >= pos; j--) {
      EMALLOC_STATS_INC_LOOPS(1);

      nodes[j + 1] = nodes[j];
      EMALLOC_STATS_INC_RDWR(2 * 2);
    }
    nodes[pos] = key;
    EMALLOC_STATS_INC_RDWR(2);
  }
}
#endif

#if (EMALLOC_USE_INSERTION_SORT_SENTINEL == 1)
static void sort_nodes(sEMALLOC_ctx* a_emalloc_ctx) {
  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;
  const uint32_t n = a_emalloc_ctx->node_count;

  for (uint32_t i = (a_emalloc_ctx->start_idx_of_unsorted_node + 1); i < n;
       i++) {
    EMALLOC_STATS_INC_LOOPS(1);

    sEMALLOC_node key;

    EMALLOC_STATS_INC_RDWR(1);
    key.offset = nodes[i].offset;

    EMALLOC_STATS_INC_RDWR(1);
    key.alloc_info = nodes[i].alloc_info;

    int32_t j = i - 1;

    while ((j >= 0) && (nodes[j].offset > key.offset)) {
      EMALLOC_STATS_INC_LOOPS(1);
      EMALLOC_STATS_INC_IF(1);
      EMALLOC_STATS_INC_RDWR(1);

      nodes[j + 1] = nodes[j];
      EMALLOC_STATS_INC_RDWR(2 + 2);

      j--;
    }

    EMALLOC_STATS_INC_RDWR(2);
    nodes[j + 1] = key;
  }
}
#endif

#if (EMALLOC_USE_HEAP_SORT_MAX == 1)
static void node_swap(sEMALLOC_ctx* a_emalloc_ctx, uint32_t i, uint32_t j) {
  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;
  const sEMALLOC_node temp = nodes[i];
  nodes[i] = nodes[j];
  nodes[j] = temp;
  EMALLOC_STATS_INC_RDWR(4 * 2);
}

void max_heapify(sEMALLOC_ctx* a_emalloc_ctx, uint32_t n, uint32_t i) {
  uint32_t largest = i;
  uint32_t l = (2 * i) + 1;
  uint32_t r = (2 * i) + 2;

  const sEMALLOC_node* nodes = (const sEMALLOC_node*)a_emalloc_ctx->nodes_poll;

  EMALLOC_STATS_INC_RDWR(2);
  EMALLOC_STATS_INC_IF(2);  // optimize
  if ((l < n) && (nodes[l].offset > nodes[largest].offset)) {
    largest = l;
  }

  EMALLOC_STATS_INC_RDWR(2);
  EMALLOC_STATS_INC_IF(2);  // optimize
  if ((r < n) && (nodes[r].offset > nodes[largest].offset)) {
    largest = r;
  }

  EMALLOC_STATS_INC_IF(2);
  if (largest != i) {
    node_swap(a_emalloc_ctx, i, largest);
    EMALLOC_STATS_INC_LOOPS(1);
    max_heapify(a_emalloc_ctx, n, largest);
  }
}

static void sort_nodes(sEMALLOC_ctx* a_emalloc_ctx) {
  const sEMALLOC_node* nodes = (const sEMALLOC_node*)a_emalloc_ctx->nodes_poll;
  const uint32_t n = a_emalloc_ctx->node_count;

  for (int i = (n / 2) - 1; i >= 0; i--) {
    EMALLOC_STATS_INC_LOOPS(1);
    max_heapify(a_emalloc_ctx, n, i);
  }

  for (int i = n - 1; i >= 0; i--) {
    EMALLOC_STATS_INC_LOOPS(1);
    node_swap(a_emalloc_ctx, i, 0);
    max_heapify(a_emalloc_ctx, i, 0);
  }
}
#endif

#if (EMALLOC_USE_SEARCH_INTERPOLATION == 1)
static uint32_t interpolationSearch(const sEMALLOC_ctx* a_emalloc_ctx,
                                    uint32_t a_offset_to_search,
                                    uint32_t a_mask, uint32_t a_high_idx) {
  uint32_t lowIdx = 0;
  uint32_t highIdx = a_high_idx;

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

static void coalesce(sEMALLOC_ctx* a_emalloc_ctx, uint32_t a_released_idx) {
  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;

  uint32_t new_free_idx = a_released_idx;

  EMALLOC_ASSERT(is_node_free(&nodes[a_released_idx]));

  EMALLOC_STATS_INC_IF(1);
  if (a_released_idx == 0) {
    EMALLOC_STATS_INC_IF(1);
    if (is_node_free(&nodes[1])) {
      // no need to mask offsets because EMALLOC_NODE_FREE == 0
      EMALLOC_STATS_INC_IF(1);
      EMALLOC_STATS_INC_RDWR(3);
      if ((nodes[0].offset + nodes[0].alloc_info) == nodes[1].offset) {
        // Merge nodes
        nodes[0].alloc_info += nodes[1].alloc_info;
        EMALLOC_STATS_INC_RDWR(2);

        EMALLOC_STATS_INC_IF(1);
        if (a_emalloc_ctx->node_count > 2) {
          nodes[1].offset |= EMALLOC_NODE_ALLOCATED_BUT_NOT_USED;
          a_emalloc_ctx->allocated_but_not_used_count++;
          EMALLOC_STATS_INC_RDWR(2);
        } else {
          a_emalloc_ctx->allocated_but_not_used_count = 0;
          a_emalloc_ctx->node_count--;
          EMALLOC_STATS_INC_RDWR(2);
        }

        a_emalloc_ctx->node_free_count--;
        EMALLOC_STATS_INC_RDWR(2);
      }
    }
#if (EMALLOC_INTERNAL_CHECKS == 1)
    EMALLOC_ASSERT(emalloc_total_memory_is_valid(a_emalloc_ctx));
#endif
  } else {
    EMALLOC_STATS_INC_IF(1);
    if (a_released_idx == (a_emalloc_ctx->node_count - 1)) {
      EMALLOC_STATS_INC_IF(1);
      if (is_node_free(&nodes[a_released_idx - 1])) {
        EMALLOC_STATS_INC_IF(1);
        EMALLOC_STATS_INC_RDWR(3);
        // no need to mask offsets because EMALLOC_NODE_FREE == 0
        if ((nodes[a_released_idx - 1].offset +
             nodes[a_released_idx - 1].alloc_info) ==
            nodes[a_released_idx].offset) {
          // Merge nodes
          nodes[a_released_idx - 1].alloc_info +=
              nodes[a_released_idx].alloc_info;
          EMALLOC_STATS_INC_RDWR(2);

          a_emalloc_ctx->node_count--;
          a_emalloc_ctx->node_free_count--;
          EMALLOC_STATS_INC_RDWR(2);

          new_free_idx = a_released_idx - 1;
        }
      }
#if (EMALLOC_INTERNAL_CHECKS == 1)
      EMALLOC_ASSERT(emalloc_total_memory_is_valid(a_emalloc_ctx));
#endif
    } else {
      uint8_t shift_state = 0;

      EMALLOC_STATS_INC_IF(1);
      if (is_node_free(&nodes[a_released_idx + 1])) {
        EMALLOC_STATS_INC_IF(1);
        EMALLOC_STATS_INC_RDWR(3);
        // no need to mask offsets because EMALLOC_NODE_FREE == 0
        if ((nodes[a_released_idx].offset + nodes[a_released_idx].alloc_info) ==
            nodes[a_released_idx + 1].offset) {
          // Merge nodes
          nodes[a_released_idx].alloc_info +=
              nodes[a_released_idx + 1].alloc_info;
          EMALLOC_STATS_INC_RDWR(2);

          EMALLOC_STATS_INC_RDWR(1);
          shift_state = 0x01;
        }
      }

      EMALLOC_STATS_INC_IF(1);
      if (is_node_free(&nodes[a_released_idx - 1])) {
        EMALLOC_STATS_INC_IF(1);
        EMALLOC_STATS_INC_RDWR(3);
        // no need to mask offsets because EMALLOC_NODE_FREE == 0
        if ((nodes[a_released_idx - 1].offset +
             nodes[a_released_idx - 1].alloc_info) ==
            nodes[a_released_idx].offset) {
          // Merge nodes
          nodes[a_released_idx - 1].alloc_info +=
              nodes[a_released_idx].alloc_info;
          EMALLOC_STATS_INC_RDWR(2);

          EMALLOC_STATS_INC_RDWR(1);
          shift_state |= 0x02;
        }
      }

      // Shift remaining nodes ?
      EMALLOC_STATS_INC_IF(1);
      if (shift_state == 0x01) {
        const uint32_t dangling_idx = a_released_idx + 1;
        nodes[dangling_idx].offset |= EMALLOC_NODE_ALLOCATED_BUT_NOT_USED;
        a_emalloc_ctx->allocated_but_not_used_count++;
        EMALLOC_STATS_INC_RDWR(2);

        a_emalloc_ctx->node_free_count--;
        EMALLOC_STATS_INC_RDWR(2);
      } else {
        if (shift_state == 0x02) {
          const uint32_t dangling_idx = a_released_idx;

          nodes[dangling_idx].offset |= EMALLOC_NODE_ALLOCATED_BUT_NOT_USED;
          a_emalloc_ctx->allocated_but_not_used_count++;
          EMALLOC_STATS_INC_RDWR(2);

          a_emalloc_ctx->node_free_count--;
          EMALLOC_STATS_INC_RDWR(2);

          new_free_idx = a_released_idx - 1;
        } else {
          if (shift_state == (0x02 | 0x01)) {
            nodes[a_released_idx + 0].offset |=
                EMALLOC_NODE_ALLOCATED_BUT_NOT_USED;
            nodes[a_released_idx + 1].offset |=
                EMALLOC_NODE_ALLOCATED_BUT_NOT_USED;
            EMALLOC_STATS_INC_RDWR(2);

            a_emalloc_ctx->allocated_but_not_used_count += 2;
            a_emalloc_ctx->node_free_count -= 2;
            new_free_idx = a_released_idx - 1;

            EMALLOC_STATS_INC_RDWR(2);
          } else {
            // No merged happened
          }
        }
      }

#if (EMALLOC_INTERNAL_CHECKS == 1)
      EMALLOC_ASSERT(emalloc_total_memory_is_valid(a_emalloc_ctx));
#endif
    }
  }

  EMALLOC_STATS_INC_IF(1);
  if (new_free_idx < a_emalloc_ctx->low_free_node_idx) {
    a_emalloc_ctx->low_free_node_idx = new_free_idx;
    EMALLOC_STATS_INC_RDWR(1);
  }

  EMALLOC_STATS_INC_IF(1);
  if (new_free_idx >= (a_emalloc_ctx->node_count - 1)) {
    a_emalloc_ctx->hi_free_node_idx = a_emalloc_ctx->node_count - 1;
    EMALLOC_STATS_INC_RDWR(1);
  } else {
    EMALLOC_STATS_INC_IF(1);
    if (new_free_idx > a_emalloc_ctx->hi_free_node_idx) {
      a_emalloc_ctx->hi_free_node_idx = new_free_idx;
      EMALLOC_STATS_INC_RDWR(1);
    }
  }
}

uint32_t de_dangling_and_search_best_fit(sEMALLOC_ctx* a_emalloc_ctx,
                                         uint32_t a_alloc_size) {
#if (EMALLOC_INTERNAL_CHECKS == 1)
  EMALLOC_ASSERT(emalloc_total_memory_is_valid(a_emalloc_ctx));
#endif

  EMALLOC_ASSERT(a_emalloc_ctx);
  EMALLOC_ASSERT(a_emalloc_ctx->allocated_but_not_used_count > 0);
  EMALLOC_ASSERT(a_emalloc_ctx->node_count > 2);

  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;

  uint32_t best_fit_idx = EMALLOC_ERR_OFFSET_NOT_FOUND;
  uint32_t best_fit_size = 0xFFFFFFFF;

  // Compact the array by removing dangling nodes and merging free nodes
  uint32_t write_idx = 0;
  uint32_t read_idx = write_idx;

  // Compact nodes in a single pass:
  // - skip dangling nodes
  // - shift remaining nodes
  // - search for best-fit
  EMALLOC_STATS_INC_RDWR(1);
  while (read_idx < a_emalloc_ctx->node_count) {
    EMALLOC_STATS_INC_LOOPS(1);
    EMALLOC_STATS_INC_RDWR(1);

    const sEMALLOC_node* read_node = &nodes[read_idx];
    const uint32_t read_alloc_info_bits =
        read_node->offset & EMALLOC_ALLOC_INFO_MASK;

    EMALLOC_STATS_INC_IF(1);
    if (read_alloc_info_bits != EMALLOC_NODE_ALLOCATED_BUT_NOT_USED) {
      nodes[write_idx] = *read_node;
      EMALLOC_STATS_INC_RDWR(2 * 2);

      // Best-fit search
      EMALLOC_STATS_INC_IF(1);
      if (read_alloc_info_bits == EMALLOC_NODE_FREE) {
        EMALLOC_STATS_INC_RDWR(1);
        EMALLOC_STATS_INC_IF(1);
        if (read_node->alloc_info >= a_alloc_size) {
          EMALLOC_STATS_INC_IF(1);
          if (read_node->alloc_info < best_fit_size) {
            best_fit_size = read_node->alloc_info;
            best_fit_idx =
                write_idx;  // the read node become now the write index
            EMALLOC_STATS_INC_RDWR(2);
          }
        }
      }

      write_idx++;
    }

    read_idx++;
  }

  EMALLOC_STATS_INC_RDWR(3);
  a_emalloc_ctx->node_count = write_idx;
  a_emalloc_ctx->allocated_but_not_used_count = 0;

#if (EMALLOC_INTERNAL_CHECKS == 1)
  EMALLOC_ASSERT(emalloc_total_memory_is_valid(a_emalloc_ctx));
#endif

  EMALLOC_STATS_INC_IF(1);
  if (best_fit_idx != EMALLOC_ERR_OFFSET_NOT_FOUND) {
    return best_fit_idx;
  }

  // If no best fit found,
  // try to merge free nodes from end to back
  uint32_t back = 0;
  for (uint32_t i = a_emalloc_ctx->node_count - 1; i > back; --i) {
    EMALLOC_STATS_INC_LOOPS(1);

    EMALLOC_STATS_INC_IF(2);
    if (is_node_free(&nodes[i - 1]) && is_node_free(&nodes[i])) {
      EMALLOC_STATS_INC_IF(1);
      EMALLOC_STATS_INC_RDWR(3);
      if ((nodes[i - 1].offset + nodes[i - 1].alloc_info) == nodes[i].offset) {
        const uint32_t new_free_space =
            nodes[i - 1].alloc_info + nodes[i].alloc_info;
        nodes[i - 1].alloc_info = new_free_space;
        EMALLOC_STATS_INC_RDWR(2);

        // Shift remaining nodes
        for (uint32_t j = i; j < (a_emalloc_ctx->node_count - 1); ++j) {
          EMALLOC_STATS_INC_LOOPS(1);
          nodes[j] = nodes[j + 1];
          EMALLOC_STATS_INC_RDWR(4);
        }

        a_emalloc_ctx->node_count--;
        a_emalloc_ctx->node_free_count--;
        EMALLOC_STATS_INC_RDWR(2);

        // Only search First-fit on new merged nodes
        // (because the other free nodes were searched on previous pass,
        // with Best-fit)
        EMALLOC_STATS_INC_IF(1);
        if (new_free_space >= a_alloc_size) {
          best_fit_idx = i - 1;
          EMALLOC_STATS_INC_RDWR(1);

          return best_fit_idx;
        }
      }

#if (EMALLOC_INTERNAL_CHECKS == 1)
      EMALLOC_ASSERT(emalloc_total_memory_is_valid(a_emalloc_ctx));
#endif
    }
  }

  return best_fit_idx;
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
    // Round to min alloc size
    a_alloc_size = ((a_alloc_size >> EMALLOC_MIN_ALLOC_SHIFTS) + 1)
                   << EMALLOC_MIN_ALLOC_SHIFTS;
  }

  uint32_t best_fit_idx = EMALLOC_ERR_OFFSET_NOT_FOUND;

  if (a_emalloc_ctx->allocated_but_not_used_count) {
    best_fit_idx = de_dangling_and_search_best_fit(a_emalloc_ctx, a_alloc_size);
  }

  int32_t idx = (int32_t)best_fit_idx;

  EMALLOC_STATS_INC_IF(1);
  if (best_fit_idx == EMALLOC_ERR_OFFSET_NOT_FOUND) {
    idx = find_free_node(a_emalloc_ctx, a_alloc_size);
  }

  EMALLOC_STATS_INC_IF(1);
  if (idx == -1) {
    return EMALLOC_ERR_NO_EXTERNAL_MEMORY;
  }

  EMALLOC_STATS_INC_IF(1);
  if (idx == -2) {
    return EMALLOC_ERR_NO_MORE_FREE_NODES;
  }

  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;
  sEMALLOC_node* node = &nodes[idx];

  const uint32_t offset = node->offset & ~EMALLOC_ALLOC_INFO_MASK;
  node->offset |= EMALLOC_NODE_VARIABLE_SIZE;
  EMALLOC_STATS_INC_RDWR(2);

  // Split node if there's leftover space
  const uint32_t node_count = a_emalloc_ctx->node_count;

  EMALLOC_STATS_INC_RDWR(3);
  EMALLOC_STATS_INC_IF(2);

  if (node->alloc_info > a_alloc_size) {
    if (node_count < a_emalloc_ctx->nodes_poll_length) {
      // Create new node for remainder
      nodes[node_count].offset = (offset + a_alloc_size) | EMALLOC_NODE_FREE;
      nodes[node_count].alloc_info = node->alloc_info - a_alloc_size;
      node->alloc_info = a_alloc_size;

      EMALLOC_STATS_INC_IF(1);
      if ((idx + 1) != node_count) {
        // It is only unsorted if created node is not adjacent.

        const uint32_t start_idx_of_unsorted_node = idx + 1;

        EMALLOC_STATS_INC_IF(1);
        if (start_idx_of_unsorted_node <
            a_emalloc_ctx->start_idx_of_unsorted_node) {
          a_emalloc_ctx->start_idx_of_unsorted_node =
              start_idx_of_unsorted_node;
          a_emalloc_ctx->offset_before_unsorted_node = offset;
          EMALLOC_STATS_INC_RDWR(2);
        }
      }

      a_emalloc_ctx->hi_free_node_idx = node_count;
      a_emalloc_ctx->node_count = node_count + 1;

      EMALLOC_STATS_INC_RDWR(7);
    }
  }

  a_emalloc_ctx->external_allocated_bytes += a_alloc_size;
  EMALLOC_STATS_INC_RDWR(1);

  return offset;
}

uint32_t emalloc_free(sEMALLOC_ctx* a_emalloc_ctx,
                      uint32_t a_allocated_offset) {
#if (EMALLOC_STATISTICS == 1)
  s_pOpStats = &s_stats.free;
  EMALLOC_STATS_INC_CALLS(1);
#endif

  uint32_t idx = EMALLOC_ERR_OFFSET_NOT_FOUND;

  EMALLOC_STATS_INC_IF(1);
  if (a_emalloc_ctx->start_idx_of_unsorted_node != EMALLOC_IS_SORTED) {
    // If offset is below the one we know that all indexes are ordered up there
    // we don't need to sort and perform only search of ordered part.
    EMALLOC_STATS_INC_IF(1);
    if (a_allocated_offset <= a_emalloc_ctx->offset_before_unsorted_node) {
      idx = interpolationSearch(a_emalloc_ctx, a_allocated_offset,
                                ~EMALLOC_ERR_MASK,
                                a_emalloc_ctx->start_idx_of_unsorted_node - 1);
      EMALLOC_ASSERT(idx != EMALLOC_ERR_OFFSET_NOT_FOUND);
    } else {
      sort_nodes(a_emalloc_ctx);

      EMALLOC_STATS_INC_RDWR(1);
      a_emalloc_ctx->start_idx_of_unsorted_node = EMALLOC_IS_SORTED;
      // Lost free indexes order, set to unknown order
      a_emalloc_ctx->low_free_node_idx = EMALLOC_HIGHEST_IDX;
      a_emalloc_ctx->hi_free_node_idx = 0;
    }
  }

  EMALLOC_STATS_INC_IF(1);
  if (idx == EMALLOC_ERR_OFFSET_NOT_FOUND) {
    idx = interpolationSearch(a_emalloc_ctx, a_allocated_offset,
                              ~EMALLOC_ERR_MASK, a_emalloc_ctx->node_count - 1);

    EMALLOC_STATS_INC_IF(1);
    if (idx == EMALLOC_ERR_OFFSET_NOT_FOUND) {
      return EMALLOC_ERR_OFFSET_NOT_FOUND;
    }
  }

  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;
  sEMALLOC_node* node = &nodes[idx];

  EMALLOC_ASSERT(!is_node_free(node));

  // Free node
  node->offset = a_allocated_offset;
  node->offset |= EMALLOC_NODE_FREE;

  a_emalloc_ctx->external_allocated_bytes -= node->alloc_info;

  EMALLOC_STATS_INC_RDWR(3);

  EMALLOC_STATS_INC_RDWR(1);
  a_emalloc_ctx->node_free_count++;

  EMALLOC_STATS_INC_IF(1);
  if (a_emalloc_ctx->node_free_count > 1) {
    coalesce(a_emalloc_ctx, idx);
  } else {
    EMALLOC_STATS_INC_RDWR(2);
    a_emalloc_ctx->low_free_node_idx = idx;
    a_emalloc_ctx->hi_free_node_idx = idx;
  }

  return EMALLOC_OK;
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

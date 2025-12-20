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
#include <assert.h>
#include <emalloc/emalloc.h>
#include <stdbool.h>

// Definitions
// /////////////////////////////////////////////////////////////////////////////

#define EMALLOC_MIN_ALLOC_SHIFTS (4)
#define EMALLOC_MIN_ALLOC_SIZE (1 << (EMALLOC_MIN_ALLOC_SHIFTS))  // 16 bytes
#define EMALLOC_ALLOC_INFO_MASK (0x0000000F)
#define EMALLOC_IS_SORTED (0xFFFFFFFF)
#define EMALLOC_HIGHEST_IDX (0xFFFFFFFF)
#define EMALLOC_NO_DANGLING (0xFFFFFFFF)

#define EMALLOC_INTERNAL_CHECKS 1

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

#define EMALLOC_STATS_INC_IF(n)                         \
  if (s_pOpStats->n_ifs < (0xFFFFFFFFFFFFFFFF - (n))) { \
    s_pOpStats->n_ifs += (n);                           \
  }

#define EMALLOC_STATS_INC_LOOPS(n)                        \
  if (s_pOpStats->n_loops < (0xFFFFFFFFFFFFFFFF - (n))) { \
    s_pOpStats->n_loops += (n);                           \
  }

#define EMALLOC_STATS_INC_RDWR(n)                               \
  if (s_pOpStats->n_nodes_rd_wr < (0xFFFFFFFFFFFFFFFF - (n))) { \
    s_pOpStats->n_nodes_rd_wr += (n);                           \
  }

#define EMALLOC_STATS_INC_CALLS(n)                \
  if (s_pOpStats->n_calls < (0xFFFFFFFF - (n))) { \
    s_pOpStats->n_calls += (n);                   \
  }
#else
#define EMALLOC_STATS_INC_IF(n)
#define EMALLOC_STATS_INC_LOOPS(n)
#define EMALLOC_STATS_INC_RDWR(n)
#define EMALLOC_STATS_INC_CALLS(n)
#endif

#if (EMALLOC_INTERNAL_CHECKS == 1)
static bool emalloc_memory_is_valid(sEMALLOC_ctx* a_emalloc_ctx) {
  if (a_emalloc_ctx->node_count > a_emalloc_ctx->nodes_poll_length) {
    return false;
  }

  if (a_emalloc_ctx->start_idx_of_unsorted_node != EMALLOC_IS_SORTED) {
    if (a_emalloc_ctx->start_idx_of_unsorted_node >=
        a_emalloc_ctx->node_count) {
      return false;
    }
  }

  sEMALLOC_node* node = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;

  uint32_t total_size_bytes = 0;  // free + allocated
  uint32_t total_nodes_free = 0;  // free nodes
  uint32_t total_allocated_size_bytes = 0;
  uint32_t allocated_but_not_used_count = 0;

  for (uint32_t i = 0; i < a_emalloc_ctx->node_count; i++) {
    const uint32_t alloc_info = node[i].offset & EMALLOC_ALLOC_INFO_MASK;
    if (alloc_info != EMALLOC_NODE_ALLOCATED_BUT_NOT_USED) {
      const uint32_t size_bytes_node = node[i].alloc_info;
      total_size_bytes += size_bytes_node;

      if (alloc_info == EMALLOC_NODE_VARIABLE_SIZE) {
        total_allocated_size_bytes += size_bytes_node;
      }
    } else {
      allocated_but_not_used_count++;
    }

    if (alloc_info == EMALLOC_NODE_FREE) {
      total_nodes_free++;
    }
  }

  if (total_allocated_size_bytes != a_emalloc_ctx->external_allocated_bytes) {
    return false;
  }

  if (allocated_but_not_used_count !=
      a_emalloc_ctx->allocated_but_not_used_count) {
    return false;
  }

  if (total_nodes_free != a_emalloc_ctx->node_free_count) {
    return false;
  }

  if (total_size_bytes != a_emalloc_ctx->external_memory_size_bytes) {
    return false;
  }

  return true;
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
  a_emalloc_ctx->start_idx_of_unsorted_node = EMALLOC_IS_SORTED;
  a_emalloc_ctx->allocated_but_not_used_count = 0;

  // Initialize first node, covering entire external memory
  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_configuration->nodes_poll;

  nodes[0].offset = EMALLOC_NODE_FREE;
  nodes[0].alloc_info = a_emalloc_configuration->external_memory_size_bytes;

#if (EMALLOC_INTERNAL_CHECKS == 1)
  EMALLOC_ASSERT(emalloc_memory_is_valid(a_emalloc_ctx));
#endif

  return EMALLOC_OK;
}

static bool is_node_free(const sEMALLOC_node* a_node) {
  EMALLOC_STATS_INC_RDWR(1);
  return (a_node->offset & EMALLOC_ALLOC_INFO_MASK) == EMALLOC_NODE_FREE;
}

static uint32_t find_free_node(sEMALLOC_ctx* a_emalloc_ctx, uint32_t a_size) {
  sEMALLOC_node* node = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;

  // Start from the end and work backwards
  for (int32_t i = (int32_t)(a_emalloc_ctx->node_count) - 1; i >= 0; --i) {
    EMALLOC_STATS_INC_LOOPS(1);

    const sEMALLOC_node* node_i = &node[i];

    EMALLOC_STATS_INC_RDWR(1);
    const bool isNodeFree =
        (node_i->offset & EMALLOC_ALLOC_INFO_MASK) == EMALLOC_NODE_FREE;

    EMALLOC_STATS_INC_IF(1);
    if (isNodeFree) {
      uint32_t total_size = node_i->alloc_info;
      uint32_t merge_count = 0;
      EMALLOC_STATS_INC_RDWR(1);

      // Check adjacent free nodes backwards and calculate total size
      int32_t j = i - 1;  // NOLINT
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

      const int32_t merge_start_idx = j + 1;

      // Merge adjacent free nodes if any were found
      EMALLOC_STATS_INC_IF(1);
      if (merge_count > 0) {
        // Update i to skip merged nodes
        i = j;

        // Set total size of the merge
        node[merge_start_idx].alloc_info = total_size;
        EMALLOC_STATS_INC_RDWR(1);

        a_emalloc_ctx->allocated_but_not_used_count += merge_count;

        EMALLOC_ASSERT(a_emalloc_ctx->node_free_count >= merge_count);

        a_emalloc_ctx->node_free_count -= merge_count;
        EMALLOC_STATS_INC_RDWR(2);
      }

      // First-fit
      EMALLOC_STATS_INC_IF(1);
      if (total_size >= a_size) {
#if (EMALLOC_INTERNAL_CHECKS == 1)
        EMALLOC_ASSERT(emalloc_memory_is_valid(a_emalloc_ctx));
#endif

        return merge_start_idx;
      }
    }

#if (EMALLOC_INTERNAL_CHECKS == 1)
    EMALLOC_ASSERT(emalloc_memory_is_valid(a_emalloc_ctx));
#endif
  }

  EMALLOC_STATS_INC_IF(1);

  if (a_emalloc_ctx->node_count == a_emalloc_ctx->nodes_poll_length) {
    return EMALLOC_ERR_NO_MORE_FREE_NODES;
  }

  return EMALLOC_ERR_NO_EXTERNAL_MEMORY;
}

/**
 * @brief INSERTION SORT
 *  - Worst-case performance  O( n^2 )
 *  - Best-case performance O( n )
 *  - Average performance O( n^2 )
 * It gives the best results on simulations with tested algorithms.
 * Other algorithms tested: bubble, shell, shell tokuda, insertion binary,
 * insertion sentinel, heap sort.
 *
 * @param a_emalloc_ctx current context
 */
static void sort_nodes(sEMALLOC_ctx* a_emalloc_ctx) {
  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;

  for (uint32_t i = (a_emalloc_ctx->start_idx_of_unsorted_node + 1);
       i < a_emalloc_ctx->node_count; i++) {
    EMALLOC_STATS_INC_LOOPS(1);

    sEMALLOC_node key;

    EMALLOC_STATS_INC_RDWR(1);
    key.offset = nodes[i].offset;

    EMALLOC_STATS_INC_RDWR(1);
    key.alloc_info = nodes[i].alloc_info;

    int32_t j = (int32_t)(i - 1);  // NOLINT

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

/**
 * @brief Perform a interpolation method search.
 * pre-conditions: nodes need to be sort.
 *
 * @param a_emalloc_ctx - current context
 * @param a_offset_to_search - offset value to search
 * @param a_high_idx
 * @return uint32_t
 */
static uint32_t interpolationSearch(const sEMALLOC_ctx* a_emalloc_ctx,
                                    uint32_t a_offset_to_search,
                                    uint32_t a_high_idx) {
  uint32_t lowIdx = 0;
  uint32_t highIdx = a_high_idx;

  const sEMALLOC_node* nodes = (const sEMALLOC_node*)a_emalloc_ctx->nodes_poll;

  EMALLOC_STATS_INC_RDWR(1);
  const uint32_t lowestOffset = nodes[lowIdx].offset & ~EMALLOC_ERR_MASK;

  EMALLOC_STATS_INC_IF(1);
  if (a_offset_to_search < lowestOffset) {
    return EMALLOC_ERR_OFFSET_NOT_FOUND;
  }

  EMALLOC_STATS_INC_RDWR(1);
  const uint32_t highestOffset = nodes[highIdx].offset & ~EMALLOC_ERR_MASK;

  EMALLOC_STATS_INC_IF(1);
  if (a_offset_to_search > highestOffset) {
    return EMALLOC_ERR_OFFSET_NOT_FOUND;
  }

  // Assume values inside range only,
  // they will return EMALLOC_ERR_OFFSET_NOT_FOUND anyway, but will take longer.

  EMALLOC_STATS_INC_IF(1);
  while (lowIdx <= highIdx) {
    EMALLOC_STATS_INC_LOOPS(1);

    EMALLOC_STATS_INC_RDWR(1);
    const uint32_t lowOffset = nodes[lowIdx].offset & ~EMALLOC_ERR_MASK;

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
    const uint32_t highOffset = nodes[highIdx].offset & ~EMALLOC_ERR_MASK;

    EMALLOC_ASSERT(highOffset > lowOffset);

    // Estimate position using interpolation formula
    const uint32_t estimatedIdx =
        lowIdx + (((highIdx - lowIdx) * (a_offset_to_search - lowOffset)) /
                  (highOffset - lowOffset));

    EMALLOC_STATS_INC_RDWR(1);
    const uint32_t offsetFromEstimatedIdx =
        nodes[estimatedIdx].offset & ~EMALLOC_ERR_MASK;

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

static void coalesce(sEMALLOC_ctx* a_emalloc_ctx, uint32_t a_released_idx) {
  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;

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
    EMALLOC_ASSERT(emalloc_memory_is_valid(a_emalloc_ctx));
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
        }
      }
#if (EMALLOC_INTERNAL_CHECKS == 1)
      EMALLOC_ASSERT(emalloc_memory_is_valid(a_emalloc_ctx));
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
        } else {
          if (shift_state == (0x02 | 0x01)) {
            nodes[a_released_idx + 0].offset |=
                EMALLOC_NODE_ALLOCATED_BUT_NOT_USED;
            nodes[a_released_idx + 1].offset |=
                EMALLOC_NODE_ALLOCATED_BUT_NOT_USED;
            EMALLOC_STATS_INC_RDWR(2);

            a_emalloc_ctx->allocated_but_not_used_count += 2;
            a_emalloc_ctx->node_free_count -= 2;

            EMALLOC_STATS_INC_RDWR(2);
          } else {
            // No merged happened
          }
        }
      }

#if (EMALLOC_INTERNAL_CHECKS == 1)
      EMALLOC_ASSERT(emalloc_memory_is_valid(a_emalloc_ctx));
#endif
    }
  }
}

uint32_t de_dangling_and_search_first_fit(sEMALLOC_ctx* a_emalloc_ctx,
                                          uint32_t a_alloc_size) {
#if (EMALLOC_INTERNAL_CHECKS == 1)
  EMALLOC_ASSERT(emalloc_memory_is_valid(a_emalloc_ctx));
#endif

  EMALLOC_ASSERT(a_emalloc_ctx);
  EMALLOC_ASSERT(a_emalloc_ctx->allocated_but_not_used_count > 0);
  EMALLOC_ASSERT(a_emalloc_ctx->node_count > 2);

  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;

  // Compact the array by removing dangling nodes and searching for first fit
  uint32_t write_idx = 0;
  uint32_t read_idx = a_emalloc_ctx->node_count - 1;

  EMALLOC_STATS_INC_RDWR(1);
  while (write_idx <= read_idx) {
    EMALLOC_STATS_INC_LOOPS(1);

    const sEMALLOC_node* write_node = &nodes[write_idx];

    const uint32_t write_alloc_info_bits =
        write_node->offset & EMALLOC_ALLOC_INFO_MASK;
    EMALLOC_STATS_INC_RDWR(1);

    EMALLOC_STATS_INC_IF(1);
    if (write_alloc_info_bits != EMALLOC_NODE_VARIABLE_SIZE) {
      EMALLOC_STATS_INC_IF(1);
      if (write_alloc_info_bits == EMALLOC_NODE_ALLOCATED_BUT_NOT_USED) {
        // Check if we've finished searching
        EMALLOC_STATS_INC_IF(1);
        if (read_idx == write_idx) {
          EMALLOC_STATS_INC_IF(1);
          if ((nodes[write_idx].offset & EMALLOC_ALLOC_INFO_MASK) ==
              EMALLOC_NODE_ALLOCATED_BUT_NOT_USED) {
            a_emalloc_ctx->node_count--;
            a_emalloc_ctx->allocated_but_not_used_count--;
            EMALLOC_STATS_INC_RDWR(2);
            break;
          }
        }

        // Find non-dangling node from the end (read_idx) to write idx and swap
        while (read_idx > write_idx) {
          EMALLOC_STATS_INC_LOOPS(1);

          const sEMALLOC_node* read_node = &nodes[read_idx];
          const uint32_t read_alloc_info_bits =
              read_node->offset & EMALLOC_ALLOC_INFO_MASK;
          EMALLOC_STATS_INC_RDWR(1);

          EMALLOC_STATS_INC_IF(1);
          if (read_alloc_info_bits != EMALLOC_NODE_ALLOCATED_BUT_NOT_USED) {
            // Update start_idx_of_unsorted_node
            const uint32_t start_idx_of_unsorted_node = write_idx;

            EMALLOC_STATS_INC_IF(1);
            if (start_idx_of_unsorted_node <
                a_emalloc_ctx->start_idx_of_unsorted_node) {
              a_emalloc_ctx->start_idx_of_unsorted_node =
                  start_idx_of_unsorted_node;

              EMALLOC_STATS_INC_IF(1);
              if (write_idx == 0) {
                a_emalloc_ctx->offset_before_unsorted_node =
                    nodes[0].offset & EMALLOC_ALLOC_INFO_MASK;
              } else {
                a_emalloc_ctx->offset_before_unsorted_node =
                    (nodes[write_idx - 1].offset & EMALLOC_ALLOC_INFO_MASK);
              }
              EMALLOC_STATS_INC_RDWR(2);
            }

            // Swap nodes
            nodes[write_idx] = *read_node;
            EMALLOC_STATS_INC_RDWR(4);

            a_emalloc_ctx->node_count--;
            a_emalloc_ctx->allocated_but_not_used_count--;
            EMALLOC_STATS_INC_RDWR(2);

            read_idx--;

            // An used node was found, nothing more to do
            break;
          }

          // Skip/discard dangling node at end
          a_emalloc_ctx->node_count--;
          a_emalloc_ctx->allocated_but_not_used_count--;
          EMALLOC_STATS_INC_RDWR(2);
          read_idx--;
        }

        // Check if we've finished searching
        EMALLOC_STATS_INC_IF(1);
        if (read_idx == write_idx) {
          if ((nodes[write_idx].offset & EMALLOC_ALLOC_INFO_MASK) ==
              EMALLOC_NODE_ALLOCATED_BUT_NOT_USED) {
            a_emalloc_ctx->node_count--;
            a_emalloc_ctx->allocated_but_not_used_count--;
            EMALLOC_STATS_INC_RDWR(2);
            break;
          }
        }

        // First-fit search (nodes[write_idx] may had changed here)
        EMALLOC_STATS_INC_IF(1);
        if ((nodes[write_idx].offset & EMALLOC_ALLOC_INFO_MASK) ==
            EMALLOC_NODE_FREE) {
          EMALLOC_STATS_INC_RDWR(1);
          EMALLOC_STATS_INC_IF(1);
          if (nodes[write_idx].alloc_info >= a_alloc_size) {
#if (EMALLOC_INTERNAL_CHECKS == 1)
            EMALLOC_ASSERT(emalloc_memory_is_valid(a_emalloc_ctx));
#endif

            return write_idx;
          }
        }
      } else {
        // First-fit search
        EMALLOC_STATS_INC_IF(1);
        if (write_alloc_info_bits == EMALLOC_NODE_FREE) {
          EMALLOC_STATS_INC_RDWR(1);
          EMALLOC_STATS_INC_IF(1);
          if (write_node->alloc_info >= a_alloc_size) {
#if (EMALLOC_INTERNAL_CHECKS == 1)
            EMALLOC_ASSERT(emalloc_memory_is_valid(a_emalloc_ctx));
#endif

            return write_idx;
          }
        }
      }
    }

    write_idx++;
  }

#if (EMALLOC_INTERNAL_CHECKS == 1)
  EMALLOC_ASSERT(emalloc_memory_is_valid(a_emalloc_ctx));
#endif

  EMALLOC_STATS_INC_IF(1);
  if (a_emalloc_ctx->start_idx_of_unsorted_node != EMALLOC_IS_SORTED) {
    sort_nodes(a_emalloc_ctx);

    EMALLOC_STATS_INC_RDWR(1);
    a_emalloc_ctx->start_idx_of_unsorted_node = EMALLOC_IS_SORTED;
  }

  // If no First-fit found,
  // try to merge free nodes from end to begin
  for (int32_t i = (int32_t)(a_emalloc_ctx->node_count - 1); i > 0; --i) {
    EMALLOC_STATS_INC_LOOPS(1);

    const bool is_i_m1_free = is_node_free(&nodes[i - 1]);

    EMALLOC_STATS_INC_IF(1);
    if (!is_i_m1_free) {
      // No need to retest this index again on thext loop.
      i--;  // if -1 index is free, it is possible to skip the next one
      continue;
    }

    EMALLOC_STATS_INC_IF(1);
    if (is_node_free(&nodes[i])) {
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
#if (EMALLOC_INTERNAL_CHECKS == 1)
          EMALLOC_ASSERT(emalloc_memory_is_valid(a_emalloc_ctx));
#endif

          return i - 1;
        }
      }

#if (EMALLOC_INTERNAL_CHECKS == 1)
      EMALLOC_ASSERT(emalloc_memory_is_valid(a_emalloc_ctx));
#endif
    }
  }

  return EMALLOC_ERR_OFFSET_NOT_FOUND;
}

uint32_t emalloc_alloc(sEMALLOC_ctx* a_emalloc_ctx, uint32_t a_alloc_size) {
#if (EMALLOC_STATISTICS == 1)
  s_pOpStats = &s_stats.alloc;
  EMALLOC_STATS_INC_CALLS(1);
#endif

  EMALLOC_STATS_INC_IF(1);
  if (a_emalloc_ctx == NULL) {
    return EMALLOC_ERR_INVALID_PARAMETER;
  }

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

  uint32_t idx_found = EMALLOC_ERR_OFFSET_NOT_FOUND;

  if (a_emalloc_ctx->allocated_but_not_used_count) {
    idx_found = de_dangling_and_search_first_fit(a_emalloc_ctx, a_alloc_size);

    EMALLOC_STATS_INC_IF(1);
    if (idx_found == EMALLOC_ERR_OFFSET_NOT_FOUND) {
      idx_found = find_free_node(a_emalloc_ctx, a_alloc_size);
    }
  } else {
    idx_found = find_free_node(a_emalloc_ctx, a_alloc_size);
  }

  EMALLOC_STATS_INC_IF(1);
  if (idx_found >= EMALLOC_ERR_OFFSET_NOT_FOUND) {
    return idx_found;
  }

  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;
  sEMALLOC_node* node = &nodes[idx_found];

  EMALLOC_ASSERT(is_node_free(node));

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
      a_emalloc_ctx->node_count = node_count + 1;

      const uint32_t new_free_node_idx = node_count;

      nodes[new_free_node_idx].offset =
          (offset + a_alloc_size) | EMALLOC_NODE_FREE;
      nodes[new_free_node_idx].alloc_info = node->alloc_info - a_alloc_size;
      node->alloc_info = a_alloc_size;

      // free node count does not change here,
      // because one (free) node was allocated and a new (free) node is created.
      // So, -1 + 1 = 0

      EMALLOC_STATS_INC_RDWR(5);

      EMALLOC_STATS_INC_IF(1);
      if ((idx_found + 1) != new_free_node_idx) {
        // It is only unsorted if created node is not adjacent.

        const uint32_t start_idx_of_unsorted_node = idx_found + 1;

        EMALLOC_STATS_INC_IF(1);
        if (start_idx_of_unsorted_node <
            a_emalloc_ctx->start_idx_of_unsorted_node) {
          a_emalloc_ctx->start_idx_of_unsorted_node =
              start_idx_of_unsorted_node;
          a_emalloc_ctx->offset_before_unsorted_node = offset;
          EMALLOC_STATS_INC_RDWR(2);
        }
      }
    } else {
      EMALLOC_ASSERT(a_emalloc_ctx->node_free_count > 0);
      a_emalloc_ctx->node_free_count--;
      EMALLOC_STATS_INC_RDWR(1);
    }
  } else {
    EMALLOC_ASSERT(node->alloc_info == a_alloc_size);
    EMALLOC_ASSERT(a_emalloc_ctx->node_free_count > 0);
    a_emalloc_ctx->node_free_count--;
    EMALLOC_STATS_INC_RDWR(1);
  }

  // NOTE: usually node->alloc_info == a_alloc_size, but,
  // when no more nodes left, the node allocated has all the remain size
  a_emalloc_ctx->external_allocated_bytes += node->alloc_info;
  EMALLOC_STATS_INC_RDWR(1);

#if (EMALLOC_INTERNAL_CHECKS == 1)
  EMALLOC_ASSERT(emalloc_memory_is_valid(a_emalloc_ctx));
#endif

  return offset;
}

uint32_t emalloc_free(sEMALLOC_ctx* a_emalloc_ctx,
                      uint32_t a_allocated_offset) {
#if (EMALLOC_STATISTICS == 1)
  s_pOpStats = &s_stats.free;
  EMALLOC_STATS_INC_CALLS(1);
#endif
  EMALLOC_STATS_INC_IF(1);
  if (a_emalloc_ctx == NULL) {
    return EMALLOC_ERR_INVALID_PARAMETER;
  }

  EMALLOC_STATS_INC_IF(1);
  if ((a_allocated_offset & EMALLOC_ALLOC_INFO_MASK) != 0) {
    return EMALLOC_ERR_INVALID_PARAMETER;
  }

  uint32_t idx = EMALLOC_ERR_OFFSET_NOT_FOUND;

  EMALLOC_STATS_INC_IF(1);
  if (a_emalloc_ctx->start_idx_of_unsorted_node != EMALLOC_IS_SORTED) {
    EMALLOC_STATS_INC_IF(1);
    if (a_allocated_offset <= a_emalloc_ctx->offset_before_unsorted_node) {
      // If the offset to search is below the one we know that all indexes are
      // ordered up there, we don't need to sort. Only perform search of the
      // ordered part.
      idx = interpolationSearch(a_emalloc_ctx, a_allocated_offset,
                                a_emalloc_ctx->start_idx_of_unsorted_node - 1);
      EMALLOC_ASSERT(idx != EMALLOC_ERR_OFFSET_NOT_FOUND);
    } else {
      sort_nodes(a_emalloc_ctx);

      EMALLOC_STATS_INC_RDWR(1);
      a_emalloc_ctx->start_idx_of_unsorted_node = EMALLOC_IS_SORTED;
    }
  }

  EMALLOC_STATS_INC_IF(1);
  if (idx == EMALLOC_ERR_OFFSET_NOT_FOUND) {
    idx = interpolationSearch(a_emalloc_ctx, a_allocated_offset,
                              a_emalloc_ctx->node_count - 1);

    EMALLOC_STATS_INC_IF(1);
    if (idx == EMALLOC_ERR_OFFSET_NOT_FOUND) {
      return EMALLOC_ERR_OFFSET_NOT_FOUND;
    }
  }

  sEMALLOC_node* nodes = (sEMALLOC_node*)a_emalloc_ctx->nodes_poll;
  sEMALLOC_node* node = &nodes[idx];

  // Check is need because it can be a double free
  EMALLOC_STATS_INC_IF(1);
  if (is_node_free(node)) {
    return EMALLOC_ERR_OFFSET_NOT_FOUND;
  }

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
  }

  return EMALLOC_OK;
}

#if (EMALLOC_STATISTICS == 1)
void emalloc_reset_statistics(void) {
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

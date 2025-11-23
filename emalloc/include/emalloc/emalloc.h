// /////////////////////////////////////////////////////////////////////////////
/// @file emalloc.h
/// @brief Declaration for emalloc
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
#ifndef EMALLOC_INCLUDE_EMALLOC_EMALLOC_H_
#define EMALLOC_INCLUDE_EMALLOC_EMALLOC_H_

// Includes
// /////////////////////////////////////////////////////////////////////////////
#include <stdbool.h>
#include <stdint.h>

// clang-format off
#ifdef __cplusplus
extern "C" {
#endif
// clang-format on

// Definitions
// /////////////////////////////////////////////////////////////////////////////

// INSERTION SORT
//  Worst-case performance  O( n^2 )
//  Best-case performance O( n )
//  Average performance O( n^2 )
// It gives the best results on simulations.

#define EMALLOC_USE_BUBBLE_SORT 0
#define EMALLOC_USE_SHELL_SORT 0
#define EMALLOC_USE_INSERTION_SORT 1
#define EMALLOC_USE_HEAP_SORT_MAX 0

#define EMALLOC_USE_SEARCH_INTERPOLATION 1

#ifndef EMALLOC_STATISTICS
#define EMALLOC_STATISTICS 0
#endif

#ifndef EMALLOC_USE_ASSERT
#define EMALLOC_USE_ASSERT 0
#define EMALLOC_ASSERT()
#endif

#if (EMALLOC_USE_ASSERT == 0)
#define EMALLOC_ASSERT()
#else
#include <assert.h>
#define EMALLOC_ASSERT(a) assert(a)
#endif

typedef struct s_emalloc_config {
  uint64_t* nodes_poll;                 ///< each node size is 8 bytes
  uint32_t nodes_poll_length;           ///< number of nodes in the nodes_poll
  uint32_t external_memory_size_bytes;  ///< external storage size to manage
} sEMALLOC_cfg;

typedef struct s_emalloc_state {
  uint64_t* nodes_poll;                 ///< each node size is 8 bytes
  uint32_t nodes_poll_length;           ///< number of nodes in the nodes_poll
  uint32_t external_memory_size_bytes;  ///< external storage size to manage
  uint32_t external_allocated_bytes;    ///< current external memory allocated
  uint32_t node_count;                  ///< current node counter
  uint32_t node_free_count;             ///< current free node counter
  uint32_t low_free_node_idx;           ///< lowest index of a free node
  uint32_t hi_free_node_idx;            ///< highest index of a free node
  uint32_t start_idx_of_unsorted_node;  ///< 0xFFFFFFFF means it is sorted.
  uint32_t start_idx_of_alloc_not_used;
  uint32_t allocated_but_not_used_count;
} sEMALLOC_ctx;

// Return errors
typedef enum e_emalloc_err {
  EMALLOC_ERR_NO_EXTERNAL_MEMORY = 0xFFFFFFFF,
  EMALLOC_ERR_NO_MORE_FREE_NODES = 0xFFFFFFFE,
  EMALLOC_ERR_INVALID_PARAMETER = 0xFFFFFFFD,
  EMALLOC_ERR_ZERO_REQUESTED = 0xFFFFFFFC,
  EMALLOC_ERR_OFFSET_NOT_FOUND = 0xFFFFFFFB,
  EMALLOC_OK = 0
} eEMALLOC_err;

#define EMALLOC_ERR_MASK (0x0000000F)

/**
 * @brief Initialize emalloc context
 *
 * @param a_emalloc_ctx context to initialize
 * @param sEMALLOC_cfg pointer to configuration
 * @return EMALLOC_ERR
 */
uint32_t emalloc_init(sEMALLOC_ctx* a_emalloc_ctx,
                      const sEMALLOC_cfg* a_emalloc_configuration);

/**
 * @brief Allocate the a_alloc_size on the current context
 *
 * @param a_emalloc_ctx current context
 * @param a_alloc_size desired size to allocate
 * @retval uint32_t offset (starts at 0) of the external storage to be used.
 * @retval if return EMALLOC_ERR_MASK is no zero, it is an error. Check defines EMALLOC_ERR_*
 */
uint32_t emalloc_alloc(sEMALLOC_ctx* a_emalloc_ctx, uint32_t a_alloc_size);

/**
 * @brief Free the allocated given offset
 *
 * @param a_emalloc_ctx current context
 * @param a_allocated_offset desired offset to deallocate
 * @retval 0 if ok
 * @retval if return EMALLOC_ERR_MASK is no zero, it is an error. Check defines EMALLOC_ERR_*
 */
uint32_t emalloc_free(sEMALLOC_ctx* a_emalloc_ctx, uint32_t a_allocated_offset);

#if (EMALLOC_STATISTICS == 1)
typedef struct s_emalloc_operation_stats {
  uint32_t n_calls;
  uint64_t n_ifs;
  uint64_t n_loops;
  uint64_t n_nodes_rd_wr;  ///< read or write of 4 bytes
} sEMALLOC_operation_stats;

typedef struct s_emalloc_stats {
  sEMALLOC_operation_stats alloc;
  sEMALLOC_operation_stats free;
} sEMALLOC_statistics;

void emalloc_reset_statistics();
void emalloc_get_statistics(sEMALLOC_statistics* a_out_statistics);
#endif

#ifdef __cplusplus
}
#endif

#endif  // EMALLOC_INCLUDE_EMALLOC_EMALLOC_H_

// EOF
// /////////////////////////////////////////////////////////////////////////////

// /////////////////////////////////////////////////////////////////////////////
/// @file test_emalloc_overlap.cpp
/// @brief Unit tests for detect overlap regions
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
#include <CppUTest/TestHarness.h>
#include <CppUTest/UtestMacros.h>
#include <emalloc/emalloc.h>
#include <vector>
#include "helper_log.hpp"

// Structure to track active allocations
struct Allocation {
  uint32_t offset;
  uint32_t size;
  bool active;

  Allocation(uint32_t o, uint32_t s) : offset(o), size(s), active(true) {}

  uint32_t end() const { return offset + size; }

  bool overlaps(const Allocation& other) const {
    if (!active || !other.active) {
      return false;
    }

    // Check if ranges overlap
    return !((end() <= other.offset) || (other.end() <= offset));
  }
};

// NOLINTBEGIN
TEST_GROUP(MemoryOverlapDetection) {
  static constexpr uint32_t EXT_RAM_SIZE = 8192;
  static constexpr uint32_t MAX_NODES = EXT_RAM_SIZE / 16;

  uint64_t nodes_poll_[MAX_NODES];
  uint8_t external_ram_[EXT_RAM_SIZE];
  std::vector<Allocation> allocations_;

  sEMALLOC_ctx emalloc_ctx_;

  // Use fixed seed for reproducibility
  uint32_t random_seed = 42;

  void setup() {
    memset(nodes_poll_, 0, sizeof(nodes_poll_));
    memset(external_ram_, 0, sizeof(external_ram_));
    allocations_.clear();

    sEMALLOC_cfg emalloc_configuration;

    emalloc_configuration.nodes_poll = nodes_poll_;
    emalloc_configuration.nodes_poll_length = MAX_NODES;
    emalloc_configuration.external_memory_size_bytes = EXT_RAM_SIZE;

    CHECK_EQUAL(EMALLOC_OK,
                emalloc_init(&emalloc_ctx_, &emalloc_configuration));
  }

  void teardown() {}

  // Helper: Check if new allocation overlaps with any active allocation
  bool check_overlap(uint32_t offset, uint32_t size) {
    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY)
      return false;  // Failed allocation

    Allocation new_alloc(offset, size);

    // cppcheck-suppress useStlAlgorithm
    for (const auto& alloc : allocations_) {
      if (alloc.active && new_alloc.overlaps(alloc)) {
        return true;  // Overlap detected!
      }
    }
    return false;
  }

  // Helper: Add allocation to tracking list
  void track_allocation(uint32_t offset, uint32_t size) {
    if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      allocations_.push_back(Allocation(offset, size));
    }
  }

  // Helper: Mark allocation as freed
  void mark_freed(uint32_t offset) {
    for (auto& alloc : allocations_) {
      // cppcheck-suppress useStlAlgorithm
      if (alloc.offset == offset && alloc.active) {
        alloc.active = false;
        return;
      }
    }
  }

  // Helper: Verify no overlaps in all active allocations
  bool verify_no_overlaps() {
    for (size_t i = 0; i < allocations_.size(); i++) {
      if (!allocations_[i].active)
        continue;

      for (size_t j = i + 1; j < allocations_.size(); j++) {
        if (!allocations_[j].active)
          continue;

        if (allocations_[i].overlaps(allocations_[j])) {
          printf("OVERLAP DETECTED: [%u-%u] overlaps [%u-%u]\n",
                 allocations_[i].offset, allocations_[i].end(),
                 allocations_[j].offset, allocations_[j].end());
          return false;
        }
      }
    }
    return true;
  }
};
// NOLINTEND

TEST(MemoryOverlapDetection, TwoSequentialAllocationsDoNotOverlap) {
  uint32_t offset1 = emalloc_alloc(&emalloc_ctx_, 100);
  CHECK_EQUAL(0, offset1 & EMALLOC_ERR_MASK);
  track_allocation(offset1, 100);

  uint32_t offset2 = emalloc_alloc(&emalloc_ctx_, 100);
  CHECK_EQUAL(0, offset2 & EMALLOC_ERR_MASK);

  CHECK(!check_overlap(offset2, 100));
  track_allocation(offset2, 100);

  CHECK(verify_no_overlaps());
}

TEST(MemoryOverlapDetection, MultipleAllocationsNoOverlap) {
  for (int i = 0; i < 10; i++) {
    uint32_t offset = emalloc_alloc(&emalloc_ctx_, 100);
    CHECK_EQUAL(0, offset & EMALLOC_ERR_MASK);

    CHECK(!check_overlap(offset, 100));
    track_allocation(offset, 100);
  }

  CHECK(verify_no_overlaps());
}

TEST(MemoryOverlapDetection, VaryingSizesNoOverlap) {
  uint32_t sizes[] = {50, 100, 200, 75, 150, 300, 25, 125};

  for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
    uint32_t offset = emalloc_alloc(&emalloc_ctx_, sizes[i]);
    CHECK_EQUAL(0, offset & EMALLOC_ERR_MASK);

    CHECK(!check_overlap(offset, sizes[i]));
    track_allocation(offset, sizes[i]);
  }

  CHECK(verify_no_overlaps());
}

TEST(MemoryOverlapDetection, FreeAndReallocateNoOverlap) {
  uint32_t offset1 = emalloc_alloc(&emalloc_ctx_, 200);
  CHECK_EQUAL(0, offset1 & EMALLOC_ERR_MASK);

  track_allocation(offset1, 200);

  uint32_t offset2 = emalloc_alloc(&emalloc_ctx_, 200);
  CHECK_EQUAL(0, offset2 & EMALLOC_ERR_MASK);

  track_allocation(offset2, 200);

  uint32_t offset3 = emalloc_alloc(&emalloc_ctx_, 200);
  CHECK_EQUAL(0, offset3 & EMALLOC_ERR_MASK);

  track_allocation(offset3, 200);

  // Free middle allocation
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx_, offset2));
  mark_freed(offset2);

  // Allocate in freed space
  uint32_t offset4 = emalloc_alloc(&emalloc_ctx_, 200);
  CHECK_EQUAL(0, offset4 & EMALLOC_ERR_MASK);

  CHECK(!check_overlap(offset4, 200));

  track_allocation(offset4, 200);

  CHECK(verify_no_overlaps());
}

TEST(MemoryOverlapDetection, FreeAndReallocateSmallerNoOverlap) {
  uint32_t offset1 = emalloc_alloc(&emalloc_ctx_, 400);
  CHECK_EQUAL(0, offset1 & EMALLOC_ERR_MASK);
  track_allocation(offset1, 400);

  uint32_t offset2 = emalloc_alloc(&emalloc_ctx_, 400);
  CHECK_EQUAL(0, offset2 & EMALLOC_ERR_MASK);
  track_allocation(offset2, 400);

  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx_, offset1));
  mark_freed(offset1);

  // Allocate smaller in freed space
  uint32_t offset3 = emalloc_alloc(&emalloc_ctx_, 100);
  CHECK_EQUAL(0, offset3 & EMALLOC_ERR_MASK);
  CHECK(!check_overlap(offset3, 100));
  track_allocation(offset3, 100);

  uint32_t offset4 = emalloc_alloc(&emalloc_ctx_, 100);
  CHECK_EQUAL(0, offset4 & EMALLOC_ERR_MASK);
  CHECK(!check_overlap(offset4, 100));
  track_allocation(offset4, 100);

  CHECK(verify_no_overlaps());
}

TEST(MemoryOverlapDetection, AlternatingFreeAndAllocateNoOverlap) {
  // Allocate 5 blocks
  for (int i = 0; i < 5; i++) {
    uint32_t offset = emalloc_alloc(&emalloc_ctx_, 200);
    CHECK_EQUAL(0, offset & EMALLOC_ERR_MASK);
    track_allocation(offset, 200);
  }

  // Free alternating blocks (0, 2, 4)
  for (size_t i = 0; i < allocations_.size(); i += 2) {
    CHECK_EQUAL(EMALLOC_OK,
                emalloc_free(&emalloc_ctx_, allocations_[i].offset));
    mark_freed(allocations_[i].offset);
  }

  // Allocate in freed spaces
  for (int i = 0; i < 3; i++) {
    uint32_t offset = emalloc_alloc(&emalloc_ctx_, 150);
    CHECK_EQUAL(0, offset & EMALLOC_ERR_MASK);
    CHECK(!check_overlap(offset, 150));
    track_allocation(offset, 150);
  }

  CHECK(verify_no_overlaps());
}

TEST(MemoryOverlapDetection, RandomSizesNoOverlap) {
  for (int i = 0; i < 30; i++) {
    uint32_t size = 50 + (rand_r(&random_seed) % 300);
    uint32_t offset = emalloc_alloc(&emalloc_ctx_, size);

    CHECK_EQUAL(0, offset & EMALLOC_ERR_MASK);

    if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      CHECK(!check_overlap(offset, size));
      track_allocation(offset, size);
    }
  }

  CHECK(verify_no_overlaps());
}

TEST(MemoryOverlapDetection, MaximumFragmentationNoOverlap) {
  // Allocate many blocks
  for (int i = 0; i < 20; i++) {
    uint32_t offset = emalloc_alloc(&emalloc_ctx_, 200);
    CHECK_EQUAL(0, offset & EMALLOC_ERR_MASK);

    track_allocation(offset, 200);
  }

  // Free every other block
  for (size_t i = 0; i < allocations_.size(); i += 2) {
    CHECK_EQUAL(EMALLOC_OK,
                emalloc_free(&emalloc_ctx_, allocations_[i].offset));
    mark_freed(allocations_[i].offset);
  }

  // Try to allocate in fragments
  for (int i = 0; i < 10; i++) {
    uint32_t offset = emalloc_alloc(&emalloc_ctx_, 100);
    if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      CHECK(!check_overlap(offset, 100));
      track_allocation(offset, 100);
    }
  }

  CHECK(verify_no_overlaps());
}

TEST(MemoryOverlapDetection, CoalescingDoesNotCreateOverlap) {
  uint32_t offset1 = emalloc_alloc(&emalloc_ctx_, 300);
  CHECK_EQUAL(0, offset1 & EMALLOC_ERR_MASK);
  track_allocation(offset1, 300);

  uint32_t offset2 = emalloc_alloc(&emalloc_ctx_, 300);
  CHECK_EQUAL(0, offset2 & EMALLOC_ERR_MASK);
  track_allocation(offset2, 300);

  uint32_t offset3 = emalloc_alloc(&emalloc_ctx_, 300);
  CHECK_EQUAL(0, offset3 & EMALLOC_ERR_MASK);
  track_allocation(offset3, 300);

  uint32_t offset4 = emalloc_alloc(&emalloc_ctx_, 300);
  CHECK_EQUAL(0, offset4 & EMALLOC_ERR_MASK);
  track_allocation(offset4, 300);

  // Free adjacent blocks (should coalesce)
  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx_, offset1));
  mark_freed(offset1);

  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx_, offset2));
  mark_freed(offset2);

  // Allocate larger block in coalesced space
  uint32_t offset5 = emalloc_alloc(&emalloc_ctx_, 600);
  CHECK(!check_overlap(offset5, 600));
  track_allocation(offset5, 600);

  CHECK(verify_no_overlaps());
}

TEST(MemoryOverlapDetection, BoundaryConditionsNoOverlap) {
  // Allocate at start
  uint32_t offset1 = emalloc_alloc(&emalloc_ctx_, 100);
  CHECK_EQUAL(0, offset1 & EMALLOC_ERR_MASK);
  track_allocation(offset1, 100);

  // Allocate next to it
  uint32_t offset2 = emalloc_alloc(&emalloc_ctx_, 100);
  CHECK_EQUAL(0, offset2 & EMALLOC_ERR_MASK);
  CHECK(!check_overlap(offset2, 100));
  track_allocation(offset2, 100);

  CHECK(verify_no_overlaps());
}

TEST(MemoryOverlapDetection, TinyAllocationsNoOverlap) {
  for (int i = 0; i < 50; i++) {
    uint32_t offset = emalloc_alloc(&emalloc_ctx_, 32);
    CHECK_EQUAL(0, offset & EMALLOC_ERR_MASK);
    if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      CHECK(!check_overlap(offset, 32));
      track_allocation(offset, 32);
    }
  }

  CHECK(verify_no_overlaps());
}

TEST(MemoryOverlapDetection, LargeAllocationsNoOverlap) {
  uint32_t offset1 = emalloc_alloc(&emalloc_ctx_, 2000);
  CHECK_EQUAL(0, offset1 & EMALLOC_ERR_MASK);
  track_allocation(offset1, 2000);

  uint32_t offset2 = emalloc_alloc(&emalloc_ctx_, 2000);
  CHECK_EQUAL(0, offset2 & EMALLOC_ERR_MASK);
  CHECK(!check_overlap(offset2, 2000));
  track_allocation(offset2, 2000);

  uint32_t offset3 = emalloc_alloc(&emalloc_ctx_, 2000);
  CHECK_EQUAL(0, offset3 & EMALLOC_ERR_MASK);
  CHECK(!check_overlap(offset3, 2000));
  track_allocation(offset3, 2000);

  CHECK(verify_no_overlaps());
}

TEST(MemoryOverlapDetection, MixedOperationsStressTest) {
  for (int cycle = 0; cycle < 20; cycle++) {
    // Allocate phase
    for (int i = 0; i < 5; i++) {
      uint32_t size = 50 + (rand_r(&random_seed) % 200);
      uint32_t offset = emalloc_alloc(&emalloc_ctx_, size);

      if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
        CHECK(!check_overlap(offset, size));
        track_allocation(offset, size);
      }
    }

    // Free phase - free random allocations
    for (int i = 0; i < 3; i++) {
      if (!allocations_.empty()) {
        size_t idx = rand_r(&random_seed) % allocations_.size();
        if (allocations_[idx].active) {
          CHECK_EQUAL(EMALLOC_OK,
                      emalloc_free(&emalloc_ctx_, allocations_[idx].offset));
          mark_freed(allocations_[idx].offset);
        }
      }
    }

    // Verify no overlaps after each cycle
    CHECK(verify_no_overlaps());
  }
}

TEST(MemoryOverlapDetection, SplitBlockNoOverlap) {
  uint32_t offset1 = emalloc_alloc(&emalloc_ctx_, 1000);
  CHECK_EQUAL(0, offset1 & EMALLOC_ERR_MASK);
  track_allocation(offset1, 1000);

  CHECK_EQUAL(EMALLOC_OK, emalloc_free(&emalloc_ctx_, offset1));
  mark_freed(offset1);

  // Allocate smaller blocks from split
  uint32_t offset2 = emalloc_alloc(&emalloc_ctx_, 200);
  CHECK_EQUAL(0, offset2 & EMALLOC_ERR_MASK);
  CHECK(!check_overlap(offset2, 200));
  track_allocation(offset2, 200);

  uint32_t offset3 = emalloc_alloc(&emalloc_ctx_, 300);
  CHECK_EQUAL(0, offset3 & EMALLOC_ERR_MASK);
  CHECK(!check_overlap(offset3, 300));
  track_allocation(offset3, 300);

  uint32_t offset4 = emalloc_alloc(&emalloc_ctx_, 400);
  CHECK_EQUAL(0, offset4 & EMALLOC_ERR_MASK);
  CHECK(!check_overlap(offset4, 400));
  track_allocation(offset4, 400);

  CHECK(verify_no_overlaps());
}

TEST(MemoryOverlapDetection, FullMemoryUtilizationNoOverlap) {
  uint32_t total_allocated = 0;

  while (total_allocated < EXT_RAM_SIZE) {
    uint32_t size = 100;
    uint32_t offset = emalloc_alloc(&emalloc_ctx_, size);

    if (offset == EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
      break;  // Out of memory
    }

    CHECK(!check_overlap(offset, size));
    track_allocation(offset, size);
    total_allocated += size;
  }

  CHECK(verify_no_overlaps());
}

TEST(MemoryOverlapDetection, ReallocateAfterMultipleFrees) {
  // Allocate 10 blocks
  for (int i = 0; i < 10; i++) {
    uint32_t offset = emalloc_alloc(&emalloc_ctx_, 200);
    CHECK_EQUAL(0, offset & EMALLOC_ERR_MASK);
    track_allocation(offset, 200);
  }

  // Free blocks 2, 3, 4
  for (size_t i = 2; i <= 4; i++) {
    CHECK_EQUAL(EMALLOC_OK,
                emalloc_free(&emalloc_ctx_, allocations_[i].offset));
    mark_freed(allocations_[i].offset);
  }

  // Allocate in freed space
  uint32_t offset = emalloc_alloc(&emalloc_ctx_, 600);
  CHECK_EQUAL(0, offset & EMALLOC_ERR_MASK);

  if (offset != EMALLOC_ERR_NO_EXTERNAL_MEMORY) {
    CHECK(!check_overlap(offset, 600));
    track_allocation(offset, 600);
  }

  CHECK(verify_no_overlaps());
}

TEST(MemoryOverlapDetection, ZeroSizeAllocationSafety) {
  uint32_t offset1 = emalloc_alloc(&emalloc_ctx_, 100);
  CHECK_EQUAL(0, offset1 & EMALLOC_ERR_MASK);
  track_allocation(offset1, 100);

  // Zero size should fail
  uint32_t offset2 = emalloc_alloc(&emalloc_ctx_, 0);
  CHECK_EQUAL(EMALLOC_ERR_ZERO_REQUESTED, offset2);

  uint32_t offset3 = emalloc_alloc(&emalloc_ctx_, 100);
  CHECK_EQUAL(0, offset3 & EMALLOC_ERR_MASK);
  CHECK(!check_overlap(offset3, 100));
  track_allocation(offset3, 100);

  CHECK(verify_no_overlaps());
}

// EOF
// /////////////////////////////////////////////////////////////////////////////

// /////////////////////////////////////////////////////////////////////////////
/// @file test_emalloc_init.cpp
/// @brief Unit tests for emalloc_init
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

// Definitions
// /////////////////////////////////////////////////////////////////////////////
#define EMALLOC_MIN_MEMORY_SIZE (16)

// Setup
// /////////////////////////////////////////////////////////////////////////////

// clang-format off
// NOLINTBEGIN
TEST_GROUP(Initialization) {
  void setup() {
  }

  void teardown() {
  }
};
// NOLINTEND
// clang-format on

TEST(Initialization, null_pointers) {
  sEMALLOC_ctx emalloc_ctx;
  sEMALLOC_cfg emalloc_configuration;

  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER, emalloc_init(NULL, NULL));
  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER, emalloc_init(&emalloc_ctx, NULL));
  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_init(NULL, &emalloc_configuration));
}

TEST(Initialization, invalid_config) {
  sEMALLOC_ctx emalloc_ctx;
  sEMALLOC_cfg emalloc_configuration;
  uint64_t nodes_poll[1];
  emalloc_configuration.nodes_poll = NULL;
  emalloc_configuration.nodes_poll_length = 0;
  emalloc_configuration.external_memory_size_bytes = 0;

  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_init(&emalloc_ctx, &emalloc_configuration));

  emalloc_configuration.nodes_poll = nodes_poll;
  emalloc_configuration.nodes_poll_length = 0;
  emalloc_configuration.external_memory_size_bytes = 0;

  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_init(&emalloc_ctx, &emalloc_configuration));

  emalloc_configuration.nodes_poll = nodes_poll;
  emalloc_configuration.nodes_poll_length = 1;
  emalloc_configuration.external_memory_size_bytes = 0;

  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_init(&emalloc_ctx, &emalloc_configuration));

  emalloc_configuration.nodes_poll = nodes_poll;
  emalloc_configuration.nodes_poll_length = 1;
  emalloc_configuration.external_memory_size_bytes =
      EMALLOC_MIN_MEMORY_SIZE - 1;

  CHECK_EQUAL(EMALLOC_ERR_INVALID_PARAMETER,
              emalloc_init(&emalloc_ctx, &emalloc_configuration));
}

TEST(Initialization, valid_config) {
  sEMALLOC_ctx emalloc_ctx;
  sEMALLOC_cfg emalloc_configuration;
  uint64_t nodes_poll[1];

  emalloc_configuration.nodes_poll = nodes_poll;
  emalloc_configuration.nodes_poll_length = 1;
  emalloc_configuration.external_memory_size_bytes = EMALLOC_MIN_MEMORY_SIZE;

  CHECK_EQUAL(EMALLOC_OK, emalloc_init(&emalloc_ctx, &emalloc_configuration));
}

// EOF
// /////////////////////////////////////////////////////////////////////////////

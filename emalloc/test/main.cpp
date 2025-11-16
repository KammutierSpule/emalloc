// /////////////////////////////////////////////////////////////////////////////
/// @file main.cpp
/// @brief Test entry
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

#include <CppUTest/CommandLineTestRunner.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>

FILE* s_benchmark_frag_log_file = nullptr;
FILE* s_benchmark_tests_log_file = nullptr;

int main(int argc, char** argv) {
  srand(time(nullptr));
  int ret = RUN_ALL_TESTS(argc, argv);

  if (s_benchmark_frag_log_file != nullptr) {
    fclose(s_benchmark_frag_log_file);
  }

  if (s_benchmark_tests_log_file != nullptr) {
    fclose(s_benchmark_tests_log_file);
  }

  return ret;
}

// EOF
// /////////////////////////////////////////////////////////////////////////////

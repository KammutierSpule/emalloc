# Include FetchContent module
include(FetchContent)

# Fetch CppUTest
FetchContent_Declare(CppUTest
                     GIT_REPOSITORY https://github.com/cpputest/cpputest.git)

# Configure CppUTest options
set(CPPUTEST_MEM_LEAK_DETECTION
    ON
    CACHE BOOL "Enable memory leak detection")
set(CPPUTEST_STD_CPP_LIB_DISABLED
    OFF
    CACHE BOOL "Use standard C++ library")

# Make CppUTest available
FetchContent_MakeAvailable(CppUTest)

# Enable testing
enable_testing()

# Test files
set(TEST_SOURCES
    main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/emalloc/test_emalloc_init.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/emalloc/test_emalloc_fragmentation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/emalloc/test_emalloc_alloc_free.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/emalloc/test_emalloc_overlap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/emalloc/benchmark_fragmentation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/emalloc/benchmark_tests.cpp)

# Create test executable
add_executable(RunAllTests ${TEST_SOURCES})

# Link against CppUTest
target_link_libraries(RunAllTests PRIVATE emalloc CppUTest CppUTestExt)

# Include directories
target_include_directories(RunAllTests
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/emalloc)

# Add test to CTest
add_test(NAME AllTests COMMAND RunAllTests)

# Custom target to run tests with verbose output
add_custom_target(
  run_tests
  COMMAND ${CMAKE_CURRENT_BINARY_DIR}/RunAllTests -v
  DEPENDS RunAllTests
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Running CppUTest tests...")

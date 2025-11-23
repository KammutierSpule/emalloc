cmake_minimum_required(VERSION 3.28)

project(emalloc_test LANGUAGES C CXX)

add_compile_definitions(EMALLOC_STATISTICS=1)
add_compile_definitions(EMALLOC_USE_ASSERT=1)

add_subdirectory(emalloc/)
add_subdirectory(emalloc/test)

# ##############################################################################
# * From: https://github.com/abdes/asap
# * License: BSD-3-Clause license
# ##############################################################################

# Generate a clangd configuration file that points to the compilation database
# in the cmake build directory. We need to do this as the build directory is
# different for every preset and can be different as well when the user decides
# to build somewhere else. Currently we cannot configure this properly in vscode
# settings. See https://github.com/clangd/vscode-clangd/issues/48
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/.clangd)
  configure_file(.clangd.in ${CMAKE_SOURCE_DIR}/.clangd @ONLY)
endif()
# ##############################################################################

set(LIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/emalloc.c)

# Create static library from production code
add_library(emalloc STATIC ${LIB_SOURCES})

# Include directories for the library
target_include_directories(
  emalloc
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

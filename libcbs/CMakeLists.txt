cmake_minimum_required(VERSION 3.13.4)

# Use the prebuilt clang compiler to build libcbs
set(BSC_CLANG "${CMAKE_BINARY_DIR}/bin/clang")

if(NOT BSC_CLANG)
  find_program(BSC_CLANG NAMES clang clang-X.Y)  
endif()

set( CMAKE_C_COMPILER ${BSC_CLANG} )

# Compile the source code, create multiple object files from each folder.
add_subdirectory(src)

# TODO: learn from libc
# Pack the object files and create a final library we want to ship.
# add_subdirectory(lib)

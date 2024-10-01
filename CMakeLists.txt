# Configure CMake
cmake_minimum_required(VERSION 3.10)

# Project name
project(tmascope)

# Find DynamoRIO (See https://dynamorio.org/page_build_client.html)
find_package(DynamoRIO)
if (NOT DynamoRIO_FOUND)
  message(FATAL_ERROR "DynamoRIO package required to build")
endif(NOT DynamoRIO_FOUND)

# Add the proxy
add_library(tmascope SHARED src/perf.cpp)

# Add and configure DynamoRIO
set(DynamoRIO_USE_LIBC OFF)
configure_DynamoRIO_client(tmascope)
use_DynamoRIO_extension(tmascope drmgr)
use_DynamoRIO_extension(tmascope drsyms)
use_DynamoRIO_extension(tmascope drwrap)
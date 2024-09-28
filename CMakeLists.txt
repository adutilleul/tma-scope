# Configure CMake
cmake_minimum_required(VERSION 3.10)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

# Project name
project(tmascope)

# Options
option(VERBOSE "Whether or not verbose logging is enabled" OFF)

# Find QBDI (See https://qbdi.readthedocs.io/en/stable/installation_and_integration.html?highlight=cmake#single-architecture)
find_package(QBDI REQUIRED)
find_package(QBDIPreload REQUIRED)

# Find PAPI
# find_package(PAPI REQUIRED)


# Add the library
include_directories(src)
file(GLOB LIB "src/*")

if(DEBUG)
  add_definitions(-D VERBOSE)
endif(DEBUG)

# Add the executable
add_library(tmascope SHARED src/perf.cpp ${LIB})

# Add elfutils
target_link_libraries(tmascope elf)

# Add QBDI
target_link_libraries(tmascope QBDIPreload::QBDIPreload QBDI::QBDI_static)

# Add PAPI
# target_link_libraries(tmascope ${PAPI_LIBRARIES})

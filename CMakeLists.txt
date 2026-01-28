cmake_minimum_required(VERSION 3.10)
project(aout VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/hash)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/testy)

# Hash table library
add_library(hash STATIC
    hash/hash.c
    hash/hash.h
)

# Aout library
add_library(aout STATIC
    aout.c
    aout.h
)

target_link_libraries(aout hash)

# Test framework
add_library(testy STATIC
    testy/test_main.c
    testy/test.h
)

# Test executable
add_executable(test_aout
    test_aout.c
)

target_link_libraries(test_aout aout hash testy)

# Enable testing
enable_testing()

# Add test
add_test(NAME aout_tests COMMAND test_aout)

# Install targets
install(TARGETS aout hash
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES aout.h DESTINATION include)

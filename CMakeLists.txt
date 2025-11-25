cmake_minimum_required(VERSION 3.10)
project(vISA C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")

# Source files
set(SOURCES
    src/main.c
    src/hypervisor.c
)

# Create executable
add_executable(vISA ${SOURCES})

# Include directories
target_include_directories(vISA PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Optional: Create a library for the VM core
add_library(visa_core STATIC src/vm.c)
target_include_directories(visa_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Tests (optional)
enable_testing()
# add_executable(test_vm tests/test_vm.c)
# target_link_libraries(test_vm visa_core)
# add_test(NAME test_vm COMMAND test_vm)

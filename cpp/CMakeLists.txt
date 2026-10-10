cmake_minimum_required(VERSION 3.10)
project(poisoning)

# Use only C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -march=native")

# Include directories
include_directories(include)

find_package(Boost 1.74 REQUIRED)

# Core library
file(GLOB_RECURSE CORE_SOURCES 
    "src/core/*.cpp" 
    "src/utils/*.cpp" 
    "src/generators/*.cpp"
    "src/models/*.cpp"
)
if(NOT CORE_SOURCES)
    message(WARNING "No core sources found")
endif()
add_library(poisoning_core STATIC ${CORE_SOURCES})
# consecutive_w_* sources in the core library need Boost headers
target_link_libraries(poisoning_core PUBLIC Boost::boost)

# Executables
add_executable(calc_loss apps/calc_loss_main.cpp)
add_executable(measure_query_time apps/measure_query_time_main.cpp)
add_executable(gen_sync apps/gen_sync_main.cpp)
add_executable(gen_real apps/gen_real_main.cpp)

# Add inject_poison executable  
add_executable(inject_poison apps/inject_poison_main.cpp)

# Add inject_poison_duplicate_allowed executable
add_executable(inject_poison_duplicate_allowed apps/inject_poison_duplicate_allowed_main.cpp)

# Add inject_poison_random executable (random poisoning strategy)
add_executable(inject_poison_random apps/inject_poison_random_main.cpp)

# Add inject_poison_consecutive_w_endpoints executable
add_executable(
    inject_poison_consecutive_w_endpoints
    apps/inject_poison_consecutive_w_endpoints_main.cpp
)

# Add inject_poison_consecutive_w_endpoints_duplicate_allowed executable
add_executable(
    inject_poison_consecutive_w_endpoints_duplicate_allowed
    apps/inject_poison_consecutive_w_endpoints_duplicate_allowed_main.cpp
)

# Add inject_poison_consecutive_w_endpoints_using_relaxed_solution executable
add_executable(
    inject_poison_consecutive_w_endpoints_using_relaxed_solution
    apps/inject_poison_consecutive_w_endpoints_using_relaxed_solution_main.cpp
)

# Add new split upper bound executables
add_executable(calc_upper_bound_binary apps/calc_upper_bound_binary_main.cpp)
add_executable(calc_upper_bound_golden apps/calc_upper_bound_golden_main.cpp)
add_executable(calc_upper_bound_strict apps/calc_upper_bound_strict_main.cpp)

# Brute-force optimal poison executable (step 5.1)
add_executable(calc_optimal_poison apps/calc_optimal_poison_main.cpp)

# Brute-force optimal poison executable with duplicate allowed (step 5.2)
add_executable(calc_optimal_poison_duplicate_allowed apps/calc_optimal_poison_duplicate_allowed_main.cpp)

# CI smoke test (handmade tiny datasets + golden asserts)
add_executable(smoke_test apps/smoke_test.cpp)

# Link core library to all executables (common)
set(POISONING_TARGETS
    calc_loss measure_query_time gen_sync gen_real
    inject_poison inject_poison_duplicate_allowed inject_poison_random
    inject_poison_consecutive_w_endpoints
    inject_poison_consecutive_w_endpoints_duplicate_allowed
    inject_poison_consecutive_w_endpoints_using_relaxed_solution
    calc_upper_bound_binary calc_upper_bound_golden calc_upper_bound_strict
    calc_optimal_poison calc_optimal_poison_duplicate_allowed
    smoke_test
)
foreach(target ${POISONING_TARGETS})
    target_link_libraries(${target} PRIVATE poisoning_core)
endforeach()

cmake_minimum_required(VERSION 3.0.0)
project(OptimizationTest VERSION 0.1.0)

include(CTest)
enable_testing()

add_compile_options(-O2 -Wall -Wextra -Werror -g)
add_link_options()
add_executable(old_flawed_integers old_flawed_integers.cpp)
add_executable(cursed_integer cursed_integer.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

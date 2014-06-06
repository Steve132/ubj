cmake_minimum_required(VERSION 2.8)
project(ubj)

add_library(ubjw ubjw.c ubj.h)
#add_library(ubjr ubjr.c ubjr.h)
#add_library(ubjw++ ubjw++.cpp ubjw++.hpp)
add_executable(ubjwtest ubjw_test.c)

target_link_libraries(ubjwtest ubjw)
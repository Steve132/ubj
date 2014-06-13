cmake_minimum_required(VERSION 2.8)
project(ubj)

add_library(ubj ubjw.c ubjr.c ubjrw.c ubj_internal.h ubj.h)

add_executable(ubjwtest ubjw_test.c)
add_executable(ubjrtest ubjr_test.c)

target_link_libraries(ubjwtest ubj)
target_link_libraries(ubjrtest ubj)


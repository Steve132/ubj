cmake_minimum_required(VERSION 2.8)

set(SOURCES ubjson.cpp)

add_executable(ubjtest ubjson_v2.cpp)
add_executable(ubjwtest ubjw.c ubjw.h)
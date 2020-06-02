cmake_minimum_required(VERSION 3.15)
project(aufgabe3C C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS -pthread -lcurl)



add_executable(aufgabe3C main.c)
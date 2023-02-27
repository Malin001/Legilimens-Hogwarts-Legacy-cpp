cmake_minimum_required(VERSION 3.23)
project(Legilimens_Hogwarts_Legacy_cpp)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXE_LINKER_FLAGS "-static")

add_executable(Legilimens main.cpp collectibles.cpp collectibles.h sqlite3.c sqlite3.h getsave.h getsave.cpp argparse.hpp tabulate.hpp color.hpp)

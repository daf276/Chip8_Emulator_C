cmake_minimum_required(VERSION 3.7)
project(Chip8_Emulator_C++)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lSDL2")

add_executable(Chip8 main.cpp Chip8.cpp Chip8.h)
add_executable(UnitTests Chip8.cpp Chip8.h Chip8_Test.cpp)
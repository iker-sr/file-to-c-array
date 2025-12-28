cmake_minimum_required(VERSION 3.16)

project(FileToCArray C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(ftocarray)
target_sources(ftocarray PRIVATE
    src/file_to_c_array.c
    src/main.c
)
target_include_directories(ftocarray PRIVATE include)

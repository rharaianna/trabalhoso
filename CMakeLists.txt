cmake_minimum_required(VERSION 3.10)

project(trabalhoso C)

set(CMAKE_C_STANDARD 11)

add_executable(trabalhoso
        disk.c
        inode.c
        main.c
        myfs.c
        util.c
        vfs.c
)


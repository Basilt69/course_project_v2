cmake_minimum_required(VERSION 3.22)
project (course_project_v2 C)

set(CMAKE_C_STANDARD 99)

add_executable(course_project_v2 server.c client.c)

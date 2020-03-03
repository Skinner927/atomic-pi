cmake_minimum_required (VERSION 3.11)
project (atomic-pi LANGUAGES C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O2")

include_directories("ArmbianIO")
add_library(armbianio STATIC ArmbianIO/armbianio.c)

include_directories("MAX7219")
add_library(max7219 STATIC MAX7219/max7219.c)
target_link_libraries(max7219 pthread m armbianio)

include_directories("src")
add_executable(display src/display.c)
target_link_libraries(display max7219)

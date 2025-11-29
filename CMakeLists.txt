cmake_minimum_required(VERSION 3.10)
project(hypercam2)

set(CMAKE_C_STANDARD 11)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-video-1.0 gstreamer-app-1.0)

include_directories(${GTK3_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})

link_directories(${GTK3_LIBRARY_DIRS})
link_directories(${GSTREAMER_LIBRARY_DIRS})

add_definitions(${GTK3_CFLAGS_OTHER})
add_definitions(${GSTREAMER_CFLAGS_OTHER})

add_executable(hypercam2 main.c)

target_link_libraries(hypercam2 ${GTK3_LIBRARIES})
target_link_libraries(hypercam2 ${GSTREAMER_LIBRARIES})
target_link_libraries(hypercam2 X11)
target_link_libraries(hypercam2 m)

target_compile_options(hypercam2 PRIVATE ${GTK3_CFLAGS})
target_compile_options(hypercam2 PRIVATE ${GSTREAMER_CFLAGS})

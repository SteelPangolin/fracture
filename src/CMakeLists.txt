cmake_minimum_required(VERSION 2.6)
project(FRACTURE)

find_path(CF_INC_DIR CoreFoundation/CoreFoundation.h)
find_library(CF_LIB CoreFoundation)

find_path(APPSVCS_INC_DIR ApplicationServices/ApplicationServices.h)
find_library(APPSVCS_LIB ApplicationServices)

find_path(OPENGL_INC_DIR OpenGL/OpenGL.h)
find_library(OPENGL_LIB OpenGL)

add_executable(fracture fracture.c errors.c glio.c)
target_link_libraries(fracture ${CF_LIB} ${APPSVCS_LIB} ${OPENGL_LIB})

add_executable(fpstats fpstats.c errors.c)
target_link_libraries(fpstats ${CF_LIB} ${APPSVCS_LIB} ${OPENGL_LIB})
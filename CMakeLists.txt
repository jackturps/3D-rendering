cmake_minimum_required(VERSION 3.10)
project(3D)

add_executable(main main.c shaders.h cJSON/cJSON.c base64/base64.c)

find_library(OPENGL_LIBRARY OpenGL)
find_library(GLUT_LIBRARY GLUT)
target_link_libraries(main ${OPENGL_LIBRARY} ${GLUT_LIBRARY})
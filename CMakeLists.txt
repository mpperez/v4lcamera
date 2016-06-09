cmake_minimum_required(VERSION 2.9.1)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
add_subdirectory (src)
include_directories (./include/)
include_directories (./src/)
set (EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/./bin CACHE PATH "Directory for Executables")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

#PROJECT(v4lcamera)

SET(SRC ./src/main.cpp)

ADD_EXECUTABLE(qtwebcam ${SRC})
target_link_libraries(qtwebcam v4lcamera Qt5::Widgets )



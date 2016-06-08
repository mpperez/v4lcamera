cmake_minimum_required(VERSION 2.8.3)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
include_directories (../include/)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

SET ( LIBSRC
 v4lcamera.cpp  v4lcapture.cpp
)
ADD_LIBRARY ( v4lcamera SHARED ${LIBSRC})
target_link_libraries(v4lcamera Qt5::Widgets Qt5::Gui Qt5::Core)

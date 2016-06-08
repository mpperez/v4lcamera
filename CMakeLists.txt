cmake_minimum_required(VERSION 2.8.3)
add_subdirectory (src)

set (LIBRARY_OUTPUT_PATH ${libv4lcamera}/build/lib
     CACHE PATH "Directory for Libraries")
set (EXECUTABLE_OUTPUT_PATH ${v4lcamera}/build/bin
     CACHE PATH "Directory for Executables")
PROJECT(v4lcamera)
SET(SRC
v4lcamera.cpp  v4lcapture.cpp
)

SET ( LIBSRC
 v4lcamera.cpp  v4lcapture.cpp
)
ADD_EXECUTABLE(v4lcamera ${SRC})
ADD_LIBRARY ( libv4lcamera SHARED ${LIBSRC})

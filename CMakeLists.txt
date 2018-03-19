# Copyright 2018, mike at github.com/iskinmike
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required ( VERSION 2.8.12 )

project ( ff_test CXX )

set(AVCONV_INC "/usr/include/x86_64-linux-gnu")
set(AVCONV_PATH "/usr/lib/x86_64-linux-gnu")

include_directories ( ${PROJECT_NAME}
        ${AVCONV_INC}
        ${LIBAVDEVICE_PATH})

link_directories("${AVCONV_PATH}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g")
set(SOURCES
    src/frame_keeper.cpp
    src/decoder.cpp
    src/encoder.cpp
    src/display.cpp
    src/video_api.cpp
    src/api_usage.cpp)


add_executable(${PROJECT_NAME} ${SOURCES})

set(LIBS 
  avformat
  avcodec
  swscale
  avdevice
  swresample
  avutil
  SDL2
  pthread
  )

target_link_libraries(${PROJECT_NAME} ${LIBS})

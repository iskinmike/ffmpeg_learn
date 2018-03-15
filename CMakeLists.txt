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
set(LIBAVDEVICE_PATH "/home/mikhail/workspace/utils/ffmpeg-2.8.14/libavdevice")

include_directories ( ${PROJECT_NAME}
        ${AVCONV_INC}
        ${LIBAVDEVICE_PATH})

link_directories("${AVCONV_PATH}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g")

add_executable(${PROJECT_NAME} main.cpp)

set(LIBS 
#  ${AVCONV_PATH}/libavcodec-ffmpeg.so
#  avformat
#  avutil
  avformat
  avcodec
  swscale
  /usr/lib/x86_64-linux-gnu/libavdevice-ffmpeg.so.56 # TODO
  swresample
  avutil
  SDL2
#  /lib/x86_64-linux-gnu/libbz2.so.1

#  ${AVCONV_PATH}/libavdevice-ffmpeg.so.56
#  avcodec
#  avdevice
#  avfilter
#  avresample
#  swresample
#  postproc
  )

target_link_libraries(${PROJECT_NAME} ${LIBS})

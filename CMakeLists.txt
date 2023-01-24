cmake_minimum_required(VERSION 3.10.0)

project(comedirecord VERSION 1.31 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED)

add_executable(comedirecord
  comedirecord.cpp
  comediscope.cpp
  gain.cpp
  ext_data_receive.cpp
  dc_sub.cpp
  lp.cpp
  hp.cpp
  fftscope.cpp
  channel.cpp
)

target_link_libraries(comedirecord Qt5::Widgets qwt-qt5 comedi iir fftw3)

install(TARGETS comedirecord)

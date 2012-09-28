cmake_minimum_required(VERSION 2.8)
project(export-quadtree)

find_package(EIGEN  CONFIG PATHS cmake)
find_package(BOOST  CONFIG PATHS cmake)

include_directories(src)
include_directories(${EIGEN_INCLUDE_DIRS})
include_directories(${BOOST_INCLUDE_DIRS})

file(GLOB SRCS src/*.c src/*.cc src/*.h src/*.hh)

set(_target export-quadtree)
add_executable(${_target} export-quadtree.c ${SRCS})
target_link_libraries(${_target}
  ${BOOST_LIBRARIES}
  tilebase
  )
tilebase_copy_plugins_to_target(export-quadtree)
nd_copy_plugins_to_target(export-quadtree)
install(TARGETS ${_target} RUNTIME DESTINATION bin)
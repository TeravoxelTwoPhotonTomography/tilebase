cmake_minimum_required(VERSION 2.8)
project(render)

find_package(EIGEN  CONFIG PATHS cmake)

set(HAVE_CUDA 0)
find_package(CUDA 4.0)
if(CUDA_FOUND)
  include_directories(${CUDA_INCLUDE_DIRS})
  set(HAVE_CUDA 1)
endif()
configure_file(${PROJECT_SOURCE_DIR}/config.h.in ${PROJECT_BINARY_DIR}/config.h)

include_directories(src)
include_directories(${PROJECT_BINARY_DIR})
include_directories(${EIGEN_INCLUDE_DIRS})
include_directories(${BOOST_INCLUDE_DIRS})

file(GLOB SRCS src/*.c src/*.cc src/*.h src/*.hh)

set(EXTRA_LIBS)
if(UNIX AND NOT APPLE)
  set(EXTRA_LIBS rt) # for tictoc timer
endif()

set(_target render)
add_executable(${_target} app.c ${SRCS})
target_link_libraries(${_target}
  ${BOOST_LIBRARIES}
  ${EXTRA_LIBS}
  tilebase
  )
set_target_properties(${_target} PROPERTIES INSTALL_RPATH ${RPATH})
tilebase_copy_plugins_to_target(render)
nd_copy_plugins_to_target(render ${ND_PLUGINS})
install(TARGETS ${_target} RUNTIME DESTINATION bin)

install(DIRECTORY util/ DESTINATION scripts/render)

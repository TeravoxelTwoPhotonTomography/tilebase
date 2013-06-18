cmake_minimum_required(VERSION 2.8)
project(align-pair)

find_package(EIGEN  CONFIG PATHS cmake)
find_package(TRE CONFIG PATHS cmake)
include_directories(${TRE_INCLUDE_DIR})
include_directories(${EIGEN_INCLUDE_DIRS})

file(GLOB SRCS src/*.h src/*.c src/*.cc)

set(_target align-pair)
add_executable(${_target} align-pair.c ${SRCS})
target_link_libraries(${_target}
  tilebase
  ${TRE_LIBRARIES}
  )
set_target_properties(${_target} PROPERTIES INSTALL_RPATH ${RPATH})
tilebase_copy_plugins_to_target(${_target})
nd_copy_plugins_to_target(${_target} ${ND_PLUGINS})
install(TARGETS ${_target} RUNTIME DESTINATION bin)

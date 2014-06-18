#
# Eigen 
#

include(ExternalProject)
include(FindPackageHandleStandardArgs)

#set(EIGEN_URL http://bitbucket.org/eigen/eigen/get/3.1.3.tar.gz)
#set(EIGEN_MD5 dc4247efd4f5d796041f999e8774af04)

set(EIGEN_URL http://dl.dropbox.com/u/782372/Software/eigen-eigen-43d9075b23ef.tar.gz)
set(EIGEN_MD5 7f1de87d4bfef65d0c59f15f6697829d)

if(NOT TARGET eigen)
  ExternalProject_Add(eigen
    URL     ${EIGEN_URL}
    URL_MD5 ${EIGEN_MD5}
    PATCH_COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EigenTesting.cmake.patch <SOURCE_DIR>/cmake/EigenTesting.cmake
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
               -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
               -Wno-dev
  )
endif()
get_target_property(EIGEN_ROOT_DIR eigen _EP_INSTALL_DIR)

set(EIGEN_INCLUDE_DIR  ${EIGEN_ROOT_DIR}/include/eigen3)
set(EIGEN_INCLUDE_DIRS ${EIGEN_INCLUDE_DIR})

find_package_handle_standard_args(EIGEN DEFAULT_MSG
  EIGEN_INCLUDE_DIR
)

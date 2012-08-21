include(ExternalProject)
include(FindPackageHandleStandardArgs)

if(NOT TARGET gtest)
  # DOWNLOAD AND BUILD
  ExternalProject_Add(gtest
    SVN_REPOSITORY http://googletest.googlecode.com/svn/trunk/
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
    )
endif()

get_target_property(GTEST_SRC_DIR gtest _EP_SOURCE_DIR)
get_target_property(GTEST_ROOT_DIR gtest _EP_BINARY_DIR)

add_library(libgtest IMPORTED STATIC)
add_library(libgtest-main IMPORTED STATIC)

set_property(TARGET libgtest PROPERTY IMPORTED_LOCATION ${GTEST_ROOT_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}gtest${CMAKE_STATIC_LIBRARY_SUFFIX})
set_property(TARGET libgtest-main PROPERTY IMPORTED_LOCATION ${GTEST_ROOT_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}gtest_main${CMAKE_STATIC_LIBRARY_SUFFIX})

set(GTEST_LIBRARY libgtest)
set(GTEST_MAIN_LIBRARY libgtest-main)
set(GTEST_BOTH_LIBRARIES libgtest libgtest-main)
set(GTEST_INCLUDE_DIR ${GTEST_SRC_DIR}/include)

find_package_handle_standard_args(GTEST DEFAULT_MSG
  GTEST_BOTH_LIBRARIES
  GTEST_INCLUDE_DIR
)
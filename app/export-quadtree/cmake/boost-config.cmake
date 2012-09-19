#
# BOOST Config
#

include(ExternalProject)
include(FindPackageHandleStandardArgs)

if(NOT TARGET boost)
  set( _PATCH_FILE "${PROJECT_BINARY_DIR}/cmake/boost-patch.cmake" )
  set( _SOURCE_DIR "${PROJECT_BINARY_DIR}/boost-prefix/src/boost" ) # Assumes default prefix path
  configure_file(  "${PROJECT_SOURCE_DIR}/cmake/boost-patch.cmake.in" "${_PATCH_FILE}" @ONLY )
  ## DOWNLOAD AND BUILD
  ExternalProject_Add(boost
    URL http://sourceforge.net/projects/boost/files/boost/1.51.0/boost_1_51_0.tar.gz
    URL_MD5 6a1f32d902203ac70fbec78af95b3cf8
    PATCH_COMMAND "${CMAKE_COMMAND};-P;${_PATCH_FILE}" 
    CONFIGURE_COMMAND "<SOURCE_DIR>/bootstrap.sh;--prefix=<INSTALL_DIR>"
    INSTALL_COMMAND ""
    BUILD_IN_SOURCE 1
    BUILD_COMMAND "<SOURCE_DIR>/b2;install"
)
endif()

get_target_property(BOOST_ROOT_DIR boost _EP_INSTALL_DIR)

#set(BOOST_LIBRARIES PARENT_SCOPE)
foreach(_lib chrono context date_time exception filesystem graph iostreams locale
             math_c99 math_c99f math_c99l math_tr1 math_tr1f math_tr1l
             prg_exec_monitor program_options python random regex serialization
             signals system test_exec_monitor thread timer unit_test_framework
             wave wserialization
           )
  if(NOT TARGET libboost_${_lib})
    add_library(libboost_${_lib} STATIC IMPORTED)
    set(BOOST_LIBRARIES ${BOOST_LIBRARIES} libboost_${_lib})
    add_dependencies(libboost_${_lib} boost)
    set_target_properties(libboost_${_lib} PROPERTIES
      IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
      #IMPORTED_IMPLIB           "${BOOST_ROOT_DIR}/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}boost${CMAKE_STATIC_LIBRARY_SUFFIX}"
      IMPORTED_LOCATION         "${BOOST_ROOT_DIR}/lib/${CMAKE_CFG_INTDIR}/${CMAKE_SHARED_LIBRARY_PREFIX}boost_${_lib}${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )
  endif()
endforeach()

set(BOOST_INCLUDE_DIR ${BOOST_ROOT_DIR}/include)
set(BOOST_INCLUDE_DIRS ${BOOST_INCLUDE_DIR})

find_package_handle_standard_args(BOOST DEFAULT_MSG
  BOOST_LIBRARIES
  BOOST_INCLUDE_DIR
)

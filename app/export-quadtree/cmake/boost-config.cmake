#
# BOOST Config
#

# On windows, build 64-bit by default

include(ExternalProject)
include(FindPackageHandleStandardArgs)


if(WIN32)
  macro(msvc_select v1 v2)
    if(MSVC${v1})
      set(MSVC_STR vc${v2})
    endif()
  endmacro()
  msvc_select(60 60)
  msvc_select(70 70)
  msvc_select(71 71)
  msvc_select(80 80)
  msvc_select(90 90)
  msvc_select(10 100)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BOOST_PROPS address-model=64)
  endif()

  set(BOOST_VER "1_51")
  set(BOOTSTRAP bootstrap.bat)
  set(B2ARGS    "--prefix=<INSTALL_DIR>;${BOOST_PROPS};install")
  set(VERDIR    "boost-${BOOST_VER}")
  set(DEBUG_POSTFIX    "-${MSVC_STR}-mt-gd-${BOOST_VER}")
  set(RELEASE_POSTFIX  "-${MSVC_STR}-mt-${BOOST_VER}")
else()
  set(BOOTSTRAP "bootstrap.sh;--prefix=<INSTALL_DIR>;install")
  set(B2ARGS    "install")
  set(VERDIR    ".")
  set(DEBUG_POSTFIX "")
  set(RELEASE_POSTFIX "")
endif()

if(NOT TARGET boost)
  set( _PATCH_FILE "${PROJECT_BINARY_DIR}/cmake/boost-patch.cmake" )
  set( _SOURCE_DIR "${PROJECT_BINARY_DIR}/boost-prefix/src/boost" ) # Assumes default prefix path
  configure_file(  "${PROJECT_SOURCE_DIR}/cmake/boost-patch.cmake.in" "${_PATCH_FILE}" @ONLY )
  ## DOWNLOAD AND BUILD
  ExternalProject_Add(boost
    URL http://sourceforge.net/projects/boost/files/boost/1.51.0/boost_1_51_0.tar.gz
    URL_MD5 6a1f32d902203ac70fbec78af95b3cf8
    PATCH_COMMAND "${CMAKE_COMMAND};-P;${_PATCH_FILE}" 
    CONFIGURE_COMMAND "<SOURCE_DIR>/${BOOTSTRAP}"
    INSTALL_COMMAND ""
    BUILD_IN_SOURCE 1
    BUILD_COMMAND "<SOURCE_DIR>/b2;${B2ARGS}"
)
endif()

get_target_property(BOOST_ROOT_DIR boost _EP_INSTALL_DIR)
show(BOOST_ROOT_DIR)
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
      IMPORTED_LOCATION                 "${BOOST_ROOT_DIR}/lib/libboost_${_lib}${CMAKE_STATIC_LIBRARY_SUFFIX}"
      IMPORTED_LOCATION_RELEASE         "${BOOST_ROOT_DIR}/lib/libboost_${_lib}${RELEASE_POSTFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}"
      IMPORTED_LOCATION_DEBUG           "${BOOST_ROOT_DIR}/lib/libboost_${_lib}${DEBUG_POSTFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )
    #get_target_property(v libboost_${_lib} LOCATION)
    #show(v)
  endif()
endforeach()

set(BOOST_INCLUDE_DIR ${BOOST_ROOT_DIR}/include/${VERDIR})
set(BOOST_INCLUDE_DIRS ${BOOST_INCLUDE_DIR})

find_package_handle_standard_args(BOOST DEFAULT_MSG
  BOOST_LIBRARIES
  BOOST_INCLUDE_DIR
)
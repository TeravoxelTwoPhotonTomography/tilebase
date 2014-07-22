###############################################################################
#  Testing
###############################################################################
find_package(GTEST NO_MODULE PATHS cmake)
file(GLOB TEST_SOURCES *.cc)
if(GTEST_FOUND AND TEST_SOURCES)
  find_package(Threads)
  enable_testing()
  include_directories(${PROJECT_SOURCE_DIR})
  include_directories(${GTEST_INCLUDE_DIR})
  
  add_executable(test-tilebase ${TEST_SOURCES})
  target_link_libraries(test-tilebase
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    tilebase
    )
  add_dependencies(test-tilebase
    gtest
    ${PLUGINS}
  )
  set_target_properties(test-tilebase PROPERTIES INSTALL_RPATH ${RPATH})
  nd_copy_plugins_to_target(test-tilebase ${ND_PLUGINS})
  gtest_copy_shared_libraries(test-tilebase)
  tilebase_copy_plugins_to_target(test-tilebase)
  add_test(TestTilebase test-tilebase)
  install(TARGETS test-tilebase DESTINATION bin/test)
endif()
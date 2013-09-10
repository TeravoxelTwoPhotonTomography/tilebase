  find_package(GTEST  PATHS cmake)
  file(GLOB TEST_SOURCES *.cc)
  if(GTEST_FOUND AND TEST_SOURCES)
    find_package(Threads)
    enable_testing()
    include_directories(${PROJECT_BINARY_DIR})
    include_directories(${GTEST_INCLUDE_DIR})
    add_executable(test-meta-protobuf-v4 ${TEST_SOURCES} ${CFG})
    target_link_libraries(test-meta-protobuf-v4
      ${GTEST_BOTH_LIBRARIES}
      ${CMAKE_THREAD_LIBS_INIT}
      tilebase
      ${PROTOBUF_LIBRARY}
      )
    set_target_properties(test-meta-protobuf-v4 PROPERTIES
      INSTALL_RPATH ${RPATH}
    )
    add_dependencies(test-meta-protobuf-v4 gtest)
    gtest_copy_shared_libraries(test-meta-protobuf-v4)
    add_test(TestMetadataProtobufV4 test-meta-protobuf-v4)
    install(TARGETS test-meta-protobuf-v4
          RUNTIME DESTINATION bin/test)
  endif()


include(GoogleTest)

set(
  TEST_NAMES
    test_async_context
    test_async_tcp_service
    test_async_udp_service
    test_mock_accept
    test_mock_bind
    test_mock_listen
    test_mock_setsockopt
    test_mock_socketpair
)

foreach(TEST_NAME IN LISTS TEST_NAMES)
  add_executable(${TEST_NAME} ${TEST_NAME}.cpp)

  target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include/)

  target_link_libraries(${TEST_NAME} PRIVATE cppnet GTest::gtest_main)

  if(CPPNET_ENABLE_COVERAGE)
    target_link_libraries(${TEST_NAME} PRIVATE gcov)
  endif()

  gtest_discover_tests(${TEST_NAME})
endforeach()

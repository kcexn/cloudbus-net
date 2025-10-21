include(GoogleTest)

set(TEST_NAMES
  test_async_context
  test_async_tcp_service
  test_mock_socketpair
  test_mock_setsockopt
  test_mock_bind
  test_mock_listen
  test_mock_recvmsg
  test_mock_accept
)

foreach(TEST_NAME IN LISTS TEST_NAMES)
  add_executable(
    ${TEST_NAME}
    ${TEST_NAME}.cpp
  )

  target_include_directories(${TEST_NAME}
    PRIVATE
    "${INCLUDE_DIRS}"
  )

  target_link_libraries(
    ${TEST_NAME}
    PRIVATE
    GTest::gtest_main
  )

  if(CB_SEGMENT_ENABLE_COVERAGE)
    target_link_libraries(${TEST_NAME} PRIVATE gcov)
  endif()

  gtest_discover_tests(${TEST_NAME})
endforeach()

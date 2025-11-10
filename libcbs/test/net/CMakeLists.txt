add_libcbs_test_suite(libcbs-net-tests)

add_libcbs_test(
  test_net
  SUITE
    libcbs-net-tests
  SRCS
    test_net.cbs
)

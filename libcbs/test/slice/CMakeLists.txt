add_libcbs_test_suite(libcbs-slice-tests)

add_libcbs_test(
  slice_test
  SUITE
    libcbs-slice-tests
  SRCS
    slice_test.cbs
)

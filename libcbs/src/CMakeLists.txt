set(STDCBS_SOURCES
  bishengc_safety/bishengc_safety.cbs
  net/net.cbs
  string/string.cbs
  hash/sip.cbs
  raw_table/raw_table.cbs
)

set(STDCBS_INCLUDES
  bishengc_safety
  raw_vec
  vec
  slice
  string
  list
  hash
  map
  net
  option
  raw_table
  result
  rc
  cell
)

set(STDCBS_HEADERS
  bishengc_safety/bishengc_safety.hbs
  vec/vec.hbs
  raw_vec/raw_vec.hbs
  string/string.hbs
  list/list.hbs
  hash/sip.hbs
  map/hash_map.hbs
  net/net.hbs
  option/option.hbs
  raw_table/raw_table.hbs
  result/result.hbs
  rc/rc.hbs
  slice/slice.hbs
  cell/cell.hbs
)

if(ENABLE_BSC_LIB_FUTURE)
  list(APPEND STDCBS_SOURCES scheduler/scheduler.cbs)
  list(APPEND STDCBS_HEADERS
    scheduler/queue.hbs
    scheduler/scheduler.hbs
  )
  list(APPEND STDCBS_INCLUDES scheduler)
endif()

add_library(stdcbs STATIC ${STDCBS_SOURCES})


# Do not optimize, otherwise there will be missing symbols in std
target_compile_options(stdcbs PRIVATE -O0)

target_include_directories(stdcbs PUBLIC ${STDCBS_INCLUDES})

# TODO: make it work
# add_dependencies(stdcbs clang)

# install the static library
install(TARGETS stdcbs
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

# install the header files
install(FILES ${STDCBS_HEADERS}
  DESTINATION include/libcbs
)

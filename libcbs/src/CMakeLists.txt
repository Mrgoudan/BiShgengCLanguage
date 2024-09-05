add_library(
    stdcbs STATIC
    bishengc_safety/bishengc_safety.cbs
    string/string.cbs
)

# Do not optimize, otherwise there will be missing symbols in std
target_compile_options(stdcbs PRIVATE -O0)

target_include_directories(stdcbs
    PUBLIC
    bishengc_safety
    raw_vec
    vec
    string
    list
)

# TODO: make it work
# add_dependencies(stdcbs clang)

# install the static library
install(TARGETS stdcbs
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# install the header files
install(FILES  
    bishengc_safety/bishengc_safety.hbs
    vec/vec.hbs 
    raw_vec/raw_vec.hbs 
    string/string.hbs
    list/list.hbs
    DESTINATION include/libcbs
)

include_directories(${LIBINTL_INCLUDE_DIR})
include_directories(${LIBICONV_INCLUDE_DIR})
include_directories(${LIBEXECINFO_INCLUDE_DIR})
include_directories(${DL_INCLUDE_DIR})
include_directories(${PTHREAD_INCLUDE_DIR})

add_subdirectory(lib)
add_subdirectory(core)
add_subdirectory(module)
add_subdirectory(im)
add_subdirectory(frontend)
add_subdirectory(ui)

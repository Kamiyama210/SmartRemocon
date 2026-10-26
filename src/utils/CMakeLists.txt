add_library(utils STATIC
    sys.c
    json.c
    logger.c
)

target_include_directories(utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(utils PUBLIC
    wrappers
)

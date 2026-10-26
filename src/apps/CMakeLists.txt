add_library(apps STATIC
    wdt_monitor.c
    control/control.c
    control/temp_filter.c
    control/control_fsm.c
    network/network.c
    network/command_parser.c
    network/payload.c
)

target_include_directories(apps PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/control
    ${CMAKE_CURRENT_SOURCE_DIR}/network
)

target_link_libraries(apps PUBLIC
    modules
    utils
)

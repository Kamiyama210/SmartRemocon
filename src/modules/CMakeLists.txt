add_library(modules STATIC
    ir_tx.c
    ac_ir.c
    bme280.c
    wifi.c
    mqtt.c
    wdt.c
    rtos.c
)

target_include_directories(modules PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(modules PUBLIC
    wrappers
    utils
)

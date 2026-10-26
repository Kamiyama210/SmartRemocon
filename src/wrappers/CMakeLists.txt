add_library(wrappers STATIC
    wrapper_sys.c
    wrapper_i2c.c
    wrapper_wdt.c
    wrapper_wifi.c
    wrapper_mqtt.c
    wrapper_rtos.c
    wrapper_json.c
    wrapper_pio_ir.c
    ${CMAKE_SOURCE_DIR}/lib/cJSON/cJSON.c
)

pico_generate_pio_header(wrappers
    ${CMAKE_CURRENT_LIST_DIR}/ir_tx.pio
)

target_include_directories(wrappers PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lib/cJSON
    )

target_link_libraries(wrappers PUBLIC
    utils
    pico_stdlib
    hardware_watchdog
    hardware_i2c
    hardware_pio
    hardware_dma
    FreeRTOS-Kernel-Heap4
    pico_cyw43_arch_lwip_sys_freertos
    pico_lwip_mqtt
)

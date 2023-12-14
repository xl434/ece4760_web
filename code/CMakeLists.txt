add_executable(
    looper 
    looper.c
    )

# Add pico_multicore which is required for multicore functionality
target_link_libraries(
        looper 
        pico_stdlib 
        pico_multicore 
        pico_bootsel_via_double_reset 

        hardware_gpio 
        hardware_timer 
        hardware_uart 
        hardware_spi 
        hardware_adc 
        hardware_sync 
        hardware_irq 

        

        hardware_pio 
        hardware_dma 
        pico_float
        
        )

# create map/bin/hex file etc.
pico_add_extra_outputs(looper)
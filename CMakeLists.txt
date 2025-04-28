if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

cmake_minimum_required(VERSION 3.13)

# Configurações básicas
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")
# Configuração do SDK do Raspberry Pi Pico
include(pico_sdk_import.cmake)

# Define o projeto
project(Ohmimetro C CXX ASM)

# Inicializa o SDK do Pico
pico_sdk_init()

# Adiciona os arquivos do programa
add_executable(Ohmimetro
    main.c
    lib/Matriz_Bibliotecas/matriz_led.c   # Mantido para uso futuro
    lib/Display_Bibliotecas/ssd1306.c
)

pico_generate_pio_header(Ohmimetro ${CMAKE_CURRENT_LIST_DIR}/lib/Matriz_Bibliotecas/ws2812.pio
    OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/lib/Matriz_Bibliotecas/generated
    )
pico_set_program_name(Ohmimetro "Ohmimetro")
pico_set_program_version(Ohmimetro "0.1") 

# Habilita comunicação serial
pico_enable_stdio_uart(Ohmimetro 1)
pico_enable_stdio_usb(Ohmimetro 1)  # Ativa comunicação USB

# Vincula as bibliotecas necessárias
target_link_libraries(Ohmimetro PRIVATE
    pico_stdlib      # Biblioteca padrão do Pico
    hardware_i2c     # Suporte para comunicação I2C (Display)
    hardware_adc     # Suporte para ADC 
    hardware_pio     # Suporte para PIO (para Matriz WS2812)
    m                # Biblioteca matemática (pode ser útil)
)

# Adiciona saídas extras (binário, UF2, etc.)
pico_add_extra_outputs(Ohmimetro)
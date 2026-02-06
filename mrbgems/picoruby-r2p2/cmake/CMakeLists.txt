cmake_minimum_required(VERSION 3.22)

#set(CMAKE_VERBOSE_MAKEFILE ON)

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

# Initialize the SDK
pico_sdk_init()

execute_process (COMMAND date +%Y%m%d OUTPUT_VARIABLE CMAKE_BUILDDATE OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process (COMMAND git rev-parse --short HEAD OUTPUT_VARIABLE CMAKE_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE WORKING_DIRECTORY ${PICORUBY_ROOT})
# R2P2_VERSION is passed via -D from rake task
set (R2P2_BUILDDATE ${CMAKE_BUILDDATE})
set (R2P2_REVISION  ${CMAKE_REVISION})

project("${R2P2_NAME}-${R2P2_VERSION}-${R2P2_BOARD_NAME}-${R2P2_BUILDDATE}-${R2P2_REVISION}")

add_compile_options(-Wall
  # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
  -Wno-format
  -Wno-unused-function
  -ffunction-sections
  -fdata-sections
  -fshort-enums
  -mthumb
)

if(PICORUBY_DEBUG)
  add_compile_definitions(PICORUBY_DEBUG=1)
  add_compile_options(
    -O0
    -g3
  )
else()
  add_compile_definitions(NDEBUG=1)
  add_compile_options(
    -O2
  )
endif()

file(GLOB SOURCE_FILES CONFIGURE_DEPENDS
  ${R2P2_GEM_DIR}/src/ports/r2p2/*.c
  ${PICORUBY_ROOT}/mrbgems/*/ports/rp2040/*.c
  ${PICORUBY_ROOT}/mrbgems/*/ports/common/*.c
)
if(USE_WIFI)
  add_compile_definitions(USE_WIFI=1)
  file(GLOB ADD_WIFI_SOURCE_FILES CONFIGURE_DEPENDS
    ${PICO_SDK_PATH}/src/rp2_common/pico_mbedtls/pico_mbedtls.c
  )
  list(APPEND SOURCE_FILES ${ADD_WIFI_SOURCE_FILES})
  if(PICO_BOARD STREQUAL "pico2_w")
    file(GLOB REMOVE_WIFI_SRCS CONFIGURE_DEPENDS
      ${PICORUBY_ROOT}/mrbgems/picoruby-net/ports/rp2040/*.c
    )
  else()
    file(GLOB REMOVE_WIFI_SRCS CONFIGURE_DEPENDS
      ${PICORUBY_ROOT}/mrbgems/picoruby-socket/ports/rp2040/*.c
    )
  endif()
else()
  file(GLOB REMOVE_WIFI_SRCS CONFIGURE_DEPENDS
    ${PICORUBY_ROOT}/mrbgems/picoruby-cyw43/ports/rp2040/*.c
    ${PICORUBY_ROOT}/mrbgems/picoruby-net/ports/rp2040/*.c
    ${PICORUBY_ROOT}/mrbgems/picoruby-socket/ports/rp2040/*.c
    ${PICORUBY_ROOT}/mrbgems/picoruby-ble/ports/rp2040/*.c
    ${PICORUBY_ROOT}/mrbgems/picoruby-mbedtls/ports/**/*.c
    ${PICO_SDK_PATH}/src/rp2_common/pico_mbedtls/pico_mbedtls.c
  )
endif()

list(REMOVE_ITEM SOURCE_FILES ${REMOVE_WIFI_SRCS})

if(PICORB_VM_MRUBYC)
  file(GLOB REMOVE_MRUBY_SRCS CONFIGURE_DEPENDS
    ${PICORUBY_ROOT}/mrbgems/picoruby-mruby/ports/rp2040/*.c
  )
  list(REMOVE_ITEM SOURCE_FILES ${REMOVE_MRUBY_SRCS})
endif()

add_executable(${PROJECT_NAME}
  ${SOURCE_FILES}
)

pico_enable_stdio_usb(${PROJECT_NAME} 0)
if(PICORUBY_DEBUG)
  pico_enable_stdio_uart(${PROJECT_NAME} 1)
else()
  pico_enable_stdio_uart(${PROJECT_NAME} 0)
endif()

if(PICO_BOARD STREQUAL "pico" OR PICO_BOARD STREQUAL "pico_w")
  add_compile_definitions(PICO_RP2040=1)
  target_compile_definitions(${PROJECT_NAME} PRIVATE FLASH_TARGET_OFFSET=0x00140000)
elseif(PICO_BOARD STREQUAL "pico2" OR PICO_BOARD STREQUAL "pico2_w")
  add_compile_definitions(PICO_RP2350=1)
  target_compile_definitions(${PROJECT_NAME} PRIVATE FLASH_TARGET_OFFSET=0x00340000)
endif()

# Fix for pico-sdk 2.2.0: Assume core1 is safe for flash operations
# Core1 is used for PSG audio processing but doesn't access flash during BTStack operations
add_compile_definitions(PICO_FLASH_ASSUME_CORE1_SAFE=1)

set(PICORBC ${PICORUBY_ROOT}/bin/picorbc)

add_custom_target(mrbdir ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/mrb
)

set(RUBY_FILES main_task)
add_dependencies(${PROJECT_NAME}
  mrbdir
  ${RUBY_FILES}
)
foreach(rb ${RUBY_FILES})
  add_custom_target(${rb}
    COMMAND
      mkdir -p ${CMAKE_BINARY_DIR}/mrb &&
      ${PICORBC} -B${rb} -o${CMAKE_BINARY_DIR}/mrb/${rb}.c ${R2P2_GEM_DIR}/mrblib/${rb}.rb
    DEPENDS ${R2P2_GEM_DIR}/mrblib/${rb}.rb
  )
endforeach(rb)

target_link_directories(${PROJECT_NAME} PRIVATE
  ${EXTRA_LIBRARY_PATH}
  ${PICORUBY_ROOT}/build/${MRUBY_CONFIG}/lib
)

pico_generate_pio_header(${PROJECT_NAME}
  ${PICORUBY_ROOT}/mrbgems/picoruby-psg/ports/rp2040/psg_drv_mcp4922_pio.pio
)

target_link_options(${PROJECT_NAME} PRIVATE -Wl,--gc-sections)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  PICO_STACK_SIZE=0x1000       # 4KB
  PICO_CORE1_STACK_SIZE=0x0800 # 2KB
  PICO_HEAP_SIZE=0x0400        # 1KB
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  pico_stdlib
  pico_stdio
  pico_stdio_uart
  pico_bootsel_via_double_reset
  tinyusb_device
  tinyusb_board
  hardware_flash
  hardware_adc
  hardware_uart
  hardware_i2c
  hardware_spi
  hardware_pwm
  pico_runtime
  pico_aon_timer
  cmsis_core      # for RP2350
  pico_multicore  # for RP2040
  hardware_claim
  hardware_pio
  hardware_dma
  mruby
)

# Sleep functionality for both RP2040 and RP2350
target_link_libraries(${PROJECT_NAME} PRIVATE
  hardware_sleep
)

# RTC only for RP2040
if(PICO_BOARD STREQUAL "pico" OR PICO_BOARD STREQUAL "pico_w")
  target_link_libraries(${PROJECT_NAME} PRIVATE
    hardware_rtc
  )
endif()

if(USE_WIFI)
  # these are used only in WiFi build
  target_link_libraries(${PROJECT_NAME} PRIVATE
    pico_cyw43_arch_lwip_threadsafe_background
    pico_btstack_ble
    pico_btstack_cyw43
  )
  target_include_directories(${PROJECT_NAME} PRIVATE
    ${PICORUBY_ROOT}/mrbgems/picoruby-ble/include
  )
endif()

add_compile_definitions(MRB_INT64=1)

if(PICORUBY_MSC_FLASH)
  add_compile_definitions(PICORUBY_MSC_FLASH=1)
elseif(PICORUBY_MSC_SD)
  add_compile_definitions(PICORUBY_MSC_SD=1)
else()
  message(FATAL_ERROR "PICORUBY_MSC_xxxx is not defined.")
endif()

if(PICORB_VM_MRUBY)
  add_compile_definitions(PICORB_VM_MRUBY=1)
  target_include_directories(${PROJECT_NAME} PRIVATE
    ${PICORUBY_ROOT}/mrbgems/picoruby-mruby/lib/mruby/include
  )
elseif(PICORB_VM_MRUBYC)
  add_compile_definitions(PICORB_VM_MRUBYC=1)
  target_include_directories(${PROJECT_NAME} PRIVATE
    ${PICORUBY_ROOT}/mrbgems/picoruby-mrubyc/lib/mrubyc/src
  )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_BINARY_DIR}/mrb
  ${R2P2_GEM_DIR}/include
  ${PICORUBY_ROOT}/include
  ${PICORUBY_ROOT}/mrbgems/mruby-compiler2/lib/prism/include
  ${PICORUBY_ROOT}/mrbgems/mruby-compiler2/include
  ${PICORUBY_ROOT}/build/${MRUBY_CONFIG}/include
  ${PICORUBY_ROOT}/mrbgems/picoruby-net/include

  ${R2P2_GEM_DIR}/cmake
  ${PICO_SDK_PATH}/src/rp2_common/pico_stdio_usb/include
  ${PICO_SDK_PATH}/src/rp2_common/hardware_dma/include
  ${PICORUBY_ROOT}/mrbgems/picoruby-machine/include
  ${PICORUBY_ROOT}/mrbgems/picoruby-filesystem-fat/lib/ff14b/source
  ${PICORUBY_ROOT}/build/${MRUBY_CONFIG}/mrbgems
  ${PICORUBY_ROOT}/mrbgems/picoruby-mbedtls/include
  ${PICORUBY_ROOT}/mrbgems/picoruby-mbedtls/lib/mbedtls/include
  ${PICORUBY_ROOT}/mrbgems/picoruby-mbedtls/lib/mbedtls/library
  ${PICORUBY_ROOT}/mrbgems/picoruby-net/include
  ${EXTRA_INCLUDE_DIR}
)

pico_add_extra_outputs(${PROJECT_NAME})

add_custom_command(
  TARGET ${PROJECT_NAME}
  POST_BUILD
  COMMAND gzip -kf ${PROJECT_NAME}.uf2
  COMMAND zip -r ${PROJECT_NAME}.uf2.zip ${PROJECT_NAME}.uf2
)

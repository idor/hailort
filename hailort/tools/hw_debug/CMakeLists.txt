cmake_minimum_required(VERSION 3.0.0)

set(FILES
    main.cpp
    shell.cpp
    readline_wrapper.cpp
    driver_memory.cpp
    memory_commands.cpp
    mercury_fields.cpp

    # Depends on hailort_driver and its dependencies
    ${HAILO_OS_DIR}/hailort_driver.cpp
    ${HAILO_OS_DIR}/file_descriptor.cpp
    ${HAILO_FULL_OS_DIR}/driver_scan.cpp
    # TODO: HRT-3816 remove mmap header
    ${HAILO_OS_DIR}/mmap_buffer.cpp
)

if(WIN32)
    # hailort_driver.cpp in windows depends on string_conversion
    set(FILES ${FILES}
        ${HAILORT_COMMON_OS_DIR}/string_conversion.cpp)
endif()

add_executable(debalex ${FILES})
target_compile_options(debalex PRIVATE ${HAILORT_COMPILE_OPTIONS})
set_property(TARGET debalex PROPERTY CXX_STANDARD 14)
target_link_libraries(debalex PRIVATE
    libhailort
    spdlog::spdlog
    CLI11::CLI11
    )
target_include_directories(debalex
    PRIVATE
    ${HAILORT_COMMON_DIR}
    ${HAILORT_SRC_DIR}
    ${DRIVER_INC_DIR}
)

if(CMAKE_SYSTEM_NAME STREQUAL QNX)
    target_link_libraries(debalex PRIVATE pci)
endif()

find_path(READLINE_INCLUDE_DIR NAMES readline/readline.h)
find_library(READLINE_LIBRARY NAMES readline)

if(READLINE_INCLUDE_DIR AND READLINE_LIBRARY)
    target_link_libraries(debalex PRIVATE ${READLINE_LIBRARY})
    target_include_directories(debalex PRIVATE  ${READLINE_INCLUDE_DIR})
    add_definitions(-DUSE_READLINE)
else()
    message(WARNING "Could not find readline library. To better UI, please install it by calling `sudo apt install libreadline6-dev`")
endif()

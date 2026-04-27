cmake_minimum_required(VERSION 3.16)

project(KokoroC VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED ONNXRUNTIME_ROOT)
    set(ONNXRUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime")
endif()

get_filename_component(ONNXRUNTIME_ROOT "${ONNXRUNTIME_ROOT}" ABSOLUTE)

set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_ROOT}/build/native/include")
set(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_ROOT}/runtimes/win-x64/native")
set(ONNXRUNTIME_LIB "${ONNXRUNTIME_LIB_DIR}/onnxruntime.lib")
set(ONNXRUNTIME_DLL "${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll")
set(ONNXRUNTIME_SHARED_DLL "${ONNXRUNTIME_LIB_DIR}/onnxruntime_providers_shared.dll")

if(NOT EXISTS "${ONNXRUNTIME_INCLUDE_DIR}/onnxruntime_cxx_api.h")
    message(FATAL_ERROR "ONNX Runtime headers not found. Expected extracted package under ${ONNXRUNTIME_ROOT}")
endif()

if(NOT EXISTS "${ONNXRUNTIME_LIB}")
    message(FATAL_ERROR "ONNX Runtime import library not found in ${ONNXRUNTIME_LIB_DIR}")
endif()

add_library(kokoro_core STATIC
    src/kokoro_engine.cpp
)

target_include_directories(kokoro_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${ONNXRUNTIME_INCLUDE_DIR}
)

target_link_libraries(kokoro_core PRIVATE ${ONNXRUNTIME_LIB})

if(MSVC)
    target_compile_options(kokoro_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(kokoro_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(kokoro_cpp_demo examples/demo_cpp.cpp)
target_link_libraries(kokoro_cpp_demo PRIVATE kokoro_core)

add_executable(kokoro_cli examples/kokoro_cli.cpp)
target_link_libraries(kokoro_cli PRIVATE kokoro_core)

set_target_properties(kokoro_cpp_demo kokoro_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_custom_command(TARGET kokoro_cpp_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_DLL}"
        "$<TARGET_FILE_DIR:kokoro_cpp_demo>/onnxruntime.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_SHARED_DLL}"
        "$<TARGET_FILE_DIR:kokoro_cpp_demo>/onnxruntime_providers_shared.dll"
)

add_custom_command(TARGET kokoro_cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_DLL}"
        "$<TARGET_FILE_DIR:kokoro_cli>/onnxruntime.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_SHARED_DLL}"
        "$<TARGET_FILE_DIR:kokoro_cli>/onnxruntime_providers_shared.dll"
)

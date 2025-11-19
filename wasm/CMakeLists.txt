cmake_minimum_required(VERSION 3.10)
project(cbf-wasm)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Emscripten-specific settings
if(EMSCRIPTEN)
    message(STATUS "Building for WebAssembly with Emscripten")

    # Compiler flags for WASM
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++17")

    # Emscripten link flags
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} \
        -s WASM=1 \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString'] \
        -s MODULARIZE=1 \
        -s EXPORT_NAME='createCBFModule' \
        -s ENVIRONMENT='web' \
        --bind \
        -s TOTAL_MEMORY=16777216 \
        -s NO_EXIT_RUNTIME=1 \
        -s ASSERTIONS=1 \
        -s DISABLE_EXCEPTION_CATCHING=0"
    )

    # For development: Add debug info and source maps
    # Uncomment for debugging:
    # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g -s ASSERTIONS=2 -s SAFE_HEAP=1")

else()
    message(FATAL_ERROR "This CMakeLists.txt is for Emscripten builds only. Use: emcmake cmake ..")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/../include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Source files
set(SOURCES
    src/wasm_bindings.cpp
)

# Build WASM module
add_executable(cbf_wasm ${SOURCES})

# Output files will be:
# - cbf_wasm.js   (JavaScript glue code)
# - cbf_wasm.wasm (WebAssembly binary)

# Installation
install(FILES
    ${CMAKE_BINARY_DIR}/cbf_wasm.js
    ${CMAKE_BINARY_DIR}/cbf_wasm.wasm
    DESTINATION ${CMAKE_SOURCE_DIR}/../frontend/public/wasm
)

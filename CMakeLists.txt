cmake_minimum_required(VERSION 3.20)
project(NanoScript C CXX)   # C is needed by LLVM's FindLibEdit.cmake

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Locate Homebrew LLVM (Apple Silicon: /opt/homebrew, Intel: /usr/local) ──
if(APPLE)
    if(EXISTS "/opt/homebrew/opt/llvm")
        set(_LLVM_PREFIX "/opt/homebrew/opt/llvm")
    elseif(EXISTS "/usr/local/opt/llvm")
        set(_LLVM_PREFIX "/usr/local/opt/llvm")
    endif()
    if(_LLVM_PREFIX)
        set(LLVM_DIR "${_LLVM_PREFIX}/lib/cmake/llvm"
            CACHE PATH "Homebrew LLVM cmake dir" FORCE)
    endif()
endif()

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} — ${LLVM_DIR}")

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# ── Compiler executable ──────────────────────────────────────────────────────
add_executable(nanoscript
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/codegen.cpp
)

target_include_directories(nanoscript PRIVATE src)

# On macOS Homebrew LLVM ships as a single shared library; the imported
# target "LLVM" is exported by LLVMExports.cmake and is the cleanest link.
target_link_libraries(nanoscript PRIVATE LLVM)

if(APPLE AND _LLVM_PREFIX)
    target_link_options(nanoscript PRIVATE
        "-L${_LLVM_PREFIX}/lib"
        "-Wl,-rpath,${_LLVM_PREFIX}/lib"
    )
endif()

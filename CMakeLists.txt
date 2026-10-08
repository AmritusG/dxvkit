# DXVKit - MIT License. Copyright (c) 2026 Amrit Rosell.
cmake_minimum_required(VERSION 3.16)

if(APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES AND NOT DXVK_FUZZ)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal build")
endif()

project(DXVKit VERSION 0.1.0 LANGUAGES C CXX)

option(DXVK_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(DXVK_FUZZ "Build the libFuzzer harnesses (needs a clang with libFuzzer)" OFF)
option(DXVK_BUILD_TOOLS "Build dxvtool and tests" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DXVK_WARNINGS -Wall -Wextra -Werror -Wshadow -Wconversion -Wno-sign-conversion)

if(DXVK_SANITIZE OR DXVK_FUZZ)
  set(DXVK_SAN_FLAGS -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g)
  add_compile_options(${DXVK_SAN_FLAGS})
  add_link_options(-fsanitize=address,undefined)
endif()

add_library(dxvkit STATIC
  src/packet.c
  src/legacy.c
  src/blockcopy.c
  src/hq.c
  src/pixels.c
  src/mov.c)
target_include_directories(dxvkit PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_options(dxvkit PRIVATE ${DXVK_WARNINGS})

install(TARGETS dxvkit ARCHIVE DESTINATION lib)
install(FILES include/dxvkit.h include/dxvkit.hpp DESTINATION include)

if(DXVK_BUILD_TOOLS)
  find_package(Threads REQUIRED)
  add_executable(dxvtool tools/dxvtool.c)
  target_link_libraries(dxvtool PRIVATE dxvkit Threads::Threads)
  target_compile_options(dxvtool PRIVATE -Wall -Wextra -Werror)

  add_executable(dxvkit_cpp_test tests/cpp_wrapper_test.cpp)
  target_link_libraries(dxvkit_cpp_test PRIVATE dxvkit Threads::Threads)
  target_compile_options(dxvkit_cpp_test PRIVATE -Wall -Wextra -Werror)

  enable_testing()
  add_test(NAME vectors COMMAND dxvtool verify ${CMAKE_CURRENT_SOURCE_DIR}/test_vectors/vectors.tsv)
  add_test(NAME vectors_threaded
           COMMAND dxvtool verify ${CMAKE_CURRENT_SOURCE_DIR}/test_vectors/vectors.tsv --threads 4)
  add_test(NAME cpp_wrapper COMMAND dxvkit_cpp_test ${CMAKE_CURRENT_SOURCE_DIR}/test_vectors)
endif()

if(DXVK_FUZZ)
  foreach(target fuzz_packet fuzz_mov)
    add_executable(${target} tools/${target}.c)
    target_link_libraries(${target} PRIVATE dxvkit)
    target_compile_options(${target} PRIVATE -fsanitize=fuzzer)
    target_link_options(${target} PRIVATE -fsanitize=fuzzer)
  endforeach()
  target_compile_options(dxvkit PRIVATE -fsanitize=fuzzer-no-link)
  add_executable(fuzz_corpus tools/fuzz_corpus.c)
  target_link_libraries(fuzz_corpus PRIVATE dxvkit)
endif()

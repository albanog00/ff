cmake_minimum_required(VERSION 3.22)

project(ff LANGUAGES CXX C)

include(GNUInstallDirs)
include(CheckIPOSupported)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
    ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

include_directories("${CMAKE_SOURCE_DIR}/src")

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp" "src/*.hpp")

add_executable(${PROJECT_NAME} ${SRCFILES})

target_link_libraries(${PROJECT_NAME} atomic)

if(TESTS)
  enable_testing()
  add_executable(ff_ignore_tests tests/ignore_tests.cpp src/ignore.cpp)
  target_link_libraries(ff_ignore_tests atomic)
  add_test(NAME ignore COMMAND ff_ignore_tests)
endif()

find_package(spdlog REQUIRED)
if (spdlog_FOUND)
    message(STATUS "spdlog found: ${spdlog_VERSION}")
    target_link_libraries(${PROJECT_NAME} spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)
    if(TESTS)
      target_link_libraries(ff_ignore_tests spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)
    endif()
endif()

find_library(PCRE2_LIBRARIES NAMES pcre2 pcre2-8)
find_path(PCRE2_INCLUDE_DIRS pcre2.h)

if(PCRE2_LIBRARIES AND PCRE2_INCLUDE_DIRS)
    message(STATUS "PCRE2 libs: ${PCRE2_LIBRARIES}")
    message(STATUS "PCRE2 include directory: ${PCRE2_INCLUDE_DIRS}")
    include_directories(${PCRE2_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${PCRE2_LIBRARIES})
else()
    message(STATUS "PCRE2 library not found.")
endif()

if (CMAKE_BUILD_TYPE)
  string(TOLOWER ${CMAKE_BUILD_TYPE} BUILDTYPE_LOWER)
  target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    # -Wconversion
    -Wshadow
    -Wformat=2
    -Wnull-dereference
    -Wdouble-promotion
    -Wimplicit-fallthrough
    -Wno-missing-field-initializers
  )

  if(TESTS)
    target_compile_options(ff_ignore_tests PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wformat=2
      -Wnull-dereference
      -Wdouble-promotion
      -Wimplicit-fallthrough
      -Wno-missing-field-initializers
    )
  endif()

  if(${BUILDTYPE_LOWER} STREQUAL "debug")
    target_compile_options(${PROJECT_NAME} PRIVATE
      -g                               # Debug symbols
      -fsanitize=address,undefined             # UB sanitizer (null deref, overflow, etc.)
      -fno-omit-frame-pointer          # Better stack traces with sanitizers
    )

    target_link_options(${PROJECT_NAME} PRIVATE
      -fsanitize=address,undefined        # Link UBSan runtime
    )

    if(TESTS)
      target_compile_options(ff_ignore_tests PRIVATE
        -g
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
      )

      target_link_options(ff_ignore_tests PRIVATE
        -fsanitize=address,undefined
      )
    endif()

    target_compile_definitions(${PROJECT_NAME} PRIVATE
      DEBUG                       # Define DEBUG macro
    )

    if(TESTS)
      target_compile_definitions(ff_ignore_tests PRIVATE DEBUG)
    endif()

    add_link_options(-static-libasan)
  else()
    target_compile_options(${PROJECT_NAME} PRIVATE
      -O3
      -Werror                          # Treat warning as errors
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE
      NDEBUG                      # Disable asserts
    )

    if(TESTS)
      target_compile_options(ff_ignore_tests PRIVATE -O3 -Werror)
      target_compile_definitions(ff_ignore_tests PRIVATE NDEBUG)
    endif()
  endif()

  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

  if(ipo_supported)
    set_property(TARGET ${PROJECT_NAME}
        PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
    )
  else()
    message(WARNING "IPO/LTO not supported: ${ipo_error}")
  endif()

endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

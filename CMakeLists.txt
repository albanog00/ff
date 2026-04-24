cmake_minimum_required(VERSION 3.22)

project(fgrep LANGUAGES CXX C)
include(GNUInstallDirs)

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

find_package(spdlog REQUIRED)
if (spdlog_FOUND)
    message(STATUS "spdlog found: ${spdlog_VERSION}")
    target_link_libraries(${PROJECT_NAME} spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)
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

    if(${BUILDTYPE_LOWER} STREQUAL "debug")
        target_compile_options(${PROJECT_NAME} PRIVATE
            -g                               # Debug symbols
            -Wall                            # All warnings
            -Wextra                          # Extra warnings
            -Wno-missing-field-initializers  #
            -fsanitize=undefined             # UB sanitizer (null deref, overflow, etc.)
            -fno-omit-frame-pointer          # Better stack traces with sanitizers
        )

        target_link_options(${PROJECT_NAME} PRIVATE
            -fsanitize=undefined        # Link UBSan runtime
        )

        target_compile_definitions(${PROJECT_NAME} PRIVATE
            DEBUG                       # Define DEBUG macro
        )

        add_compile_options(
            -fsanitize=address          # Address sanitizer (heap errors, leaks)
        )

        add_link_options(
            -fsanitize=address          # Link ASan runtime
            -static-libasan
        )
    else()
      target_compile_options(${PROJECT_NAME} PRIVATE
          -O3                              # Aggressive optimization
          -Wall                            # All warnings
          -Werror                          # Treat warning as errors
          -Wextra                          # Extra warnings
          -Wno-missing-field-initializers  #
          -flto                            # Link-time optimization
      )

      target_link_options(${PROJECT_NAME} PRIVATE
          -flto                       # LTO at link time
      )

      target_compile_definitions(${PROJECT_NAME} PRIVATE
          NDEBUG                      # Disable asserts
      )
    endif()

endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

cmake_minimum_required(VERSION 3.20)

# Read version from CH341Locator.h
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/CH341Locator.h" VERSION_LINE
     REGEX "#define CH341LOCATOR_VERSION ")
if(VERSION_LINE MATCHES "\"([0-9]+\\.[0-9]+\\.[0-9]+)\"")
    set(PROJECT_VERSION ${CMAKE_MATCH_1})
else()
    set(PROJECT_VERSION "0.0.0")
endif()

project(CH341Locator VERSION ${PROJECT_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ---------- Shared Library ----------

add_library(CH341Locator SHARED
    src/CH341Locator.cpp
)

target_include_directories(CH341Locator
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(CH341Locator
    PRIVATE
        CH341LOCATOR_EXPORTS
)

set_target_properties(CH341Locator PROPERTIES
    OUTPUT_NAME "CH341Locator"
)

if(WIN32)
    target_sources(CH341Locator PRIVATE src/backend_windows.cpp)
    target_link_libraries(CH341Locator PRIVATE setupapi)
elseif(APPLE)
    target_sources(CH341Locator PRIVATE src/backend_macos.mm)
    target_link_libraries(CH341Locator PRIVATE "-framework IOKit" "-framework CoreFoundation")
elseif(UNIX)
    target_sources(CH341Locator PRIVATE src/backend_linux.cpp)
else()
    message(WARNING "CH341Locator: no backend for this platform.")
endif()

# ---------- Demo ----------

add_executable(ch341locator_demo
    examples/ch341locator_demo.cpp
)

target_compile_definitions(ch341locator_demo
    PRIVATE
        CH341LOCATOR_EXPORTS
)

target_include_directories(ch341locator_demo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ch341locator_demo
    PRIVATE
        CH341Locator
)

# ---------- Test ----------

enable_testing()

add_executable(CH341Locator_test
    tests/test_ch341locator.cpp
)

target_compile_definitions(CH341Locator_test
    PRIVATE
        CH341LOCATOR_EXPORTS
)

target_include_directories(CH341Locator_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_test(
    NAME CH341Locator_test
    COMMAND CH341Locator_test
)

if(WIN32)
    target_link_libraries(CH341Locator_test PRIVATE setupapi)
endif()

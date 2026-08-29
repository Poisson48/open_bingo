cmake_minimum_required(VERSION 3.24)

project(openbingo VERSION 2.0.51 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS Core Network Quick QuickControls2 Sql PrintSupport WebSockets Widgets)
else()
    find_package(Qt6 REQUIRED COMPONENTS Core Network Quick QuickControls2 Sql PrintSupport WebSockets Widgets Test)
    find_package(Qt6 OPTIONAL_COMPONENTS Widgets)
endif()

find_package(Qt6 OPTIONAL_COMPONENTS Multimedia)
if(TARGET Qt6::Multimedia)
    set(BINGO_HAS_CAMERA ON)
else()
    set(BINGO_HAS_CAMERA OFF)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
pkg_check_modules(SECP256K1 REQUIRED IMPORTED_TARGET libsecp256k1)

find_package(ZLIB REQUIRED)

qt_standard_project_setup()

include(FetchContent)
set(ZXING_READERS ON CACHE BOOL "" FORCE)
set(ZXING_WRITERS OFF CACHE BOOL "" FORCE)
set(ZXING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZXING_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(ZXING_BLACKBOX_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zxing
    GIT_REPOSITORY https://github.com/zxing-cpp/zxing-cpp.git
    GIT_TAG v2.2.1
)
FetchContent_MakeAvailable(zxing)

add_subdirectory(src)

if(NOT ANDROID)
    enable_testing()
    add_subdirectory(tests)
endif()

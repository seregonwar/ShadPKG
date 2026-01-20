cmake_minimum_required(VERSION 3.16)
project(shadpkg)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# ╔══════════════════════════════════════════════════════════════════════════╗
# ║  C++23 required for std::byteswap and modern features                   ║
# ╚══════════════════════════════════════════════════════════════════════════╝
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(ZLIB REQUIRED)
find_package(CryptoPP REQUIRED)
find_package(fmt REQUIRED)

add_executable(shadpkg
    main.cpp
    core/file_format/pkg.cpp
    core/file_format/trp.cpp
    core/file_format/psf.cpp
    core/file_format/rif_generator.cpp
    core/crypto/crypto.cpp
    common/io_file.cpp
    common/path_util.cpp
    common/error.cpp
    common/assert.cpp
    common/string_util.cpp
    core/file_format/entry_name.cpp
    common/ntapi_stub.cpp
    common/config.cpp
    common/logging/backend.cpp
    common/logging/filter.cpp
    common/logging/text_formatter.cpp
    common/thread.cpp
)

if(WIN32)
    target_compile_definitions(shadpkg PRIVATE _WIN32_WINNT=NTDDI_WIN10_RS5)
endif()

# Include directories
target_include_directories(shadpkg PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/core/file_format
    ${CMAKE_SOURCE_DIR}/core/crypto
    ${CMAKE_SOURCE_DIR}/common
)

# ╔══════════════════════════════════════════════════════════════════════════╗
# ║  Platform-specific linkage                                               ║
# ╚══════════════════════════════════════════════════════════════════════════╝
target_link_libraries(shadpkg PRIVATE ZLIB::ZLIB fmt::fmt cryptopp::cryptopp)

if(WIN32)
    target_link_libraries(shadpkg PRIVATE onecore)
elseif(APPLE)
    # CoreFoundation needed for path_util.cpp bundle functions
    target_link_libraries(shadpkg PRIVATE "-framework CoreFoundation")
endif()

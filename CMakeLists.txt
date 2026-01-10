cmake_minimum_required(VERSION 3.16)
project(pkgtool_minimal)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Usa C++23 per abilitare std::byteswap e altre feature moderne
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Trova i pacchetti richiesti tramite vcpkg
find_package(ZLIB REQUIRED)
find_package(CryptoPP REQUIRED)
find_package(fmt REQUIRED)

add_executable(pkgtool
    main.cpp
    core/file_format/pkg.cpp
    core/file_format/trp.cpp
    core/file_format/psf.cpp
    core/file_format/rif_generator.cpp
    core/crypto/crypto.cpp
    core/file_sys/file.cpp
    core/file_sys/fs.cpp
    common/io_file.cpp
    common/path_util.cpp
    common/error.cpp
    common/assert.cpp
    common/string_util.cpp
    core/file_format/entry_name.cpp
    common/ntapi_stub.cpp
    common/io_file_stub.cpp
    common/config.cpp
    common/logging/backend.cpp
    common/logging/filter.cpp
    common/logging/text_formatter.cpp
    common/thread.cpp
    core/devices/logger.cpp
    core/devices/base_device.cpp
)

# Includi tutte le directory necessarie
# (aggiungi altre se servono per gli header)
target_include_directories(pkgtool PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/core/file_format
    ${CMAKE_SOURCE_DIR}/core/crypto
    ${CMAKE_SOURCE_DIR}/core/file_sys
    ${CMAKE_SOURCE_DIR}/common
)

# Link alle librerie tramite vcpkg
# (usa i target moderni)
target_link_libraries(pkgtool PRIVATE ZLIB::ZLIB fmt::fmt cryptopp::cryptopp)
cmake_minimum_required(VERSION 3.16)
project(pkgtool_minimal)

# Usa C++23 per abilitare std::byteswap e altre feature moderne
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Percorsi vcpkg (modifica se necessario)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(VCPKG_TARGET_TRIPLET "x64-windows")
set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/share")
set(ZLIB_DIR "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/share/zlib")
set(fmt_DIR "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/share/fmt")
set(cryptopp_DIR "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/share/cryptopp")

# Trova i pacchetti richiesti tramite vcpkg
find_package(ZLIB REQUIRED)
find_package(CryptoPP REQUIRED)
find_package(fmt REQUIRED)

add_executable(pkgtool
    main.cpp
    core/file_format/pkg.cpp
    core/file_format/trp.cpp
    core/file_format/psf.cpp
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
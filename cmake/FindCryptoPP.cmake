include(FindPackageHandleStandardArgs)

find_library(CryptoPP_LIBRARY NAMES cryptopp)
find_path(CryptoPP_INCLUDE_PATH NAMES cryptopp/cryptlib.h)

find_package_handle_standard_args(
    CryptoPP
    REQUIRED_VARS CryptoPP_LIBRARY CryptoPP_INCLUDE_PATH
)

if(CryptoPP_FOUND AND NOT TARGET cryptopp::cryptopp)
    add_library(cryptopp::cryptopp UNKNOWN IMPORTED)
    set_property(TARGET cryptopp::cryptopp PROPERTY IMPORTED_LOCATION "${CryptoPP_LIBRARY}")
    set_property(TARGET cryptopp::cryptopp PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CryptoPP_INCLUDE_PATH}")
endif()

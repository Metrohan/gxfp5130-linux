# Fallback for distributions that ship Mbed TLS pkg-config metadata but no
# MbedTLSConfig.cmake. The userspace build tries the upstream config first.
find_package(PkgConfig REQUIRED)

pkg_check_modules(MBEDTLS REQUIRED IMPORTED_TARGET mbedtls)
pkg_check_modules(MBEDCRYPTO REQUIRED IMPORTED_TARGET mbedcrypto)
pkg_check_modules(MBEDX509 REQUIRED IMPORTED_TARGET mbedx509)

if(NOT TARGET MbedTLS::mbedtls)
    add_library(MbedTLS::mbedtls INTERFACE IMPORTED)
    target_link_libraries(MbedTLS::mbedtls INTERFACE
        PkgConfig::MBEDTLS
        PkgConfig::MBEDCRYPTO
        PkgConfig::MBEDX509
    )
endif()

set(MbedTLS_FOUND TRUE)

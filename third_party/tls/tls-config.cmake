string(TOLOWER ${CMAKE_SYSTEM_NAME} TLS_SYSTEM_NAME)

if(NOT TARGET tls::crypto)
  add_library(tls::crypto STATIC IMPORTED)
  set_target_properties(tls::crypto PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/include"
    INTERFACE_LINK_LIBRARIES "$<$<PLATFORM_ID:Windows>:crypt32>"
    IMPORTED_LINK_INTERFACE_LANGUAGES "C")
  if(WIN32)
    set_target_properties(tls::crypto PROPERTIES
      IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_LIST_DIR}/lib/windows/debug/crypto.lib"
      IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_LIST_DIR}/lib/windows/release/crypto.lib"
      IMPORTED_CONFIGURATIONS "DEBUG;RELEASE")
  else()
    set_target_properties(tls::crypto PROPERTIES
      IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/lib/${TLS_SYSTEM_NAME}/libcrypto.a")
  endif()
endif()

if(NOT TARGET tls::ssl)
  add_library(tls::ssl STATIC IMPORTED)
  set_target_properties(tls::ssl PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/include"
    INTERFACE_LINK_LIBRARIES "$<$<PLATFORM_ID:Windows>:ws2_32>;tls::crypto"
    IMPORTED_LINK_INTERFACE_LANGUAGES "C")
  if(WIN32)
    set_target_properties(tls::ssl PROPERTIES
      IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_LIST_DIR}/lib/windows/debug/ssl.lib"
      IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_LIST_DIR}/lib/windows/release/ssl.lib"
      IMPORTED_CONFIGURATIONS "DEBUG;RELEASE")
  else()
    set_target_properties(tls::ssl PROPERTIES
      IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/lib/${TLS_SYSTEM_NAME}/libssl.a")
  endif()
endif()

if(NOT TARGET tls::tls)
  add_library(tls::tls STATIC IMPORTED)
  set_target_properties(tls::tls PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/include"
    INTERFACE_LINK_LIBRARIES "tls::ssl"
    IMPORTED_LINK_INTERFACE_LANGUAGES "C")
  if(WIN32)
    set_target_properties(tls::tls PROPERTIES
      IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_LIST_DIR}/lib/windows/debug/tls.lib"
      IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_LIST_DIR}/lib/windows/release/tls.lib"
      IMPORTED_CONFIGURATIONS "DEBUG;RELEASE")
  else()
    set_target_properties(tls::tls PROPERTIES
      IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/lib/${TLS_SYSTEM_NAME}/libtls.a")
  endif()
endif()

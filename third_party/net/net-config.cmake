string(TOLOWER ${CMAKE_SYSTEM_NAME} net_SYSTEM_NAME)

if(NOT TARGET net::net)
  find_package(tls REQUIRED PATHS ${CMAKE_CURRENT_LIST_DIR}/../tls NO_DEFAULT_PATH)
  add_library(net::net STATIC IMPORTED)
  set_target_properties(net::net PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/include"
    IMPORTED_LINK_INTERFACE_LANGUAGES "C")
  if(WIN32)
    set_target_properties(net::net PROPERTIES
      INTERFACE_LINK_LIBRARIES "tls::tls;mswsock"
      IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_LIST_DIR}/lib/windows/debug/net.lib"
      IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_LIST_DIR}/lib/windows/release/net.lib"
      IMPORTED_CONFIGURATIONS "DEBUG;RELEASE")
  else()
    set_target_properties(net::net PROPERTIES
      INTERFACE_LINK_LIBRARIES "tls::tls"
      IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/lib/${net_SYSTEM_NAME}/libnet.a")
  endif()
endif()

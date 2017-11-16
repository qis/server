string(TOLOWER ${CMAKE_SYSTEM_NAME} FMT_SYSTEM_NAME)

if(NOT TARGET fmt::fmt)
  add_library(fmt::fmt STATIC IMPORTED)
  set_target_properties(fmt::fmt PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/include"
    IMPORTED_LINK_INTERFACE_LANGUAGES "CXX")
  if(WIN32)
    set_target_properties(fmt::fmt PROPERTIES
      IMPORTED_LOCATION_DEBUG "${CMAKE_CURRENT_LIST_DIR}/lib/windows/debug/fmt.lib"
      IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_LIST_DIR}/lib/windows/release/fmt.lib"
      IMPORTED_CONFIGURATIONS "DEBUG;RELEASE")
  else()
    set_target_properties(fmt::fmt PROPERTIES
      IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/lib/${FMT_SYSTEM_NAME}/libfmt.a")
  endif()
endif()

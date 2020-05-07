cmake_minimum_required(VERSION 3.17 FATAL_ERROR)
project(server DESCRIPTION "Server" VERSION 0.1.0 LANGUAGES CXX)

# Version
set(PROJECT_VENDOR "Company")
configure_file(res/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/version.h LF)

# Modules
list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/res/cmake)

# Sources
file(GLOB_RECURSE sources src/*.[hc]pp)

# Executable
add_executable(${PROJECT_NAME} ${sources} src/main.manifest src/main.rc)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/src src)

if(WIN32)
  target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# Precompiled Headers
target_precompile_headers(${PROJECT_NAME} PRIVATE src/common.hpp)
source_group(TREE ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}.dir PREFIX res FILES
  ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}.dir/cmake_pch.hxx
  ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${PROJECT_NAME}.dir/cmake_pch.cxx)

# Dependencies
find_package(Boost 1.72.0 CONFIG REQUIRED COMPONENTS headers program_options)
target_link_libraries(${PROJECT_NAME} PRIVATE Boost::headers Boost::program_options)
target_compile_definitions(${PROJECT_NAME} PRIVATE
  BOOST_ASIO_HAS_CO_AWAIT
  BOOST_ASIO_DISABLE_CONCEPTS
  BOOST_ASIO_SEPARATE_COMPILATION
  BOOST_BEAST_SEPARATE_COMPILATION
  BOOST_BEAST_USE_STD_STRING_VIEW
  BOOST_JSON_STANDALONE)

find_package(date CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE date::date date::tz)

find_package(fmt CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

include(tzdata)
tzdata(${PROJECT_NAME} 2020a ${CMAKE_CURRENT_BINARY_DIR}/tzdata)

if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
  target_link_libraries(${PROJECT_NAME} PRIVATE stdc++fs)
endif()

# Bundle
add_custom_target(bundle ALL
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} -E remove_directory build/bundle/release
  COMMAND npm run build
  USES_TERMINAL)

# Install
install(CODE "file(REMOVE_RECURSE \"${CMAKE_INSTALL_PREFIX}/html\")")
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/ DESTINATION data)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/bundle/release/ DESTINATION html)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(FILES ${PROJECT_NAME}.ini DESTINATION etc)

if(WIN32)
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tzdata DESTINATION share)
  install(CODE [[
    file(GLOB libraries ${CMAKE_BINARY_DIR}/*.dll)
    file(INSTALL ${libraries} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
  ]])
endif()

# Package
if(NOT WIN32)
  set(CPACK_GENERATOR "DEB")
  set(CPACK_PACKAGE_VENDOR "${PROJECT_VENDOR}")
  set(CPACK_PACKAGE_DESCRIPTION "${PROJECT_DESCRIPTION}")
  set(CPACK_PACKAGE_EXECUTABLES "${PROJECT_NAME}" "${PROJECT_DESCRIPTION}")
  set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_VENDOR}")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.27)")
  set(CPACK_DEBIAN_PACKAGE_SECTION "misc")

  set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/${PROJECT_NAME}")

  configure_file(res/debian/postinst.in ${CMAKE_CURRENT_BINARY_DIR}/res/postinst @ONLY LF)
  configure_file(res/debian/prerm.in ${CMAKE_CURRENT_BINARY_DIR}/res/prerm @ONLY LF)
  configure_file(res/debian/postrm.in ${CMAKE_CURRENT_BINARY_DIR}/res/postrm @ONLY LF)

  set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_BINARY_DIR}/res/postinst"
    "${CMAKE_CURRENT_BINARY_DIR}/res/prerm"
    "${CMAKE_CURRENT_BINARY_DIR}/res/postrm")

  include(CPack)
endif()

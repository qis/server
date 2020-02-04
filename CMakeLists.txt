cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(server VERSION 0.1.0 LANGUAGES CXX)

# Version
set(PROJECT_VENDOR "Company")
configure_file(res/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/version.h LF)

# Sources
file(GLOB_RECURSE sources src/*.[hc]pp)

# Executable
add_executable(${PROJECT_NAME} ${sources} src/main.manifest src/main.rc)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/src src)
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# Precompiled Headers
target_precompile_headers(${PROJECT_NAME} PRIVATE src/common.hpp)

# Dependencies
find_package(Boost REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Boost::disable_autolinking Boost::headers)
target_compile_definitions(${PROJECT_NAME} PRIVATE
  BOOST_ASIO_HAS_CO_AWAIT
  BOOST_ASIO_DISABLE_CONCEPTS
  BOOST_ASIO_SEPARATE_COMPILATION
  BOOST_BEAST_SEPARATE_COMPILATION
  BOOST_BEAST_USE_STD_STRING_VIEW
  BOOST_JSON_STANDALONE)

find_package(fmt CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)

# Bundle
add_custom_target(bundle ALL
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} -E remove_directory build/bundle/release
  COMMAND npm run build
  USES_TERMINAL)

# Install
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/ DESTINATION data)

install(CODE "file(REMOVE_RECURSE \"${CMAKE_INSTALL_PREFIX}/html\")")
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/bundle/release/ DESTINATION html)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(FILES ${PROJECT_NAME}.ini DESTINATION etc)

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

# Visual Studio Code
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  configure_file(res/config/launch.json.in ${CMAKE_SOURCE_DIR}/.vscode/launch.json @ONLY LF)
  configure_file(res/config/tasks.json.in ${CMAKE_SOURCE_DIR}/.vscode/tasks.json @ONLY LF)
endif()

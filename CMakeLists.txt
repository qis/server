cmake_minimum_required(VERSION 3.9 FATAL_ERROR)
project(server VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  string(REPLACE "/W3" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "/std:c++latest /await ${CMAKE_CXX_FLAGS} /utf-8 /wd4100")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /manifestuac:NO /ignore:4098 /ignore:4099")
  add_definitions(-D_UNICODE -DUNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -D_ATL_SECURE_NO_DEPRECATE)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS)
  add_definitions(-DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00)
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fcoroutines-ts")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wconversion -Wno-unused-parameter")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-absolute-paths -fcolor-diagnostics")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
endif()

if(MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GS- /GL")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
  set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
else()
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")
  #set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native -flto")
  #set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -fuse-ld=lld -flto")
endif()

if(MSVC)
  string(REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
elseif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

if(WIN32)
  set(NET_API "IOCP")
  set(NET_SYSTEM "windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(NET_API "EPOLL")
  set(NET_SYSTEM "unix")
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  set(NET_API "KQUEUE")
  set(NET_SYSTEM "unix")
endif()

file(GLOB_RECURSE headers include/net/*.h)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include/net PREFIX include FILES ${headers})

file(GLOB sources
  src/net/*.h src/net/*.cpp
  src/net/tls/*.h src/net/tls/*.cpp
  src/net/http/*.h src/net/http/*.cpp
  src/net/${NET_SYSTEM}/*.cpp)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src/net PREFIX src FILES ${sources})

add_library(net STATIC ${headers} ${sources})
target_compile_definitions(net PRIVATE "-DNET_USE_${NET_API}=1")

target_include_directories(net PRIVATE src PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

if(WIN32)
  set_target_properties(net PROPERTIES OUTPUT_NAME net-$<LOWER_CASE:$<CONFIG>>)
  target_link_libraries(net PUBLIC ws2_32 mswsock)
endif()

find_package(fmt REQUIRED PATHS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fmt NO_DEFAULT_PATH)
target_link_libraries(net PUBLIC fmt::fmt)

find_package(tls REQUIRED PATHS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tls NO_DEFAULT_PATH)
target_link_libraries(net PRIVATE tls::tls)

#install(DIRECTORY include/net DESTINATION include FILES_MATCHING PATTERN "*.h")

#install(TARGETS net EXPORT net_export
#  RUNTIME DESTINATION bin
#  LIBRARY DESTINATION lib
#  ARCHIVE DESTINATION lib)

#if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/net-config.cmake)
#  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/net-config.cmake
#    [[include("${CMAKE_CURRENT_LIST_DIR}/net-targets.cmake")]])
#endif()

#install(FILES ${CMAKE_CURRENT_BINARY_DIR}/net-config.cmake DESTINATION lib/cmake)
#install(EXPORT net_export FILE net-targets.cmake NAMESPACE net:: DESTINATION lib/cmake)

file(GLOB sources src/*.h src/*.cpp)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX src FILES ${sources})
add_executable(${PROJECT_NAME} ${sources})
target_link_libraries(${PROJECT_NAME} PRIVATE net)
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

if(MSVC)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
  set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER build)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
  set_target_properties(server PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

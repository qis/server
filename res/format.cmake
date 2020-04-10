set(ProgramFilesX86 "ProgramFiles(x86)")
set(ProgramFilesX86 "$ENV{${ProgramFilesX86}}")

find_program(clang_format NAMES clang-format PATHS
  $ENV{VCPKG_ROOT}/triplets/toolchains/llvm/bin
  $ENV{ProgramW6432}/LLVM/bin
  $ENV{ProgramFiles}/LLVM/bin
  ${ProgramFilesX86}/LLVM/bin)

if(NOT clang_format)
  message(FATAL_ERROR "Could not find executable: clang-format")
endif()

file(GLOB_RECURSE sources include/*.hpp include/*.h src/*.hpp src/*.cpp src/*.h src/*.c)

if(sources)
  set(report)
  set(sources_relative)
  if(clang_format MATCHES " ")
    string(APPEND report "\"${clang_format}\" -i")
  else()
    string(APPEND report "${clang_format} -i")
  endif()
  foreach(file_absolute ${sources})
    file(RELATIVE_PATH file_relative ${CMAKE_CURRENT_SOURCE_DIR} ${file_absolute})
    list(APPEND sources_relative ${file_relative})
    if(WIN32)
      string(APPEND report " ^\n  ${file_relative}")
    else()
      string(APPEND report " \\\n  ${file_relative}")
    endif()
  endforeach()
  message("${report}")
  execute_process(COMMAND "${clang_format}" -i ${sources_relative})
endif()

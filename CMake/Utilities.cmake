# only fetch target repo for add_subdirectory later
function(fetch_repo lib_name)
  set(supported_libs
      kvspic)
  list(FIND supported_libs ${lib_name} index)
  if(${index} EQUAL -1)
    message(WARNING "${lib_name} is not supported for fetch_repo")
    return()
  endif()

  # anything after lib_name(${ARGN}) are assumed to be arguments passed over to
  # library building cmake.
  set(build_args ${ARGN})

  # build library
  configure_file(
    ./CMake/Dependencies/lib${lib_name}-CMakeLists.txt
    ${DEPENDENCY_DOWNLOAD_PATH}/lib${lib_name}/CMakeLists.txt COPYONLY)
  execute_process(
    COMMAND ${CMAKE_COMMAND} ${build_args}
            ${CMAKE_GENERATOR} .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${DEPENDENCY_DOWNLOAD_PATH}/lib${lib_name})
  if(result)
    message(FATAL_ERROR "CMake step for lib${lib_name} failed: ${result}")
  endif()
  execute_process(
    COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${DEPENDENCY_DOWNLOAD_PATH}/lib${lib_name})
  if(result)
    message(FATAL_ERROR "CMake step for lib${lib_name} failed: ${result}")
  endif()
endfunction()

# build library from source
function(build_dependency lib_name)
  set(supported_libs
      gtest
      openssl
      srtp
      usrsctp
      websockets
      curl
      mbedtls)
  list(FIND supported_libs ${lib_name} index)
  if(${index} EQUAL -1)
    message(WARNING "${lib_name} is not supported to build from source")
    return()
  endif()

  set(lib_file_name ${lib_name})
  if (${lib_name} STREQUAL "openssl")
    set(lib_file_name ssl)
  elseif(${lib_name} STREQUAL "srtp")
    set(lib_file_name srtp2)
  endif()
  set(library_found NOTFOUND)
  find_library(
    library_found
    NAMES ${lib_file_name}
    PATHS ${OPEN_SRC_INSTALL_PREFIX}/lib
    NO_DEFAULT_PATH)
  if(library_found)
    message(STATUS "${lib_name} already built")
    return()
  endif()

  # anything after lib_name(${ARGN}) are assumed to be arguments passed over to
  # library building cmake.
  set(build_args ${ARGN})

  file(REMOVE_RECURSE ${OPEN_SRC_INSTALL_PREFIX}/lib${lib_name})

  # build library
  configure_file(
    ./CMake/Dependencies/lib${lib_name}-CMakeLists.txt
    ${OPEN_SRC_INSTALL_PREFIX}/lib${lib_name}/CMakeLists.txt COPYONLY)
  execute_process(
    COMMAND ${CMAKE_COMMAND} ${build_args}
            -DOPEN_SRC_INSTALL_PREFIX=${OPEN_SRC_INSTALL_PREFIX} -G
            ${CMAKE_GENERATOR} .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${OPEN_SRC_INSTALL_PREFIX}/lib${lib_name})
  if(result)
    message(FATAL_ERROR "CMake step for lib${lib_name} failed: ${result}")
  endif()
  execute_process(
    COMMAND ${CMAKE_COMMAND} --build . --parallel
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${OPEN_SRC_INSTALL_PREFIX}/lib${lib_name})
  if(result)
    message(FATAL_ERROR "CMake step for lib${lib_name} failed: ${result}")
  endif()

  file(REMOVE_RECURSE ${OPEN_SRC_INSTALL_PREFIX}/lib${lib_name})

endfunction()

function(enableSanitizer SANITIZER)
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -O0 -g -fsanitize=${SANITIZER} -fno-omit-frame-pointer"
      PARENT_SCOPE)
  set(CMAKE_C_FLAGS
      "${CMAKE_C_FLAGS} -O0 -g -fsanitize=${SANITIZER} -fno-omit-frame-pointer -fno-optimize-sibling-calls"
      PARENT_SCOPE)
  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${SANITIZER}"
      PARENT_SCOPE)
endfunction()

# Returns a list of arguments that evaluate to true
function(count_true output_count_var)
  set(lst_len 0)
  foreach(option_var IN LISTS ARGN)
    if(${option_var})
      math(EXPR lst_len "${lst_len} + 1")
    endif()
  endforeach()
  set(${output_count_var} ${lst_len} PARENT_SCOPE)
endfunction()


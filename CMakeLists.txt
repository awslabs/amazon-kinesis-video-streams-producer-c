cmake_minimum_required(VERSION 2.8)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")
include(Utilities)
project(KinesisVideoProducerC LANGUAGES C)

SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR mips)

# User Flags
option(ADD_MUCLIBC "Add -muclibc c flag" ON)
option(BUILD_DEPENDENCIES "Whether or not to build depending libraries from source" OFF)
option(USE_OPENSSL "Use openssl as crypto library" ON)
option(BUILD_COMMON_LWS "Whether or not to build ProducerC libwebsockets common library" OFF)
option(BUILD_COMMON_CURL "Whether or not to build ProducerC curl common library" ON)
option(BUILD_OPENSSL_PLATFORM "If buildng OpenSSL what is the target platform" OFF)

# Developer Flags
option(BUILD_TEST "Build the testing tree." OFF)
option(CODE_COVERAGE "Enable coverage reporting" OFF)
option(COMPILER_WARNINGS "Enable all compiler warnings." OFF)
option(ADDRESS_SANITIZER "Build with AddressSanitizer." OFF)
option(MEMORY_SANITIZER "Build with MemorySanitizer." OFF)
option(THREAD_SANITIZER "Build with ThreadSanitizer." OFF)
option(UNDEFINED_BEHAVIOR_SANITIZER "Build with UndefinedBehaviorSanitizer." OFF)
option(ALIGNED_MEMORY_MODEL "Aligned memory model ONLY." OFF)

set(CMAKE_MACOSX_RPATH TRUE)

get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
set(KINESIS_VIDEO_PRODUCER_C_SRC "${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "Kinesis Video Producer path is ${KINESIS_VIDEO_PRODUCER_C_SRC}")

if(BUILD_DEPENDENCIES)
  if (NOT OPEN_SRC_INSTALL_PREFIX)
    set(OPEN_SRC_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/open-source)
    set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${OPEN_SRC_INSTALL_PREFIX}/lib/pkgconfig")
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${OPEN_SRC_INSTALL_PREFIX})
  endif()

  if(NOT EXISTS ${OPEN_SRC_INSTALL_PREFIX})
    file(MAKE_DIRECTORY ${OPEN_SRC_INSTALL_PREFIX})
  endif()

  message(STATUS "Begin building dependencies.")

  if(USE_OPENSSL)
    set(BUILD_ARGS -DBUILD_STATIC=${BUILD_STATIC}
      -DBUILD_OPENSSL_PLATFORM=${BUILD_OPENSSL_PLATFORM})
    build_dependency(openssl ${BUILD_ARGS})
  endif()

  if (BUILD_COMMON_LWS)
    set(BUILD_ARGS  -DBUILD_STATIC=${BUILD_STATIC}
      -DOPENSSL_DIR=${OPEN_SRC_INSTALL_PREFIX}
      -DUSE_OPENSSL=${USE_OPENSSL})
    build_dependency(websockets ${BUILD_ARGS})
  endif()

  if (BUILD_COMMON_CURL)
    set(BUILD_ARGS  -DBUILD_STATIC=${BUILD_STATIC}
      -DUSE_OPENSSL=${USE_OPENSSL})
    build_dependency(curl ${BUILD_ARGS})
  endif()

  if(BUILD_TEST)
    build_dependency(gtest)
  endif()

  message(STATUS "Finished building dependencies.")
endif()

############# fetch repos that we need do add_subdirectory ############

# repos that we will build using add_subdirectory will be stored in this path
set(DEPENDENCY_DOWNLOAD_PATH ${CMAKE_CURRENT_SOURCE_DIR}/dependency)
set(BUILD_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
fetch_repo(kvspic ${BUILD_ARGS})
add_subdirectory("${DEPENDENCY_DOWNLOAD_PATH}/libkvspic/kvspic-src")
file(GLOB PIC_HEADERS "${pic_project_SOURCE_DIR}/src/*/include")
include_directories("${PIC_HEADERS}")

############# fetch repos that we need do add_subdirectory done ############


############# find dependent libraries ############

find_package(Threads)
find_package(PkgConfig REQUIRED)

set(OPEN_SRC_INCLUDE_DIRS ${LIBKVSPIC_INCLUDE_DIRS})

if(USE_OPENSSL)
  find_package(OpenSSL REQUIRED)
  if(NOT OPENSSL_FOUND AND NOT BUILD_DEPENDENCIES)
    message(FATAL_ERROR "OpenSSL is not found. Make sure to export PKG_CONFIG_PATH to where OpenSSL's pc file is")
  endif()
  set(OPEN_SRC_INCLUDE_DIRS ${OPEN_SRC_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
endif()

if(BUILD_COMMON_CURL)
  if (OPEN_SRC_INSTALL_PREFIX)
    find_package(CURL REQUIRED PATHS ${OPEN_SRC_INSTALL_PREFIX})
  else()
    find_package(CURL REQUIRED)
  endif()

  set(OPEN_SRC_INCLUDE_DIRS ${OPEN_SRC_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})
  link_directories(${CURL_LIBRARY_DIRS})
endif()

if (BUILD_COMMON_LWS)
  pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)
  set(OPEN_SRC_INCLUDE_DIRS ${OPEN_SRC_INCLUDE_DIRS} ${LIBWEBSOCKETS_INCLUDE_DIRS})
  link_directories(${LIBWEBSOCKETS_LIBRARY_DIRS})
endif()

############# find dependent libraries end ############


message(STATUS "Bitness 32 bits")
set(KINESIS_VIDEO_BUILD_BITNESS "mips32")
set(KINESIS_VIDEO_BUILD_BITNESS_TYPE "linux-mips32")
set(KINESIS_VIDEO_BUILD_BITNESS_NAME "mips32")


if(ADD_MUCLIBC)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -muclibc")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

file(GLOB KVS_COMMON_SOURCE_FILES_BASE "src/source/Common/*.c")
file(GLOB KVS_COMMON_SOURCE_FILES_LWS "src/source/Common/Lws/*.c")
file(GLOB KVS_COMMON_SOURCE_FILES_CURL "src/source/Common/Curl/*.c")
file(GLOB PRODUCER_C_SOURCE_FILES "src/source/*.c")

include_directories(${KINESIS_VIDEO_PRODUCER_C_SRC}/src/include)
include_directories(${OPEN_SRC_INCLUDE_DIRS})

if(ALIGNED_MEMORY_MODEL)
  add_definitions(-DALIGNED_MEMORY_MODEL)
endif()

# use openssl by default
set(CPRODUCER_COMMON_TLS_OPTION KVS_USE_OPENSSL)
set(PRODUCER_CRYPTO_LIBRARY
  OpenSSL::Crypto
  OpenSSL::SSL)

install(
  DIRECTORY ${KINESIS_VIDEO_PRODUCER_C_SRC}/src/include
  DESTINATION .)

if(BUILD_COMMON_LWS)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/libkvsCommonLws.pc.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/libkvsCommonLws.pc" @ONLY)

  add_library(kvsCommonLws STATIC ${KVS_COMMON_SOURCE_FILES_BASE} ${KVS_COMMON_SOURCE_FILES_LWS})
  target_compile_definitions(kvsCommonLws PRIVATE KVS_BUILD_WITH_LWS ${CPRODUCER_COMMON_TLS_OPTION})
  target_link_libraries(kvsCommonLws
    ${OPENSSL_CRYPTO_LIBRARY}
    ${OPENSSL_SSL_LIBRARY}
    ${LIBWEBSOCKETS_LIBRARIES}
    kvspicUtils)

  install(
    TARGETS kvsCommonLws
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libkvsCommonLws.pc
    DESTINATION lib/pkgconfig)
endif()

if(BUILD_COMMON_CURL)
  # producer only uses curl right now
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/libkvsCommonCurl.pc.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/libkvsCommonCurl.pc" @ONLY)

  add_library(kvsCommonCurl STATIC ${KVS_COMMON_SOURCE_FILES_BASE} ${KVS_COMMON_SOURCE_FILES_CURL})
  target_compile_definitions(kvsCommonCurl PRIVATE KVS_BUILD_WITH_CURL ${CPRODUCER_COMMON_TLS_OPTION})
  target_link_libraries(kvsCommonCurl
    ${PRODUCER_CRYPTO_LIBRARY}
    kvspicUtils
    CURL::libcurl)

  install(
    TARGETS kvsCommonCurl
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libkvsCommonCurl.pc
    DESTINATION lib/pkgconfig)

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/libcproducer.pc.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/libcproducer.pc" @ONLY)

  add_library(cproducer SHARED ${PRODUCER_C_SOURCE_FILES})
  target_link_libraries(cproducer PUBLIC kvsCommonCurl kvspic)

  install(
    TARGETS cproducer
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libcproducer.pc
    DESTINATION lib/pkgconfig)

  add_executable(kvsVideoOnlyStreamingSample ${KINESIS_VIDEO_PRODUCER_C_SRC}/samples/KvsVideoOnlyStreamingSample.c)
  target_link_libraries(kvsVideoOnlyStreamingSample cproducer)

  add_executable(kvsAacAudioVideoStreamingSample ${KINESIS_VIDEO_PRODUCER_C_SRC}/samples/KvsAacAudioVideoStreamingSample.c)
  target_link_libraries(kvsAacAudioVideoStreamingSample cproducer)

  if (BUILD_TEST)
    add_subdirectory(tst)
  endif()

endif()


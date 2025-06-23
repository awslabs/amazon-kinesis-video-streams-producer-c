cmake_minimum_required(VERSION 3.6.3)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")
include(Utilities)
project(KinesisVideoProducerC VERSION 1.6.0 LANGUAGES C)

set(KINESIS_VIDEO_PRODUCER_C_MAJOR_VERSION 1)
set(KINESIS_VIDEO_PRODUCER_C_MINOR_VERSION 6)
set(KINESIS_VIDEO_PRODUCER_C_PATCH_VERSION 0)
set(KINESIS_VIDEO_PRODUCER_C_VERSION ${KINESIS_VIDEO_PRODUCER_C_MAJOR_VERSION}.${KINESIS_VIDEO_PRODUCER_C_MINOR_VERSION}.${KINESIS_VIDEO_PRODUCER_C_PATCH_VERSION})

include(GNUInstallDirs)


# User Flags
option(ADD_MUCLIBC "Add -muclibc c flag" OFF)
option(BUILD_STATIC "Static build" OFF)
option(BUILD_DEPENDENCIES "Whether or not to build depending libraries from source" ON)
option(USE_OPENSSL "Use openssl as crypto library" ON)
option(USE_MBEDTLS "Use mbedtls as crypto library" OFF)
option(BUILD_COMMON_LWS "Whether or not to build ProducerC libwebsockets common library" OFF)
option(BUILD_COMMON_CURL "Whether or not to build ProducerC curl common library" ON)
option(BUILD_OPENSSL_PLATFORM "If buildng OpenSSL what is the target platform" OFF)
option(LOCAL_OPENSSL_BUILD "Whether or not to use local OpenSSL build" OFF)
option(CONSTRAINED_DEVICE "Change pthread stack size" OFF)

# Endpoint selection - mutually exclusive
option(AWS_KVS_USE_LEGACY_ENDPOINT_ONLY "Use only legacy IPv4-only endpoints (ignores env vars)" OFF)
option(AWS_KVS_USE_DUAL_STACK_ENDPOINT_ONLY "Use only dual-stack endpoints (ignores env vars)" OFF)

# DNS IP resolution modes - mutually exclusive
option(AWS_KVS_IPV4_ONLY "Use only IPv4 addresses from DNS (ignores env vars)" OFF)
option(AWS_KVS_IPV6_ONLY "Use only IPv6 addresses from DNS (ignores env vars)" OFF)
option(AWS_KVS_IPV4_AND_IPV6_ONLY "Use both IPv4 and IPv6 addresses from DNS (ignores env vars)" OFF)

# Developer Flags
option(BUILD_TEST "Build the testing tree." OFF)
option(CODE_COVERAGE "Enable coverage reporting" OFF)
option(COMPILER_WARNINGS "Enable all compiler warnings." OFF)
option(ADDRESS_SANITIZER "Build with AddressSanitizer." OFF)
option(MEMORY_SANITIZER "Build with MemorySanitizer." OFF)
option(THREAD_SANITIZER "Build with ThreadSanitizer." OFF)
option(UNDEFINED_BEHAVIOR_SANITIZER "Build with UndefinedBehaviorSanitizer." OFF)
option(ALIGNED_MEMORY_MODEL "Aligned memory model ONLY." OFF)
option(SET_SSL_CALLBACKS "Set SSL thread and lock callbacks." OFF)

set(TYPE_OF_LIB SHARED)
if (BUILD_STATIC)
  set(TYPE_OF_LIB STATIC)
endif()

set(CMAKE_MACOSX_RPATH TRUE)

get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)

add_definitions(-DVERSION_STRING=\"${PROJECT_VERSION}\")

if(CONSTRAINED_DEVICE)
    add_definitions(-DCONSTRAINED_DEVICE)
endif()

if(NOT KINESIS_VIDEO_PRODUCER_C_SRC)
    if(DEFINED ENV{KINESIS_VIDEO_PRODUCER_C_SRC})
        set(KINESIS_VIDEO_PRODUCER_C_SRC $ENV{KINESIS_VIDEO_PRODUCER_C_SRC})
    else()
        set(KINESIS_VIDEO_PRODUCER_C_SRC "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
endif()

message(STATUS "Kinesis Video Producer path is ${KINESIS_VIDEO_PRODUCER_C_SRC}")

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting CMAKE_BUILD_TYPE to Release by default")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

# Validate mutually exclusive endpoint options
if(AWS_KVS_USE_LEGACY_ENDPOINT_ONLY AND AWS_KVS_USE_DUAL_STACK_ENDPOINT_ONLY)
    message(FATAL_ERROR "Cannot enable both AWS_KVS_USE_LEGACY_ENDPOINT_ONLY and AWS_KVS_USE_DUAL_STACK_ENDPOINT_ONLY at the same time.")
endif()

# Validate mutually exclusive IP resolution options
set(IPV4_ONLY_FLAG 0)
set(IPV6_ONLY_FLAG 0)
set(IPV4_AND_IPV6_FLAG 0)

if(AWS_KVS_IPV4_ONLY)
    set(IPV4_ONLY_FLAG 1)
endif()

if(AWS_KVS_IPV6_ONLY)
    set(IPV6_ONLY_FLAG 1)
endif()

if(AWS_KVS_IPV4_AND_IPV6_ONLY)
    set(IPV4_AND_IPV6_FLAG 1)
endif()

# Sum the enabled flags
math(EXPR IPV_OPTIONS_ENABLED_COUNT
        "${IPV4_ONLY_FLAG}+${IPV6_ONLY_FLAG}+${IPV4_AND_IPV6_FLAG}")

# Check for conflicts
if(IPV_OPTIONS_ENABLED_COUNT GREATER 1)
    message(FATAL_ERROR "Only one of AWS_KVS_IPV4_ONLY, AWS_KVS_IPV6_ONLY, or AWS_KVS_IPV4_AND_IPV6_ONLY can be enabled.")
endif()

# Translate the CMake flags to the pre-processor macros
if(AWS_KVS_IPV4_ONLY)
    add_definitions(-DAWS_KVS_IPV4_ONLY)
endif()

if(AWS_KVS_IPV6_ONLY)
    add_definitions(-DAWS_KVS_IPV6_ONLY)
endif()

if(AWS_KVS_IPV4_AND_IPV6_ONLY)
    add_definitions(-DAWS_KVS_IPV4_AND_IPV6_ONLY)
endif()

if(AWS_KVS_USE_LEGACY_ENDPOINT_ONLY)
    add_definitions(-DAWS_KVS_USE_LEGACY_ENDPOINT_ONLY)
endif()

if(AWS_KVS_USE_DUAL_STACK_ENDPOINT_ONLY)
    add_definitions(-DAWS_KVS_USE_DUAL_STACK_ENDPOINT_ONLY)
endif()

if(BUILD_DEPENDENCIES)
  if (NOT OPEN_SRC_INSTALL_PREFIX)
    set(OPEN_SRC_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/open-source)

    # Note: PKG_CONFIG_PATH is an environment variable. Should point to the
    # folder containing the .pc files
    # Needed for libcurl to find OpenSSL peer dependency
    set(ENV{PKG_CONFIG_PATH}
          "$ENV{PKG_CONFIG_PATH}:${OPEN_SRC_INSTALL_PREFIX}/lib/pkgconfig")
    set(CMAKE_PREFIX_PATH ${OPEN_SRC_INSTALL_PREFIX} ${CMAKE_PREFIX_PATH})
    message(STATUS "PKG_CONFIG_PATH: $ENV{PKG_CONFIG_PATH}")
  endif()

  if(NOT EXISTS ${OPEN_SRC_INSTALL_PREFIX})
    file(MAKE_DIRECTORY ${OPEN_SRC_INSTALL_PREFIX})
  endif()

  message(STATUS "Begin building dependencies.")

  set(SSL_OPTIONS USE_OPENSSL USE_MBEDTLS)
  count_true(ENABLED_SSL_OPTION_COUNT
    ${SSL_OPTIONS})

  if(ENABLED_SSL_OPTION_COUNT GREATER "1")
    message(FATAL_ERROR "Only one of ${SSL_OPTIONS} can be enabled")
  endif()

  if(NOT LOCAL_OPENSSL_BUILD)
    message(STATUS "Building non-local OpenSSL")
    if(USE_OPENSSL)
      set(BUILD_ARGS -DBUILD_STATIC=${BUILD_STATIC}
                    -DBUILD_OPENSSL_PLATFORM=${BUILD_OPENSSL_PLATFORM})
      build_dependency(openssl ${BUILD_ARGS})
    elseif(USE_MBEDTLS)
      set(BUILD_ARGS -DBUILD_STATIC=${BUILD_STATIC} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
      build_dependency(mbedtls ${BUILD_ARGS})
    else()
      message(FATAL_ERROR "No crypto library selected.")
    endif()
  endif()

  if (BUILD_COMMON_LWS)
    set(BUILD_ARGS  -DBUILD_STATIC=${BUILD_STATIC}
                    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                    -DOPENSSL_DIR=${OPEN_SRC_INSTALL_PREFIX}
                    -DUSE_OPENSSL=${USE_OPENSSL}
                    -DUSE_MBEDTLS=${USE_MBEDTLS})
    build_dependency(websockets ${BUILD_ARGS})
  endif()

  if (BUILD_COMMON_CURL)
    set(BUILD_ARGS  -DBUILD_STATIC=${BUILD_STATIC}
                    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                    -DUSE_OPENSSL=${USE_OPENSSL}
                    -DUSE_MBEDTLS=${USE_MBEDTLS})
    build_dependency(curl ${BUILD_ARGS})

  endif()

  if(BUILD_TEST)
    build_dependency(gtest)
  endif()

  message(STATUS "Finished building dependencies.")
endif()

find_package(PkgConfig REQUIRED)

############# Check system for kvspic #############

pkg_check_modules(KVSPIC libkvspicUtils)
if(KVSPIC_FOUND)
  set(OPEN_SRC_INCLUDE_DIRS ${OPEN_SRC_INCLUDE_DIRS} ${KVSPIC_INCLUDE_DIRS})
  link_directories(${KVSPIC_LIBRARY_DIRS})
else()
  ############# fetch repos that we need do add_subdirectory ############
  # repos that we will build using add_subdirectory will be stored in this path
  set(DEPENDENCY_DOWNLOAD_PATH ${CMAKE_CURRENT_SOURCE_DIR}/dependency)
  set(BUILD_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
  fetch_repo(kvspic ${BUILD_ARGS})
  add_subdirectory("${DEPENDENCY_DOWNLOAD_PATH}/libkvspic/kvspic-src")
  file(GLOB PIC_HEADERS "${pic_project_SOURCE_DIR}/src/*/include")
  include_directories("${PIC_HEADERS}")

  ############# fetch repos that we need do add_subdirectory done ############
endif()

############# Check system for kvspic done #############

############# find dependent libraries ############

find_package(Threads)

set(OPEN_SRC_INCLUDE_DIRS ${LIBKVSPIC_INCLUDE_DIRS})

if(USE_OPENSSL)
  find_package(OpenSSL)
  if(NOT OPENSSL_FOUND AND NOT BUILD_DEPENDENCIES)
    message(FATAL_ERROR "OpenSSL is not found. Make sure to export PKG_CONFIG_PATH to where OpenSSL's pc file is")
  endif()
  set(OPEN_SRC_INCLUDE_DIRS ${OPEN_SRC_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
elseif(USE_MBEDTLS)
  find_package(MbedTLS REQUIRED)
  set(OPEN_SRC_INCLUDE_DIRS ${OPEN_SRC_INCLUDE_DIRS} ${MBEDTLS_INCLUDE_DIRS})
else()
  message(FATAL_ERROR "No crypto library selected.")
endif()

if(BUILD_COMMON_CURL)
  if (WIN32)
    find_package(CURL REQUIRED)
    set(CURL_LIBRARIES CURL::libcurl)
  else()
    pkg_check_modules(CURL REQUIRED libcurl)
  endif()

  set(OPEN_SRC_INCLUDE_DIRS ${OPEN_SRC_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})
  link_directories(${CURL_LIBRARY_DIRS})
endif()

if (BUILD_COMMON_LWS)
  if (WIN32)
    find_package(LIBWEBSOCKETS REQUIRED PATHS ${OPEN_SRC_INSTALL_PREFIX})
  else()
    pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)
  endif()

  set(OPEN_SRC_INCLUDE_DIRS ${OPEN_SRC_INCLUDE_DIRS} ${LIBWEBSOCKETS_INCLUDE_DIRS})
  link_directories(${LIBWEBSOCKETS_LIBRARY_DIRS})
endif()

############# find dependent libraries end ############

if(CMAKE_SIZEOF_VOID_P STREQUAL 4)
    message(STATUS "Bitness 32 bits")
    set(KINESIS_VIDEO_BUILD_BITNESS "x86")
    set(KINESIS_VIDEO_BUILD_BITNESS_TYPE "Win32")
    set(KINESIS_VIDEO_BUILD_BITNESS_NAME "x86")
elseif(CMAKE_SIZEOF_VOID_P STREQUAL 8)
    message(STATUS "Bitness 64 bits")
    set(KINESIS_VIDEO_BUILD_BITNESS "x64")
    set(KINESIS_VIDEO_BUILD_BITNESS_TYPE "x64")
    set(KINESIS_VIDEO_BUILD_BITNESS_NAME "x86_64")
else()
    message(FATAL_ERROR "Unknown bitness")
endif()

if(${CMAKE_C_COMPILER_ID} MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

    if(ADD_MUCLIBC)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -muclibc")
    endif()

    if(CODE_COVERAGE)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -fprofile-arcs -ftest-coverage")
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    endif()
    function(enableSanitizer SANITIZER)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -fsanitize=${SANITIZER} -fno-omit-frame-pointer" PARENT_SCOPE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -fsanitize=${SANITIZER} -fno-omit-frame-pointer -fno-optimize-sibling-calls" PARENT_SCOPE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${SANITIZER}" PARENT_SCOPE)
    endfunction()
    if(ADDRESS_SANITIZER)
        enableSanitizer("address")
    endif()
    if(MEMORY_SANITIZER)
        enableSanitizer("memory")
    endif()
    if(THREAD_SANITIZER)
        enableSanitizer("thread")
    endif()
    if(UNDEFINED_BEHAVIOR_SANITIZER)
        enableSanitizer("undefined")
    endif()
endif()

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING)
endif()

file(GLOB KVS_COMMON_SOURCE_FILES_BASE "src/source/Common/*.c")
file(GLOB KVS_COMMON_SOURCE_FILES_LWS "src/source/Common/Lws/*.c")
file(GLOB KVS_COMMON_SOURCE_FILES_CURL "src/source/Common/Curl/*.c")
file(GLOB PRODUCER_C_SOURCE_FILES "src/source/*.c")

include_directories(${KINESIS_VIDEO_PRODUCER_C_SRC}/src/include)
include_directories(${OPEN_SRC_INCLUDE_DIRS})

if(ALIGNED_MEMORY_MODEL)
    add_definitions(-DALIGNED_MEMORY_MODEL)
endif()

if(SET_SSL_CALLBACKS)
    add_definitions(-DSET_SSL_CALLBACKS)
endif()


# use openssl by default
set(CPRODUCER_COMMON_TLS_OPTION KVS_USE_OPENSSL)

set(PRODUCER_CRYPTO_LIBRARY
        OpenSSL::Crypto
        OpenSSL::SSL)
if (USE_MBEDTLS)
    set(CPRODUCER_COMMON_TLS_OPTION KVS_USE_MBEDTLS)
    set(PRODUCER_CRYPTO_LIBRARY
            MbedTLS
            MbedCrypto)
endif()

install(
  DIRECTORY ${KINESIS_VIDEO_PRODUCER_C_SRC}/src/include
  DESTINATION .)

if(BUILD_COMMON_LWS)
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/libkvsCommonLws.pc.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/libkvsCommonLws.pc" @ONLY)

    if (WIN32)
      add_library(kvsCommonLws STATIC ${KVS_COMMON_SOURCE_FILES_BASE} ${KVS_COMMON_SOURCE_FILES_LWS})
    else()
      add_library(kvsCommonLws ${TYPE_OF_LIB} ${KVS_COMMON_SOURCE_FILES_BASE} ${KVS_COMMON_SOURCE_FILES_LWS})
    endif()
    target_compile_definitions(kvsCommonLws PRIVATE KVS_BUILD_WITH_LWS ${CPRODUCER_COMMON_TLS_OPTION})
    if(NOT BUILD_STATIC)
      set_target_properties(kvsCommonLws PROPERTIES VERSION ${KINESIS_VIDEO_PRODUCER_C_VERSION} SOVERSION ${KINESIS_VIDEO_PRODUCER_C_MAJOR_VERSION})
    endif()
    target_link_libraries(kvsCommonLws
            ${PRODUCER_CRYPTO_LIBRARY}
            ${OPENSSL_CRYPTO_LIBRARY}
            ${OPENSSL_SSL_LIBRARY}
            ${LIBWEBSOCKETS_LIBRARIES}
            kvspicUtils)

    install(
      TARGETS kvsCommonLws
      ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
      LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(
      FILES ${CMAKE_CURRENT_BINARY_DIR}/libkvsCommonLws.pc
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif()

if(BUILD_COMMON_CURL)
    # producer only uses curl right now
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/libkvsCommonCurl.pc.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/libkvsCommonCurl.pc" @ONLY)

    if(BUILD_STATIC)
      # Curl will enable ZLIB as part of its build if it finds
      # the package. We need to therefore link it for static builds. 
      find_package(ZLIB)
      if(ZLIB_FOUND)
        list(APPEND CURL_LIBRARIES z)
      endif()
    endif()

    if (WIN32)
      add_library(kvsCommonCurl STATIC ${KVS_COMMON_SOURCE_FILES_BASE} ${KVS_COMMON_SOURCE_FILES_CURL})
    else()
      add_library(kvsCommonCurl ${TYPE_OF_LIB} ${KVS_COMMON_SOURCE_FILES_BASE} ${KVS_COMMON_SOURCE_FILES_CURL})
    endif()
    target_compile_definitions(kvsCommonCurl PRIVATE KVS_BUILD_WITH_CURL ${CPRODUCER_COMMON_TLS_OPTION})
    if(NOT BUILD_STATIC)
      set_target_properties(kvsCommonCurl PROPERTIES VERSION ${KINESIS_VIDEO_PRODUCER_C_VERSION} SOVERSION ${KINESIS_VIDEO_PRODUCER_C_MAJOR_VERSION})
    endif()
    target_link_libraries(kvsCommonCurl
            kvspicUtils
            ${CURL_LIBRARIES}
            ${PRODUCER_CRYPTO_LIBRARY})

    install(
      TARGETS kvsCommonCurl
      ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
      LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(
      FILES ${CMAKE_CURRENT_BINARY_DIR}/libkvsCommonCurl.pc
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/libcproducer.pc.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/libcproducer.pc" @ONLY)

    if (WIN32)
      add_library(cproducer STATIC ${PRODUCER_C_SOURCE_FILES})
    else()
      add_library(cproducer ${TYPE_OF_LIB} ${PRODUCER_C_SOURCE_FILES})
    endif()
    if(NOT BUILD_STATIC)
      set_target_properties(cproducer PROPERTIES VERSION ${KINESIS_VIDEO_PRODUCER_C_VERSION} SOVERSION ${KINESIS_VIDEO_PRODUCER_C_MAJOR_VERSION})
    endif()
    target_link_libraries(cproducer PUBLIC kvsCommonCurl kvspic)

    install(
      TARGETS cproducer
      ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
      LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(
      FILES ${CMAKE_CURRENT_BINARY_DIR}/libcproducer.pc
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

    add_executable(kvsVideoOnlyRealtimeStreamingSample ${KINESIS_VIDEO_PRODUCER_C_SRC}/samples/KvsVideoOnlyRealtimeStreamingSample.c ${KINESIS_VIDEO_PRODUCER_C_SRC}/samples/Common.c)
    target_link_libraries(kvsVideoOnlyRealtimeStreamingSample cproducer)

    add_executable(kvsVideoOnlyOfflineStreamingSample ${KINESIS_VIDEO_PRODUCER_C_SRC}/samples/KvsVideoOnlyOfflineStreamingSample.c ${KINESIS_VIDEO_PRODUCER_C_SRC}/samples/Common.c)
    target_link_libraries(kvsVideoOnlyOfflineStreamingSample cproducer)

    add_executable(kvsAudioVideoStreamingSample ${KINESIS_VIDEO_PRODUCER_C_SRC}/samples/KvsAudioVideoStreamingSample.c ${KINESIS_VIDEO_PRODUCER_C_SRC}/samples/Common.c)
    target_link_libraries(kvsAudioVideoStreamingSample cproducer)

    add_executable(kvsAudioOnlyStreamingSample ${KINESIS_VIDEO_PRODUCER_C_SRC}/samples/KvsAudioOnlyStreamingSample.c ${KINESIS_VIDEO_PRODUCER_C_SRC}/samples/Common.c)
    target_link_libraries(kvsAudioOnlyStreamingSample cproducer)

    if (BUILD_TEST)
        add_subdirectory(tst)
    endif()
endif()

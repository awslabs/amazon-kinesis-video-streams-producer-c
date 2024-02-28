cmake_minimum_required(VERSION 3.6.3)

# Include custom utilites.
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_MODULE_PATH}")
include(Utilities)

project(KinesisVideoProducerC VERSION 1.5.1 LANGUAGES C)

set(KINESIS_VIDEO_PRODUCER_C_MAJOR_VERSION 1)
set(KINESIS_VIDEO_PRODUCER_C_MINOR_VERSION 5)
set(KINESIS_VIDEO_PRODUCER_C_PATCH_VERSION 1)
set(KINESIS_VIDEO_PRODUCER_C_VERSION ${KINESIS_VIDEO_PRODUCER_C_MAJOR_VERSION}.${KINESIS_VIDEO_PRODUCER_C_MINOR_VERSION}.${KINESIS_VIDEO_PRODUCER_C_PATCH_VERSION})

set(SUPPORTED_CRYPTO_LIBRARIES "{OpenSSL, MbedTLS}")

# User Flags
option(ADD_MUCLIBC "Add -muclibc c flag" OFF)
option(BUILD_STATIC "Static build" OFF)
option(BUILD_DEPENDENCIES "Whether or not to build depending libraries from source" ON)
option(USE_OPENSSL "Use openssl as crypto library" ON)
option(USE_MBEDTLS "Use mbedtls as crypto library" OFF)
option(BUILD_COMMON_LWS "Whether or not to build ProducerC libwebsockets common library" OFF)
option(BUILD_COMMON_CURL "Whether or not to build ProducerC curl common library" ON) # TODO: remove this and any other un-needed flags
option(BUILD_OPENSSL_PLATFORM "If buildng OpenSSL what is the target platform" OFF)
option(BUILD_CRYPTO "Whether or not to build OpenSSL or MbedTLS, if not, will use system-installed version" ON)
option(CONSTRAINED_DEVICE "Change pthread stack size" OFF)
option(BUILD_KVS_PIC "Whether to build PIC SDK, else will use system-installed version" ON)
option(BUILD_SAMPLES "Whether to build Producer C samples" ON)


# Maintainer Flags
option(BUILD_TEST "Build the testing tree." OFF)
option(CODE_COVERAGE "Enable coverage reporting" OFF)
option(COMPILER_WARNINGS "Enable all compiler warnings." OFF)
option(ADDRESS_SANITIZER "Build with AddressSanitizer." OFF)
option(MEMORY_SANITIZER "Build with MemorySanitizer." OFF)
option(THREAD_SANITIZER "Build with ThreadSanitizer." OFF)
option(UNDEFINED_BEHAVIOR_SANITIZER "Build with UndefinedBehaviorSanitizer." OFF)
option(ALIGNED_MEMORY_MODEL "Aligned memory model ONLY." OFF)
option(SET_SSL_CALLBACKS "Set SSL thread and lock callbacks." OFF)

if (BUILD_STATIC)
    set(TYPE_OF_LIB STATIC)
else()
    set(TYPE_OF_LIB SHARED)
endif()

if(CONSTRAINED_DEVICE)
    add_definitions(-DCONSTRAINED_DEVICE)
endif()

set(CMAKE_MACOSX_RPATH TRUE)
add_definitions(-DVERSION_STRING=\"${PROJECT_VERSION}\")

# Set where to build this project.
if(DEFINED ENV{KINESIS_VIDEO_PRODUCER_C_SRC})
    set(KINESIS_VIDEO_PRODUCER_C_SRC $ENV{KINESIS_VIDEO_PRODUCER_C_SRC})
else()
    set(KINESIS_VIDEO_PRODUCER_C_SRC "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# Default to release build if unspecified.
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting CMAKE_BUILD_TYPE to Release by default")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

# Fetch and build 3rd-party dependencies if specified.
if(BUILD_DEPENDENCIES)
  message(STATUS "Begin building dependencies.")

  # Set directory where to build the dependencies.
  if (NOT OPEN_SRC_INSTALL_PREFIX)
    set(OPEN_SRC_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/open-source)
    set(ENV{PKG_CONFIG_PATH}
          "$ENV{PKG_CONFIG_PATH}:${OPEN_SRC_INSTALL_PREFIX}/lib/pkgconfig")
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${OPEN_SRC_INSTALL_PREFIX})
  endif()

  # Create the dependency directory if it doesn't exist.
  if(NOT EXISTS ${OPEN_SRC_INSTALL_PREFIX})
    file(MAKE_DIRECTORY ${OPEN_SRC_INSTALL_PREFIX})
  endif()

  # Verify only one of SUPPORTED_CRYPTO_LIBRARIES are selected.
  set(SSL_OPTIONS USE_OPENSSL USE_MBEDTLS)
  count_true(ENABLED_SSL_OPTION_COUNT ${SSL_OPTIONS})
  if(ENABLED_SSL_OPTION_COUNT GREATER "1")
    message(FATAL_ERROR "Only one of ${SSL_OPTIONS} can be enabled")
  endif()

  # If not using system's crypto library.
  if(BUILD_CRYPTO)
    if(USE_OPENSSL)
      message(STATUS "Building OpenSSL")
      set(BUILD_ARGS -DBUILD_STATIC=${BUILD_STATIC} -DBUILD_OPENSSL_PLATFORM=${BUILD_OPENSSL_PLATFORM})
      build_dependency(openssl ${BUILD_ARGS})
    elseif(USE_MBEDTLS)
      message(STATUS "Building MbedTLS")
      set(BUILD_ARGS -DBUILD_STATIC=${BUILD_STATIC} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
      build_dependency(mbedtls ${BUILD_ARGS})
    else()
      message(FATAL_ERROR "No crypto library selected. Please specify one of the following: " ${SUPPORTED_CRYPTO_LIBRARIES})
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

message("CMAKE_PREFIX_PATH : ${CMAKE_PREFIX_PATH}") #TODO: Remove this.


############# Checking system for kvspic #############

set(CMAKE_FIND_DEBUG_MODE ON) #TODO: Remove this.

# NOTE: this is possibly a breaking change, if not specified, system PIC will not be used.
# Prev behavior: look for system PIC if not found, build PIC.
# Now: is BUILD_KVS_PIC ? build PIC, else find syste PIC - if system PIC not found, fail build
# TODO: Leave a not in the PIC file libkvspic.pc.cmake that the below comments/messages must be updated 
#       if we change "prefix=" from CMAKE_INSTALL_PREFIX.
if(BUILD_KVS_PIC)
  # TODO: double check with the original comment that used to be here... make sure this comment is correct.
  # KVS PIC will live in the DEPENDENCY_DOWNLOAD_PATH.
  # TODO: Double check all the below is correct/necessary.
  set(DEPENDENCY_DOWNLOAD_PATH ${CMAKE_CURRENT_SOURCE_DIR}/dependency)
  set(BUILD_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
  fetch_repo(kvspic ${BUILD_ARGS})
  add_subdirectory("${DEPENDENCY_DOWNLOAD_PATH}/libkvspic/kvspic-src")
  file(GLOB PIC_HEADERS "${pic_project_SOURCE_DIR}/src/*/include")
  include_directories("${PIC_HEADERS}")
else()
  # Finding PIC on system using the libcproducer.pc.cmake file.
  pkg_check_modules(KVSPIC libkvspicUtils)
  if(KVSPIC_FOUND)
    message("Found system KVS PIC located: ${KVSPIC_PREFIX}")
    set(OPEN_SRC_INCLUDE_DIRS ${OPEN_SRC_INCLUDE_DIRS} ${KVSPIC_INCLUDE_DIRS})
    link_directories(${KVSPIC_LIBRARY_DIRS})
  else()
    message(FATAL_ERROR "Could not find system KVS PIC. PkgConfig searched in: ${KVSPIC_INCLUDE_DIRS} \
                         Please check PIC installation or set BUILD_KVS_PIC to TRUE to build PIC with this project.")
  endif()
endif()

############# Done checking system for kvspic #############



############# Finding 3rd-party dependencies ############

find_package(Threads)

# TODO: Should maybe change "BUILD_CRYPTO" to be similar to the other ones: "BUILD_COMMON_CRYPTO"
#       (and so BUILD_CRYPTO == !BUILD_COMMON_CRYPTO)
if(NOT BUILD_DEPENDENCIES OR NOT BUILD_CRYPTO)
  if(USE_OPENSSL)
      find_package(OpenSSL REQUIRED)
      set(OPEN_SRC_INCLUDE_DIRS ${OPEN_SRC_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
  elseif(USE_MBEDTLS)
    find_package(MbedTLS REQUIRED)
    set(OPEN_SRC_INCLUDE_DIRS ${OPEN_SRC_INCLUDE_DIRS} ${MBEDTLS_INCLUDE_DIRS})
  else()
    message(FATAL_ERROR "No crypto library selected. Please specify one of the following: " ${SUPPORTED_CRYPTO_LIBRARIES})
  endif()
endif()


# TODO: Need to figure out the use case here.
#       If we are building deps, should we check for system libraries at all?
#         -   Is there ever a case that buildDeps=TRUE AND buildCommon<>=TRUE?
#         -   And then is there ever a case buildDeps=FALSE AND buildCommon<>=FALSE?
#         -   Which takes precedence?
#       I would think buildCommon is a way of overriding the in-house building and avoiding 
#         fetching the repo in this project. This way you can keep buildDeps=TRUE and then
#         specify which ones you actually don't want built with the project by setting buildCommon to true.
#       However, currently I have the logic setup reverse from the above: we only use common/system build
#         if buildDeps is off AND buildCommon<> is TRUE for the package.

if(NOT BUILD_DEPENDENCIES AND BUILD_COMMON_CURL) 
  if (WIN32)
      find_package(CURL REQUIRED)
      set(CURL_LIBRARIES CURL::libcurl)
  else()
    pkg_check_modules(CURL REQUIRED libcurl)
  endif()

  set(OPEN_SRC_INCLUDE_DIRS ${OPEN_SRC_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})
  link_directories(${CURL_LIBRARY_DIRS})
endif()

if(NOT BUILD_DEPENDENCIES AND BUILD_COMMON_LWS)
  if (WIN32)
    find_package(LIBWEBSOCKETS REQUIRED PATHS ${OPEN_SRC_INSTALL_PREFIX})
  else()
    pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)
  endif()

  set(OPEN_SRC_INCLUDE_DIRS ${OPEN_SRC_INCLUDE_DIRS} ${LIBWEBSOCKETS_INCLUDE_DIRS})
  link_directories(${LIBWEBSOCKETS_LIBRARY_DIRS})
endif()

############# Done finding 3rd-party dependencies ############


############# TODO: Doing....   ############

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


# TODO: Figure out what is going on in here and add comments.
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

# TODO: What and why is this MSVC thing?
if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING)
endif()


file(GLOB KVS_COMMON_SOURCE_FILES_BASE "src/source/Common/*.c")
file(GLOB KVS_COMMON_SOURCE_FILES_LWS "src/source/Common/Lws/*.c")
file(GLOB KVS_COMMON_SOURCE_FILES_CURL "src/source/Common/Curl/*.c")

file(GLOB PRODUCER_C_SOURCE_FILES "src/source/*.c")
#file(GLOB_RECURSE PRODUCER_C_SOURCE_FILES "src/source/*.c")

list(APPEND PRODUCER_C_SOURCE_FILES
      ${KVS_COMMON_SOURCE_FILES_BASE}
      ${KVS_COMMON_SOURCE_FILES_CURL})

set(OPEN_SRC_INCLUDE_DIRS ${OPEN_SRC_INCLUDE_DIRS} ${OPEN_SRC_INSTALL_PREFIX}/include)


include_directories(${KINESIS_VIDEO_PRODUCER_C_SRC}/src/include)
include_directories(${OPEN_SRC_INCLUDE_DIRS})

# TODO: Probably good to remove the CMAKE_CURRENT_SOURCE_DIR one
link_directories(${CMAKE_SOURCE_DIR}/open-source/lib)

if(ALIGNED_MEMORY_MODEL)
    add_definitions(-DALIGNED_MEMORY_MODEL)
endif()

if(SET_SSL_CALLBACKS)
    add_definitions(-DSET_SSL_CALLBACKS)
endif()

# use openssl by default
set(CPRODUCER_COMMON_TLS_OPTION KVS_USE_OPENSSL)

# set(PRODUCER_CRYPTO_LIBRARY
#         OpenSSL::Crypto
#         OpenSSL::SSL)
if (USE_MBEDTLS)
  set(CPRODUCER_COMMON_TLS_OPTION KVS_USE_MBEDTLS)
#     set(PRODUCER_CRYPTO_LIBRARY
#             MbedTLS
#             MbedCrypto)
endif()

install(
  DIRECTORY ${KINESIS_VIDEO_PRODUCER_C_SRC}/src/include
  DESTINATION .)



# if(BUILD_COMMON_LWS)

  # .......

# endif()


# TODO: I wonder, why would producer ever need lws?
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

  # if (WIN32)
  #   add_library(kvsCommonCurl STATIC ${KVS_COMMON_SOURCE_FILES_BASE} ${KVS_COMMON_SOURCE_FILES_CURL})
  # else()
  #   add_library(kvsCommonCurl ${TYPE_OF_LIB} ${KVS_COMMON_SOURCE_FILES_BASE} ${KVS_COMMON_SOURCE_FILES_CURL})
  # endif()
  # target_compile_definitions(kvsCommonCurl PRIVATE KVS_BUILD_WITH_CURL ${CPRODUCER_COMMON_TLS_OPTION})
  # if(NOT BUILD_STATIC)
  #   set_target_properties(kvsCommonCurl PROPERTIES VERSION ${KINESIS_VIDEO_PRODUCER_C_VERSION} SOVERSION ${KINESIS_VIDEO_PRODUCER_C_MAJOR_VERSION})
  # endif()
  # target_link_libraries(kvsCommonCurl
  #         kvspicUtils
  #         ${CURL_LIBRARIES}
  #         ${PRODUCER_CRYPTO_LIBRARY})

  # install(
  #   TARGETS kvsCommonCurl
  #   ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  #   LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  #   RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
  # install(
  #   FILES ${CMAKE_CURRENT_BINARY_DIR}/libkvsCommonCurl.pc
  #   DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

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

  target_compile_definitions(cproducer PRIVATE KVS_BUILD_WITH_CURL ${CPRODUCER_COMMON_TLS_OPTION})
  
  # TODO: I'd like to know why there is no openssl library and we instead have to use
  #        ssl and crypto (openssl's dependencies).
  target_link_libraries(cproducer PUBLIC kvspic curl ssl crypto)

  message("CMAKE_INSTALL_LIBDIR: ${CMAKE_INSTALL_LIBDIR}")
  install(
    TARGETS cproducer
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libcproducer.pc
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

  add_executable(kvsVideoOnlyRealtimeStreamingSample ${KINESIS_VIDEO_PRODUCER_C_SRC}/samples/KvsVideoOnlyRealtimeStreamingSample.c)
  target_link_libraries(kvsVideoOnlyRealtimeStreamingSample cproducer)

  add_executable(kvsVideoOnlyOfflineStreamingSample ${KINESIS_VIDEO_PRODUCER_C_SRC}/samples/KvsVideoOnlyOfflineStreamingSample.c)
  target_link_libraries(kvsVideoOnlyOfflineStreamingSample cproducer)

  add_executable(kvsAudioVideoStreamingSample ${KINESIS_VIDEO_PRODUCER_C_SRC}/samples/KvsAudioVideoStreamingSample.c)
  target_link_libraries(kvsAudioVideoStreamingSample cproducer)

  add_executable(kvsAudioOnlyStreamingSample ${KINESIS_VIDEO_PRODUCER_C_SRC}/samples/KvsAudioOnlyStreamingSample.c)
  target_link_libraries(kvsAudioOnlyStreamingSample cproducer)

  if (BUILD_TEST)
      add_subdirectory(tst)
  endif()
endif()

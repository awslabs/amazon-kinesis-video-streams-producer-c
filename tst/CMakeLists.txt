cmake_minimum_required(VERSION 3.6.3)

project (producerTest)

set(CMAKE_CXX_STANDARD 11)
set(KINESIS_VIDEO_PRODUCER_C_SRC "${CMAKE_CURRENT_SOURCE_DIR}/..")

if (OPEN_SRC_INSTALL_PREFIX)
  find_package(GTest REQUIRED PATHS ${OPEN_SRC_INSTALL_PREFIX})
else()
  find_package(GTest REQUIRED)
endif()

SET(GTEST_LIBNAME GTest::gtest)
if (TARGET GTest::GTest)
  SET(GTEST_LIBNAME GTest::GTest)
endif()

include_directories(${KINESIS_VIDEO_PRODUCER_C_SRC})

file(GLOB PRODUCER_TEST_SOURCE_FILES "*.cpp")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/sample_file_creds.txt" DESTINATION .)

add_executable(producer_test ${PRODUCER_TEST_SOURCE_FILES})
target_link_libraries(producer_test cproducer ${GTEST_LIBNAME})

if(BUILD_COMMON_LWS)
  add_definitions(-DKVS_CA_CERT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/cert.pem")
  add_definitions(-DCMAKE_DETECTED_CACERT_PATH)
  target_compile_definitions(producer_test PRIVATE KVS_BUILD_WITH_LWS)
  target_link_libraries(producer_test kvsCommonLws ${GTEST_LIBNAME})
endif()
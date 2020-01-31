cmake_minimum_required(VERSION 2.8)

project (producerTest)

set(CMAKE_CXX_STANDARD 11)
set(KINESIS_VIDEO_PRODUCER_C_SRC "${CMAKE_CURRENT_SOURCE_DIR}/..")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)
set(EXE_LIBRARIES Threads::Threads ${CMAKE_DL_LIBS})

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" AND NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(EXE_LIBRARIES ${EXE_LIBRARIES} rt)
endif()

set(GTEST_ROOT $ENV{KVS_GTEST_ROOT})
find_package(GTest REQUIRED)

include_directories(${KINESIS_VIDEO_PRODUCER_C_SRC})

file(GLOB PRODUCER_TEST_SOURCE_FILES "*.cpp")

add_executable(producer_test ${PRODUCER_TEST_SOURCE_FILES})
target_link_libraries(producer_test
        cproducer
        GTest::GTest
        GTest::Main
        ${EXE_LIBRARIES}
        ${Jsmn})

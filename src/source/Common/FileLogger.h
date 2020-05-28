
#ifndef __KINESIS_VIDEO_FILE_LOGGER_INCLUDE_I__
#define __KINESIS_VIDEO_FILE_LOGGER_INCLUDE_I__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * Default values for the limits
 */
#define KVS_COMMON_FILE_INDEX_BUFFER_SIZE           256

/**
 * file logger declaration
 */
typedef struct {
    // string buffer. once the buffer is full, its content will be flushed to file
    PCHAR stringBuffer;

    // Size of the buffer in bytes
    // This will point to the end of the FileLogger to allow for single allocation and preserve the processor cache locality
    UINT64 stringBufferLen;

    // lock protecting the print operation
    MUTEX lock;

    // bytes starting from beginning of stringBuffer that contains valid data
    UINT64 currentOffset;

    // directory to put the log file
    CHAR logFileDir[MAX_PATH_LEN + 1];

    // file to store last log file index
    CHAR indexFilePath[MAX_PATH_LEN + 1];

    // max number of log file allowed
    UINT64 maxFileCount;

    // index for next log file
    UINT64 currentFileIndex;

    // print log to stdout too
    BOOL printLog;

    // file logger logPrint callback
    logPrintFunc fileLoggerLogPrintFn;

    // Original stored logger function
    logPrintFunc storedLoggerLogPrintFn;
} FileLogger, *PFileLogger;

/////////////////////////////////////////////////////////////////////
// Internal functionality
/////////////////////////////////////////////////////////////////////
/**
 * Flushes currentOffset number of chars from stringBuffer into logfile.
 * If maxFileCount is exceeded, the earliest file is deleted before writing to the new file.
 * After flushLogToFile finishes, currentOffset is set to 0, whether the status of execution was success or not.
 *
 * @return - STATUS of execution
 */
STATUS flushLogToFile();

#ifdef  __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_FILE_LOGGER_INCLUDE_I__ */

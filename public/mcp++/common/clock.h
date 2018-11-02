// Copyright 2013, Tencent Inc.
// Author: Liu Cheng <codingliu@tencent.com>
//
#ifndef  MCP_FRAME_COMMON_CLOCK_H
#define  MCP_FRAME_COMMON_CLOCK_H

#include <sys/time.h>
#include <time.h>

namespace tools {

inline struct timeval GET_WALL_CLOCK() {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now;
}

inline struct timeval GET_MONOTONIC_CLOCK() {
    struct timespec monotonic_time;
    struct timeval now;

    clock_gettime(CLOCK_MONOTONIC, &monotonic_time);
    now.tv_sec = monotonic_time.tv_sec;
    now.tv_usec = monotonic_time.tv_nsec / 1000;
    return now;
}

}  // namespace tools

#endif  // MCP_FRAME_COMMON_CLOCK_H

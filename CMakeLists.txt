# This file is here to make CLion users' lives better, but it is not complete.
# One cannot build sipnet from this file. But, it does let CLion do a much
# better job with code analysis.

# As C files are added and/or removed, this file should be updated.

cmake_minimum_required(VERSION 3.30)
project(sipnet C)

set(CMAKE_C_STANDARD 23)

include_directories(src)
include_directories(src/common)
include_directories(src/sipnet)
include_directories(tests)
include_directories(tests/sipnet/test_events_infrastructure)
include_directories(tests/sipnet/test_events_types)
include_directories(tests/sipnet/test_bugfixes)
include_directories(tests/sipnet/test_sipnet_infrastructure)
include_directories(tests/utils)

add_compile_options(-Wall -Wpedantic -Werror)

add_library(commonlib
        src/common/spatialParams.c
        src/common/util.c
)

add_library(sipnetlib
        src/sipnet/cli.c
        src/sipnet/context.c
        src/sipnet/events.c
        src/sipnet/frontend.c
        src/sipnet/outputItems.c
        src/sipnet/runmean.c
        src/sipnet/sipnet.c
)

# This file is here to make CLion users' lives better, but it is not complete.
# One cannot build sipnet from this file. But, it does let CLion do a much
# better job with code analysis.

# As C files are added and/or removed, this file should be updated.

cmake_minimum_required(VERSION 3.30)
project(sipnet C)

set(CMAKE_C_STANDARD 23)

add_compile_definitions(_POSIX_C_SOURCE=200809L)

include_directories(src)
include_directories(src/common)
include_directories(src/sipnet)
include_directories(tests)
include_directories(tests/sipnet/test_events_infrastructure)
include_directories(tests/sipnet/test_events_types)
include_directories(tests/sipnet/test_bugfixes)
include_directories(tests/sipnet/test_sipnet_infrastructure)
include_directories(tests/sipnet/test_modeling)
include_directories(tests/utils)

add_compile_options(-Wall -Wpedantic -Werror)

add_library(commonlib
        src/common/context.c
        src/common/logging.c
        src/common/modelParams.c
        src/common/util.c
)

add_library(sipnetlib
        src/sipnet/cli.c
        src/sipnet/events.c
        src/sipnet/frontend.c
        src/sipnet/outputItems.c
        src/sipnet/runmean.c
        src/sipnet/sipnet.c
        src/sipnet/state.c
)

add_library(tests
        tests/sipnet/test_sipnet_infrastructure/testParamInput.c
        tests/sipnet/test_sipnet_infrastructure/testClimInput.c
        tests/sipnet/test_events_infrastructure/testEventInfra.c
        tests/sipnet/test_events_infrastructure/testEventOutputFile.c
        tests/sipnet/test_events_infrastructure/testEventInfraNeg.c
        tests/sipnet/test_events_types/testEventPlanting.c
        tests/sipnet/test_events_types/testEventIrrigation.c
        tests/sipnet/test_events_types/testEventFertilization.c
        tests/sipnet/test_events_types/testEventHarvest.c
        tests/sipnet/test_events_types/testEventTillage.c
        tests/sipnet/test_bugfixes/testEventFileOrderChecks.c
        tests/sipnet/test_modeling/testNitrogenCycle.c
)

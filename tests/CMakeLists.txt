# GoogleTest setup
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.12.1
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Test sources
set(TEST_SOURCES
    ../src/Process.cpp
    ../src/Core.cpp
    ../src/FCFSScheduler.cpp
    ../src/SJFScheduler.cpp
    ../src/SRTFScheduler.cpp
    ../src/RoundRobinScheduler.cpp
    ../src/PriorityScheduler.cpp
    ../src/MLFQScheduler.cpp
    ../src/SimulationEngine.cpp
    ../src/MetricsCollector.cpp
    ../src/WorkloadLoader.cpp
    ../src/GanttChart.cpp
    ../src/LoadBalancer.cpp
    ../src/PowerModel.cpp
    ../src/ParallelEngine.cpp
    ../src/BenchmarkHarness.cpp
    ../src/gpu/WarpScheduler.cpp
    ../src/gpu/CudaStreamGraph.cpp
)

# Test executable
add_executable(run_tests
    test_fcfs.cpp
    test_phase2.cpp
    test_phase3.cpp
    test_phase4.cpp
    test_phase5.cpp
    ${TEST_SOURCES}
)

target_link_libraries(run_tests
    GTest::gtest_main
)

# Discover tests
include(GoogleTest)
gtest_discover_tests(run_tests)

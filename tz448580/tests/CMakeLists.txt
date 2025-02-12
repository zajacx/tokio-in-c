# CMakeLists.txt in tests/

add_library(test_utils utils.c)
target_link_libraries(test_utils err)

add_executable(executor_test executor_test.c)
target_link_libraries(executor_test executor mio future)

add_executable(hard_work_test hard_work_test.c)
target_link_libraries(hard_work_test executor mio future err)

add_executable(mio_test mio_test.c)
target_link_libraries(mio_test executor mio future test_utils)

add_executable(then_test then_test.c)
target_link_libraries(then_test executor mio future err test_utils)

# to delete!
add_executable(combined_test combined_test.c)
target_link_libraries(combined_test executor mio future err test_utils)

# to delete!
add_executable(basic_then_test basic_then_test.c)
target_link_libraries(basic_then_test executor mio future err test_utils)

# to delete!
add_executable(join_test join_test.c)
target_link_libraries(join_test executor mio future err test_utils)

# to delete!
add_executable(basic_join_test basic_join_test.c)
target_link_libraries(basic_join_test executor mio future err test_utils)


enable_testing()
add_test(NAME ExecutorTest COMMAND executor_test)
add_test(NAME HardWorkTest COMMAND hard_work_test)
add_test(NAME MioTest COMMAND mio_test)
add_test(NAME ThenTest COMMAND then_test)
add_test(NAME CombinedTest COMMAND combined_test)
add_test(NAME BasicThenTest COMMAND basic_then_test)
add_test(NAME JoinTest COMMAND join_test)
add_test(NAME BasicJoinTest COMMAND basic_join_test)

// Required for `unistd.h` include to contain `pipe2`.
#define _GNU_SOURCE

#include <assert.h>
#include <fcntl.h>
#include <stdint.h> // For uint64_t
#include <stdio.h> // For printf
#include <stdlib.h> // For exit
#include <string.h> // For memcmp
#include <sys/timerfd.h> // For timerfd
#include <unistd.h> // For pipe, read, write

#include "err.h"
#include "executor.h"
#include "future.h"
#include "future_examples.h"
#include "mio.h"
#include "utils.h"

void* increment(void* arg)
{
    intptr_t number = (intptr_t)arg;
    return (void*)(number + 1);
}

int main()
{
    // In this test, we create two futures that read from two slow pipes, independently.
    // They should be able to work concurrently (with one future progressing when the other is waiting to read).

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    const char* message = "AAABBBCCCD";

    // Create a pipe that will be filled with `message` 3 bytes at a time (including '\0'),
    // with a 1-second delay before each write (for a total of 4 seconds).
    int read_fd1 = create_example_read_pipe_end(message, 3, 1, 0);

    // Make a future that reads from the pipe, exactly the number of bytes written to it.
    uint8_t buffer1[strlen(message) + 1];
    PipeReadFuture f1 = pipe_read_future_create(read_fd1, buffer1, sizeof(buffer1));

    // Make another pipe and another future like that.
    int read_fd2 = create_example_read_pipe_end(message, 3, 1, 0);
    uint8_t buffer2[strlen(message) + 1];
    PipeReadFuture f2 = pipe_read_future_create(read_fd2, buffer2, sizeof(buffer2));

    // Create simple increment tasks.
    ApplyFuture f3 = apply_future_create(increment);
    f3.base.arg = (void*)10;

    ApplyFuture f4 = apply_future_create(increment);
    f4.base.arg = (void*)20;

    ApplyFuture f5 = apply_future_create(increment);
    f5.base.arg = (void*)30;

    // Create and run executor.
    Executor* executor = executor_create(10);

    executor_spawn(executor, (Future*)&f1);
    executor_spawn(executor, (Future*)&f2);
    executor_spawn(executor, (Future*)&f3);
    executor_spawn(executor, (Future*)&f4);
    executor_spawn(executor, (Future*)&f5);

    executor_run(executor);

    // Checked elapsed time.
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Elapsed time: %f seconds\n", elapsed_time);
    assert(elapsed_time >= 4.0);
    assert(elapsed_time < 8.0);

    // Get results from futures.
    intptr_t result3 = (intptr_t)f3.base.ok;
    intptr_t result4 = (intptr_t)f4.base.ok;
    intptr_t result5 = (intptr_t)f5.base.ok;
    debug("Result 3: %ld\n", (long)result3);
    debug("Result 4: %ld\n", (long)result4);
    debug("Result 5: %ld\n", (long)result5);
    assert(result3 == 11);
    assert(result4 == 21);
    assert(result5 == 31);

    // Destroy the executor
    executor_destroy(executor);

    return 0;
}

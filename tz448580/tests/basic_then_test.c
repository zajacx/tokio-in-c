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
#include "future_combinators.h"
#include "future_examples.h"
#include "mio.h"
#include "utils.h"


/** A function that capitalizes a c-string in place and returns it. */
void* capitalize(void* arg)
{
    char* buffer = arg;
    debug("capitalize received string at: %p\n", buffer);
    size_t const len = strlen(buffer);
    debug("capitalize received string of length %d\n", len);
    for (int i = 0; i < len; ++i) {
        if ('a' <= buffer[i] && buffer[i] <= 'z') {
            buffer[i] = buffer[i] - 'a' + 'A';
        }
    }
    return buffer;
}

int main()
{
    // Basic test of the ThenFuture combinator.

    // Create a pipe that will be filled with `message` 3 bytes at a time,
    // then wait 2 seconds before closing the write end.
    const char* message = "aaabbbcccd\n";
    int read_fd = create_example_read_pipe_end(message, 3, 0, 2);

    Executor* executor = executor_create(42);    

    // Make a future that reads the message from the pipe, then capitalizes it, the prints it.
    // This should succeed, because we read exactly all the bytes of message, including '\0'.
    uint8_t buffer[strlen(message) + 1];
    PipeReadFuture f1 = pipe_read_future_create(read_fd, buffer, sizeof(buffer));
    ApplyFuture f2 = apply_future_create(capitalize);
    ThenFuture f12 = future_then((Future*)&f1, (Future*)&f2);
    Future* good_future = (Future*)&f12;

    executor_spawn(executor, good_future);

    // Run the executor
    debug("Running the executor\n");
    executor_run(executor);
    debug("Executor finished\n");

    // Check everything is as expected.
    assert(good_future->errcode == FUTURE_SUCCESS);
    assert(good_future->ok == buffer); // capitalize and PipeWriteFuture just return the buffer.
    assert(memcmp(buffer, "AAABBBCCCD\n", sizeof(buffer)) == 0); // the buffer was capitalized.

    // Destroy the executor
    executor_destroy(executor);

    close(read_fd);

    return 0;
}

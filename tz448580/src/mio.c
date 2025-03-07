#include <stdint.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>

#include <fcntl.h>
#include <stdio.h>

#include "mio.h"
#include "debug.h"
#include "executor.h"
#include "waker.h"
#include "err.h"

// Maximum number of events to handle per epoll_wait call.
#define MAX_EVENTS 64

// ================================= Utils ================================

void set_nonblocking(int fd)
{
    // Get current flags.
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        debug("fcntl(F_GETFL)");
        return;
    }
    // Check if O_NONBLOCK is already set.
    if (flags & O_NONBLOCK) {
        return;
    }
    // Set O_NONBLOCK.
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        debug("fcntl(F_SETFL, O_NONBLOCK)");
    }
}

// ================================== Mio =================================

struct Mio {
    Executor* executor;
    int epoll_fd;
    struct epoll_event events[MAX_EVENTS];
};

Mio* mio_create(Executor* executor)
{
    // Allocate memory for the Mio instance.
    Mio* mio = (Mio*) malloc(sizeof(Mio));
    if (!mio) {
        debug("mio_create (malloc)");
        return NULL;
    }
    // Create an epoll instance.
    mio->epoll_fd = epoll_create1(0);
    if (mio->epoll_fd == -1) {
        free(mio);
        debug("mio_create (epoll_create1)");
        return NULL;
    }
    // Save the executor.
    mio->executor = executor;

    return mio;
}

void mio_destroy(Mio* mio)
{
    close(mio->epoll_fd);
    free(mio);
}

int mio_register(Mio* mio, int fd, uint32_t events, Waker waker)
{
    set_nonblocking(fd); // Sets O_NONBLOCK on fd if necessary.
    Future* fut = waker.future;
    debug("Registering (in Mio = %p) fd = %d\n with future %p\n", mio, fd, fut);
    
    // Create an epoll event structure.
    struct epoll_event event;
    event.events = events;
    event.data.fd = fd;
    event.data.ptr = (void*) fut; // Save the future in the data.ptr field.
    // Register fd.
    if (epoll_ctl(mio->epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        if (errno == EEXIST) {
            debug("[Mio] fd already registered\n");
            return 0;
        }
        debug("epoll_ctl (EPOLL_CTL_ADD)");
        return -1;
    }
    return 0;
}

int mio_unregister(Mio* mio, int fd)
{
    debug("Unregistering (from Mio = %p) fd = %d\n", mio, fd);
    // Unregister.
    if (epoll_ctl(mio->epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1) {
        debug("epoll_ctl (EPOLL_CTL_DEL)");
        return -1;
    }
    return 0;
}

void mio_poll(Mio* mio)
{
    debug("Mio (%p) polling\n", mio);
    // Wait for events.
    int n = epoll_wait(mio->epoll_fd, mio->events, MAX_EVENTS, -1);
    if (n == -1) {
        // Error in poll() leaves no hope.
        executor_destroy(mio->executor);
        fatal("epoll_wait");
    }
    // Handle events.
    for (int i = 0; i < n; i++) {
        int fd = mio->events[i].data.fd;
        debug("Mio (%p) received event on fd = %d\n", mio, fd);
        Future* fut = (Future*) mio->events[i].data.ptr;
        debug("Mio (%p) waking up future %p\n", mio, fut);
        Waker waker = { .executor = mio->executor, .future = fut };
        waker_wake(&waker);
    }
}



#include "executor.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "debug.h"
#include "future.h"
#include "mio.h"
#include "waker.h"
#include "err.h"

// ============================== FutureQueue =============================

typedef struct FutureQueue {
    Future** futures;
    size_t size;
    size_t max_queue_size;
    size_t head;
    size_t tail;
} FutureQueue;

void queue_debug_print(FutureQueue* queue, bool pop)
{
    if (!queue) {
        debug("[DEBUG] Queue is NULL\n");
        return;
    }

    if (pop) {
        debug("[DEBUG] Popped from queue.\n");
    } else {
        debug("[DEBUG] Pushed to queue.\n");
    }
    debug("[DEBUG] Queue State:\n");
    debug("  - Size: %zu / %zu\n", queue->size, queue->max_queue_size);
    debug("  - Head: %zu, Tail: %zu\n", queue->head, queue->tail);

    debug("  - Elements: ");
    if (queue->size == 0) {
        debug("(empty)\n");
        return;
    }

    size_t index = queue->head;
    for (size_t i = 0; i < queue->size; i++) {
        debug("[%p] ", (Future*) queue->futures[index]);
        index = (index + 1) % queue->max_queue_size;
    }
    debug("\n");
}

FutureQueue* queue_create(size_t max_queue_size)
{
    FutureQueue* queue = (FutureQueue*) malloc(sizeof(FutureQueue));
    if (!queue) {
        return NULL;
    }

    Future** buffer = (Future**) malloc(max_queue_size * sizeof(Future*));
    if (!buffer) {
        free(queue);
        return NULL;
    }

    for (size_t i = 0; i < max_queue_size; i++) {
        buffer[i] = (Future*) NULL;
    }

    queue->futures = buffer;
    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;
    queue->max_queue_size = max_queue_size;

    return queue;
}

bool queue_push(FutureQueue* queue, Future* future)
{
    if (!queue || !future) {
        return false;
    }

    if (queue->size == queue->max_queue_size) {
        return false;
    }

    queue->futures[queue->tail] = future;
    queue->tail = (queue->tail + 1) % queue->max_queue_size;
    queue->size++;

    // DEBUG
    queue_debug_print(queue, false);

    return true;
}

Future* queue_pop(FutureQueue* queue)
{
    if (!queue || queue->size == 0) {
        return NULL;
    }

    Future* fut = queue->futures[queue->head];
    queue->head = (queue->head + 1) % queue->max_queue_size;
    queue->size--;

    // DEBUG
    queue_debug_print(queue, true);

    return fut;
}

void queue_destroy(FutureQueue* queue)
{
    if (queue) {
        free(queue->futures);
        free(queue);
    }
}

// =============================== Executor ===============================

/**
 * @brief Structure to represent the current-thread executor.
 */
struct Executor {
    FutureQueue* queue;
    Mio* mio;
    int active; // Counter of futures with is_active set to true.
};

Executor* executor_create(size_t max_queue_size)
{
    Executor* executor = (Executor*) malloc(sizeof(Executor));
    if (!executor) {
        fatal("executor create (malloc)");
    }

    executor->queue = queue_create(max_queue_size);
    if (!executor->queue) {
        free(executor);
        fatal("queue_create (malloc)");
    }

    executor->mio = mio_create(executor);
    if (!executor->mio) {
        queue_destroy(executor->queue);
        free(executor);
        fatal("mio_create (malloc)");
    }

    executor->active = 0;
    return executor;
}

void waker_wake(Waker* waker)
{
    Executor* executor = (Executor*) waker->executor;
    Future* fut = waker->future;
    // Put the future back into the executor's queue (if it's not already there).
    FutureQueue* queue = executor->queue;
    for (size_t i = queue->head; i < queue->size; i = (i + 1) % queue->max_queue_size) {
        debug("[WAKER] In the queue: %p\n", queue->futures[i]);
        if (queue->futures[i] == fut) {
            debug("[WAKER] Not requeuing the future, it's already in the queue\n");
            return;
        }
    }
    debug("[WAKER] Requeuing the future\n");
    executor_spawn(executor, fut);
}

void executor_spawn(Executor* executor, Future* fut)
{
    if (!executor) {
        fatal("executor_spawn");
    }

    if (!fut) {
        debug("fut is NULL\n");
        return;
    }
    
    debug("[Executor] Spawned future\n");
    if (!(fut->is_active)) {
        fut->is_active = true;
        executor->active++;
    }

    bool ret = queue_push(executor->queue, fut);
    if (!ret) {
        debug("Spawn failed\n");
    }
}

void executor_run(Executor* executor)
{
    if (!executor) {
        fatal("executor_run: executor is NULL\n");
    }

    debug("[Executor] Starting with %d tasks\n", executor->active);

    // Main loop, stopping if there are no tasks in general.
    while (executor->active > 0) {
        debug("[Executor] Main loop: found %d tasks, processing...\n", executor->active);
        // Inner loop, stopping if there are no tasks in the queue.
        while (executor->queue->size > 0) {
            debug("[Executor] Inner loop: found %zu tasks in the queue\n", executor->queue->size);
            debug("[Executor] All active tasks: %d\n", executor->active);
            Future* fut = queue_pop(executor->queue);
            // fut is not null here.
            Waker waker = { .executor = executor, .future = fut };
            FutureState state = fut->progress(fut, executor->mio, waker);
            switch (state) {
                case FUTURE_COMPLETED:
                    fut->is_active = false;
                    executor->active--;
                    break;
                case FUTURE_FAILURE:
                    fut->is_active = false;
                    executor->active--;
                    break;
                case FUTURE_PENDING:
                    break;
            }
        }
        // After processing everything from the queue, call mio_poll().
        if (executor->active > 0) {
            mio_poll(executor->mio);
        }
    }
    
}

void executor_destroy(Executor* executor)
{
    queue_destroy(executor->queue);
    mio_destroy(executor->mio);
    free(executor);
}

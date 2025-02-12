#include "future_combinators.h"
#include <stdlib.h>

#include "future.h"
#include "waker.h"

// TODO: delete this once not needed.
#define UNIMPLEMENTED (exit(42))

/** Progress function for ThenFuture */
static FutureState then_future_progress(Future* base, Mio* mio, Waker waker) {
    ThenFuture* self = (ThenFuture*)base;
    debug("ThenFuture %p progress. fut1_completed=%d\n", self, self->fut1_completed);

    if (!self->fut1_completed) {
        // Progress the first future.
        FutureState state = self->fut1->progress(self->fut1, mio, waker);
        if (state == FUTURE_COMPLETED) {
            self->fut1_completed = true;
            self->fut2->arg = self->fut1->ok;
        } else if (state == FUTURE_FAILURE) {
            self->base.errcode = THEN_FUTURE_ERR_FUT1_FAILED;
            return FUTURE_FAILURE;
        } else {
            debug("ThenFuture %p progress. fut1 is pending\n", self);
            return FUTURE_PENDING;
        }
    }

    // The first future has completed.
    FutureState state = self->fut2->progress(self->fut2, mio, waker);
    if (state == FUTURE_COMPLETED) {
        self->base.ok = self->fut2->ok;
        return FUTURE_COMPLETED;
    } else if (state == FUTURE_FAILURE) {
        self->base.errcode = THEN_FUTURE_ERR_FUT2_FAILED;
        return FUTURE_FAILURE;
    } else {
        debug("ThenFuture %p progress. fut2 is pending\n", self);
        return FUTURE_PENDING;
    }
}

ThenFuture future_then(Future* fut1, Future* fut2)
{
    return (ThenFuture) {
        .base = future_create(then_future_progress),
        .fut1 = fut1,
        .fut2 = fut2,
        .fut1_completed = false,
    };
}

JoinFuture future_join(Future* fut1, Future* fut2)
{
    UNIMPLEMENTED;
    return (JoinFuture) {
        // TODO: correct initialization.
        // .base = ... ,
        // .fut1 = ... ,
        // ...
    };
}

SelectFuture future_select(Future* fut1, Future* fut2)
{
    UNIMPLEMENTED;
    return (SelectFuture) {
        // TODO: correct initialization.
        // .base = ... ,
        // .fut1 = ... ,
        // ...
    };
}

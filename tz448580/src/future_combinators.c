#include "future_combinators.h"
#include <stdlib.h>

#include "future.h"
#include "waker.h"

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

    debug("ThenFuture %p progress. fut1 has completed\n", self);

    // The first future has completed.
    FutureState state = self->fut2->progress(self->fut2, mio, waker);
    if (state == FUTURE_COMPLETED) {
        self->base.ok = self->fut2->ok;
        debug("ThenFuture %p progress. fut2 has completed\n", self);
        return FUTURE_COMPLETED;
    } else if (state == FUTURE_FAILURE) {
        self->base.errcode = THEN_FUTURE_ERR_FUT2_FAILED;
        debug("ThenFuture %p progress. fut2 failed\n", self);
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

/** Progress function for JoinFuture */
static FutureState join_future_progress(Future* base, Mio* mio, Waker waker) {
    JoinFuture* self = (JoinFuture*)base;
    debug("JoinFuture %p progress. fut1_completed=%d, fut2_completed=%d\n",
          self, self->fut1_completed, self->fut2_completed);
    
    if (self->fut1_completed == FUTURE_PENDING) {
        // Progress the first future.
        self->fut1_completed = self->fut1->progress(self->fut1, mio, waker);
        if (self->fut1_completed == FUTURE_FAILURE) {
            if (self->base.errcode == 0) {
                self->base.errcode = JOIN_FUTURE_ERR_FUT1_FAILED;
            } else {
                self->base.errcode = JOIN_FUTURE_ERR_BOTH_FUTS_FAILED;
            }
        } else if (self->fut1_completed == FUTURE_COMPLETED) {
            self->result.fut1.ok = self->fut1->ok;
        }
    }

    if (self->fut2_completed == FUTURE_PENDING) {
        // Progress the second future.
        self->fut2_completed = self->fut2->progress(self->fut2, mio, waker);
        if (self->fut2_completed == FUTURE_FAILURE) {
            if (self->base.errcode == 0) {
                self->base.errcode = JOIN_FUTURE_ERR_FUT2_FAILED;
            } else {
                self->base.errcode = JOIN_FUTURE_ERR_BOTH_FUTS_FAILED;
            }
        } else if (self->fut2_completed == FUTURE_COMPLETED) {
            self->result.fut2.ok = self->fut2->ok;
        }
    }

    if (self->fut1_completed != FUTURE_PENDING && self->fut2_completed != FUTURE_PENDING) {
        if (self->base.errcode != 0) {
            return FUTURE_FAILURE;
        }
        return FUTURE_COMPLETED;
    }
    return FUTURE_PENDING;
}

JoinFuture future_join(Future* fut1, Future* fut2)
{
    return (JoinFuture) {
        .base = future_create(join_future_progress),
        .fut1 = fut1,
        .fut2 = fut2,
        .fut1_completed = FUTURE_PENDING,
        .fut2_completed = FUTURE_PENDING,
        .result = {
            .fut1 = {
                .errcode = 0,
                .ok = NULL,
            },
            .fut2 = {
                .errcode = 0,
                .ok = NULL,
            },
        }, 
    };
}

/** Progress function for SelectFuture */
static FutureState select_future_progress(Future* base, Mio* mio, Waker waker) {
    SelectFuture* self = (SelectFuture*)base;
    debug("SelectFuture %p progress. which_completed=%d\n", self, self->which_completed);

    if (self->which_completed == SELECT_COMPLETED_NONE) {
        // Progress the first future.
        FutureState state = self->fut1->progress(self->fut1, mio, waker);
        if (state == FUTURE_COMPLETED) {
            self->which_completed = SELECT_COMPLETED_FUT1;
            self->base.ok = self->fut1->ok;
            return FUTURE_COMPLETED;
        } else if (state == FUTURE_FAILURE) {
            self->which_completed = SELECT_FAILED_FUT1;
            self->base.errcode = FUTURE_FAILURE;
        } else {
            return FUTURE_PENDING;
        }
    }

    if (self->which_completed == SELECT_COMPLETED_NONE) {
        // Progress the second future.
        FutureState state = self->fut2->progress(self->fut2, mio, waker);
        if (state == FUTURE_COMPLETED) {
            self->which_completed = SELECT_COMPLETED_FUT2;
            self->base.ok = self->fut2->ok;
            return FUTURE_COMPLETED;
        } else if (state == FUTURE_FAILURE) {
            self->which_completed = SELECT_FAILED_FUT2;
            self->base.errcode = FUTURE_FAILURE;
        } else {
            return FUTURE_PENDING;
        }
    }

    if (self->which_completed == SELECT_FAILED_FUT1) {
        // Progress the second future.
        FutureState state = self->fut2->progress(self->fut2, mio, waker);
        if (state == FUTURE_COMPLETED) {
            self->which_completed = SELECT_COMPLETED_FUT2;
            self->base.ok = self->fut2->ok;
            return FUTURE_COMPLETED;
        } else if (state == FUTURE_FAILURE) {
            self->which_completed = SELECT_FAILED_BOTH;
            self->base.errcode = FUTURE_FAILURE;
        } else {
            return FUTURE_PENDING;
        }
    }

    if (self->which_completed == SELECT_FAILED_FUT2) {
        // Progress the first future.
        FutureState state = self->fut1->progress(self->fut1, mio, waker);
        if (state == FUTURE_COMPLETED) {
            self->which_completed = SELECT_COMPLETED_FUT1;
            self->base.ok = self->fut1->ok;
            return FUTURE_COMPLETED;
        } else if (state == FUTURE_FAILURE) {
            self->which_completed = SELECT_FAILED_BOTH;
            self->base.errcode = FUTURE_FAILURE;
        } else {
            return FUTURE_PENDING;
        }
    }

    return FUTURE_PENDING;
}

SelectFuture future_select(Future* fut1, Future* fut2)
{
    return (SelectFuture) {
        .base = future_create(select_future_progress),
        .fut1 = fut1,
        .fut2 = fut2,
        .which_completed = SELECT_COMPLETED_NONE,
    };
}

#include "future_combinators.h"
#include <stdlib.h>

#include "future.h"
#include "waker.h"

// TODO: delete this once not needed.
#define UNIMPLEMENTED (exit(42))

ThenFuture future_then(Future* fut1, Future* fut2)
{
    UNIMPLEMENTED;
    return (ThenFuture) {
        // TODO: correct initialization.
        // .base = ... ,
        // .fut1 = ... ,
        // ...
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

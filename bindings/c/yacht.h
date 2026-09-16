#ifndef YACHT_H
#define YACHT_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* ABI 1. Thread safe; calls on independent tables may run concurrently.
   request: borrowed UTF-8 JSON, NUL terminated. Response: owned UTF-8 JSON,
   always free with yacht_free (including errors). Cancellation runs on the
   calling thread, synchronously; callback/context live until this call returns.
   Callback must never throw/unwind. NULL callback means no cancellation.
   Table handles are retained by read/parse/records/sample; release exactly once.
   An active call retains its table even if another thread releases the handle.
   Neither response pointers nor table handles may be persisted across launches. */
char *yacht_request(const char *request, bool (*cancelled)(void *), void *context);
void yacht_free(char *response);
#ifdef __cplusplus
}
#endif
#endif

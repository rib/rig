/*
 * Copyright (C) 2014 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <clib.h>

/* Note: we have to build this file as C++ under MSVC */
#include <atomic>
static CRITICAL_SECTION tls_lock;
static std::atomic<c_tls_t *> tls_head = nullptr;

void
c_tls_init(c_tls_t *tls, void (*destroy)(void *data))
{
    tls->key = TlsAlloc();
    tls->destroy = destroy;

    EnterCriticalSection(&tls_lock);
    tls->next = tls_head;

    /* NB: we don't take tls_head in DllMain() otherwise we could
     * hit a deadlock if a tls destroy callback were to call
     * c_tls_init() */
    atomic_store(&tls_head, tls);
    LeaveCriticalSection(&tls_lock);
}

static void
destroy_tls_data(void)
{
    bool repeat = false;
    c_tls_t *tls;

    /* Follow the same rinse and repeat model as pthreads... */
    do {
        for (tls = atomic_load(&tls_head); tls; tls = tls->next) {
            void *data;

            if (!tls->destroy)
                continue;

            data = TlsGetValue(tls->key);
            if (data) {
                tls->destroy(data);
                repeat = true;
            }
        }
    } while(repeat);
}

BOOL WINAPI
DllMain(HINSTANCE instance,
        DWORD reason,
        LPVOID reserved)
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        InitializeCriticalSection(&tls_lock);
        break;
    case DLL_THREAD_DETACH:
        destroy_tls_data();
        break;
    }
    return TRUE;
}

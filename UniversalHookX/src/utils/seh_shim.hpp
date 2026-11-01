#pragma once
#include <windows.h>
#include <csetjmp>

// GCC/MinGW has no MSVC __try / __except (SEH) support, so this stands in
// for the common "__try { risky pointer chase } __except (EXCEPTION_EXECUTE_HANDLER) { fallback }"
// pattern used throughout the menu/esp/entity code to survive dereferencing
// stale game-entity pointers. A process-wide vectored handler intercepts
// access violations and long-jumps back to the innermost active SEH_TRY
// frame on the faulting thread; frames outside any SEH_TRY (or on other
// threads) are left untouched (EXCEPTION_CONTINUE_SEARCH).
//
// Caveat vs. real SEH: longjmp does not run destructors of objects that are
// live between the SEH_TRY and the fault. Every existing call site only
// keeps trivial/POD locals (ints, floats, pointers, char buffers) live
// across the protected region, so this is safe here, but don't introduce a
// non-trivial-destructor local inside a SEH_TRY block without checking that
// assumption still holds.
namespace SehShim {

    struct Frame;
    inline thread_local Frame* g_topFrame = nullptr;

    struct Frame {
        jmp_buf buf;
        Frame* prev;

        Frame( ) : prev(g_topFrame) { g_topFrame = this; }
        ~Frame( ) { if (g_topFrame == this) g_topFrame = prev; }
    };

    inline LONG WINAPI VectoredHandler(PEXCEPTION_POINTERS info) {
        const DWORD code = info->ExceptionRecord->ExceptionCode;
        if (g_topFrame && (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR)) {
            Frame* frame = g_topFrame;
            g_topFrame = frame->prev;
            longjmp(frame->buf, 1);
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // Call once, early, before any SEH_TRY block can run.
    inline void Install( ) {
        AddVectoredExceptionHandler(1, &VectoredHandler);
    }
}

#define SEH_TRY    if (SehShim::Frame seh_frame_; setjmp(seh_frame_.buf) == 0)
#define SEH_EXCEPT else

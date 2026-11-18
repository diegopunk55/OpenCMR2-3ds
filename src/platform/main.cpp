// Process entry point: sets up the platform and runs the game's main loop
// (CMain::Initialize, which was WinMain's body).

#include "platform/platform.h"
#include "port/sys.h"
#include "diagnostics/timing.h"
#include "main.h"

// On Android, SDLActivity calls SDL_main, which SDL_main.h renames main to.
#ifndef __ANDROID__
#define SDL_MAIN_HANDLED
#endif
#include <SDL3/SDL_main.h>

#ifdef __3DS__
#include <3ds.h>
// Linear memory (the GPU's): command and vertex buffers, the textures that do
// not fit in VRAM, audio. The rest of the application's memory is the heap.
// The main thread's stack (libctru's default is 32 KB): the game keeps large
// buffers on it.
extern "C" {
u32 __ctru_linear_heap_size = 20 * 1024 * 1024;
u32 __stacksize__ = 1024 * 1024;

extern char *fake_heap_start;
extern char *fake_heap_end;
extern u32 __ctru_heap, __ctru_heap_size, __ctru_linear_heap;

// Replaces libctru's (weak) one. libctru 2.7.0 gives the heap all the memory
// left after the linear part, but the heap's address range is only 96 MB
// (OS_HEAP_AREA_*): in the New 3DS's 124 MB mode that is about 97 MB, the
// allocation fails and the game stops at once (svcBreak). Here the heap gets
// at most the range and the rest goes to linear memory, which is never less
// than __ctru_linear_heap_size.
void __system_allocateHeaps(void)
{
    Handle reslimit = 0;
    if (R_FAILED(svcGetResourceLimit(&reslimit, CUR_PROCESS_HANDLE)))
        svcBreak(USERBREAK_PANIC);
    s64 maxCommit = 0, currentCommit = 0;
    ResourceLimitType type = RESLIMIT_COMMIT;
    svcGetResourceLimitLimitValues(&maxCommit, reslimit, &type, 1);
    svcGetResourceLimitCurrentValues(&currentCommit, reslimit, &type, 1);
    svcCloseHandle(reslimit);

    u32 remaining = (u32)(maxCommit - currentCommit) & ~0xFFFu;
    u32 linear = __ctru_linear_heap_size;
    if (linear > remaining)
        svcBreak(USERBREAK_PANIC);
    u32 heap = remaining - linear;
    const u32 area = OS_HEAP_AREA_END - OS_HEAP_AREA_BEGIN;
    if (heap > area) {
        linear += heap - area;
        heap = area;
    }
    __ctru_heap_size = heap;
    __ctru_linear_heap_size = linear;

    if (R_FAILED(svcControlMemory(&__ctru_heap, OS_HEAP_AREA_BEGIN, 0x0, heap, MEMOP_ALLOC,
                                  (MemPerm)(MEMPERM_READ | MEMPERM_WRITE))))
        svcBreak(USERBREAK_PANIC);
    if (R_FAILED(svcControlMemory(&__ctru_linear_heap, 0x0, 0x0, linear, MEMOP_ALLOC_LINEAR,
                                  (MemPerm)(MEMPERM_READ | MEMPERM_WRITE))))
        svcBreak(USERBREAK_PANIC);
    mappableInit(OS_MAP_AREA_BEGIN, OS_MAP_AREA_END);
    fake_heap_start = (char *)__ctru_heap;
    fake_heap_end = fake_heap_start + heap;
}
}
#endif

int main(int argc, char **argv)
{
    SDL_SetMainReady();
    if (!Platform_Init(argc, argv))
        return Platform_WasSetupCanceled() ? 0 : 1;
    Diagnostics::StartProfileFromEnvironment();
    unsigned int code = CMain::Initialize(Sys_GetCommandLine());
    Diagnostics::StopProfile();
    Platform_Shutdown();
    return (int)code;
}

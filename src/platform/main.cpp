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

// Replaces libctru's (weak) one, for two reasons:
// - libctru 2.7.0 gives the heap all the memory left after the linear part,
//   but the heap's address range is only 96 MB (OS_HEAP_AREA_*): with 124 MB
//   or more the allocation fails and the game stops at once (svcBreak).
// - Linear memory must be one physically contiguous block, and the New 3DS's
//   178 MB region spans its two 128 MB banks of FCRAM, so the largest block
//   the kernel can give is not known in advance (the NFSU2 3DS port hit this).
// So linear memory is taken first, while the region is least fragmented: all
// that the heap's 96 MB leave, at least __ctru_linear_heap_size, stepping
// down 4 MB at a time until the kernel gives it. The heap then takes the
// rest, up to its range. Ctr_LogSystem logs what was asked and given.
u32 g_ctrLinearWanted = 0, g_ctrLinearTries = 0;

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

    const u32 area = OS_HEAP_AREA_END - OS_HEAP_AREA_BEGIN;
    const u32 minimum = __ctru_linear_heap_size;
    const u32 step = 4u << 20;
    u32 remaining = (u32)(maxCommit - currentCommit) & ~0xFFFu;
    if (minimum > remaining)
        svcBreak(USERBREAK_PANIC);
    u32 linear = remaining - minimum > area ? remaining - area : minimum;
    g_ctrLinearWanted = linear;
    for (;;) {
        g_ctrLinearTries++;
        if (R_SUCCEEDED(svcControlMemory(&__ctru_linear_heap, 0x0, 0x0, linear, MEMOP_ALLOC_LINEAR,
                                         (MemPerm)(MEMPERM_READ | MEMPERM_WRITE))))
            break;
        if (linear < minimum + step)
            svcBreak(USERBREAK_PANIC);
        linear -= step;
    }
    u32 heap = remaining - linear;
    if (heap > area)
        heap = area;
    while (R_FAILED(svcControlMemory(&__ctru_heap, OS_HEAP_AREA_BEGIN, 0x0, heap, MEMOP_ALLOC,
                                     (MemPerm)(MEMPERM_READ | MEMPERM_WRITE)))) {
        if (heap < step * 4)
            svcBreak(USERBREAK_PANIC);
        heap -= step;
    }
    __ctru_heap_size = heap;
    __ctru_linear_heap_size = linear;
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

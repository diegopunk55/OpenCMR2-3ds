#include "diagnostics/frame_log.h"

#include <cstdio>
#include <new>
#include <utility>

namespace FrameLog {

namespace {

char *Unsigned(char *o, uint64_t v)
{
    char digits[24];
    int n = 0;
    do {
        digits[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v != 0);
    while (n > 0)
        *o++ = digits[--n];
    return o;
}

char *Signed(char *o, int64_t v)
{
    if (v < 0) {
        *o++ = '-';
        return Unsigned(o, (uint64_t)(-v));
    }
    return Unsigned(o, (uint64_t)v);
}

// v in units of 10^-decimals (rounded by the caller), then a comma:
// Fixed(o, 1234, 2) writes "12.34,".
char *Fixed(char *o, uint64_t v, int decimals)
{
    uint64_t scale = decimals == 1 ? 10 : 100;
    o = Unsigned(o, v / scale);
    *o++ = '.';
    uint64_t frac = v % scale;
    if (decimals == 2 && frac < 10)
        *o++ = '0';
    o = Unsigned(o, frac);
    *o++ = ',';
    return o;
}

} // namespace

Recorder::Recorder(std::string directory_, std::string stamp_, size_t capacity_, unsigned idleFrames_)
    : directory(std::move(directory_)), stamp(std::move(stamp_)), capacity(capacity_ ? capacity_ : 1),
      idleFrames(idleFrames_)
{
}

Written Recorder::Add(const Sample &sample, bool driving)
{
    if (!driving) {
        if (count > 0 && ++idle >= idleFrames)
            return Flush();
        return {};
    }
    idle = 0;
    if (failed)
        return {};
    if (!samples) {
        // Once, on the first stage: no allocation while frames are timed.
        samples.reset(new (std::nothrow) Sample[capacity]);
        if (!samples) {
            failed = true;
            return {};
        }
    }
    samples[count++] = sample;
    if (count == capacity)
        return Flush();
    return {};
}

Written Recorder::Flush()
{
    Written result;
    idle = 0;
    if (count == 0)
        return result;
    char name[96];
    snprintf(name, sizeof(name), "frames-%s-%u.csv", stamp.c_str(), ++files);
    result.path = directory + name;
    result.frames = count;
    FILE *file = fopen(result.path.c_str(), "w");
    if (file != NULL) {
        // Few, large writes: the SD card is slow with small ones.
        static char buffer[64 * 1024];
        setvbuf(file, buffer, _IOFBF, sizeof(buffer));
        fputs("frame,time_ms,interval_ms,game_ms,render_ms,wait_ms,gpu_ms,draws,cmd_pct,vertex_kb,car_x,car_z,flags\n",
              file);
        // Integer formatting: printf's floating point is slow on a 3DS
        // (8 s for a stage's 8,800 frames).
        uint64_t timeUs = 0;
        char line[160];
        for (size_t i = 0; i < count; ++i) {
            const Sample &s = samples[i];
            timeUs += s.intervalUs;
            char *o = line;
            o = Unsigned(o, i);
            *o++ = ',';
            o = Fixed(o, (timeUs + 50) / 100, 1);
            o = Fixed(o, (s.intervalUs + 5) / 10, 2);
            o = Fixed(o, (s.gameUs + 5) / 10, 2);
            o = Fixed(o, (s.renderUs + 5) / 10, 2);
            o = Fixed(o, (s.waitUs + 5) / 10, 2);
            o = Fixed(o, (s.gpuUs + 5) / 10, 2);
            o = Unsigned(o, s.draws);
            *o++ = ',';
            o = Unsigned(o, s.cmdPercent);
            *o++ = ',';
            o = Unsigned(o, s.vertexKB);
            *o++ = ',';
            o = Signed(o, s.carX);
            *o++ = ',';
            o = Signed(o, s.carZ);
            *o++ = ',';
            o = Unsigned(o, s.flags);
            *o++ = '\n';
            fwrite(line, 1, (size_t)(o - line), file);
        }
        result.ok = !ferror(file);
        result.ok = fclose(file) == 0 && result.ok;
    }
    count = 0;
    return result;
}

} // namespace FrameLog

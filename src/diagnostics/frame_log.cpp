#include "diagnostics/frame_log.h"

#include <cstdio>
#include <new>
#include <utility>

namespace FrameLog {

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
        uint64_t timeUs = 0;
        for (size_t i = 0; i < count; ++i) {
            const Sample &s = samples[i];
            timeUs += s.intervalUs;
            fprintf(file, "%u,%.1f,%.2f,%.2f,%.2f,%.2f,%.2f,%u,%u,%u,%d,%d,%u\n", (unsigned)i, timeUs / 1000.0,
                    s.intervalUs / 1000.0, s.gameUs / 1000.0, s.renderUs / 1000.0, s.waitUs / 1000.0,
                    s.gpuUs / 1000.0, s.draws, s.cmdPercent, s.vertexKB, (int)s.carX, (int)s.carZ, s.flags);
        }
        result.ok = !ferror(file);
        result.ok = fclose(file) == 0 && result.ok;
    }
    count = 0;
    return result;
}

} // namespace FrameLog

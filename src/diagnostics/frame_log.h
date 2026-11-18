// Per-frame timings of a stage, kept in memory while it is driven and written
// as one CSV when it ends: the 3DS renderer (render/gfx_c3d.cpp) fills it.
// Writing every frame to the SD card would itself cost frames, so nothing
// is written while driving unless the buffer fills up.
//
// No platform calls here, so the host tests can run it (tests/frame_log.cpp).
#ifndef OPENCMR2_DIAGNOSTICS_FRAME_LOG_H
#define OPENCMR2_DIAGNOSTICS_FRAME_LOG_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace FrameLog {

enum Flags : uint8_t {
    SummaryLogged = 1,      // the periodic summary was written to the log in this frame
    TexturesConverted = 2,  // textures were converted (a load) before this frame
    CutShort = 4,           // the command buffer filled and the rest of the frame was dropped
    FileWritten = 8,        // a full buffer was written to the SD card before this frame
    Profiled = 16,          // the renderer's parts were timed (render_profile): a little slower
};

struct Sample {
    uint32_t intervalUs;    // since the previous frame was presented (0: the first)
    uint32_t gameUs;        // the game's own work between two frames
    uint32_t renderUs;      // translating the frame for the GPU, without the waiting
    uint32_t waitUs;        // waiting for the GPU or the refresh
    uint32_t gpuUs;         // the GPU's drawing time (citro3d's last measurement)
    uint16_t draws;
    uint16_t vertexKB;
    int32_t carX, carZ;     // the player's car, whole world units
    uint8_t cmdPercent;     // command buffer use
    uint8_t flags;          // Flags
};

struct Written {
    std::string path;       // empty: nothing was written
    size_t frames = 0;
    bool ok = false;
    bool later = false;     // not written yet: TakeReady has it for the caller
};

// A CSV file ready to be written.
struct Batch {
    std::string path;
    size_t frames = 0;
    std::string text;
};
// Writes a batch (it may take seconds on an SD card).
Written Write(const Batch &batch);

class Recorder {
public:
    // directory ends with '/'; stamp names the session's files
    // (frames-<stamp>-<n>.csv). capacity: frames kept before a write.
    Recorder(std::string directory, std::string stamp, size_t capacity = 36000, unsigned idleFrames = 90);

    // Once a presented frame. Frames are kept while driving; after idleFrames
    // frames in a row without driving (the stage ended), or when the buffer is
    // full, the frames kept so far are written.
    Written Add(const Sample &sample, bool driving);
    // Writes what is kept (on exit).
    Written Flush();
    // Writing later: instead of writing them, Add and Flush keep the frames
    // as a Batch for TakeReady (returning a Written with later set), which
    // the caller writes where it does not hold up the frames (a thread).
    void SetWriteLater(bool on) { writeLater = on; }
    Batch TakeReady();
    size_t Kept() const { return count; }
    bool Failed() const { return failed; }

private:
    std::string directory, stamp;
    size_t capacity;
    unsigned idleFrames;
    std::unique_ptr<Sample[]> samples;
    size_t count = 0;
    unsigned idle = 0;
    unsigned files = 0;
    bool failed = false;    // the buffer could not be allocated
    bool writeLater = false;
    Batch ready;
    Batch Take();
};

} // namespace FrameLog

#endif

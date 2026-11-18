// The 3DS frame log (src/diagnostics/frame_log.h): frames are kept only while
// driving, written when the stage ends or the buffer fills, as a CSV.
#include "diagnostics/frame_log.h"

#include <cstdio>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

static std::vector<std::string> Lines(const std::string &path)
{
    std::ifstream in(path);
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);)
        lines.push_back(line);
    return lines;
}

static FrameLog::Sample Frame(uint32_t game)
{
    FrameLog::Sample s = {};
    s.intervalUs = 16667;
    s.gameUs = game;
    s.renderUs = 4000;
    s.waitUs = 500;
    s.gpuUs = 12000;
    s.draws = 1500;
    s.vertexKB = 300;
    s.carX = -120;
    s.carZ = 4500;
    s.cmdPercent = 25;
    s.flags = FrameLog::SummaryLogged;
    return s;
}

int main()
{
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() /
        ("frame_log_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    std::string directory = dir.generic_string() + "/";

    // Menus are not kept; driving is; the stage's end writes after idleFrames.
    FrameLog::Recorder log(directory, "test", 100, 3);
    CHECK(log.Add(Frame(1000), false).path.empty());
    CHECK(log.Kept() == 0);
    for (int i = 0; i < 5; ++i)
        CHECK(log.Add(Frame(10000 + i), true).path.empty());
    CHECK(log.Kept() == 5);
    CHECK(log.Add(Frame(0), false).path.empty());
    CHECK(log.Add(Frame(0), false).path.empty());
    // A short pause (fewer than idleFrames) does not end the stage.
    CHECK(log.Add(Frame(10005), true).path.empty());
    CHECK(log.Kept() == 6);
    CHECK(log.Add(Frame(0), false).path.empty());
    CHECK(log.Add(Frame(0), false).path.empty());
    FrameLog::Written written = log.Add(Frame(0), false);
    CHECK(written.ok);
    CHECK(written.frames == 6);
    CHECK(written.path == directory + "frames-test-1.csv");
    CHECK(log.Kept() == 0);

    std::vector<std::string> lines = Lines(written.path);
    CHECK(lines.size() == 7);
    if (lines.size() == 7) {
        CHECK(lines[0] == "frame,time_ms,interval_ms,game_ms,render_ms,wait_ms,gpu_ms,draws,cmd_pct,vertex_kb,car_x,car_z,flags");
        CHECK(lines[1] == "0,16.7,16.67,10.00,4.00,0.50,12.00,1500,25,300,-120,4500,1");
        CHECK(lines[6] == "5,100.0,16.67,10.01,4.00,0.50,12.00,1500,25,300,-120,4500,1");
    }

    // A full buffer is written at once and recording goes on in a new file.
    FrameLog::Recorder small(directory, "full", 4, 3);
    for (int i = 0; i < 3; ++i)
        CHECK(small.Add(Frame(1), true).path.empty());
    written = small.Add(Frame(1), true);
    CHECK(written.ok && written.frames == 4 && written.path == directory + "frames-full-1.csv");
    CHECK(small.Add(Frame(1), true).path.empty());
    written = small.Flush();
    CHECK(written.ok && written.frames == 1 && written.path == directory + "frames-full-2.csv");
    CHECK(small.Flush().path.empty());

    // An unwritable directory reports failure and drops the frames.
    FrameLog::Recorder broken("/nonexistent-frame-log-dir/", "x", 10, 1);
    broken.Add(Frame(1), true);
    written = broken.Add(Frame(1), false);
    CHECK(!written.path.empty() && !written.ok);
    CHECK(broken.Kept() == 0);

    // Writing later: the stage's frames come back as a batch to write.
    FrameLog::Recorder later(directory, "later", 100, 2);
    later.SetWriteLater(true);
    for (int i = 0; i < 3; ++i)
        CHECK(later.Add(Frame(10000 + i), true).path.empty());
    CHECK(later.Add(Frame(0), false).path.empty());
    written = later.Add(Frame(0), false);
    CHECK(written.later && written.frames == 3 && written.path == directory + "frames-later-1.csv");
    CHECK(!fs::exists(written.path));
    FrameLog::Batch batch = later.TakeReady();
    CHECK(batch.frames == 3 && batch.path == written.path);
    written = FrameLog::Write(batch);
    CHECK(written.ok && written.frames == 3);
    lines = Lines(batch.path);
    CHECK(lines.size() == 4);
    if (lines.size() == 4)
        CHECK(lines[3] == "2,50.0,16.67,10.00,4.00,0.50,12.00,1500,25,300,-120,4500,1");
    CHECK(later.TakeReady().frames == 0);

    std::error_code ignored;
    fs::remove_all(dir, ignored);
    if (failures == 0)
        std::puts("frame_log: all checks passed");
    return failures == 0 ? 0 : 1;
}

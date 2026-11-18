# Nintendo 3DS

OpenCMR2 runs on the Nintendo 3DS family as a homebrew application: an
installable `OpenCMR2.cia` for consoles with custom firmware (Luma3DS) and
`opencmr2.3dsx` for the Homebrew Launcher. It is built with devkitARM,
libctru and citro3d. The game is the same code as on the other platforms,
built for the 3DS's 32-bit ARM11, so its data structures keep the original
layout. Only the platform layer and the renderer are specific to the 3DS.

> [!IMPORTANT]
> The 3DS port is experimental. On an Old 3DS the menus work; stages are
> being brought up. The renderer is checked against a test scene in an
> emulator, and the game's fixed-point maths against the original. Reports
> with the log (see [Problems](#problems)) are welcome.

## Why the 3DS and not the DS

A Nintendo DS version is not possible. The game needs tens of megabytes of
memory (a stage's archive is read whole), while the DS has 4 MB (16 MB in
DSi mode). Its 67 MHz ARM9 has no floating-point unit, and its GPU draws at
most 2048 triangles per frame. Smaller textures would not change that. The
3DS has 64 to 124 MB for an application, a CPU with hardware floating point,
and a GPU with a depth buffer and texture combiners close to Direct3D 7's
fixed-function pipeline.

## What to expect

| | Old 3DS / 2DS | New 3DS / New 2DS XL |
| --- | --- | --- |
| CPU | 268 MHz | 804 MHz, with the L2 cache |
| Memory for the game | 64 MB (the game refuses to start) | 178 MB (CIA) |
| Expected speed | slow; stages may run well below 30 fps | better |

The game runs at 800x480, twice the 400x240 top screen, and its picture is
drawn at half size over the whole screen. Its camera keeps the vertical view
of the original 4:3 and widens the horizontal one, so the wider screen shows
more of the stage instead of stretching it; the menus and the HUD keep their
places as fractions of the screen. The menus are drawn at the full 800x480
and halved smoothly by the display transfer, so their text stays readable;
the fonts are never shrunk. The menus' small fonts are also drawn 30% larger
(`text_scale`), each line kept where it is aligned. With `widescreen = 0` the game keeps the
original 640x480, 4:3 in the middle of the screen (or stretched with
`stretch = 1`).

While driving, the screens share the HUD. The top one keeps only the rev
counter with the gear (and the speed), so that the road is clear. The bottom
one shows a map of the stage (the route, splits, start and finish, your
arrow and the other cars), with the rest of the HUD over it at half size:
stage and split times, laps and position, the co-driver's pace notes and
messages, and the top screen's frame rate in its corner. The game's own
circuit minimap is not drawn. In the menus, the results and the pause, the
bottom screen shows the log.

Sound goes straight to the DSP, mixed by a thread of its own on the
console's second core. Closing the game from the HOME Menu stops the
threads, the GPU and the sound in order before the system takes over.

To fit the memory, stage textures are shrunk (to at most 128x128 by default,
256x256 for the menus') and
stored in 16-bit formats. The full-size copy in main memory is released once
a texture is on the GPU; a 16-bit copy of the shrunk texture stays, so that
the textures the game changes later (car lights, for example) are rebuilt
from it. Some effects are left out:

* car reflections (a cube map the game renders every frame) are not drawn;
* the intro and country videos are skipped by default (decoding Bink is too
  slow);
* there is no specular light, no bump mapping and no lens flare, and spot
  lights are lit as point lights.

Network play (new, being tested) works over the console's wifi, with other 3DS consoles and with
the PC versions on the same network (LAN search) or by address
(`[net] peers`, see [NETWORK.md](NETWORK.md)). Text, such as a player's name
or the chat, is typed on the system keyboard: press X in a menu.

## Installing

This fork needs a **New 3DS, New 3DS XL or New 2DS XL**. Its CIA asks for
their largest memory mode, 178 MB: a 96 MB heap and about 75 MB of linear
(GPU) memory. A stage needs at least the 124 MB mode, and the game says so
and stops when it gets less (the first lines of the log show what it got).
Start it from its HOME Menu icon; after installing a new version, turn the
console off and on first (a launch right after an install may get 64 MB).

The easiest way is a **standalone CIA** with your game files inside, made on
your PC by the [CIA Builder](../tools/cia_builder/README.md) (the
`OpenCMR2-CIA-Builder` download of a build): pick your `CMR2.exe`, build,
install the CIA with FBI, then do step 3 below. The game reads its files from
the CIA first and from the SD card second.

Otherwise, with the plain CIA:

1. Install `OpenCMR2.cia` with FBI (or another title manager). Instead, you
   can copy `opencmr2.3dsx` to `sd:/3ds/` and start it from the Homebrew
   Launcher. The CIA gets more memory and, on a New 3DS, the faster CPU.
2. Copy your installed PC game's folders (`Game`, `FrontEnd`, `Sounds`, ...)
   to `sd:/3ds/OpenCMR2/Data/`, so that `sd:/3ds/OpenCMR2/Data/Game/Bigfiles/common.bfl`
   exists. The movie folder can be left out, as the videos are skipped. If
   you have the CD, install the game on a PC first: the 3DS reads the
   installed folders. The CIA Builder's "SD card folder" output does this
   copy for you.
3. Sound needs the console's DSP firmware dump (`sd:/3ds/dspfirm.cdc`). Run
   the DSP1 homebrew once if you do not have it. Without it the game runs
   silently.

Settings, saves and the log go to `sd:/3ds/OpenCMR2/`.

## Controls

The buttons press the game's default keyboard keys, so the controls menu
shows the keyboard. In menus and on a stage they mean different things:

| Button | Menus | Stage |
| --- | --- | --- |
| Circle Pad | Move | Steer (only) |
| D-Pad | Move | Steer; up and down also accelerate and brake |
| R | | Accelerate |
| L | | Brake (held when stopped: reverse, with the automatic gearbox) |
| A | Confirm | Handbrake |
| B | Back | Gear up |
| Y | | Gear down |
| X | System keyboard (typing) | Quick reverse: straight into reverse, or from reverse into first (manual gearbox) |
| START | Confirm | Pause |
| SELECT | | Confirm |
| ZL (New 3DS) | | Camera |
| ZR (New 3DS) | | Key R of the keyboard controls |
| Touch screen | Confirm | Confirm |

The pause menu, the results after a stage and its replay are menus: there A
or START confirm and B goes back.

## Settings

`sd:/3ds/OpenCMR2/opencmr2.ini` takes these in its `[3ds]` section, in
addition to the [general settings](../README.md#configuration):

| Setting | Default | Description |
| --- | --- | --- |
| `texture_max` | `128` | Largest texture side on a stage, 8 to 1024. Higher is sharper and needs more memory |
| `texture_max_menu` | `256` | The same for textures loaded in the menus |
| `menu_supersample` | `1` | `1` draws the menus at 640x480 and halves them (sharper text); `0` draws them at half size |
| `text_scale` | `130` | Size of the menus' small text, in percent of the original (100 to 200); `100` keeps it as on the PC |
| `text_scale_below` | `24` | Fonts with lines shorter than this many pixels count as small (the 12 and 15 point ones) |
| `map` | `1` | `1` shows the stage map and the HUD on the bottom screen while driving; `0` keeps the log there and the whole HUD on top |
| `movies` | `0` | `1` plays the Bink videos |
| `widescreen` | `1` | The game at 800x480, filling the top screen with a wider view; `0`: the original 640x480, 4:3 |
| `stretch` | `0` | With `widescreen = 0`, `1` stretches the 4:3 picture over the whole top screen |
| `vsync` | `0` | `1` starts each frame on the screen's refresh: steadier, but the frame rate drops in steps (60, 30, 20, 15) |
| `console` | `1` | `0` stops writing the log on the bottom screen |
| `allow_low_memory` | `0` | `1` starts the game with less than 124 MB (or on an Old 3DS), for tests: stages may run out of memory |
| `stats_interval` | `120` | Frames between the log's summaries (30 to 3600) |
| `render_profile` | `8` | Every Nth frame, time the renderer's parts for the log's `render:` lines (`0`: off) |
| `frame_log` | `1` | `1` keeps every frame of a stage in memory and writes it to `frames-*.csv` when the stage ends; `0` turns it off |
| `debug_lighting`, `debug_fog`, `debug_mipmaps` | `1` | `0` draws without lighting, fog or mipmaps, to find a feature the GPU does not take |
| `debug_shader` | `0` | `1` draws 3D with the minimal vertex shader (`fixedmin.v.pica`: no lighting or texture matrices) |

## Problems

The log is `sd:/3ds/OpenCMR2/opencmr2.log`. Every 120 frames (`stats_interval`)
it gets four lines: memory, drawing, where a frame's time goes on average (the
game's own work, the renderer, waiting for the GPU or the screen refresh (at
most 60 fps), and the GPU's drawing time), and the worst frame with how many
frames missed 60 and 30 fps. The log is written by a thread of its own, so
the game never waits for the SD card (a write can stall for seconds):

```
mem: heap 7072 of 65268 KB used, linear 26040 KB, VRAM 5343 KB free
gfx: 24.0 fps, up to 3006 draws and 30% of the command buffer a frame, 0 frames cut short
time: a frame 30.1 ms game, 9.2 ms renderer, 0.4 ms waiting for the GPU or the refresh (GPU 12.0 ms); 410 KB of vertices
worst: frame 52.3 ms, game 38.0 ms; 120 of 120 frames over 16.7 ms, 97 over 33.3 ms
```

### The renderer's parts

With `render_profile` (every 8th frame by default), each summary adds:

```
render: 11.7 ms a frame: textures 0.10, flush 0.40, uniforms 3.10, state 2.50, submit 4.20, bottom 0.60, end 0.20, begin 0.30, clears 0.05, loop 0.20, finish 0.10, other 0.05; a draw 21.0 us (uniforms 6.6, state 5.3, submit 9.1)
render: changes a frame: 1900 uniform vectors, 600 flags, 900 combiner stages, 400 textures, 80 viewports, 3 shaders
render: 12 textures converted while driving, 18.0 ms: 256x256/3 64x64/2
```

(example figures). The parts: textures converted before the frame, the
flush of its vertices and indices from the data cache (they are recorded in
linear memory, where the GPU reads them), and for each draw its uniforms
(matrices and lights, worked out again only when they change),
fixed-function state (depth, blending, texture stages, fog) and submission
(citro3d sends what changed and the draw); then the bottom screen, the end of
the frame, the start of the next one (without its wait for the GPU), the
clears, the loop over the draws itself, and the bookkeeping after the frame.
Timed frames carry flag 16 in the frame log.

### Frame log

While a stage is driven, every frame's timings stay in memory (about 1.3 MB,
allocated once) and are written to `sd:/3ds/OpenCMR2/frames-<date>-<time>-<n>.csv`
three seconds after the stage ends (or after 36,000 frames). Nothing is written
to the SD card while driving, so recording costs a few stores a frame. When the
stage ends the frames are turned into text (`frame log: N frames formatted in
X ms, being written to ...`) and written by the log's thread, so the results
screen does not wait for the card (`frame log: ... written in X s`).

| Column | Meaning |
| --- | --- |
| `time_ms`, `interval_ms` | When the frame was shown, and since the previous one |
| `game_ms`, `render_ms`, `wait_ms`, `gpu_ms` | As in the `time:` line, for this frame alone |
| `draws`, `cmd_pct`, `vertex_kb` | Draws, command buffer use and vertex data of the frame |
| `car_x`, `car_z` | The player's car (world units), to find where on the stage a spike happens |
| `flags` | 1: a summary was written to the log; 2: textures were converted; 4: the frame was cut short; 8: a full frame log was written before it |

Paused frames are not kept, so `time_ms` skips over pauses. A pause of more
than three seconds counts as the end of the stage: the frames so far are
written, and the rest of the stage goes to the next file.

On a stage, the game's graphics options (draw distance, effects, shadows,
reflections) change most of the game's and the GPU's share. Car reflections
of the "environment" kind are not drawn on the 3DS, so they cost nothing.

Files of 512 KB or more are logged when they are opened, with the heap in use
then (`file Game/Bigfiles/....bfl: ... KB, heap ... of ... KB used`). A frame
that would overflow the GPU's command buffer is cut short instead, and
logged (`gfx: command buffer full`). If the game stops or
returns to the HOME menu, please send that file with a description of what
you were doing. With Luma3DS, a crash screen or dump (`sd:/luma/dumps/`)
helps too.

## Building

You need devkitPro with devkitARM, libctru and citro3d (the
`devkitpro/devkitarm` Docker image has them), Ninja, and for the CIA
[makerom](https://github.com/3DSGuy/Project_CTR) and
[bannertool](https://github.com/diasurgical/bannertool) on the `PATH` or in
`OPENCMR2_3DS_TOOLS`:

```sh
docker run --rm -v "$PWD:/src" -w /src devkitpro/devkitarm \
    sh -c 'cmake --preset 3ds -DOPENCMR2_3DS_TOOLS=/path/to/tools && cmake --build --preset 3ds'
```

CMake downloads and builds SDL3 for the 3DS (its threads, files and audio
format conversion are used; its video and audio devices are not: the sound
goes to the DSP through `ndsp` in `src/audio/audio_sdl.cpp`). The results are in `build/3ds/src/`:

| File | |
| --- | --- |
| `OpenCMR2.cia` | Installable title (only with makerom and bannertool) |
| `opencmr2.3dsx` | Homebrew Launcher application |
| `opencmr2_gfx_scene.3dsx` | Renderer test scene (`tests/3ds/gfx_scene.cpp`) |
| `opencmr2_gfx_stress.3dsx` | The test scene with 3000 more draws a frame, about a stage's load |
| `opencmr2_gpu_bisect.3dsx` | Draws one feature at a time, from the simplest up, and writes each phase that worked to `sd:/opencmr2-gpu.log`; the phase after the last one logged stops the GPU |
| `opencmr2_test_math_tables.3dsx`, `opencmr2_test_car_scheduler.3dsx` | Host tests that run game code; their output goes to `sd:/opencmr2-test.log` (the math tables test needs `tests/reference/finland-stage1/math-tables.bin` at `sd:/math-tables.bin`) |

The CI job `nintendo-3ds` builds them in the same container and uploads
`OpenCMR2-3DS.zip`.

### How it is put together

* `src/platform/ctr.cpp`: screens before the renderer starts, the log on the
  bottom screen, and the buttons, which press keyboard keys. The game reports
  whether a stage is being driven (`Input_SetDrivingContext`, a `PORT:` line
  in `CGame::DispatchFrontendResourceState` and in the pause menu).
* `src/render/gfx_c3d.cpp`: the executor of the recorded frames on citro3d,
  with the vertex shaders `src/render/shaders/fixed.v.pica` (lighting, up to
  4 lights, texture coordinates), `fixedtl.v.pica` (pre-transformed 2D) and
  `fixedmin.v.pica` (3D without lighting, for debugging). The shaders write
  their outputs whole and declare the texture coordinates with two
  components (`texcoord0.xy`): an emulator accepts a shader that leaves
  output components unwritten, a console's GPU may not.
  Fog uses the PICA's fog table, and the texture stages use its combiners.
* `src/render/gfx_core.cpp`: hands finished textures to the executor and
  releases their CPU copies (`ReleaseTextureCopies`). Vertex buffers get
  their memory on the first lock.
* `src/net/`: network play as on the PC, ENet on the console's sockets
  (`third_party/enet/unix.c` gathers the buffers for `sendto`/`recvfrom`,
  which libctru has instead of `sendmsg`/`recvmsg`).
* `res/3ds/`: icon, banner and the makerom settings (`opencmr2.rsf`).

The game builds with GCC here, with `-fms-extensions -fpermissive` and empty
`__fastcall`/`__forceinline` (`src/CMakeLists.txt`). The 3DS build keeps the
compiler options the game relies on (wrapping signed overflow, no strict
aliasing, signed `char`, no fused multiply-add).

### The test scene

`opencmr2_gfx_scene.3dsx` draws, through `port/gfx.h`:

* a blue background, the 4:3 picture centred with black bars;
* top left, a textured square with red (top left), green (top right), blue
  (bottom left) and white (bottom right) quarters, and a half-transparent
  yellow square over it;
* top right, a green triangle (clockwise, drawn) and no red one
  (counter-clockwise, culled);
* a white cube lit from the upper left, whose front face has the value 133 of
  255. L turns it;
* a checkered ground that fades into grey fog with distance;
* bottom left, a copy of the corners texture made after its CPU copy was
  released (the same four colours), while the original's green quarter has
  turned magenta, changed through a lock after the release.

`opencmr2_gfx_stress.3dsx` adds 3000 small fogged, lit triangles, each with
its own world matrix; the log shows the command buffer use (about 30%).

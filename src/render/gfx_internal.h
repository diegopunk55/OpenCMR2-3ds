// Internals of the renderer: the state gfx_core.cpp records and the
// executor that turns a recorded frame into GPU work (gfx_gpu.cpp, or the
// null one for headless runs).
#ifndef OPENCMR2_GFX_INTERNAL_H
#define OPENCMR2_GFX_INTERNAL_H

#include "port/gfx.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <vector>

// ---- textures and buffers --------------------------------------------------------

struct GfxTexture {
    int width, height;
    int format;                 // GFX_FORMAT_*
    int mipLevels;
    DWORD flags;
    GfxTexture *cube;           // a face view: the cube it belongs to
    int face;                   // a face view: its face
    GfxTexture *faceViews[6];   // a cube: its face views (1..5; 0 is itself)
    // CPU copy of every level of every face, tightly packed rows.
    std::vector<uint8_t> levels[6][16];
    bool dirty;                 // CPU copy newer than the GPU's
    bool renderedTo;            // the GPU copy is newer (render targets)
    int locks;                  // Gfx_LockTexture calls not yet unlocked
    bool released;              // CPU copy released after an Upload
    bool keepCopy;              // locked again too often: keep the copy
    int restores;               // locks and copies after a release
    bool keepSize;              // created under Gfx_KeepTextureSize
    void *gpu;                  // executor's object
};

struct GfxVertexBuffer {
    DWORD count = 0;
    std::vector<GfxVertex> vertices;    // allocated on the first lock
};

// Bytes of one level, given its size, for a format.
size_t Gfx_LevelSize(int format, int width, int height, LONG *pitch);
// Bytes per pixel of the CPU copy (0 for compressed formats).
int Gfx_BytesPerPixel(int format);

// ---- recorded state ---------------------------------------------------------------

struct GfxStageState {
    int colorOp = GFX_TOP_MODULATE, colorArg1 = GFX_TA_TEXTURE, colorArg2 = GFX_TA_CURRENT;
    int alphaOp = GFX_TOP_SELECTARG1, alphaArg1 = GFX_TA_TEXTURE, alphaArg2 = GFX_TA_CURRENT;
    int address = GFX_ADDRESS_WRAP;
    int minFilter = GFX_FILTER_POINT, magFilter = GFX_FILTER_POINT, mipFilter = GFX_MIPFILTER_NONE;
    int maxMipLevel = 0;
    DWORD texCoordIndex = 0;
    int textureTransform = GFX_TTFF_DISABLE;
    float bumpMat[4] = { 0, 0, 0, 0 };
    float bumpLumScale = 1.0f, bumpLumOffset = 0.0f;
    GfxTexture *texture = nullptr;
};

// Everything a draw depends on, copied into each recorded draw.
struct GfxDrawState {
    GfxMatrix world, view, projection, texture[2];
    GfxLight lights[8];
    bool lightEnabled[8] = {};
    GfxMaterial material;
    bool lighting = true;
    DWORD ambient = 0;
    bool specular = false;
    bool localViewer = true;
    bool normalizeNormals = false;
    bool colorVertex = true;
    int diffuseSource = GFX_MCS_COLOR1, specularSource = GFX_MCS_COLOR2;
    int ambientSource = GFX_MCS_MATERIAL, emissiveSource = GFX_MCS_MATERIAL;
    bool fog = false;
    DWORD fogColor = 0;
    int fogVertexMode = GFX_FOG_NONE, fogTableMode = GFX_FOG_NONE;
    float fogStart = 0.0f, fogEnd = 1.0f;
    bool depthTest = true, depthWrite = true;
    int depthFunc = GFX_CMP_LESSEQUAL;
    int cull = GFX_CULL_CCW;
    bool alphaBlend = false;
    int srcBlend = GFX_BLEND_ONE, destBlend = GFX_BLEND_ZERO;
    bool alphaTest = false;
    int alphaFunc = GFX_CMP_ALWAYS;
    DWORD alphaRef = 0;
    DWORD textureFactor = 0xffffffff;
    GfxStageState stages[2];
    GfxViewport viewport;
    // Change with the matrices and the lights (a new value at each set), so
    // that an executor can tell that draws share them without comparing.
    uint32_t worldKey = 0, viewKey = 0, projectionKey = 0, lightKey = 0;
};

enum GfxCommandType {
    GFX_CMD_DRAW,
    GFX_CMD_CLEAR,
    GFX_CMD_TARGET
};

struct GfxCommand {
    GfxCommandType type;
    // draw
    int primitive;              // list/strip types only (fans are converted)
    bool transformed;           // GfxTLVertex data
    uint8_t screen;             // GFX_SCREEN_*: 2D for a handheld's second screen
    uint32_t firstVertexByte;   // into the frame's vertex data
    uint32_t vertexCount;
    uint32_t firstIndex;        // into the frame's indices, or ~0 for none
    uint32_t indexCount;
    uint32_t state;             // index into the frame's states
    // clear
    DWORD clearFlags;
    DWORD clearColor;
    float clearZ;
    GfxViewport clearViewport;
    // target
    GfxTexture *target;         // NULL: the back buffer
};

// Memory for a frame's vertices and indices (gfx_core.cpp): on the 3DS
// linear memory, which the GPU reads where the draws were recorded.
void *Gfx_AllocGeometry(size_t bytes);
void Gfx_FreeGeometry(void *block);

// A frame's vertices or indices. A block that grows keeps its old one
// until it is cleared: draws already sent to the GPU may still read it.
template <typename T> struct GfxArray {
    T *items = nullptr;
    size_t count = 0, capacity = 0;
    std::vector<void *> old;

    GfxArray() {}
    GfxArray(const GfxArray &) = delete;
    GfxArray &operator=(const GfxArray &) = delete;
    ~GfxArray()
    {
        clear();
        Gfx_FreeGeometry(items);
    }
    size_t size() const { return count; }
    bool empty() const { return count == 0; }
    T *data() { return items; }
    const T *data() const { return items; }
    T &operator[](size_t i) { return items[i]; }
    const T &operator[](size_t i) const { return items[i]; }
    void clear()
    {
        count = 0;
        for (void *block : old)
            Gfx_FreeGeometry(block);
        old.clear();
    }
    // Room for n more items at the end, returned (NULL without memory).
    T *Append(size_t n)
    {
        if (count + n > capacity) {
            size_t wanted = std::max<size_t>(count + n, capacity < 4096 ? 65536 / sizeof(T) : capacity * 2);
            T *bigger = (T *)Gfx_AllocGeometry(wanted * sizeof(T));
            if (bigger == nullptr)
                return nullptr;
            if (count != 0)
                memcpy(bigger, items, count * sizeof(T));
            if (items != nullptr)
                old.push_back(items);
            items = bigger;
            capacity = wanted;
        }
        T *p = items + count;
        count += n;
        return p;
    }
    void Swap(GfxArray &other)
    {
        std::swap(items, other.items);
        std::swap(count, other.count);
        std::swap(capacity, other.capacity);
        old.swap(other.old);
    }
};

struct GfxFrame {
    GfxArray<uint8_t> vertices;
    GfxArray<uint16_t> indices;
    std::vector<GfxDrawState> states;
    std::vector<GfxCommand> commands;
    // keepGeometry: the frame goes on after a flush, and the vertices and
    // indices already recorded stay where they are (the 3DS's GPU reads
    // them at the end of the frame).
    void Reset(bool keepGeometry = false)
    {
        if (!keepGeometry) {
            vertices.clear();
            indices.clear();
        }
        states.clear();
        commands.clear();
    }
};

struct GfxReadback {
    bool requested = false;
    int x = 0, y = 0, width = 0, height = 0;
    bool ready = false;
    int readyWidth = 0, readyHeight = 0;
    std::vector<uint32_t> pixels;   // 0xAARRGGBB
};

// ---- executor -----------------------------------------------------------------------

struct GfxExecutor {
    virtual ~GfxExecutor() {}
    virtual bool SetVideoMode(int width, int height, bool fullscreen) = 0;
    virtual void SetFullscreen(bool fullscreen) = 0;
    // Runs the recorded commands; with present, shows the back buffer and
    // ends the frame (otherwise the next commands continue it).
    virtual void Execute(GfxFrame &frame, std::vector<GfxTexture *> &textures, GfxReadback &readback, bool present,
                         bool vsync, bool stretch = false) = 0;
    virtual void DestroyTexture(GfxTexture *texture) = 0;
    // Brings a render target's pixels back into its CPU copy (for locks).
    virtual void ReadTexture(GfxTexture *texture) = 0;
    // Takes a texture's contents now, so that its CPU copy can be released
    // (true), for executors short of memory (3DS); see gfx_core.cpp.
    virtual bool Upload(GfxTexture *) { return false; }
    // The top level of a texture taken by Upload, as BGRA pixels at the
    // texture's size (from the executor's smaller copy).
    virtual bool ReadPixels(GfxTexture *, std::vector<uint32_t> &) { return false; }
    // Whether draws into a texture target are carried out.
    virtual bool RendersToTextures() { return true; }
    // Whether a second screen takes HUD draws now (Hud_SetScreen).
    virtual bool HasSecondScreen() { return false; }
};

// Gfx_SetRaceMode's value.
bool Gfx_IsRaceMode(void);

GfxExecutor *Gfx_CreateGpuExecutor(void);
GfxExecutor *Gfx_CreateNullExecutor(void);

// Decodes a BC1/BC3 4x4 block into 16 BGRA pixels.
void Gfx_DecodeBlock(int format, const uint8_t *block, uint32_t out[16]);
// Encodes 16 BGRA pixels as a BC1/BC3 block (simple endpoints, for textures
// rebuilt from the executor's copy).
void Gfx_EncodeBlock(int format, const uint32_t in[16], uint8_t *block);

#endif

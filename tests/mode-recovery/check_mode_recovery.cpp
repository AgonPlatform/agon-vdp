#include <cassert>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

enum Event { PoolAllocated, ViewportRetired, ControllerDeleted, CanvasDeleted, Swap, ModeReply, Restart };
static std::vector<Event> events;
static unsigned poolRefusals, resolutionRefusals;
static unsigned liveViewports, livePools;
static bool requireReleasedViewport;
static const unsigned MALLOC_CAP_8BIT = 0x004, MALLOC_CAP_INTERNAL = 0x800, MALLOC_CAP_32BIT = 0x002;

static void *heap_caps_malloc(size_t size, unsigned caps) {
    assert(size == 514 && caps == (MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));
    if (requireReleasedViewport)
        assert(liveViewports == 0);
    if (poolRefusals) {
        --poolRefusals;
        return nullptr;
    }
    events.push_back(PoolAllocated);
    ++livePools;
    return std::malloc(size);
}

static void heap_caps_free(void *memory) {
    if (memory) {
        assert(livePools);
        --livePools;
        std::free(memory);
    }
}

static int heap_caps_get_free_size(unsigned) { return 1000; }

namespace fabgl {
class LightMemoryPool {
public:
    explicit LightMemoryPool(int size);
    ~LightMemoryPool();
    bool ready() const { return m_mem != nullptr; }
private:
    void mark(int, int16_t, bool);
    uint8_t *m_mem;
    int m_poolSize;
};

struct VGABaseController {
    int width = 0, height = 0;
    bool begun = false, doubled = false;
    LightMemoryPool pool{512};

    virtual ~VGABaseController() {
        assert(!begun || height == 0);
        events.push_back(ControllerDeleted);
    }
    bool drawingResourcesReady() const { return pool.ready(); }
    void begin() { assert(pool.ready()); begun = true; }
    void end() {
        assert(begun);
        if (height) {
            --liveViewports;
            events.push_back(ViewportRetired);
        }
        height = 0;
    }
    void setResolution(const char *line, int, int, bool doubleBuffered) {
        end();
        if (resolutionRefusals) {
            --resolutionRefusals;
            return;
        }
        assert(std::sscanf(std::strchr(line, '_') + 1, "%dx%d", &width, &height) == 2);
        ++liveViewports;
        doubled = doubleBuffered;
    }
    int getViewPortWidth() const { return width; }
    int getViewPortHeight() const { return height; }
    int getScreenHeight() const { return height; }
    void enableBackgroundPrimitiveExecution(bool) {}
    void enableBackgroundPrimitiveTimeout(bool) {}
};
struct VGA2Controller : VGABaseController {};
struct VGA4Controller : VGABaseController {};
struct VGA8Controller : VGABaseController {};
struct VGA16Controller : VGABaseController {};
struct VGA64Controller : VGABaseController {};

struct Canvas {
    VGABaseController *owner;
    explicit Canvas(VGABaseController *controller) : owner(controller) { assert(controller->height); }
    ~Canvas() { assert(owner->height); events.push_back(CanvasDeleted); }
    unsigned getWidth() const { return owner->width; }
    unsigned getHeight() const { return owner->height; }
};
}

static std::unique_ptr<fabgl::Canvas> canvas;
static std::unique_ptr<fabgl::VGABaseController> _VGAController;
static uint8_t _VGAColourDepth = 255, videoMode;
static unsigned canvasW, canvasH;
static double logicalScaleX, logicalScaleY;
static const double LOGICAL_SCRW = 1280, LOGICAL_SCRH = 1024;
static bool rectangularPixels, legacyModes, ttxtMode, mouseVisible;
static constexpr unsigned CALLBACK_VSYNC = 1, CALLBACK_MODE_CHANGE = 2;
static constexpr unsigned PACKET_MODE = 6, CALLBACK_SENDING_VDPP = 0x100;
static std::array<uint8_t, 8> modeReply;
static void debug_log(const char *, ...) {}
static void restorePalette() { assert(canvas); }
static bool isDoubleBuffered() { assert(canvas); return _VGAController->doubled; }
static void waitPlotCompletion(bool) { assert(canvas); }
static void switchBuffer() { assert(isDoubleBuffered()); events.push_back(Swap); }
static void showMouseCursor() {}
static void resetMousePositioner(unsigned, unsigned, fabgl::VGABaseController *) { assert(canvas); }
static void updateMouseVars(void *) {}
static void esp_restart() { assert(!canvas && !_VGAController); events.push_back(Restart); }
static struct { int init() { return 0; } } ttxt_instance;

struct Context {
    void cls() { assert(canvas); }
    unsigned getNormalisedViewportCharWidth() { assert(canvas); return canvasW / 8; }
    unsigned getNormalisedViewportCharHeight() { assert(canvas); return canvasH / 8; }
} context_storage;
struct VDUStreamProcessor {
    Context *context = &context_storage;
    void vdu_mode(uint8_t mode);
    void bufferRemoveCallback(unsigned, unsigned) { assert(canvas); }
    void resetAllContexts() { assert(canvas); }
    void bufferCallCallbacks(unsigned) { assert(canvas); }
    void sendModeInformation();
    void send_packet(unsigned type, unsigned size, const uint8_t *packet) {
        assert(type == PACKET_MODE && size == modeReply.size());
        std::memcpy(modeReply.data(), packet, size);
        events.push_back(ModeReply);
    }
};

#include "mode-bodies.inc"

static void startMode(uint8_t mode) {
    releaseVGAController();
    assert(livePools == 0 && liveViewports == 0);
    assert(changeMode(mode) == 0);
    events.clear();
}

static void expectMode(uint8_t mode, unsigned width, unsigned height, unsigned colours) {
    assert(videoMode == mode && canvasW == width && canvasH == height && _VGAColourDepth == colours);
    assert(!isDoubleBuffered());
    assert(livePools == 1 && liveViewports == 1);
    assert(events.back() == ModeReply);
    const std::array<uint8_t, 8> expected = {
        uint8_t(width), uint8_t(width >> 8), uint8_t(height), uint8_t(height >> 8),
        uint8_t(width / 8), uint8_t(height / 8), uint8_t(colours), mode,
    };
    assert(modeReply == expected);
    for (auto event : events)
        assert(event != Swap && event != Restart);
}

int main() {
    VDUStreamProcessor vdu;
    for (uint8_t previous : {20, 136}) {
        startMode(previous);
        requireReleasedViewport = true;
        vdu.vdu_mode(0);
        expectMode(0, 640, 480, 16);
        assert((events == std::vector<Event>{CanvasDeleted, ViewportRetired, ControllerDeleted,
                                             PoolAllocated, ModeReply}));
    }

    startMode(149);
    auto *sameController = _VGAController.get();
    vdu.vdu_mode(0);
    expectMode(0, 640, 480, 16);
    assert(_VGAController.get() == sameController);
    assert((events == std::vector<Event>{CanvasDeleted, ViewportRetired, ModeReply}));

    startMode(136);
    poolRefusals = 1;
    vdu.vdu_mode(0);
    expectMode(1, 640, 480, 4);
    assert(poolRefusals == 0);
    assert((events == std::vector<Event>{CanvasDeleted, ViewportRetired, ControllerDeleted,
                                             ControllerDeleted, PoolAllocated, ModeReply}));

    startMode(136);
    resolutionRefusals = 1;
    vdu.vdu_mode(0);
    expectMode(1, 640, 480, 4);

    startMode(20);
    poolRefusals = 1;
    vdu.vdu_mode(0);
    expectMode(20, 512, 384, 64);

    startMode(136);
    poolRefusals = 2;
    vdu.vdu_mode(0);
    assert(events.back() == Restart && livePools == 0 && liveViewports == 0);
    assert(!canvas && !_VGAController && poolRefusals == 0);

    startMode(20);
    vdu.vdu_mode(255);
    expectMode(20, 512, 384, 64);
    releaseVGAController();
    assert(livePools == 0 && liveViewports == 0);
    std::puts("8 MODE recovery cases passed");
}

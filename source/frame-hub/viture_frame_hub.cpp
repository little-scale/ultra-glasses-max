#include "viture_frame_hub.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>
#include <vector>

namespace {

struct FrameStore {
    std::mutex mutex;
    std::vector<unsigned char> planes[4];
    VitureFrameInfo info{};
    bool valid = false;
};

FrameStore g_store;
std::atomic<int> g_consumers{0};

bool valid_dimensions(int width, int height, size_t& bytes) {
    if (width <= 0 || height <= 0) return false;
    const auto w = static_cast<size_t>(width);
    const auto h = static_cast<size_t>(height);
    if (w > std::numeric_limits<size_t>::max() / h) return false;
    bytes = w * h;
    return bytes > 0;
}

}  // namespace

extern "C" {

void viture_frame_hub_add_consumer(void) {
    g_consumers.fetch_add(1, std::memory_order_acq_rel);
}

void viture_frame_hub_remove_consumer(void) {
    int current = g_consumers.load(std::memory_order_acquire);
    while (current > 0 &&
           !g_consumers.compare_exchange_weak(current, current - 1,
                                              std::memory_order_acq_rel)) {
    }
}

int viture_frame_hub_has_consumers(void) {
    return g_consumers.load(std::memory_order_acquire) > 0 ? 1 : 0;
}

int viture_frame_hub_consumer_count(void) {
    return std::max(0, g_consumers.load(std::memory_order_acquire));
}

void viture_frame_hub_publish(const unsigned char* left0,
                              const unsigned char* right0,
                              const unsigned char* left1,
                              const unsigned char* right1,
                              double timestamp,
                              int width,
                              int height,
                              uint64_t sequence) {
    if (!viture_frame_hub_has_consumers()) return;

    size_t bytes = 0;
    if (!valid_dimensions(width, height, bytes)) return;

    const unsigned char* sources[4] = {left0, right0, left1, right1};
    const int valid_mask = (left0 ? 1 : 0) | (right0 ? 2 : 0) |
                           (left1 ? 4 : 0) | (right1 ? 8 : 0);
    if (valid_mask == 0) return;
    std::lock_guard<std::mutex> lock(g_store.mutex);
    for (int index = 0; index < 4; ++index) {
        if (sources[index])
            g_store.planes[index].assign(sources[index], sources[index] + bytes);
        else
            g_store.planes[index].assign(bytes, 0);
    }
    g_store.info = {width, height, timestamp, sequence, bytes, valid_mask};
    g_store.valid = true;
}

int viture_frame_hub_get_latest_info(VitureFrameInfo* info) {
    if (!info) return 0;
    std::lock_guard<std::mutex> lock(g_store.mutex);
    if (!g_store.valid) return 0;
    *info = g_store.info;
    return 1;
}

int viture_frame_hub_copy_latest(uint64_t after_sequence,
                                 unsigned char* left0,
                                 unsigned char* right0,
                                 unsigned char* left1,
                                 unsigned char* right1,
                                 size_t capacity,
                                 VitureFrameInfo* info) {
    if (!info) return -1;
    std::lock_guard<std::mutex> lock(g_store.mutex);
    if (!g_store.valid) return 0;
    *info = g_store.info;
    if (g_store.info.sequence == after_sequence) return 0;
    if (!left0 || !right0 || !left1 || !right1 || capacity < g_store.info.plane_bytes)
        return -1;

    unsigned char* destinations[4] = {left0, right0, left1, right1};
    for (int index = 0; index < 4; ++index)
        std::memcpy(destinations[index], g_store.planes[index].data(),
                    g_store.info.plane_bytes);
    return 1;
}

}  // extern "C"

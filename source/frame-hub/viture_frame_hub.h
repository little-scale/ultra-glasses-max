#pragma once

#include <cstddef>
#include <cstdint>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct VitureFrameInfo {
    int width;
    int height;
    double timestamp;
    uint64_t sequence;
    size_t plane_bytes;
    int valid_mask;
} VitureFrameInfo;

void viture_frame_hub_add_consumer(void);
void viture_frame_hub_remove_consumer(void);
int viture_frame_hub_has_consumers(void);
int viture_frame_hub_consumer_count(void);

void viture_frame_hub_publish(const unsigned char* left0,
                              const unsigned char* right0,
                              const unsigned char* left1,
                              const unsigned char* right1,
                              double timestamp,
                              int width,
                              int height,
                              uint64_t sequence);

int viture_frame_hub_get_latest_info(VitureFrameInfo* info);

// Returns 1 for a copied frame, 0 when no newer frame exists, and -1 when
// the supplied buffers are too small or invalid. `info` is filled whenever a
// frame exists, including the too-small case.
int viture_frame_hub_copy_latest(uint64_t after_sequence,
                                 unsigned char* left0,
                                 unsigned char* right0,
                                 unsigned char* left1,
                                 unsigned char* right1,
                                 size_t capacity,
                                 VitureFrameInfo* info);

#if defined(__cplusplus)
}
#endif

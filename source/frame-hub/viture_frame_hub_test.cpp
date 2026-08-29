#include "viture_frame_hub.h"

#include <array>
#include <cassert>
#include <cstdint>

int main() {
    constexpr int width = 3;
    constexpr int height = 2;
    constexpr size_t bytes = width * height;
    const std::array<unsigned char, bytes> left0{1, 2, 3, 4, 5, 6};
    const std::array<unsigned char, bytes> right0{11, 12, 13, 14, 15, 16};
    const std::array<unsigned char, bytes> left1{21, 22, 23, 24, 25, 26};
    const std::array<unsigned char, bytes> right1{31, 32, 33, 34, 35, 36};

    VitureFrameInfo info{};
    assert(viture_frame_hub_get_latest_info(&info) == 0);

    // No consumer means the capture callback does no buffer copy.
    viture_frame_hub_publish(left0.data(), right0.data(), left1.data(), right1.data(),
                             1.0, width, height, 1);
    assert(viture_frame_hub_get_latest_info(&info) == 0);

    viture_frame_hub_add_consumer();
    assert(viture_frame_hub_has_consumers() == 1);
    assert(viture_frame_hub_consumer_count() == 1);
    viture_frame_hub_publish(left0.data(), right0.data(), left1.data(), right1.data(),
                             12.5, width, height, 7);
    assert(viture_frame_hub_get_latest_info(&info) == 1);
    assert(info.width == width && info.height == height);
    assert(info.timestamp == 12.5 && info.sequence == 7 && info.plane_bytes == bytes);
    assert(info.valid_mask == 15);

    std::array<unsigned char, bytes> output0{};
    std::array<unsigned char, bytes> output1{};
    std::array<unsigned char, bytes> output2{};
    std::array<unsigned char, bytes> output3{};
    assert(viture_frame_hub_copy_latest(
               0, output0.data(), output1.data(), output2.data(), output3.data(),
               bytes, &info) == 1);
    assert(output0 == left0 && output1 == right0 && output2 == left1 && output3 == right1);
    assert(viture_frame_hub_copy_latest(
               7, output0.data(), output1.data(), output2.data(), output3.data(),
               bytes, &info) == 0);
    assert(viture_frame_hub_copy_latest(
               0, output0.data(), output1.data(), output2.data(), output3.data(),
               bytes - 1, &info) == -1);

    // Carina may supply only a subset of the four documented plane pointers.
    // Valid planes are copied and absent planes are represented as zeroes.
    output0.fill(99);
    output1.fill(99);
    output2.fill(99);
    output3.fill(99);
    const std::array<unsigned char, bytes> zeroes{};
    viture_frame_hub_publish(left0.data(), right0.data(), nullptr, nullptr,
                             13.0, width, height, 8);
    assert(viture_frame_hub_copy_latest(
               7, output0.data(), output1.data(), output2.data(), output3.data(),
               bytes, &info) == 1);
    assert(info.valid_mask == 3);
    assert(output0 == left0 && output1 == right0);
    assert(output2 == zeroes);
    assert(output3 == zeroes);

    viture_frame_hub_remove_consumer();
    assert(viture_frame_hub_has_consumers() == 0);
    assert(viture_frame_hub_consumer_count() == 0);
    viture_frame_hub_publish(left0.data(), right0.data(), left1.data(), right1.data(),
                             14.0, width, height, 9);
    assert(viture_frame_hub_copy_latest(
               8, output0.data(), output1.data(), output2.data(), output3.data(),
               bytes, &info) == 0);
    return 0;
}

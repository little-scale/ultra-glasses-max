#include "viture_device_carina.h"
#include "viture_glasses_provider.h"
#include "viture_result.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <thread>

namespace {

std::atomic<unsigned long> g_camera_count{0};
std::atomic<unsigned long> g_imu_count{0};
std::atomic<unsigned long> g_vsync_count{0};

void log_hook(int level, const char* tag, const char* message) {
    std::fprintf(stderr, "[sdk:%d:%s] %s\n", level, tag ? tag : "", message ? message : "");
}

void camera_callback(char* left0, char* right0, char* left1, char* right1,
                     double timestamp, int width, int height) {
    const auto count = ++g_camera_count;
    if (count == 1 || count % 25 == 0) {
        const int mask = (left0 ? 1 : 0) | (right0 ? 2 : 0) |
                         (left1 ? 4 : 0) | (right1 ? 8 : 0);
        std::printf("camera count=%lu mask=%d timestamp=%.6f size=%dx%d\n",
                    count, mask, timestamp, width, height);
        std::fflush(stdout);
    }
}

void imu_callback(float*, double) { ++g_imu_count; }
void vsync_callback(double) { ++g_vsync_count; }

int run_session(int product_id, bool combined) {
    g_camera_count = 0;
    g_imu_count = 0;
    g_vsync_count = 0;

    XRDeviceProviderHandle handle = xr_device_provider_create(product_id);
    if (!handle) {
        std::fprintf(stderr, "create failed\n");
        return 1;
    }

    const int device_type = xr_device_provider_get_device_type(handle);
    const int dof_rc = xr_device_provider_set_dof_type_carina(handle, 1);
    const int init_rc = xr_device_provider_initialize(handle, nullptr, nullptr);
    std::printf("session=%s device_type=%d dof=%d initialize=%d\n",
                combined ? "combined" : "camera-only", device_type, dof_rc, init_rc);
    if (init_rc != VITURE_GLASSES_SUCCESS) {
        xr_device_provider_destroy(handle);
        return 1;
    }

    const int callback_rc = xr_device_provider_register_callbacks_carina(
        handle, nullptr,
        combined ? vsync_callback : nullptr,
        combined ? imu_callback : nullptr,
        camera_callback);
    const int start_rc = callback_rc == VITURE_GLASSES_SUCCESS
        ? xr_device_provider_start(handle) : callback_rc;
    std::printf("callbacks=%d start=%d\n", callback_rc, start_rc);
    std::fflush(stdout);

    if (start_rc == VITURE_GLASSES_SUCCESS) {
        for (int i = 0; i < 100; ++i) {
            float pose[7]{};
            int status = -1;
            const int pose_rc = xr_device_provider_get_gl_pose_carina(handle, pose, 0.0, &status);
            if (i % 20 == 0) {
                std::printf("pose rc=%d status=%d p=(%.4f %.4f %.4f) counts=(cam:%lu imu:%lu vsync:%lu)\n",
                            pose_rc, status, pose[0], pose[1], pose[2],
                            g_camera_count.load(), g_imu_count.load(), g_vsync_count.load());
                std::fflush(stdout);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        xr_device_provider_stop(handle);
        xr_device_provider_shutdown(handle);
    }
    xr_device_provider_destroy(handle);
    std::printf("final counts camera=%lu imu=%lu vsync=%lu\n",
                g_camera_count.load(), g_imu_count.load(), g_vsync_count.load());
    return start_rc == VITURE_GLASSES_SUCCESS ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    const int product_id = argc > 1 ? static_cast<int>(std::strtol(argv[1], nullptr, 0)) : 0x1104;
    const bool combined = argc > 2 && std::string(argv[2]) == "combined";
    xr_device_provider_set_log_hook(log_hook);
    xr_device_provider_set_log_level(LOG_LEVEL_DEBUG);
    const int result = run_session(product_id, combined);
    xr_device_provider_set_log_hook(nullptr);
    return result;
}

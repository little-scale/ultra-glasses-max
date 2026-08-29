#include "ext.h"
#include "ext_obex.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "viture_device_carina.h"
#include "viture_glasses_provider.h"
#include "viture_protocol_public.h"
#include "viture_result.h"
#include "viture_version.h"
#include "viture_frame_hub.h"

namespace {

constexpr size_t kQueueCapacity = 8192;
constexpr size_t kMaxEventsPerQelem = 512;

enum class EventType {
    Pose,
    Euler,
    Imu,
    Vsync,
    TrackingStable,
    Stereo,
    CameraBuffers,
    State,
    Brightness,
    Volume,
    DisplayMode,
    Film,
    DutyCycle,
    Worn,
    Connected,
    Alive,
    ProductId,
    Rate,
    Recentered,
    Reset,
    DeviceName,
    SdkVersion,
    Firmware,
    SdkLog,
    Control,
    Error,
};

struct Event {
    EventType type = EventType::Error;
    int count = 0;
    double values[10]{};
    char text[128]{};
};

struct BridgeState {
    std::mutex queue_mutex;
    std::array<Event, kQueueCapacity> queue{};
    size_t head = 0;
    size_t tail = 0;
    size_t count = 0;
    uint64_t dropped = 0;
    uint64_t reported_dropped = 0;

    std::atomic<bool> stop{false};
    std::atomic<bool> connected{false};
    std::atomic<bool> request_recenter{false};
    std::atomic<bool> request_reset{false};
    std::atomic<bool> request_state{false};
    std::atomic<int> request_brightness{-1};
    std::atomic<int> request_volume{-1};
    std::atomic<double> request_film{-1.0};
    std::atomic<int> request_duty_cycle{-1};
    std::atomic<int> request_display_mode{-1};
    std::atomic<int> request_dimension{-1};
    std::atomic<bool> request_auto_exposure{false};
    std::atomic<bool> request_manual_exposure{false};
    std::atomic<double> exposure_time_ms{4.0};
    std::atomic<int> exposure_gain{4};
    std::atomic<double> rate_hz{120.0};
    std::atomic<uint32_t> stereo_sequence{0};
    std::thread worker;
};

typedef struct _viture_ultra {
    t_object object;
    void* outlet = nullptr;
    void* qelem = nullptr;
    BridgeState* state = nullptr;
} t_viture_ultra;

t_class* g_viture_ultra_class = nullptr;
std::atomic<t_viture_ultra*> g_active_instance{nullptr};

double monotonic_seconds() {
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

Event numeric_event(EventType type, std::initializer_list<double> values) {
    Event event;
    event.type = type;
    event.count = static_cast<int>(std::min<size_t>(values.size(), 10));
    int index = 0;
    for (double value : values) {
        if (index >= event.count) break;
        event.values[index++] = value;
    }
    return event;
}

Event text_event(EventType type, const char* text) {
    Event event;
    event.type = type;
    std::snprintf(event.text, sizeof(event.text), "%s", text ? text : "");
    return event;
}

Event control_event(const char* name, int result,
                    std::initializer_list<double> values = {}) {
    Event event;
    event.type = EventType::Control;
    std::snprintf(event.text, sizeof(event.text), "%s", name ? name : "unknown");
    event.values[0] = static_cast<double>(result);
    event.count = 1;
    for (double value : values) {
        if (event.count >= 10) break;
        event.values[event.count++] = value;
    }
    return event;
}

void enqueue(t_viture_ultra* x, const Event& event) {
    if (!x || !x->state) return;
    BridgeState& state = *x->state;
    {
        std::lock_guard<std::mutex> lock(state.queue_mutex);
        if (state.count == kQueueCapacity) {
            state.head = (state.head + 1) % kQueueCapacity;
            --state.count;
            ++state.dropped;
        }
        state.queue[state.tail] = event;
        state.tail = (state.tail + 1) % kQueueCapacity;
        ++state.count;
    }
    qelem_set(x->qelem);
}

bool pop_event(BridgeState& state, Event& event) {
    std::lock_guard<std::mutex> lock(state.queue_mutex);
    if (state.count == 0) return false;
    event = state.queue[state.head];
    state.head = (state.head + 1) % kQueueCapacity;
    --state.count;
    return true;
}

bool queue_has_events(BridgeState& state) {
    std::lock_guard<std::mutex> lock(state.queue_mutex);
    return state.count > 0;
}

const char* selector_for(EventType type) {
    switch (type) {
        case EventType::Pose: return "pose";
        case EventType::Euler: return "euler";
        case EventType::Imu: return "imu";
        case EventType::Vsync: return "vsync";
        case EventType::TrackingStable: return "tracking_stable";
        case EventType::Stereo: return "stereo";
        case EventType::CameraBuffers: return "camera_buffers";
        case EventType::State: return "state";
        case EventType::Brightness: return "brightness";
        case EventType::Volume: return "volume";
        case EventType::DisplayMode: return "display_mode";
        case EventType::Film: return "film";
        case EventType::DutyCycle: return "dutycycle";
        case EventType::Worn: return "worn";
        case EventType::Connected: return "connected";
        case EventType::Alive: return "alive";
        case EventType::ProductId: return "product_id";
        case EventType::Rate: return "rate";
        case EventType::Recentered: return "recentered";
        case EventType::Reset: return "reset";
        case EventType::DeviceName: return "device";
        case EventType::SdkVersion: return "sdk_version";
        case EventType::Firmware: return "firmware";
        case EventType::SdkLog: return "sdklog";
        case EventType::Control: return "control";
        case EventType::Error: return "error";
    }
    return "unknown";
}

bool is_integer_event(EventType type, int index) {
    switch (type) {
        case EventType::Pose: return index == 7;
        case EventType::TrackingStable:
        case EventType::Brightness:
        case EventType::Volume:
        case EventType::DisplayMode:
        case EventType::DutyCycle:
        case EventType::Worn:
        case EventType::Connected:
        case EventType::ProductId:
        case EventType::Recentered:
        case EventType::Reset:
            return true;
        case EventType::Stereo: return index == 0 || index == 1 || index == 3;
        case EventType::CameraBuffers: return true;
        case EventType::State: return true;
        default: return false;
    }
}

void output_event(t_viture_ultra* x, const Event& event) {
    const char* selector = selector_for(event.type);
    if (event.type == EventType::Control) {
        t_atom atoms[11];
        atom_setsym(atoms, gensym(event.text));
        for (int i = 0; i < event.count; ++i) {
            if (i == 0)
                atom_setlong(atoms + i + 1, static_cast<t_atom_long>(event.values[i]));
            else
                atom_setfloat(atoms + i + 1, event.values[i]);
        }
        outlet_anything(x->outlet, gensym(selector), event.count + 1, atoms);
        return;
    }
    if (event.type == EventType::DeviceName || event.type == EventType::SdkVersion ||
        event.type == EventType::Firmware || event.type == EventType::SdkLog ||
        event.type == EventType::Error) {
        t_atom atom;
        atom_setsym(&atom, gensym(event.text));
        outlet_anything(x->outlet, gensym(selector), 1, &atom);
        return;
    }

    t_atom atoms[10];
    for (int i = 0; i < event.count; ++i) {
        if (is_integer_event(event.type, i))
            atom_setlong(atoms + i, static_cast<t_atom_long>(event.values[i]));
        else
            atom_setfloat(atoms + i, event.values[i]);
    }
    outlet_anything(x->outlet, gensym(selector), event.count, atoms);
}

void sdk_log_callback(int level, const char* tag, const char* message) {
    t_viture_ultra* x = g_active_instance.load(std::memory_order_acquire);
    if (!x) return;
    char line[128]{};
    std::snprintf(line, sizeof(line), "%d:%s: %s", level,
                  tag ? tag : "sdk", message ? message : "");
    enqueue(x, text_event(EventType::SdkLog, line));
}

void qelem_output(t_viture_ultra* x) {
    if (!x || !x->state) return;
    BridgeState& state = *x->state;
    Event event;
    size_t emitted = 0;
    while (emitted < kMaxEventsPerQelem && pop_event(state, event)) {
        output_event(x, event);
        ++emitted;
    }

    uint64_t dropped = 0;
    {
        std::lock_guard<std::mutex> lock(state.queue_mutex);
        dropped = state.dropped;
    }
    if (dropped != state.reported_dropped) {
        state.reported_dropped = dropped;
        t_atom atom;
        atom_setlong(&atom, static_cast<t_atom_long>(dropped));
        outlet_anything(x->outlet, gensym("dropped"), 1, &atom);
    }

    if (queue_has_events(state)) qelem_set(x->qelem);
}

void state_callback(int id, int value) {
    t_viture_ultra* x = g_active_instance.load(std::memory_order_acquire);
    if (!x) return;
    enqueue(x, numeric_event(EventType::State, {static_cast<double>(id), static_cast<double>(value)}));
    switch (id) {
        case VITURE_CALLBACK_ID_BRIGHTNESS:
            enqueue(x, numeric_event(EventType::Brightness, {static_cast<double>(value)}));
            break;
        case VITURE_CALLBACK_ID_VOLUME:
            enqueue(x, numeric_event(EventType::Volume, {static_cast<double>(value)}));
            break;
        case VITURE_CALLBACK_ID_DISPLAY_MODE:
            enqueue(x, numeric_event(EventType::DisplayMode, {static_cast<double>(value)}));
            break;
        case VITURE_CALLBACK_ID_ELECTROCHROMIC_FILM:
            enqueue(x, numeric_event(EventType::Film, {static_cast<double>(value)}));
            break;
        case VITURE_CALLBACK_ID_WEAR_STATUS:
            enqueue(x, numeric_event(EventType::Worn, {static_cast<double>(value)}));
            break;
        default:
            break;
    }
}

void vsync_callback(double timestamp) {
    t_viture_ultra* x = g_active_instance.load(std::memory_order_acquire);
    if (x) enqueue(x, numeric_event(EventType::Vsync, {timestamp}));
}

void imu_callback(float* imu, double timestamp) {
    t_viture_ultra* x = g_active_instance.load(std::memory_order_acquire);
    if (!x || !imu) return;
    enqueue(x, numeric_event(EventType::Imu,
        {imu[0], imu[1], imu[2], imu[3], imu[4], imu[5], timestamp}));
}

void stereo_callback(char* left0, char* right0, char* left1, char* right1,
                     double timestamp, int width, int height) {
    t_viture_ultra* x = g_active_instance.load(std::memory_order_acquire);
    if (!x || !x->state) return;
    const uint32_t sequence = ++x->state->stereo_sequence;
    if (sequence == 1) {
        const int valid_mask = (left0 ? 1 : 0) | (right0 ? 2 : 0) |
                               (left1 ? 4 : 0) | (right1 ? 8 : 0);
        enqueue(x, numeric_event(EventType::CameraBuffers,
            {static_cast<double>(valid_mask),
             static_cast<double>(viture_frame_hub_consumer_count()),
             static_cast<double>(width), static_cast<double>(height)}));
    }
    if (viture_frame_hub_has_consumers()) {
        viture_frame_hub_publish(
            reinterpret_cast<unsigned char*>(left0),
            reinterpret_cast<unsigned char*>(right0),
            reinterpret_cast<unsigned char*>(left1),
            reinterpret_cast<unsigned char*>(right1),
            timestamp, width, height, sequence);
    }
    enqueue(x, numeric_event(EventType::Stereo,
        {static_cast<double>(width), static_cast<double>(height), timestamp,
         static_cast<double>(sequence)}));
}

void quaternion_to_euler_degrees(const float pose[7], float out[3]) {
    const double w = pose[3];
    const double x = pose[4];
    const double y = pose[5];
    const double z = pose[6];
    const double roll = std::atan2(2.0 * (w * x + y * z),
                                   1.0 - 2.0 * (x * x + y * y));
    const double pitch = std::asin(std::clamp(2.0 * (w * y - z * x), -1.0, 1.0));
    const double yaw = std::atan2(2.0 * (w * z + x * y),
                                  1.0 - 2.0 * (y * y + z * z));
    constexpr double to_degrees = 57.29577951308232;
    out[0] = static_cast<float>(roll * to_degrees);
    out[1] = static_cast<float>(pitch * to_degrees);
    out[2] = static_cast<float>(yaw * to_degrees);
}

bool usb_number(io_service_t service, const char* key, uint16_t& value) {
    CFStringRef key_string = CFStringCreateWithCString(
        kCFAllocatorDefault, key, kCFStringEncodingUTF8);
    if (!key_string) return false;
    CFTypeRef property = IORegistryEntryCreateCFProperty(
        service, key_string, kCFAllocatorDefault, 0);
    CFRelease(key_string);
    if (!property) return false;
    const bool ok = CFGetTypeID(property) == CFNumberGetTypeID() &&
                    CFNumberGetValue(static_cast<CFNumberRef>(property),
                                     kCFNumberShortType, &value);
    CFRelease(property);
    return ok;
}

std::vector<int> find_viture_products() {
    std::vector<int> products;
    CFMutableDictionaryRef match = IOServiceMatching(kIOUSBDeviceClassName);
    if (!match) return products;
    io_iterator_t iterator = 0;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, match, &iterator) != KERN_SUCCESS)
        return products;
    io_service_t service = 0;
    while ((service = IOIteratorNext(iterator))) {
        uint16_t vendor = 0;
        uint16_t product = 0;
        if (usb_number(service, kUSBVendorID, vendor) && vendor == 0x35CA &&
            usb_number(service, kUSBProductID, product) &&
            xr_device_provider_is_product_id_valid(product) &&
            std::find(products.begin(), products.end(), product) == products.end()) {
            products.push_back(product);
        }
        IOObjectRelease(service);
    }
    IOObjectRelease(iterator);
    return products;
}

void enqueue_initial_state(t_viture_ultra* x, XRDeviceProviderHandle handle) {
    const int brightness = xr_device_provider_get_brightness_level(handle);
    const int volume = xr_device_provider_get_volume_level(handle);
    const int display_mode = xr_device_provider_get_display_mode(handle);
    const int duty_cycle = xr_device_provider_get_duty_cycle(handle);
    float film = 0.0f;
    char firmware[128]{};
    int firmware_length = sizeof(firmware);
    if (brightness >= 0) enqueue(x, numeric_event(EventType::Brightness, {static_cast<double>(brightness)}));
    if (volume >= 0) enqueue(x, numeric_event(EventType::Volume, {static_cast<double>(volume)}));
    if (display_mode >= 0) enqueue(x, numeric_event(EventType::DisplayMode, {static_cast<double>(display_mode)}));
    if (duty_cycle >= 0) enqueue(x, numeric_event(EventType::DutyCycle, {static_cast<double>(duty_cycle)}));
    if (xr_device_provider_get_film_mode(handle, &film) == VITURE_GLASSES_SUCCESS)
        enqueue(x, numeric_event(EventType::Film, {film}));
    if (xr_device_provider_get_glasses_version(handle, firmware, &firmware_length) ==
        VITURE_GLASSES_SUCCESS)
        enqueue(x, text_event(EventType::Firmware, firmware));
}

void process_control_requests(t_viture_ultra* x, XRDeviceProviderHandle handle) {
    BridgeState& state = *x->state;

    int int_value = state.request_brightness.exchange(-1);
    if (int_value >= 0) {
        const int result = xr_device_provider_set_brightness_level(handle, int_value);
        enqueue(x, control_event("brightness", result, {static_cast<double>(int_value)}));
    }

    int_value = state.request_volume.exchange(-1);
    if (int_value >= 0) {
        const int result = xr_device_provider_set_volume_level(handle, int_value);
        enqueue(x, control_event("volume", result, {static_cast<double>(int_value)}));
    }

    const double film = state.request_film.exchange(-1.0);
    if (film >= 0.0) {
        const int result = xr_device_provider_set_film_mode(handle, static_cast<float>(film));
        enqueue(x, control_event("film", result, {film}));
    }

    int_value = state.request_duty_cycle.exchange(-1);
    if (int_value >= 0) {
        const int result = xr_device_provider_set_duty_cycle(handle, int_value);
        enqueue(x, control_event("dutycycle", result, {static_cast<double>(int_value)}));
    }

    int_value = state.request_display_mode.exchange(-1);
    if (int_value >= 0) {
        const int result = xr_device_provider_set_display_mode(handle, int_value);
        enqueue(x, control_event("displaymode", result, {static_cast<double>(int_value)}));
    }

    int_value = state.request_dimension.exchange(-1);
    if (int_value >= 0) {
        const int result = xr_device_provider_switch_dimension(handle, int_value);
        enqueue(x, control_event("dimension", result, {static_cast<double>(int_value)}));
    }

    if (state.request_auto_exposure.exchange(false)) {
        const int result = xr_device_provider_set_auto_exposure_carina(handle);
        enqueue(x, control_event("autoexposure", result));
    }

    if (state.request_manual_exposure.exchange(false)) {
        const double time_ms = state.exposure_time_ms.load();
        const int gain = state.exposure_gain.load();
        const int result = xr_device_provider_set_manual_exposure_carina(
            handle, static_cast<float>(time_ms), gain);
        enqueue(x, control_event("exposure", result,
            {time_ms, static_cast<double>(gain)}));
    }

    if (state.request_state.exchange(false)) {
        enqueue_initial_state(x, handle);
        enqueue(x, control_event("getstate", VITURE_GLASSES_SUCCESS));
    }
}

void release_active_instance(t_viture_ultra* x) {
    t_viture_ultra* expected = x;
    g_active_instance.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
}

void worker_main(t_viture_ultra* x) {
    BridgeState& state = *x->state;
    XRDeviceProviderHandle handle = nullptr;
    int product_id = 0;

    xr_device_provider_set_log_hook(sdk_log_callback);
    xr_device_provider_set_log_level(LOG_LEVEL_INFO);

    const auto products = find_viture_products();
    for (int candidate : products) {
        XRDeviceProviderHandle candidate_handle = xr_device_provider_create(candidate);
        if (!candidate_handle) continue;
        if (xr_device_provider_get_device_type(candidate_handle) == XR_DEVICE_TYPE_VITURE_CARINA) {
            handle = candidate_handle;
            product_id = candidate;
            break;
        }
        xr_device_provider_destroy(candidate_handle);
    }

    if (!handle) {
        enqueue(x, text_event(EventType::Error, "no_luma_ultra_found"));
        xr_device_provider_set_log_hook(nullptr);
        release_active_instance(x);
        return;
    }

    char market_name[64]{};
    int market_name_length = sizeof(market_name);
    if (xr_device_provider_get_market_name(product_id, market_name, &market_name_length) !=
        VITURE_GLASSES_SUCCESS) {
        std::snprintf(market_name, sizeof(market_name), "Luma Ultra");
    }

    const int dof_result = xr_device_provider_set_dof_type_carina(handle, 1);
    enqueue(x, control_event("dof", dof_result, {1.0}));

    int rc = xr_device_provider_initialize(handle, nullptr, nullptr);
    enqueue(x, control_event("initialize", rc));
    bool initialized = rc == VITURE_GLASSES_SUCCESS;
    if (initialized) {
        // The Carina VIO engine is created during initialize(). Register every
        // callback together afterwards and before start(). The VIO engine
        // captures the callback set at start time: registering only the camera
        // here and replacing the set after start preserves camera frames but
        // silently prevents the raw IMU and VSync callbacks from firing.
        const int state_callback_result =
            xr_device_provider_register_state_callback(handle, state_callback);
        enqueue(x, control_event("state_callback", state_callback_result));

        const int callback_result = xr_device_provider_register_callbacks_carina(
            handle, nullptr, vsync_callback, imu_callback, stereo_callback);
        enqueue(x, control_event("callbacks", callback_result));
        if (callback_result != VITURE_GLASSES_SUCCESS) {
            rc = callback_result;
        } else {
            rc = xr_device_provider_start(handle);
            enqueue(x, control_event("start", rc));
        }
    }
    const bool started = rc == VITURE_GLASSES_SUCCESS;
    if (!started) {
        char error[64];
        std::snprintf(error, sizeof(error), "sdk_start_error_%d", rc);
        enqueue(x, text_event(EventType::Error, error));
        if (initialized) xr_device_provider_shutdown(handle);
        xr_device_provider_destroy(handle);
        xr_device_provider_set_log_hook(nullptr);
        release_active_instance(x);
        return;
    }

    state.connected = true;
    enqueue(x, numeric_event(EventType::Connected, {1.0}));
    enqueue(x, text_event(EventType::DeviceName, market_name));
    enqueue(x, numeric_event(EventType::ProductId, {static_cast<double>(product_id)}));
    enqueue(x, text_event(EventType::SdkVersion, GetVersionString()));
    enqueue_initial_state(x, handle);

    using Clock = std::chrono::steady_clock;
    auto next_pose = Clock::now();
    auto next_heartbeat = Clock::now();
    int previous_status = -1;

    while (!state.stop.load(std::memory_order_acquire)) {
        process_control_requests(x, handle);
        if (state.request_reset.exchange(false)) {
            const int result = xr_device_provider_reset_pose_carina(handle);
            enqueue(x, numeric_event(EventType::Reset, {static_cast<double>(result)}));
        }
        if (state.request_recenter.exchange(false)) {
            float origin[7]{};
            int status = 1;
            int result = xr_device_provider_get_gl_pose_carina(handle, origin, 0.0, &status);
            if (result == VITURE_GLASSES_SUCCESS)
                result = xr_device_provider_reset_origin_carina(handle, origin);
            enqueue(x, numeric_event(EventType::Recentered, {static_cast<double>(result)}));
        }

        float pose[7]{};
        int status = 1;
        rc = xr_device_provider_get_gl_pose_carina(handle, pose, 0.0, &status);
        if (rc == VITURE_GLASSES_SUCCESS) {
            const double timestamp = monotonic_seconds();
            enqueue(x, numeric_event(EventType::Pose,
                {pose[0], pose[1], pose[2], pose[3], pose[4], pose[5], pose[6],
                 static_cast<double>(status), timestamp}));
            float euler[3]{};
            quaternion_to_euler_degrees(pose, euler);
            enqueue(x, numeric_event(EventType::Euler,
                {euler[0], euler[1], euler[2], timestamp}));
            if (status != previous_status) {
                enqueue(x, numeric_event(EventType::TrackingStable,
                    {status == 0 ? 1.0 : 0.0}));
                previous_status = status;
            }
        }

        const auto now = Clock::now();
        if (now >= next_heartbeat) {
            enqueue(x, numeric_event(EventType::Alive, {monotonic_seconds()}));
            next_heartbeat = now + std::chrono::seconds(1);
        }

        const double rate = std::clamp(state.rate_hz.load(), 1.0, 1000.0);
        const auto interval = std::chrono::duration<double>(1.0 / rate);
        next_pose += std::chrono::duration_cast<Clock::duration>(interval);
        if (next_pose < Clock::now()) next_pose = Clock::now();
        std::this_thread::sleep_until(next_pose);
    }

    xr_device_provider_stop(handle);
    xr_device_provider_shutdown(handle);
    xr_device_provider_destroy(handle);
    xr_device_provider_set_log_hook(nullptr);
    state.connected = false;
    enqueue(x, numeric_event(EventType::Connected, {0.0}));
    release_active_instance(x);
}

void stop_worker(t_viture_ultra* x) {
    if (!x || !x->state) return;
    BridgeState& state = *x->state;
    state.stop = true;
    if (state.worker.joinable()) state.worker.join();
    release_active_instance(x);
}

void connect_method(t_viture_ultra* x) {
    if (!x || !x->state) return;
    BridgeState& state = *x->state;
    if (state.worker.joinable()) {
        if (state.connected) {
            object_post(reinterpret_cast<t_object*>(x), "already connected");
            return;
        }
        state.worker.join();
    }

    t_viture_ultra* expected = nullptr;
    if (!g_active_instance.compare_exchange_strong(expected, x, std::memory_order_acq_rel)) {
        object_error(reinterpret_cast<t_object*>(x),
                     "another viture.ultra object owns the SDK connection");
        return;
    }
    state.stop = false;
    state.stereo_sequence = 0;
    state.worker = std::thread(worker_main, x);
}

void disconnect_method(t_viture_ultra* x) {
    stop_worker(x);
}

void recenter_method(t_viture_ultra* x) {
    if (x && x->state) x->state->request_recenter = true;
}

void reset_method(t_viture_ultra* x) {
    if (x && x->state) x->state->request_reset = true;
}

void getstate_method(t_viture_ultra* x) {
    if (x && x->state) x->state->request_state = true;
}

void brightness_method(t_viture_ultra* x, long level) {
    if (!x || !x->state) return;
    if (level < 0 || level > 8) {
        object_error(reinterpret_cast<t_object*>(x), "brightness must be 0–8");
        return;
    }
    x->state->request_brightness = static_cast<int>(level);
}

void volume_method(t_viture_ultra* x, long level) {
    if (!x || !x->state) return;
    if (level < 0 || level > 8) {
        object_error(reinterpret_cast<t_object*>(x), "volume must be 0–8");
        return;
    }
    x->state->request_volume = static_cast<int>(level);
}

void film_method(t_viture_ultra* x, double value) {
    if (!x || !x->state) return;
    if (value < 0.0 || value > 1.0) {
        object_error(reinterpret_cast<t_object*>(x), "film must be 0.0–1.0");
        return;
    }
    x->state->request_film = value;
}

void dutycycle_method(t_viture_ultra* x, long value) {
    if (!x || !x->state) return;
    if (value < 0 || value > 100) {
        object_error(reinterpret_cast<t_object*>(x), "dutycycle must be 0–100");
        return;
    }
    x->state->request_duty_cycle = static_cast<int>(value);
}

int display_mode_from_symbol(const char* name) {
    struct NamedMode { const char* name; int value; };
    static constexpr NamedMode modes[] = {
        {"1080p60", VITURE_DISPLAY_MODE_1920_1080_60HZ},
        {"sbs1080p60", VITURE_DISPLAY_MODE_3840_1080_60HZ},
        {"1080p90", VITURE_DISPLAY_MODE_1920_1080_90HZ},
        {"1080p120", VITURE_DISPLAY_MODE_1920_1080_120HZ},
        {"sbs1080p90", VITURE_DISPLAY_MODE_3840_1080_90HZ},
        {"1200p60", VITURE_DISPLAY_MODE_1920_1200_60HZ},
        {"sbs1200p60", VITURE_DISPLAY_MODE_3840_1200_60HZ},
        {"1200p90", VITURE_DISPLAY_MODE_1920_1200_90HZ},
        {"1200p120", VITURE_DISPLAY_MODE_1920_1200_120HZ},
        {"sbs1200p90", VITURE_DISPLAY_MODE_3840_1200_90HZ},
    };
    for (const auto& mode : modes)
        if (std::strcmp(name, mode.name) == 0) return mode.value;
    return -1;
}

void displaymode_method(t_viture_ultra* x, t_symbol*, long argc, t_atom* argv) {
    if (!x || !x->state || argc < 1) {
        if (x) object_error(reinterpret_cast<t_object*>(x), "displaymode requires a mode name or SDK value");
        return;
    }
    int mode = -1;
    if (atom_gettype(argv) == A_SYM)
        mode = display_mode_from_symbol(atom_getsym(argv)->s_name);
    else if (atom_gettype(argv) == A_LONG || atom_gettype(argv) == A_FLOAT)
        mode = static_cast<int>(atom_getlong(argv));
    if (mode < 0) {
        object_error(reinterpret_cast<t_object*>(x),
            "unknown display mode; use 1080p60, sbs1080p60, 1080p90, 1080p120, "
            "sbs1080p90, 1200p60, sbs1200p60, 1200p90, 1200p120, or sbs1200p90");
        return;
    }
    x->state->request_display_mode = mode;
}

void dimension_method(t_viture_ultra* x, long is_3d) {
    if (!x || !x->state) return;
    if (is_3d != 0 && is_3d != 1) {
        object_error(reinterpret_cast<t_object*>(x), "dimension must be 0 (2D) or 1 (3D)");
        return;
    }
    x->state->request_dimension = static_cast<int>(is_3d);
}

void autoexposure_method(t_viture_ultra* x) {
    if (x && x->state) x->state->request_auto_exposure = true;
}

void exposure_method(t_viture_ultra* x, t_symbol*, long argc, t_atom* argv) {
    if (!x || !x->state || argc < 2) {
        if (x) object_error(reinterpret_cast<t_object*>(x), "exposure requires time_ms and gain");
        return;
    }
    const double time_ms = atom_getfloat(argv);
    const long gain = atom_getlong(argv + 1);
    if (time_ms < 0.01 || time_ms > 8.0 || gain < 0 || gain > 15) {
        object_error(reinterpret_cast<t_object*>(x),
                     "exposure ranges: time 0.01–8.0 ms, gain 0–15");
        return;
    }
    x->state->exposure_time_ms = time_ms;
    x->state->exposure_gain = static_cast<int>(gain);
    x->state->request_manual_exposure = true;
}

void rate_method(t_viture_ultra* x, double rate) {
    if (!x || !x->state) return;
    const double clamped = std::clamp(rate, 1.0, 1000.0);
    x->state->rate_hz = clamped;
    output_event(x, numeric_event(EventType::Rate, {clamped}));
}

void bang_method(t_viture_ultra* x) {
    if (!x || !x->state) return;
    output_event(x, numeric_event(EventType::Connected,
        {x->state->connected ? 1.0 : 0.0}));
    output_event(x, numeric_event(EventType::Rate, {x->state->rate_hz.load()}));
}

void assist_method(t_viture_ultra*, void*, long message, long, char* text) {
    if (message == ASSIST_INLET)
        std::snprintf(text, 512, "Connection, tracking, display, film, volume and exposure controls");
    else
        std::snprintf(text, 512, "VITURE messages: pose, euler, imu, vsync, stereo, state...");
}

void free_method(t_viture_ultra* x) {
    stop_worker(x);
    if (x->qelem) qelem_free(x->qelem);
    delete x->state;
    x->state = nullptr;
}

void* new_method(t_symbol*, long argc, t_atom* argv) {
    auto* x = static_cast<t_viture_ultra*>(object_alloc(g_viture_ultra_class));
    if (!x) return nullptr;
    x->state = new BridgeState();
    x->outlet = outlet_new(x, nullptr);
    x->qelem = qelem_new(x, reinterpret_cast<method>(qelem_output));
    if (argc > 0 && (atom_gettype(argv) == A_LONG || atom_gettype(argv) == A_FLOAT))
        x->state->rate_hz = std::clamp(atom_getfloat(argv), 1.0, 1000.0);
    return x;
}

}  // namespace

void ext_main(void*) {
    t_class* klass = class_new("viture.ultra",
        reinterpret_cast<method>(new_method), reinterpret_cast<method>(free_method),
        sizeof(t_viture_ultra), nullptr, A_GIMME, 0);
    class_addmethod(klass, reinterpret_cast<method>(connect_method), "connect", 0);
    class_addmethod(klass, reinterpret_cast<method>(disconnect_method), "disconnect", 0);
    class_addmethod(klass, reinterpret_cast<method>(recenter_method), "recenter", 0);
    class_addmethod(klass, reinterpret_cast<method>(reset_method), "reset", 0);
    class_addmethod(klass, reinterpret_cast<method>(getstate_method), "getstate", 0);
    class_addmethod(klass, reinterpret_cast<method>(brightness_method), "brightness", A_LONG, 0);
    class_addmethod(klass, reinterpret_cast<method>(volume_method), "volume", A_LONG, 0);
    class_addmethod(klass, reinterpret_cast<method>(film_method), "film", A_FLOAT, 0);
    class_addmethod(klass, reinterpret_cast<method>(dutycycle_method), "dutycycle", A_LONG, 0);
    class_addmethod(klass, reinterpret_cast<method>(displaymode_method), "displaymode", A_GIMME, 0);
    class_addmethod(klass, reinterpret_cast<method>(dimension_method), "dimension", A_LONG, 0);
    class_addmethod(klass, reinterpret_cast<method>(autoexposure_method), "autoexposure", 0);
    class_addmethod(klass, reinterpret_cast<method>(exposure_method), "exposure", A_GIMME, 0);
    class_addmethod(klass, reinterpret_cast<method>(rate_method), "rate", A_FLOAT, 0);
    class_addmethod(klass, reinterpret_cast<method>(bang_method), "bang", 0);
    class_addmethod(klass, reinterpret_cast<method>(assist_method), "assist", A_CANT, 0);
    class_register(CLASS_BOX, klass);
    g_viture_ultra_class = klass;
}

#include "ext.h"
#include "ext_obex.h"
#include "jit.common.h"
#include "jit.max.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

#include "viture_frame_hub.h"

namespace {

constexpr int kOutputPlaneCount = 2;
constexpr int kSdkPlaneCount = 4;

struct StereoState {
    std::array<std::vector<unsigned char>, kSdkPlaneCount> planes;
    uint64_t last_sequence = 0;
    std::atomic<long> interval_ms{20};
    std::atomic<bool> bang_pending{false};
    std::atomic<bool> running{false};
};

typedef struct _jit_viture_stereo {
    t_object object;
    void* obex = nullptr;
    void* matrix_outlets[kOutputPlaneCount]{};
    void* metadata_outlet = nullptr;
    void* matrices[kOutputPlaneCount]{};
    t_symbol* matrix_names[kOutputPlaneCount]{};
    t_clock* clock = nullptr;
    t_qelem* output_qelem = nullptr;
    StereoState* state = nullptr;
} t_jit_viture_stereo;

t_class* g_class = nullptr;

bool configure_matrix(void* matrix, long width, long height) {
    if (!matrix || width <= 0 || height <= 0) return false;
    const long save_lock = reinterpret_cast<long>(
        jit_object_method(matrix, _jit_sym_lock, reinterpret_cast<void*>(1)));

    t_jit_matrix_info current{};
    jit_object_method(matrix, _jit_sym_getinfo, &current);
    if (current.type != _jit_sym_char || current.planecount != 1 ||
        current.dimcount != 2 || current.dim[0] != width || current.dim[1] != height) {
        t_jit_matrix_info desired{};
        jit_matrix_info_default(&desired);
        desired.type = _jit_sym_char;
        desired.planecount = 1;
        desired.dimcount = 2;
        desired.dim[0] = width;
        desired.dim[1] = height;
        desired.flags = 0;
        const auto error = reinterpret_cast<t_jit_err>(
            jit_object_method(matrix, _jit_sym_setinfo, &desired));
        if (error != JIT_ERR_NONE) {
            jit_object_method(matrix, _jit_sym_lock, reinterpret_cast<void*>(save_lock));
            return false;
        }
    }

    jit_object_method(matrix, _jit_sym_lock, reinterpret_cast<void*>(save_lock));
    return true;
}

bool copy_plane_to_matrix(void* matrix, const unsigned char* source,
                          long width, long height) {
    if (!configure_matrix(matrix, width, height)) return false;
    const long save_lock = reinterpret_cast<long>(
        jit_object_method(matrix, _jit_sym_lock, reinterpret_cast<void*>(1)));

    t_jit_matrix_info info{};
    unsigned char* destination = nullptr;
    jit_object_method(matrix, _jit_sym_getinfo, &info);
    jit_object_method(matrix, _jit_sym_getdata, &destination);
    if (!destination || info.dimstride[1] < width) {
        jit_object_method(matrix, _jit_sym_lock, reinterpret_cast<void*>(save_lock));
        return false;
    }

    for (long row = 0; row < height; ++row) {
        std::memcpy(destination + row * info.dimstride[1], source + row * width,
                    static_cast<size_t>(width));
    }
    jit_object_method(matrix, _jit_sym_lock, reinterpret_cast<void*>(save_lock));
    return true;
}

void output_matrix(void* outlet, t_symbol* name) {
    t_atom atom;
    atom_setsym(&atom, name);
    outlet_anything(outlet, _jit_sym_jit_matrix, 1, &atom);
}

void output_frame(t_jit_viture_stereo* x, const VitureFrameInfo& info) {
    t_atom atoms[5];
    atom_setfloat(atoms, info.timestamp);
    atom_setlong(atoms + 1, info.width);
    atom_setlong(atoms + 2, info.height);
    atom_setlong(atoms + 3, static_cast<t_atom_long>(info.sequence));
    atom_setlong(atoms + 4, info.valid_mask);
    outlet_anything(x->metadata_outlet, gensym("frame"), 5, atoms);

    // Jitter convention: send rightmost data first so left is the final trigger.
    if (info.valid_mask & 2)
        output_matrix(x->matrix_outlets[1], x->matrix_names[1]);
    if (info.valid_mask & 1)
        output_matrix(x->matrix_outlets[0], x->matrix_names[0]);

}

bool fetch_and_output(t_jit_viture_stereo* x, bool allow_same_frame) {
    if (!x || !x->state) return false;
    VitureFrameInfo info{};
    if (!viture_frame_hub_get_latest_info(&info) || info.plane_bytes == 0) return false;

    for (auto& plane : x->state->planes) plane.resize(info.plane_bytes);
    const uint64_t after = allow_same_frame ? 0 : x->state->last_sequence;
    const int result = viture_frame_hub_copy_latest(
        after,
        x->state->planes[0].data(), x->state->planes[1].data(),
        x->state->planes[2].data(), x->state->planes[3].data(),
        info.plane_bytes, &info);
    if (result <= 0) return false;

    for (int index = 0; index < kOutputPlaneCount; ++index) {
        if (!(info.valid_mask & (1 << index))) continue;
        if (!copy_plane_to_matrix(x->matrices[index], x->state->planes[index].data(),
                                  info.width, info.height)) {
            object_error(reinterpret_cast<t_object*>(x),
                         "could not update Jitter matrix %d", index);
            return false;
        }
    }
    x->state->last_sequence = info.sequence;
    output_frame(x, info);
    return true;
}

void tick(t_jit_viture_stereo* x) {
    if (!x || !x->state || !x->state->running.load()) return;
    // Scheduler clocks may run in Max's interrupt context. Only request a
    // low-priority task here; all Jitter matrix work and outlet output happens
    // in output_qelem_method(), where UI objects such as jit.pwindow may draw.
    clock_delay(x->clock, x->state->interval_ms.load());
    qelem_set(x->output_qelem);
}

void output_qelem_method(t_jit_viture_stereo* x) {
    if (!x || !x->state) return;
    const bool allow_same_frame = x->state->bang_pending.exchange(false);
    if (!x->state->running.load() && !allow_same_frame) return;
    fetch_and_output(x, allow_same_frame);
}

void start_method(t_jit_viture_stereo* x) {
    if (!x || !x->state) return;
    if (!x->state->running.load()) {
        x->state->running.store(true);
        viture_frame_hub_add_consumer();
    }
    // `start` is also an explicit restart if a scheduler chain was interrupted.
    clock_unset(x->clock);
    clock_delay(x->clock, 0);
}

void stop_method(t_jit_viture_stereo* x) {
    if (!x || !x->state || !x->state->running.load()) return;
    x->state->running.store(false);
    clock_unset(x->clock);
    if (x->output_qelem) qelem_unset(x->output_qelem);
    viture_frame_hub_remove_consumer();
}

void bang_method(t_jit_viture_stereo* x) {
    if (!x || !x->state || !x->output_qelem) return;
    x->state->bang_pending.store(true);
    qelem_set(x->output_qelem);
    if (x->state->running.load()) {
        // A manual bang recovers automatic output as well as producing the
        // newest frame immediately.
        clock_unset(x->clock);
        clock_delay(x->clock, x->state->interval_ms.load());
    }
}

void interval_method(t_jit_viture_stereo* x, long milliseconds) {
    if (!x || !x->state) return;
    if (milliseconds < 1 || milliseconds > 1000) {
        object_error(reinterpret_cast<t_object*>(x), "interval must be 1–1000 ms");
        return;
    }
    x->state->interval_ms.store(milliseconds);
}

void status_method(t_jit_viture_stereo* x) {
    if (!x || !x->state) return;
    VitureFrameInfo info{};
    const bool has_frame = viture_frame_hub_get_latest_info(&info) != 0;
    t_atom atoms[7];
    atom_setlong(atoms, x->state->running.load() ? 1 : 0);
    atom_setlong(atoms + 1, viture_frame_hub_consumer_count());
    atom_setlong(atoms + 2, has_frame ? 1 : 0);
    atom_setlong(atoms + 3, has_frame ? info.width : 0);
    atom_setlong(atoms + 4, has_frame ? info.height : 0);
    atom_setlong(atoms + 5, has_frame ? static_cast<t_atom_long>(info.sequence) : 0);
    atom_setlong(atoms + 6, has_frame ? info.valid_mask : 0);
    outlet_anything(x->metadata_outlet, gensym("status"), 7, atoms);
}

void assist_method(t_jit_viture_stereo*, void*, long message, long index, char* text) {
    if (message == ASSIST_INLET) {
        std::snprintf(text, 512, "start, stop, bang, status, interval <ms>");
        return;
    }
    static const char* labels[] = {
        "left: 1-plane char Jitter matrix",
        "right: 1-plane char Jitter matrix",
        "frame: timestamp width height sequence valid_mask",
    };
    std::snprintf(text, 512, "%s", labels[std::clamp<long>(index, 0, 2)]);
}

void free_method(t_jit_viture_stereo* x) {
    stop_method(x);
    if (x->output_qelem) {
        qelem_free(x->output_qelem);
        x->output_qelem = nullptr;
    }
    if (x->clock) object_free(x->clock);
    for (void*& matrix : x->matrices) {
        if (matrix) {
            jit_object_unregister(matrix);
            jit_object_free(matrix);
            matrix = nullptr;
        }
    }
    delete x->state;
    x->state = nullptr;
    max_jit_object_free(x);
}

void* new_method(t_symbol*, long argc, t_atom* argv) {
    auto* x = static_cast<t_jit_viture_stereo*>(
        max_jit_object_alloc(g_class, gensym("jit_viture_stereo")));
    if (!x) return nullptr;

    for (int index = 0; index < kOutputPlaneCount; ++index) {
        x->matrix_outlets[index] = nullptr;
        x->matrices[index] = nullptr;
        x->matrix_names[index] = nullptr;
    }
    x->metadata_outlet = nullptr;
    x->clock = nullptr;
    x->output_qelem = nullptr;
    x->state = new StereoState();
    if (argc > 0 && (atom_gettype(argv) == A_LONG || atom_gettype(argv) == A_FLOAT))
        x->state->interval_ms.store(std::clamp<long>(atom_getlong(argv), 1, 1000));

    // Outlets are constructed right-to-left.
    x->metadata_outlet = outlet_new(x, nullptr);
    max_jit_obex_dumpout_set(x, x->metadata_outlet);
    x->matrix_outlets[1] = outlet_new(x, nullptr);
    x->matrix_outlets[0] = outlet_new(x, nullptr);

    t_jit_matrix_info info{};
    jit_matrix_info_default(&info);
    info.type = _jit_sym_char;
    info.planecount = 1;
    info.dimcount = 2;
    info.dim[0] = 1;
    info.dim[1] = 1;
    for (int index = 0; index < kOutputPlaneCount; ++index) {
        x->matrix_names[index] = jit_symbol_unique();
        x->matrices[index] = jit_object_new(_jit_sym_jit_matrix, &info);
        if (!x->matrices[index]) {
            object_error(reinterpret_cast<t_object*>(x), "could not allocate Jitter matrices");
            object_free(reinterpret_cast<t_object*>(x));
            return nullptr;
        }
        x->matrices[index] = jit_object_register(x->matrices[index], x->matrix_names[index]);
    }

    x->clock = clock_new(x, reinterpret_cast<method>(tick));
    if (!x->clock) {
        object_error(reinterpret_cast<t_object*>(x), "could not allocate polling clock");
        object_free(reinterpret_cast<t_object*>(x));
        return nullptr;
    }
    x->output_qelem = static_cast<t_qelem*>(
        qelem_new(x, reinterpret_cast<method>(output_qelem_method)));
    if (!x->output_qelem) {
        object_error(reinterpret_cast<t_object*>(x), "could not allocate output qelem");
        object_free(reinterpret_cast<t_object*>(x));
        return nullptr;
    }
    start_method(x);
    return x;
}

}  // namespace

void ext_main(void*) {
    t_class* klass = class_new("jit.viture.stereo",
        reinterpret_cast<method>(new_method), reinterpret_cast<method>(free_method),
        sizeof(t_jit_viture_stereo), nullptr, A_GIMME, 0);
    max_jit_class_obex_setup(klass, calcoffset(t_jit_viture_stereo, obex));
    max_jit_class_wrap_standard(klass, nullptr, 0);
    class_addmethod(klass, reinterpret_cast<method>(start_method), "start", 0);
    class_addmethod(klass, reinterpret_cast<method>(stop_method), "stop", 0);
    class_addmethod(klass, reinterpret_cast<method>(bang_method), "bang", 0);
    class_addmethod(klass, reinterpret_cast<method>(interval_method), "interval", A_LONG, 0);
    class_addmethod(klass, reinterpret_cast<method>(status_method), "status", 0);
    class_addmethod(klass, reinterpret_cast<method>(assist_method), "assist", A_CANT, 0);
    class_register(CLASS_BOX, klass);
    g_class = klass;
}

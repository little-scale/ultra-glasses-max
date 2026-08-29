{
  "patcher": {
    "fileversion": 1,
    "appversion": { "major": 9, "minor": 1, "revision": 4, "architecture": "arm64" },
    "classnamespace": "box",
    "rect": [80.0, 80.0, 1240.0, 760.0],
    "openinpresentation": 0,
    "default_fontsize": 12.0,
    "default_fontface": 0,
    "default_fontname": "Arial",
    "boxes": [
      { "box": { "id": "title", "maxclass": "comment", "text": "jit.viture.stereo — Luma Ultra tracking cameras as Jitter matrices", "fontsize": 20.0, "patching_rect": [30.0, 20.0, 700.0, 30.0] } },
      { "box": { "id": "intro", "maxclass": "comment", "text": "Connect the hardware through viture.ultra. The Jitter object starts listening automatically and does not open a second SDK connection.", "patching_rect": [30.0, 55.0, 850.0, 22.0] } },
      { "box": { "id": "connect", "maxclass": "message", "text": "connect", "patching_rect": [30.0, 95.0, 65.0, 22.0] } },
      { "box": { "id": "disconnect", "maxclass": "message", "text": "disconnect", "patching_rect": [105.0, 95.0, 78.0, 22.0] } },
      { "box": { "id": "sensor", "maxclass": "newobj", "text": "viture.ultra 120", "patching_rect": [30.0, 135.0, 110.0, 22.0] } },
      { "box": { "id": "sensor-print", "maxclass": "newobj", "text": "print viture.ultra", "patching_rect": [30.0, 170.0, 105.0, 22.0] } },
      { "box": { "id": "sensor-route", "maxclass": "newobj", "text": "route control error camera_buffers sdklog", "patching_rect": [150.0, 170.0, 265.0, 22.0] } },
      { "box": { "id": "diagnostic-label", "maxclass": "comment", "text": "startup diagnostics — SDK result codes should be 0", "patching_rect": [430.0, 170.0, 310.0, 22.0] } },
      { "box": { "id": "control-diagnostic", "maxclass": "message", "text": "", "patching_rect": [150.0, 200.0, 245.0, 22.0] } },
      { "box": { "id": "error-diagnostic", "maxclass": "message", "text": "", "patching_rect": [405.0, 200.0, 245.0, 22.0] } },
      { "box": { "id": "camera-diagnostic", "maxclass": "message", "text": "", "patching_rect": [660.0, 200.0, 245.0, 22.0] } },
      { "box": { "id": "log-diagnostic", "maxclass": "message", "text": "", "patching_rect": [915.0, 200.0, 275.0, 22.0] } },

      { "box": { "id": "start", "maxclass": "message", "text": "start", "patching_rect": [250.0, 95.0, 42.0, 22.0] } },
      { "box": { "id": "stop", "maxclass": "message", "text": "stop", "patching_rect": [302.0, 95.0, 40.0, 22.0] } },
      { "box": { "id": "bang", "maxclass": "button", "patching_rect": [352.0, 94.0, 24.0, 24.0] } },
      { "box": { "id": "interval", "maxclass": "message", "text": "interval 20", "patching_rect": [400.0, 95.0, 72.0, 22.0] } },
      { "box": { "id": "status", "maxclass": "message", "text": "status", "patching_rect": [482.0, 95.0, 48.0, 22.0] } },
      { "box": { "id": "stereo", "maxclass": "newobj", "text": "jit.viture.stereo", "patching_rect": [250.0, 135.0, 118.0, 22.0] } },
      { "box": { "id": "metadata", "maxclass": "message", "text": "", "patching_rect": [830.0, 135.0, 360.0, 22.0] } },
      { "box": { "id": "metadata-label", "maxclass": "comment", "text": "frame: timestamp width height sequence valid_mask", "patching_rect": [830.0, 165.0, 340.0, 22.0] } },

      { "box": { "id": "left-label", "maxclass": "comment", "text": "left tracking camera", "fontsize": 15.0, "patching_rect": [30.0, 240.0, 180.0, 22.0] } },
      { "box": { "id": "left", "maxclass": "jit.pwindow", "patching_rect": [30.0, 270.0, 555.0, 350.0] } },
      { "box": { "id": "right-label", "maxclass": "comment", "text": "right tracking camera", "fontsize": 15.0, "patching_rect": [620.0, 240.0, 180.0, 22.0] } },
      { "box": { "id": "right", "maxclass": "jit.pwindow", "patching_rect": [620.0, 270.0, 555.0, 350.0] } },

      { "box": { "id": "note1", "maxclass": "comment", "text": "The first two outlets are the Luma Ultra's synchronized left/right 1-plane 8-bit tracking-camera matrices.", "patching_rect": [30.0, 655.0, 850.0, 22.0] } },
      { "box": { "id": "note2", "maxclass": "comment", "text": "bang repeats the latest frame; start and stop control automatic low-priority output.", "patching_rect": [30.0, 685.0, 850.0, 22.0] } },
      { "box": { "id": "note3", "maxclass": "comment", "text": "Camera copying is enabled only while a jit.viture.stereo object is running, so tracking-only patches keep the previous low overhead.", "patching_rect": [30.0, 715.0, 900.0, 22.0] } }
    ],
    "lines": [
      { "patchline": { "source": ["connect", 0], "destination": ["sensor", 0] } },
      { "patchline": { "source": ["disconnect", 0], "destination": ["sensor", 0] } },
      { "patchline": { "source": ["sensor", 0], "destination": ["sensor-print", 0] } },
      { "patchline": { "source": ["sensor", 0], "destination": ["sensor-route", 0] } },
      { "patchline": { "source": ["sensor-route", 0], "destination": ["control-diagnostic", 1] } },
      { "patchline": { "source": ["sensor-route", 1], "destination": ["error-diagnostic", 1] } },
      { "patchline": { "source": ["sensor-route", 2], "destination": ["camera-diagnostic", 1] } },
      { "patchline": { "source": ["sensor-route", 3], "destination": ["log-diagnostic", 1] } },
      { "patchline": { "source": ["start", 0], "destination": ["stereo", 0] } },
      { "patchline": { "source": ["stop", 0], "destination": ["stereo", 0] } },
      { "patchline": { "source": ["bang", 0], "destination": ["stereo", 0] } },
      { "patchline": { "source": ["interval", 0], "destination": ["stereo", 0] } },
      { "patchline": { "source": ["status", 0], "destination": ["stereo", 0] } },
      { "patchline": { "source": ["stereo", 0], "destination": ["left", 0] } },
      { "patchline": { "source": ["stereo", 1], "destination": ["right", 0] } },
      { "patchline": { "source": ["stereo", 2], "destination": ["metadata", 1] } }
    ]
  }
}

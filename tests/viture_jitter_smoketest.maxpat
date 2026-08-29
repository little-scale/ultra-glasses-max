{
  "patcher": {
    "fileversion": 1,
    "appversion": { "major": 9, "minor": 1, "revision": 4, "architecture": "arm64" },
    "classnamespace": "box",
    "rect": [120.0, 120.0, 700.0, 420.0],
    "boxes": [
      { "box": { "id": "loadbang", "maxclass": "newobj", "text": "loadbang", "patching_rect": [30.0, 30.0, 60.0, 22.0] } },
      { "box": { "id": "delay", "maxclass": "newobj", "text": "delay 500", "patching_rect": [30.0, 70.0, 65.0, 22.0] } },
      { "box": { "id": "connect", "maxclass": "message", "text": "connect", "patching_rect": [30.0, 110.0, 60.0, 22.0] } },
      { "box": { "id": "status-delay", "maxclass": "newobj", "text": "delay 2500", "patching_rect": [180.0, 70.0, 72.0, 22.0] } },
      { "box": { "id": "status", "maxclass": "message", "text": "status", "patching_rect": [180.0, 110.0, 48.0, 22.0] } },
      { "box": { "id": "metro-on", "maxclass": "newobj", "text": "loadmess 1", "patching_rect": [280.0, 30.0, 72.0, 22.0] } },
      { "box": { "id": "metro", "maxclass": "newobj", "text": "metro 1000", "patching_rect": [280.0, 70.0, 72.0, 22.0] } },
      { "box": { "id": "sensor", "maxclass": "newobj", "text": "viture.ultra 120", "patching_rect": [30.0, 150.0, 110.0, 22.0] } },
      { "box": { "id": "sensor-route", "maxclass": "newobj", "text": "route stereo camera_buffers", "patching_rect": [30.0, 190.0, 165.0, 22.0] } },
      { "box": { "id": "sensor-speed", "maxclass": "newobj", "text": "speedlim 1000", "patching_rect": [30.0, 230.0, 92.0, 22.0] } },
      { "box": { "id": "sensor-print", "maxclass": "newobj", "text": "print VITURE_STEREO", "patching_rect": [30.0, 270.0, 135.0, 22.0] } },
      { "box": { "id": "buffers-print", "maxclass": "newobj", "text": "print VITURE_CAMERA_BUFFERS", "patching_rect": [180.0, 230.0, 195.0, 22.0] } },
      { "box": { "id": "stereo", "maxclass": "newobj", "text": "jit.viture.stereo", "patching_rect": [280.0, 150.0, 118.0, 22.0] } },
      { "box": { "id": "frame-speed", "maxclass": "newobj", "text": "speedlim 1000", "patching_rect": [530.0, 190.0, 92.0, 22.0] } },
      { "box": { "id": "frame-print", "maxclass": "newobj", "text": "print VITURE_JITTER_FRAME", "patching_rect": [500.0, 230.0, 175.0, 22.0] } },
      { "box": { "id": "note", "maxclass": "comment", "text": "Temporary automated hardware smoke test", "patching_rect": [30.0, 300.0, 260.0, 22.0] } }
    ],
    "lines": [
      { "patchline": { "source": ["loadbang", 0], "destination": ["delay", 0] } },
      { "patchline": { "source": ["loadbang", 0], "destination": ["status-delay", 0] } },
      { "patchline": { "source": ["metro-on", 0], "destination": ["metro", 0] } },
      { "patchline": { "source": ["metro", 0], "destination": ["status", 0] } },
      { "patchline": { "source": ["delay", 0], "destination": ["connect", 0] } },
      { "patchline": { "source": ["connect", 0], "destination": ["sensor", 0] } },
      { "patchline": { "source": ["status-delay", 0], "destination": ["status", 0] } },
      { "patchline": { "source": ["status", 0], "destination": ["stereo", 0] } },
      { "patchline": { "source": ["sensor", 0], "destination": ["sensor-route", 0] } },
      { "patchline": { "source": ["sensor-route", 0], "destination": ["sensor-speed", 0] } },
      { "patchline": { "source": ["sensor-route", 1], "destination": ["buffers-print", 0] } },
      { "patchline": { "source": ["sensor-speed", 0], "destination": ["sensor-print", 0] } },
      { "patchline": { "source": ["stereo", 2], "destination": ["frame-speed", 0] } },
      { "patchline": { "source": ["frame-speed", 0], "destination": ["frame-print", 0] } }
    ]
  }
}

# Changelog

All notable changes to this project will be documented here.

## [0.4.1] - 2026-08-30

### Added

- Native `viture.ultra` Max 9 external for Luma Ultra 6DoF pose, Euler angles,
  raw IMU, VSync, tracking state and device information.
- Display brightness, volume, electrochromic-film, duty-cycle, display-mode,
  2D/3D, recenter, reset and camera-exposure controls.
- `jit.viture.stereo` Jitter external for synchronized left/right grayscale
  tracking-camera matrices.
- Demand-driven shared frame transport so camera buffers are copied only while
  a Jitter consumer is active.
- Bounded callback queue and low-priority Max/Jitter output scheduling.
- Max help patches, hardware diagnostics and frame-hub tests.

### Fixed

- Register raw IMU, VSync and stereo callbacks together before SDK start so all
  streams remain active.
- Move Jitter matrix output out of scheduler interrupt context.
- Handle the two-camera `valid_mask 3` stream without exposing permanently
  empty matrix outlets.

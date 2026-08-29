# Ultra Glasses Max

Native Apple-silicon externals for using Luma Ultra XR glasses in Cycling '74
Max 9. The package exposes 6DoF pose, raw IMU and device controls through a Max
object, plus the synchronized grayscale tracking cameras as Jitter matrices.

This is an independent community project. It is not affiliated with, endorsed
by or certified by VITURE Inc. VITURE and Luma Ultra are trademarks of their
respective owner.

## Objects

### `viture.ultra`

Connects to the glasses and outputs:

- `pose px py pz qw qx qy qz status timestamp`
- `euler roll pitch yaw timestamp` in degrees
- `imu ax ay az gx gy gz timestamp`
- `vsync timestamp`
- tracking, connection, device-state and diagnostic messages

It also accepts controls for recentering, display mode, brightness, volume,
electrochromic film, duty cycle and tracking-camera exposure. Only one
`viture.ultra` object can own the SDK connection at a time.

### `jit.viture.stereo`

Shares the active `viture.ultra` connection and outputs the synchronized
640 x 480 grayscale left and right tracking cameras as one-plane 8-bit Jitter
matrices. It does not open a second SDK connection.

The two additional camera pointers reserved by the SDK are not exposed because
the tested macOS Luma Ultra stream supplies only the left/right pair.

## Requirements

- macOS 13 or later
- Apple silicon Mac
- Max 9
- Luma Ultra glasses
- VITURE XR Glasses SDK 2.4.0 for macOS arm64
- CMake 3.25 or later and Xcode Command Line Tools

The VITURE SDK is proprietary and is not committed to this repository. Download
the **VITURE XR Glasses SDK for macOS arm64** from the
[VITURE Glasses SDK page](https://www.viture.com/developer/glasses-sdk/glasses),
then set `VITURE_SDK_ROOT` to the extracted directory.

## Quick start from source

```sh
git clone https://github.com/little-scale/ultra-glasses-max.git
cd ultra-glasses-max

export VITURE_SDK_ROOT="/absolute/path/to/VITURE_XR_Glasses_SDK_for_MacOS_arm64"
./scripts/build.sh
./scripts/install.sh
```

Restart Max, then open `viture.ultra.maxhelp` or
`jit.viture.stereo.maxhelp`.

## Build

```sh
export VITURE_SDK_ROOT="/absolute/path/to/VITURE_XR_Glasses_SDK_for_MacOS_arm64"
./scripts/build.sh
```

The official Cycling '74 `max-sdk-base` dependency is fetched at its pinned
revision. To use a local checkout instead:

```sh
export MAX_SDK_BASE_PATH="/absolute/path/to/max-sdk-base"
./scripts/build.sh
```

The assembled package is written to `build/release/package`.

## Install a local build

```sh
./scripts/install.sh
```

This installs the package into `~/Documents/Max 9/Packages/UltraGlassesMax`.
Restart Max after replacing an external because Max keeps native externals
loaded for the life of the process.

Open `viture.ultra.maxhelp` and `jit.viture.stereo.maxhelp` from Max's help
system for working patches and the complete message reference.

## Coordinate and data notes

- Pose uses OpenGL-style coordinates: +X right, +Y up, +Z backward.
- Position is in metres.
- Pose status is `0` when stable and `1` when unstable.
- Quaternion order is `qw qx qy qz`.
- The `imu` selector contains the SDK's raw acceleration and angular-velocity
  sample followed by its timestamp.
- Camera copying is demand-driven and occurs only while a running
  `jit.viture.stereo` consumer exists.

## Source distribution and licensing

GitHub releases are source-only. They do not contain VITURE headers, libraries
or compiled Max externals. Each user downloads the official SDK separately and
uses it to create a local package. That local package embeds the two required
SDK libraries, so the SDK does not need to remain installed for normal Max use.

Release instructions are in [docs/RELEASING.md](docs/RELEASING.md).

No open-source license has yet been selected for this project's own source.
Until one is added, all rights are reserved. See [LICENSE](LICENSE).

## Status

Version 0.4.1 has been hardware-tested on Luma Ultra with Max 9 on Apple
silicon. Pose, raw IMU, device controls and the left/right tracking-camera
matrices have been confirmed on hardware.

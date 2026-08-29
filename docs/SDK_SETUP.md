# SDK setup

Ultra Glasses Max requires the VITURE XR Glasses SDK for macOS arm64. The SDK
is proprietary and must be obtained directly from VITURE.

1. Download the macOS arm64 SDK from the
   [VITURE developer portal](https://www.viture.com/en-US/developer).
2. Extract it outside this repository.
3. Set `VITURE_SDK_ROOT` to the extracted directory.

The expected layout is:

```text
VITURE_XR_Glasses_SDK_for_MacOS_arm64/
├── include/
│   ├── viture_device_carina.h
│   ├── viture_glasses_provider.h
│   └── ...
└── aarch64/
    ├── libglasses.dylib
    └── libcarina_vio.dylib
```

Example:

```sh
export VITURE_SDK_ROOT="$HOME/Downloads/VITURE_XR_Glasses_SDK_for_MacOS_arm64"
./scripts/build.sh
```

Never commit the SDK headers, documentation, archives or libraries to this
repository. The source-hygiene check rejects the known binary names.

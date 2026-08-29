# Releasing

Ultra Glasses Max is distributed as source code. GitHub releases must not
contain VITURE SDK headers, libraries, documentation, extracted archives or
compiled packages containing SDK object code.

## Release checklist

1. Update the version in:
   - `CMakeLists.txt`
   - `package-info.json`
   - both external `CMakeLists.txt` files
2. Add the release notes to `CHANGELOG.md`.
3. Run `./scripts/check-source.sh`.
4. Run `./scripts/build.sh` with the supported SDK and test with Luma Ultra
   hardware.
5. Verify pose, raw IMU, both Jitter matrices and a representative device
   control.
6. Commit the release and create an annotated `vX.Y.Z` tag.
7. Push `main` and the tag.
8. Create a GitHub release from the tag. Use GitHub's automatically generated
   source ZIP and tarball; do not attach the local `build/` directory.

Release notes should link to the
[VITURE Glasses SDK page](https://www.viture.com/developer/glasses-sdk/glasses)
and include this minimal build sequence:

```sh
git clone https://github.com/little-scale/ultra-glasses-max.git
cd ultra-glasses-max
export VITURE_SDK_ROOT="/absolute/path/to/VITURE_XR_Glasses_SDK_for_MacOS_arm64"
./scripts/build.sh
./scripts/install.sh
```

The repository currently reserves all rights to its own source. If an
open-source licence is selected later, update `LICENSE`, `README.md` and the
release notes together.

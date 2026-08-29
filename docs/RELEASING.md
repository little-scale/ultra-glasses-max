# Releasing

## One-time repository setup

1. Create the GitHub repository named `ultra-glasses-max` without adding a
   README, license or `.gitignore` on GitHub.
2. Add the new repository as this checkout's `origin`.
3. Review the source-code license in `LICENSE`. It currently reserves all
   rights; replace it only if the copyright holder deliberately chooses another
   license.
4. Review `docs/BINARY_DISTRIBUTION.md` before attaching compiled binaries.

## Release checklist

1. Update the version in:
   - `CMakeLists.txt`
   - `package-info.json`
   - both external `CMakeLists.txt` files
2. Add the release notes to `CHANGELOG.md`.
3. Run `./scripts/check-source.sh`.
4. Run `./scripts/build.sh` with SDK 2.4.0 and test with Luma Ultra hardware.
5. Verify pose, raw IMU, both Jitter matrices and a representative device
   control.
6. Quit Max and confirm a clean install can load both help patches.
7. Commit the release, create an annotated `vX.Y.Z` tag and push both.

## Binary release candidate

The packaging script intentionally refuses to create a public binary candidate
unless the distributor explicitly confirms the SDK terms and supplies a reviewed
end-user agreement:

```sh
export BINARY_EULA_FILE="/absolute/path/to/reviewed-eula.txt"
export CONFIRM_VITURE_BINARY_TERMS=yes
./scripts/package-release.sh
```

The result and its SHA-256 checksum are written to `release/`. Do not attach it
to GitHub until every item in `docs/BINARY_DISTRIBUTION.md` is satisfied.

## GitHub release

After creating and pushing the tag, draft a GitHub release with the matching
section from `CHANGELOG.md`. Attach the binary archive and checksum only when
binary distribution has been cleared. GitHub's automatically generated source
archives are sufficient for a source-only release.

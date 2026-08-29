# Contributing

Thanks for helping improve Ultra Glasses Max.

Before submitting a change:

1. Keep VITURE SDK files outside the repository.
2. Build with SDK 2.4.0 and Max 9 on Apple silicon.
3. Run `./scripts/check-source.sh` and `./scripts/build.sh`.
4. Describe the hardware and macOS/Max versions used for testing.
5. For callback or camera changes, confirm pose, raw IMU and stereo output in
   the same session; the SDK captures the complete callback set at startup.

Do not include SDK headers, libraries, documentation or extracted SDK content
in issues or pull requests. Logs should be checked for serial numbers, paths and
other personal information before posting.

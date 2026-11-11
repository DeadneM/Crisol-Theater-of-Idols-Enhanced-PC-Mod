# Antivirus and binary provenance

DXGI proxy DLLs and ASI hooks can trigger antivirus warnings. A warning is not automatically a false positive. Do not disable Defender or whitelist unreviewed files.

Build from source using [GitHub Actions](.github/workflows/windows-build.yml). Each ZIP artifact includes `SHA256SUMS.txt` and `BUILD_INFO.txt` for verification. Check the GitHub Actions run's commit and hashes, and submit suspicious files for independent analysis.

No game executables or proprietary SML files are redistributed. Report suspected issues via GitHub Issues with scanner name, detection name and SHA-256, not private data.

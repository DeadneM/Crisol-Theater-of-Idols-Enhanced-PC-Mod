# Antivirus and build verification

Our DXGI proxy (`dxgi.dll`) and ASI (`CRToiEnhanced.asi`) hook native rendering and gameplay functions. Security tools may flag them, but **do not assume an antivirus detection is a false positive**.

## Verify builds from this repository

1. Open the [official GitHub Actions workflow](https://github.com/DeadneM/Crisol-Theater-of-Idols-Enhanced-PC-Mod/actions/workflows/windows-build.yml).
2. Download the artifact from a successful run on `main` for V0.4A, or from the test branch for experimental versions.
3. Inspect `BUILD_INFO.txt` and `SHA256SUMS.txt`, then compare the SHA-256 checksums of the extracted files.
4. For newly signed builds, verify the ZIP's GitHub attestation using `gh` (GitHub CLI):

```powershell
gh attestation verify "CRToi_Enhanced_V0.4A_VALIDATED_FOV_x64.zip" -R DeadneM/Crisol-Theater-of-Idols-Enhanced-PC-Mod
```

The attestation ties an artifact to its GitHub build workflow and source revision. **It does not establish that the software is malware-free.** Do not disable Defender or whitelist files that you cannot verify. The absence of a detection is not proof of safety either.

## Report a detection

Open a GitHub Issue with the antivirus name, detection identifier, affected SHA-256, the Actions run link, and whether it affects `dxgi.dll` or the ASI. Never submit proprietary game binaries, personal files or third-party SML mod assets.

The V0.4A World/FPS FOV base is user-tested. Experimental CheatShop changes remain in the separate `dev/v0.5-cheatshop` branch.

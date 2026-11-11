# Antivirus and build provenance

A Windows antivirus warning for our `dxgi.dll` or `CRToiEnhanced.asi` **must be investigated**. DXGI proxies, hooks and the V0.5A keyboard simulation may trigger heuristic detections, but that is not proof of a false positive.

## Verification

1. Download only from the [Crisol GitHub Actions runs](https://github.com/DeadneM/Crisol-Theater-of-Idols-Enhanced-PC-Mod/actions/workflows/windows-build.yml).
2. Inspect `BUILD_INFO.txt` in the ZIP, including the build commit and run ID.
3. Compare local SHA-256 file hashes against `SHA256SUMS.txt` in the archive.
4. For workflow builds with an attestation, verify the ZIP using [GitHub CLI](https://cli.github.com/):

```powershell
gh attestation verify "CRToi_Enhanced_V0.5A_CHEATSHOP_TEST_x64.zip" -R DeadneM/Crisol-Theater-of-Idols-Enhanced-PC-Mod
```

An attestation proves which source revision and GitHub workflow produced an artifact. **It does not prove the DLL is malware-free.** Do not turn off Windows Defender, modify security exclusions or run a suspicious binary just to test a mod.

## Reporting

Open a GitHub Issue with: file SHA-256, antivirus vendor, detection name, source URL and GitHub Actions run ID. Do not upload proprietary game EXEs, personal files or SML's copyrighted files.

## External-mod compatibility

V0.5A's `cheatshop` command requires the independently installed SML and CheatMenu mod. This repository does not distribute or modify that mod. V0.4A remains the gameplay-validated FOV baseline.

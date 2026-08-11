# Ember Core release gate

The normal CI build is not release evidence by itself. A releasable macOS
artifact must pass the signed/notarized workflow in
`.github/workflows/release-macos.yml`; a successful compilation or artifact
upload cannot substitute for that job.

## Blocking conformance

Every macOS CI build runs:

- the complete CTest registry, including the thread-safety regression target;
- pluginval v1.0.4 at strictness 10 against the VST3;
- `auval -v aufx AIEQ AIAU` against the AU;
- exact bundle-count, bundle-ID and universal-architecture checks;
- packaged `Contents/Resources/models/ml_weights.bin` in every format.

The pluginval archives are pinned by SHA-256. Windows CI also runs the pinned
Windows binary at strictness 10. Missing or multiple plug-in bundles are hard
failures, not skipped uploads.

## Signed macOS release

Pushing a `v*` tag, or manually dispatching the release workflow, requires all
of these repository secrets:

- `MACOS_CERTIFICATE_P12_BASE64`
- `MACOS_CERTIFICATE_PASSWORD`
- `MACOS_SIGNING_IDENTITY`
- `MACOS_CI_KEYCHAIN_PASSWORD`
- `APPLE_NOTARY_PRIVATE_KEY_BASE64`
- `APPLE_NOTARY_KEY_ID`
- `APPLE_NOTARY_ISSUER_ID`

The workflow has no ad-hoc or unsigned fallback. It performs a universal
Release build, runs CTest, signs VST3/AU/Standalone with hardened runtime,
reruns pluginval and auval against the signed bundles, creates and signs a DMG,
submits it with `notarytool`, staples and validates the ticket, asks Gatekeeper
to assess the DMG, and publishes the validation logs with `SHA256SUMS`.

For a local release with credentials already stored in a keychain profile:

```bash
export PLUGINVAL_BIN=/absolute/path/pluginval.app/Contents/MacOS/pluginval
export AIEQ_SIGNING_IDENTITY='Developer ID Application: ...'
export AIEQ_NOTARY_PROFILE=aieq-notary
scripts/release-macos.sh build-release Release
```

The release is valid only if the script exits with status zero. Reports are
written to `build-release/validation-reports/` and distributable output to
`build-release/dist/`.

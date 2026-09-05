# Windows installer signing

The Build workflow signs the Windows installer with Azure Artifact Signing after the Windows build and tests pass.
It reuses MIDIMaster's signing account and certificate profile, with publisher `MITCHELL SOFTWARE SOLUTIONS LLC`.
The installer is signed with SHA-256 and an RFC 3161 timestamp, then verified before upload. A signing failure,
invalid signature, unexpected publisher, or missing timestamp prevents the release from being published.

Signing runs on pushes to `main`, `v*` tags, and manual runs on refs permitted by the `windows-release` environment.
Pull-request and local builds remain unsigned. The Windows ZIP and the embedded plugin DLL and uninstaller are not
Authenticode-signed by this workflow; signing applies to the downloadable installer EXE.

## GitHub and Azure configuration

Create the repository environment `windows-release` with custom deployment policies allowing the `main` branch and
`v*` tags. Copy these six **environment variables** from MIDIMaster's `windows-release` environment:

| Variable | Purpose |
| --- | --- |
| `AZURE_CLIENT_ID` | Existing signing application's client ID |
| `AZURE_TENANT_ID` | Microsoft Entra tenant ID |
| `AZURE_SUBSCRIPTION_ID` | Signing account's subscription ID |
| `AZURE_ARTIFACT_SIGNING_ENDPOINT` | `https://eus.codesigning.azure.net` |
| `AZURE_ARTIFACT_SIGNING_ACCOUNT` | `MIDIMaster` |
| `AZURE_ARTIFACT_SIGNING_PROFILE` | `MIDIMasterPublicRelease` |

Add [github-oidc-credential.json](github-oidc-credential.json) as an additional federated credential on the existing
Microsoft Entra signing application. Preserve the MIDIMaster credential. From the repository root, with Azure CLI
authenticated and `AZURE_CLIENT_ID` set to the existing signing application's client ID:

```powershell
az ad app federated-credential create --id $env:AZURE_CLIENT_ID --parameters .github/signing/github-oidc-credential.json
```

Camera uses GitHub's OIDC subject format containing immutable owner and repository IDs:
`repo:prgmitchell@86465454/streamassistant-camera@1314502863:environment:windows-release`.
This must match the subject reported by the Azure login step exactly. MIDIMaster's older name-only subject does
not apply to this repository; do not copy it or remove the numeric IDs.

The application already has permission to sign with this profile. No new client secret, exported certificate,
or private signing key is needed. Only the signing job requests `id-token: write`; the build jobs do not use Azure.
See the [Artifact Signing action documentation](https://github.com/Azure/artifact-signing-action) for service details.

## Verification and release artifacts

Run Build manually on `main` to verify signing without publishing a release. Download the `windows-x64` artifact,
which contains the signed installer and the original ZIP. `windows-x64-unsigned` is an intermediate build artifact
and is never downloaded by the release job. Release checksums are generated after signing.

To verify a downloaded installer in PowerShell:

```powershell
./.github/scripts/Assert-Authenticode.ps1 -Path ./release/streamassistant-camera-v1.1.0-windows-x64.exe `
  -ExpectedPublisher 'MITCHELL SOFTWARE SOLUTIONS LLC' -RequireTimestamp
```

For a pre-merge manual test, temporarily permit the exact test branch in the environment's deployment policies,
run Build on that branch, verify the `windows-x64` artifact, and remove that temporary policy afterwards.

To disable signing access, remove this repository's federated credential from the application. Signing will fail
closed and block new releases. This does not affect MIDIMaster's separate federated credential or signing workflows.

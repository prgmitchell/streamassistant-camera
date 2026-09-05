# StreamAssistant Camera

StreamAssistant Camera turns an iPhone or Android phone into a wireless camera source for OBS Studio. It runs in the
phone's browser, so there is no phone app to install.

## Requirements

- Windows 10/11 or macOS 12+
- OBS Studio 32.0.0 or later
- A current mobile Safari, Chrome, or Firefox browser
- Phone and computer on the same non-isolated Wi-Fi or LAN

## Setup

1. Download the Windows or macOS package from the latest release and restart OBS Studio after installation.
2. Add **StreamAssistant Camera** from the Sources dock.
3. Scan the pairing QR code with the phone and tap **Start camera**.
4. Keep the phone page open and foregrounded while streaming.

The source remembers its pairing across restarts. Camera, microphone, quality, frame-rate, and zoom controls are on the
phone page.

### Installing on macOS

The macOS installer is not signed or notarized with an Apple Developer ID, so macOS blocks it the first time it is
opened. To approve the installer:

1. Open the downloaded `.pkg` file from Finder.
2. When macOS says it cannot verify the developer or check the installer for malicious software, click **Done**. Do not
   move the installer to the Trash.
3. Open **System Settings**, select **Privacy & Security**, and scroll down to the **Security** section.
4. Find the message saying that the StreamAssistant Camera installer was blocked, then click **Open Anyway**.
5. Authenticate with Touch ID or your macOS password if prompted, then confirm **Open** in the final warning.
6. Complete the installer and restart OBS Studio. **StreamAssistant Camera** will then be available in the Sources dock.

If **Open Anyway** is not shown, open the `.pkg` file again, click **Done**, and return to **Privacy & Security**.

## Network and privacy

Video and audio travel directly between the phone and computer over WebRTC. Cloudflare carries the signaling messages
used to start the connection; it does not relay the camera stream.

## Build

Windows:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --preset windows-x64
```

macOS:

```sh
cmake --preset macos
cmake --build --preset macos
ctest --preset macos
```

Windows release maintainers: see [installer signing configuration](.github/signing/README.md).

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).

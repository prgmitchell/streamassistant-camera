# StreamAssistant Camera

Use an iPhone or Android phone as a wireless camera in OBS Studio. Everything runs in the phone's browser—no app needed.

## Requirements

- Windows 10/11, macOS 12+, or Ubuntu 24.04+ (x86_64)
- OBS Studio 33+ on Windows, or 32+ on macOS and Linux
- Safari, Chrome, or Firefox on a phone connected to the same Wi-Fi or LAN as the computer

## Setup

1. Install the [latest release](https://github.com/prgmitchell/streamassistant-camera/releases/latest) and restart OBS.
2. Add **StreamAssistant Camera** from the Sources dock.
3. Scan the pairing QR code with the phone and tap **Start camera**.
4. Keep the phone page open while streaming.

Pairing is remembered across restarts. Camera, microphone, quality, frame-rate, and zoom controls are on the phone page.

### Installing on Windows

Close OBS and run the `.exe` installer. It uses the [OBS 33 layout](https://obsproject.com/kb/legacy-plugin-developer-info):
`C:\ProgramData\obs-studio\plugins\streamassistant-camera`, with the DLL beside the `data` folder.
The installer removes the old `bin\64bit` copy during upgrades.

For manual installation, extract the ZIP into `C:\ProgramData\obs-studio\plugins`.
For portable OBS, extract it into the `plugins` folder at the root of your OBS installation instead.
When upgrading manually, remove this plugin's old DLL and PDB from `bin\64bit`, or its old DLL and data folder
from `obs-plugins\64bit` and `data\obs-plugins` if previously installed inside OBS.

### Installing on macOS

The `.pkg` installer is unsigned and not notarized. If macOS blocks it:

1. Click **Done** in the warning; keep the installer.
2. Open **System Settings → Privacy & Security → Open Anyway**.
3. Authenticate, confirm **Open**, and finish installation.

### Installing on Linux

Use native OBS from `ppa:obsproject/obs-studio`, then install the downloaded package:

```sh
sudo apt install ./streamassistant-camera-v1.3.0-linux-x86_64.deb
```

APT installs the required libraries. The `.tar.xz` alternative installs under `/usr` and requires dependencies
to be installed separately. Flatpak is not supported.

## Network and privacy

Video and audio travel directly between your phone and computer over WebRTC. Cloudflare handles connection signaling,
not the camera stream.

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

Linux (Ubuntu):

```sh
sudo add-apt-repository --yes ppa:obsproject/obs-studio
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config libgles2-mesa-dev libsimde-dev obs-studio qt6-base-dev qt6-websockets-dev dpkg-dev
cmake --preset ubuntu-x86_64
cmake --build --preset ubuntu-x86_64 --parallel
ctest --preset ubuntu-x86_64
bash .github/scripts/Package-Linux.sh 1.3.0
```

Build tooling is based on the [OBS plugin template](https://github.com/obsproject/obs-plugintemplate).
See [Windows signing configuration](.github/signing/README.md) for releases.

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).

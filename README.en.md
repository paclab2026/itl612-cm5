# ITL612 Thermal for Raspberry Pi CM5

Native C++17 / Qt6 thermal imaging for ITL612 Pro Mars USB cores. Features 640×512 RAW16 processing, motion-compensated temporal noise reduction, Autogate tone mapping, device-reported temperature markers, PNG capture and H.264 MP4 recording.

Tested: Hackberry Pi CM5 8 GB, 720×720 display, Raspberry Pi OS Bookworm 64-bit, Mars USB `04b4:f7f7`. Other cores, Pi models and distributions are not yet validated. Input is approximately 30 Hz; processed preview rate depends on settings. MP4 recording is 25 fps. No RAW recordings or RAW temporary media are generated.

## Install a Debian package

On the Raspberry Pi desktop:

```sh
sudo apt install ./itl612-cm5_0.1.0_arm64.deb
```

Reconnect the camera, then open **ITL612 CM5** from the application menu. The first launch asks you to choose **中文** or **ENG**. Change this later in **Menu → 语言/lang**.

The package installs the application, launcher, shaders, icon and a camera-specific udev rule. It does not enable automatic startup or change user settings. The installed executable is `/usr/bin/itl612-cm5`; launcher: `/usr/bin/itl612-start`.

## Build and install from source

Run these commands inside this standalone source directory on the Pi:

```sh
sudo apt install --no-install-recommends cmake g++ make pkg-config \
  libusb-1.0-0-dev qt6-base-dev libqt6opengl6-dev qt6-wayland \
  libegl-dev libgles-dev ffmpeg fonts-noto-cjk xdg-user-dirs desktop-file-utils
sh install.sh
sudo install -m 0644 70-itl612-mars.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
```

Reconnect the camera. Launch with `~/.local/bin/itl612-start` or the desktop shortcut. This installs for the current user. Do not run the GUI as root.

## Build a Debian package

Build on the target architecture and distribution. For the validated arm64/Bookworm target:

```sh
sudo apt install dpkg-dev
sh packaging/build-deb.sh
```

The `.deb` is placed in `build-deb/`. Runtime shared-library dependencies are generated from the actual binary. A clean source archive can be generated using `python3 packaging/make-source.py build-deb`; build output and development media are excluded.

## Controls

- **Autogate ON/OFF**: adaptive tone mapping or linear mapping. Denoising remains independent.
- **Motion TNR / Temporal TNR**: motion-compensated or conventional temporal denoising; Single / 4 / 8 frames.
- **Hold**: hold the display range. **FFC**: request camera shutter correction.
- **Measurement**: Center Spot, Max/Min Markers and Palette Scale. Temperature readings come from the camera, not display brightness. The palette scale is not a linear temperature scale.
- **Capture / REC / Gallery**: PNG photos and MP4 video. Files are saved under the user's system Pictures directory in `thermal/`; this may be localized.
- **Theme**: Light, Dark or Deep Amber. **语言/lang**: 中文 / ENG.
- **Exit Application**: finish recording and close the application. Escape returns from menus; Escape on the live view exits.

The first language selection and later changes are stored in `ui/language` (`zh` or `en`) in `~/.config/ITL612/CM5.conf`, respecting `XDG_CONFIG_HOME`. Upgrades retain this config. No per-pixel radiometric calibration is supplied by this application. Linux builds do not include Apple frameworks or deep models.

## Troubleshooting

- No camera: check `lsusb` for `04b4:f7f7`, reconnect after installing the udev rule, and close any other application using the camera.
- Video errors: verify `ffmpeg` is installed. Playback uses `ffplay`.
- Qt 6.4: the application uses hardware EGL via XWayland when DISPLAY is available, matching the tested desktop configuration.
- Low storage: capture requires 200 MB free. Compiled shader data is cached only under `~/.cache/itl612-cm5`, with a 16 MiB data limit plus index overhead.

Remove a package with `sudo apt remove itl612-cm5`. User settings and photos/videos are retained. A package installed system-wide and an earlier local installation can coexist; remove the earlier local launcher before expecting the system application-menu entry to take precedence.

## Developer checks

Configure with `-DBUILD_TESTING=ON -DBUILD_TOOLS=ON` to build optional tests, benchmarks and UI audit entry points. They are excluded from normal packages. No sample recordings, Apple projects or experimental model weights are required to build the application.

# ITL612 Thermal for Raspberry Pi CM5

[简体中文](README.md) | [English](README.en.md)

ITL612 Thermal is a native 640×512 thermal imaging application for Raspberry Pi CM5 and ITL612 Pro (Mars) USB cores. It features real-time RAW16 processing, motion-compensated temporal denoising, Autogate tone mapping and camera-reported temperature markers. Bilingual menus, three themes, photo capture and video recording are designed around the Hackberry Pi's 720×720 display.

The handheld hardware used for this project is based on **[ZitaoTech / HackberryPiCM5](https://github.com/ZitaoTech/HackberryPiCM5)**: a portable Linux device with a Raspberry Pi CM5, a 720×720 touchscreen and a BlackBerry keyboard. Thanks to that project for its hardware design and documentation. This repository provides the application software for use with an ITL612 Pro USB thermal imaging core.

[Download ARM64 .deb](https://github.com/paclab2026/itl612-cm5/releases/download/v0.1.0/itl612-cm5_0.1.0_arm64.deb) · [v0.1.0 Release](https://github.com/paclab2026/itl612-cm5/releases/tag/v0.1.0)

## Interface and field demo

### CM5 live view

![CM5 live view with the Deep Amber English interface, a temperature scale on the right and capture controls below](docs/images/cm5-live-view.png)

A 720×720 screenshot from the CM5, using the Deep Amber theme and English interface. Status information sits at the upper right, with the palette scale beside the image. The lower controls provide Autogate, palette selection, Range Hold, FFC, Capture, REC, Gallery and Menu.

### Outdoor cat footage

![A native 640×512 thermal video frame of a cat captured with the ITL612 Pro core](docs/images/outdoor-cat.png)

A frame selected from existing outdoor footage recorded with the ITL612 Pro (Mars) core, retaining the original 640×512 dimensions and display processing. It shows thermal contrast across the cat's face, ears and body, alongside vegetation and railings. This footage was recorded with an earlier Apple-platform application to illustrate imaging from the same camera core; it is not a CM5 recording or a CM5 frame-rate benchmark. No generative processing, super-resolution or additional sharpening has been applied. Temperature text is an overlay from the original recording, not an independently calibrated result.

## Validated configuration

Tested: CM5 8 GB, 720×720 display, Raspberry Pi OS Bookworm 64-bit and Mars USB `04b4:f7f7`. Other camera cores, OS versions and Raspberry Pi models have not yet been validated. Camera input is approximately 30 Hz; processed preview rate depends on settings. MP4 recording is 25 fps.

## Install a Debian package

Run on the Raspberry Pi desktop:

```sh
sudo apt install ./itl612-cm5_0.1.0_arm64.deb
```

Reconnect the camera, then open **ITL612 CM5** from the application menu. The package installs the application, launcher, shaders, icon and a camera-specific USB permission rule. It does not enable automatic startup. The installed executable is `/usr/bin/itl612-cm5`; the launcher is `/usr/bin/itl612-start`.

On first launch, select **中文 / ENG**. Change the language at any time in **Menu → 语言/lang**; the change takes effect immediately. Closing the first-launch language dialog exits the application. The selection is stored as `ui/language` (`zh` or `en`) in `~/.config/ITL612/CM5.conf`, respecting `XDG_CONFIG_HOME`, and is retained during upgrades. Users upgrading from an older version are also asked to select a language once.

## Build and install from source

Run in this source directory on the Pi, using your normal desktop user account for the installation script:

```sh
sudo apt install --no-install-recommends cmake g++ make pkg-config \
  libusb-1.0-0-dev qt6-base-dev libqt6opengl6-dev qt6-wayland \
  libegl-dev libgles-dev ffmpeg fonts-noto-cjk xdg-user-dirs desktop-file-utils
sh install.sh
sudo install -m 0644 70-itl612-mars.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
```

Reconnect the camera, then launch from the desktop shortcut or `~/.local/bin/itl612-start`. Do not run the GUI as root.

## Build a Debian package

Install the build dependencies above on the target architecture and distribution, then run:

```sh
sudo apt install dpkg-dev
sh packaging/build-deb.sh
```

For the validated ARM64 target, the output is `build-deb/itl612-cm5_0.1.0_arm64.deb`. Shared-library dependencies are generated from the actual binary; the application uses the system Qt, Mesa and FFmpeg libraries. Run `python3 packaging/make-source.py build-deb` to create a standalone source archive. Build output and local development scripts are excluded from that archive.

## Controls

- **Autogate ON/OFF**: adaptive or linear display mapping; denoising is configured independently.
- **Motion TNR / Temporal TNR**: motion-compensated or conventional temporal denoising; Single / 4 / 8 frames.
- **Hold**: hold the display range. **FFC**: request camera shutter correction.
- **Measurement**: Center Spot, Max/Min Markers and Palette Scale. Temperature readings come from camera parameters; the palette scale is not a linear temperature scale.
- **Capture / REC / Gallery**: photos and videos are saved in `thermal/` under the system Pictures directory, respecting localized directory names.
- **Theme**: Light, Dark or Deep Amber. **语言/lang**: 中文 / ENG.
- **Exit Application**: finalize any recording and exit the process. Escape returns from a menu to the live view; Escape on the live view exits.

The application does not capture or record sensor RAW files or generate RAW temporary media. Sensor data is processed in memory for live imaging. Per-pixel radiometric calibration, ROI thermometry and deep models are not provided.

## Troubleshooting and removal

If no camera is detected, use `lsusb` to check for `04b4:f7f7`, reconnect the camera after installing the USB permission rule, and close other applications using it. Recording requires FFmpeg; playback uses `ffplay`. Capture requires at least 200 MB of free storage. Compiled shader data is cached under `~/.cache/itl612-cm5`, with a 16 MiB data limit plus a small amount of index overhead.

With Qt 6.4, hardware EGL via XWayland is used by default when `DISPLAY` is available; this is the compatibility configuration validated on the device. The application does not manage thermal control or limit frame rate based on temperature.

Uninstall with `sudo apt remove itl612-cm5`. Personal settings, photos and videos are retained. An earlier user-local installation takes precedence over the system installation; move aside its local launcher and duplicate application-menu entry when migrating to the package.

## Developer checks

Normal packages exclude tests, benchmarks and UI audit entry points by default. To enable development checks, configure with `-DBUILD_TESTING=ON -DBUILD_TOOLS=ON`; run `ctest --test-dir <build-directory>` for the measurement-overlay check. 

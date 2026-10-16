# v0.1.0 — Initial Public Preview / 首个公开预览版

ITL612 Thermal 是面向 Raspberry Pi CM5 的 640×512 热像应用，支持 ITL612 Pro（Mars）USB 机芯、运动补偿多帧降噪、Autogate 自适应映射、机芯温度标记及拍照录像。

ITL612 Thermal is a native 640×512 thermal imaging application for Raspberry Pi CM5 and ITL612 Pro (Mars) USB cores, featuring motion-compensated temporal denoising, Autogate tone mapping, camera-reported temperature markers, and photo/video capture.

## Features / 功能

- Real-time RAW16 processing; Single / 4 / 8-frame temporal denoising.
- Independent Autogate ON/OFF and display-range hold.
- Center Spot and Max/Min Markers from camera-reported measurements.
- 中文 / ENG selection on first launch, with persistent language settings.
- Light, Dark and Deep Amber themes; nine color palettes.
- PNG capture and H.264 MP4 recording. No sensor RAW recording or RAW temporary media.

## Tested hardware / 已验证环境

- Hackberry Pi CM5, 8 GB RAM, 720×720 display.
- Raspberry Pi OS Bookworm, 64-bit arm64.
- ITL612 Pro Mars USB core, VID:PID `04b4:f7f7`.

Input is approximately 30 Hz; processed preview rate depends on configuration. MP4 output is 640×512 at 25 fps. Other cores, boards and OS versions are not yet validated. Temperature accuracy has not been independently calibrated by this application.

机芯输入约 30 Hz，增强预览帧率取决于配置；MP4 输出为 640×512、25 fps。其他机芯、主板和系统版本尚未验证。应用未独立标定测温精度。

## Install / 安装

Download the ARM64 `.deb` and run on the Pi / 下载 ARM64 安装包后在树莓派运行：

```sh
sudo apt install ./itl612-cm5_0.1.0_arm64.deb
```

Reconnect the camera and launch ITL612 from the application menu. Choose 中文 or ENG on first launch; change it later in Menu → 语言/lang. Photos and videos are saved under the system Pictures directory in `thermal/`.

重插机芯，从应用菜单启动。首次选择中文或 ENG，以后可在“菜单 → 语言/lang”中切换。照片录像保存在系统图片目录下的 `thermal/`。

## Downloads / 下载

- `itl612-cm5_0.1.0_arm64.deb`: Raspberry Pi OS ARM64 package / 安装包。
- `itl612-cm5-0.1.0-source.tar.gz`: Standalone source and build scripts / 独立源码与构建脚本。
- `SHA256SUMS`: SHA-256 checksums / 文件校验。

This is an initial public preview. Please include the board, OS version, camera model and reproduction steps when reporting an issue.

这是首个公开预览版。反馈问题时请提供主板、系统版本、机芯型号和复现步骤。

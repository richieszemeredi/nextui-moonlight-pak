# Moonlight for NextUI

A native [Moonlight](https://moonlight-stream.org/) game streaming client for TrimUI handhelds running [NextUI](https://github.com/LoveRetro/NextUI). Stream your PC games at the display's native resolution — no more 720p/1080p forced scaling.

Built with [Apostrophe](https://github.com/Helaas/Apostrophe), so the UI matches your NextUI theme.

## Why?

The stock Moonlight app on TrimUI only offers 720p and 1080p — neither integer-scales on the Brick's 1024x768 display. This Pak calls `moonlight` directly with the correct resolution, wrapped in a proper UI for managing servers, pairing, and launching streams.

## Features

- **Native resolution streaming** — 1024x768 on the Brick, 1280x720 on Smart Pro/Pro S, 640x480 on Miyoo Flip
- **Server management** — Add, edit, delete, and switch between multiple PCs
- **Pairing** — PIN displayed on screen, cancellable at any time
- **App selection** — Query and pick from available apps on your host
- **Configurable** — Resolution, FPS (30/60/120), and bitrate (1–30 Mbps, with fine-grained low-end options for weak Wi-Fi)
- **NextUI native** — Themed UI, proper input handling, stays awake during streaming

## Requirements

- [Sunshine](https://github.com/LizardByte/Sunshine) (or NVIDIA GameStream) running on your PC
- Wi-Fi enabled on your device
- `moonlight` CLI available on the device (included with the stock TrimUI system)

## Installation

### From Pak Store

Search for **Moonlight** in the Pak Store (Tools menu on your device).

### Manual

1. Download the latest `.pakz` from [Releases](https://github.com/richieszemeredi/nextui-moonlight-pak/releases)
2. Extract and copy the `Moonlight.pak` folder for your platform to `SD_ROOT/Tools/<platform>/`
3. Launch from the Tools menu

## Usage

### First time setup

1. Open **Moonlight** from Tools
2. Go to **Servers** > press **X** to add
3. Enter a name and the **IP address** of your PC (hostnames don't resolve on the device — use the IP)
4. Go back and select **Pair**
5. Enter the displayed PIN on your PC (in Sunshine's web UI)
6. Select **Stream** to pick an app and start

### Controls

| Screen | Button | Action |
|--------|--------|--------|
| Main menu | A | Select |
| Main menu | B | Quit |
| Servers | A | Open server details |
| Servers | X | Add server |
| Servers | Select | Set as active |
| Settings | Left/Right | Change value |
| Settings | A | Save |
| Pairing/Querying | B | Cancel |

### Troubleshooting

- **"Pairing failed"** — Make sure Sunshine is running and you're using the PC's IP address, not a hostname
- **Stale pairing** — If you previously used the stock Moonlight UI app, delete old keys: `rm /.cache/moonlight/*` via terminal/SSH, then re-pair
- **"Could not retrieve app list"** — The device isn't paired. Run Pair first

## Building from source

### Prerequisites

- SDL2, SDL2_ttf, SDL2_image: `brew install sdl2 sdl2_ttf sdl2_image`
- Docker (for cross-compilation to device)
- cppcheck + llvm (for linting): `brew install cppcheck llvm`

### Build

```sh
make mac          # native macOS build
make run-mac      # build and run locally
make tg5040       # cross-compile for TrimUI Brick / Smart Pro
make tg5050       # cross-compile for Smart Pro S
make my355        # cross-compile for Miyoo Flip
make deploy       # auto-detect device via adb, build, and push
make package      # build all platforms + create .pakz
make check        # run lint + tests
```

## License

[MIT](LICENSE)

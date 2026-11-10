# Moonlight Pak for NextUI

Moonlight game streaming client for TrimUI handhelds running NextUI. Built with the [Apostrophe](https://github.com/Helaas/Apostrophe) UI framework (header-only C11 + SDL2).

## Architecture

- **src/main.c** — Entry point, Apostrophe init (AP_IMPLEMENTATION defined here only)
- **src/ui.c** — All UI screens: main menu, server management, pairing, streaming, settings
- **src/config.c** — Config persistence (key=value format at `$SHARED_USERDATA_PATH/moonlight/config.txt`)
- **src/moonlight_cli.c** — Wrappers around `moonlight` CLI: pair, list, stream, plus process spawning helpers
- **src/moonlight.h** — Shared types and declarations
- **tests/test_parsers.c** — Unit tests for CLI output parsing and config format

## Key design decisions

- The `moonlight` CLI binary is already installed on NextUI devices at `/usr/bin/moonlight`. It requires `LD_LIBRARY_PATH=/usr/trimui/lib` which NextUI sets when launching paks.
- Hostnames (`.local`, NetBIOS) don't resolve on the device — servers must be added by IP address.
- Moonlight pairing keys live at `/.cache/moonlight/` on the device. Stale keys from the stock moonlightui app can cause pairing failures — delete them for a clean pair.
- Process spawning uses `fork()` + `execl("/bin/sh", ...)` with `setpgid(0,0)` for process group isolation. This allows cancellation via `kill(-pid, SIGTERM)` while keeping shell compatibility that `moonlight` depends on.
- Config is split: global settings (resolution/fps/bitrate) apply to all servers; servers are name+host pairs with an active selection index.
- Apostrophe's `dynamic_message` in `ap_process_message` supplements (not replaces) the original `message` — set `message=" "` and control everything through `dynamic_message` when you need full control.

## Build

```
make mac              # native macOS build
make run-mac          # build + run
make tg5040           # cross-compile for TrimUI Brick (Docker)
make deploy           # auto-detect device via adb, build, push
make package          # build all platforms + create .pakz
```

Requires: SDL2, SDL2_ttf, SDL2_image (`brew install sdl2 sdl2_ttf sdl2_image`). Cross-compilation requires Docker.

## Quality

After any code change, run:

```
make check
```

This runs both `make lint` (cppcheck + clang-tidy) and `make test` (unit tests). All must pass before deploying.

- **`make test`** — Parser unit tests (app list parsing, PIN extraction, config format)
- **`make lint`** — Static analysis (cppcheck for bugs/portability, clang-tidy for deeper analysis)
- Lint requires: `brew install cppcheck llvm`

## Apostrophe UI patterns

- All widgets are **blocking** — they run their own event loop and return when the user acts.
- `AP_OPT_KEYBOARD` options must use `strdup()` for label/value — the widget frees and replaces them.
- Use `ap_keyboard()` for general text, `ap_url_keyboard()` for IPs/URLs (has dot/number shortcuts).
- Footer hints are visual only — wire behavior with `action_button`, `secondary_action_button`, `confirm_button`.
- The `AP_LIST_ITEM()` macro is preferred for simple list items.

## Reference repos

- [Apostrophe](https://github.com/Helaas/Apostrophe) — UI framework docs in `docs/`
- [nextui-shortcuts-pak](https://github.com/Helaas/nextui-shortcuts-pak) — Production Apostrophe pak to reference for patterns
- [NextUI](https://github.com/LoveRetro/NextUI) — Firmware, device paths, environment variables

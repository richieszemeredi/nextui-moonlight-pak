# ──────────────────────────────────────────────────────────────
# Moonlight Pak — Build System
# ──────────────────────────────────────────────────────────────

SHELL := /bin/bash

APP_NAME := moonlight-pak
PAK_NAME := Moonlight
APOSTROPHE_DIR := third_party/apostrophe
APOSTROPHE_BRANCH := main
BUILD_DIR := build
DIST_DIR := $(BUILD_DIR)/release
STAGING_DIR := $(BUILD_DIR)/staging
CACHE_DIR := .cache
NEXTUI_PREVIEW_CACHE := $(CACHE_DIR)/nextui-preview
SRC_FILES := $(shell find src -name '*.c' -print | sort)
TEST_BUILD_DIR := $(BUILD_DIR)/tests
LLVM_BIN := $(shell brew --prefix llvm 2>/dev/null)/bin

TG5040_TOOLCHAIN := ghcr.io/loveretro/tg5040-toolchain:latest
TG5050_TOOLCHAIN := ghcr.io/loveretro/tg5050-toolchain:latest
MY355_TOOLCHAIN  := ghcr.io/loveretro/my355-toolchain:latest
ADB ?= adb

COMMON_INCLUDES := -I$(APOSTROPHE_DIR)/include -Isrc

.PHONY: all native mac run-mac run-native tg5040 tg5050 my355 \
	package package-tg5040 package-tg5050 package-my355 do-package \
	deploy deploy-platform clean help update-apostrophe \
	setup-nextui-preview-cache clean-nextui-preview-cache \
	test lint check

# ── Default target ──────────────────────────────────────────

native: mac
run-native: run-mac
all: tg5040 tg5050 my355

# ── Submodule auto-init ────────────────────────────────────

$(APOSTROPHE_DIR)/include/apostrophe.h:
	git submodule update --init

update-apostrophe: $(APOSTROPHE_DIR)/include/apostrophe.h
	@set -euo pipefail; \
	git -C "$(APOSTROPHE_DIR)" fetch origin "$(APOSTROPHE_BRANCH)"; \
	commit=$$(git -C "$(APOSTROPHE_DIR)" rev-parse "origin/$(APOSTROPHE_BRANCH)"); \
	git -C "$(APOSTROPHE_DIR)" checkout "$$commit" >/dev/null; \
	echo "Apostrophe pinned to $$commit"

# ── Native macOS build ──────────────────────────────────────

mac: $(APOSTROPHE_DIR)/include/apostrophe.h
	@$(MAKE) setup-nextui-preview-cache
	@mkdir -p $(BUILD_DIR)/mac
	cc -std=gnu11 -O0 -g \
		-DPLATFORM_MAC \
		$(COMMON_INCLUDES) \
		$(shell pkg-config --cflags sdl2 SDL2_ttf SDL2_image) \
		-o $(BUILD_DIR)/mac/$(APP_NAME) \
		$(SRC_FILES) \
		$(shell pkg-config --libs sdl2 SDL2_ttf SDL2_image) \
		-lm -lpthread

run-mac: mac
	./$(BUILD_DIR)/mac/$(APP_NAME)

setup-nextui-preview-cache: $(APOSTROPHE_DIR)/include/apostrophe.h
	@$(MAKE) -C $(APOSTROPHE_DIR) setup-nextui-preview-cache \
		CACHE_DIR=$(CURDIR)/$(CACHE_DIR)

clean-nextui-preview-cache:
	rm -rf $(NEXTUI_PREVIEW_CACHE)

# ── Docker cross-compilation ────────────────────────────────

tg5040:
	@mkdir -p $(BUILD_DIR)/tg5040
	docker run --rm \
		-v "$(CURDIR)":/workspace \
		$(TG5040_TOOLCHAIN) \
		make -C /workspace -f ports/tg5040/Makefile BUILD_DIR=/workspace/$(BUILD_DIR)/tg5040

tg5050:
	@mkdir -p $(BUILD_DIR)/tg5050
	docker run --rm \
		-v "$(CURDIR)":/workspace \
		$(TG5050_TOOLCHAIN) \
		make -C /workspace -f ports/tg5050/Makefile BUILD_DIR=/workspace/$(BUILD_DIR)/tg5050

my355:
	@mkdir -p $(BUILD_DIR)/my355
	docker run --rm \
		-v "$(CURDIR)":/workspace \
		$(MY355_TOOLCHAIN) \
		make -C /workspace -f ports/my355/Makefile BUILD_DIR=/workspace/$(BUILD_DIR)/my355

# ── Packaging ───────────────────────────────────────────────

package-tg5040: tg5040
	@$(MAKE) do-package PLATFORM=tg5040

package-tg5050: tg5050
	@$(MAKE) do-package PLATFORM=tg5050

package-my355: my355
	@$(MAKE) do-package PLATFORM=my355

do-package:
	@rm -rf $(BUILD_DIR)/$(PLATFORM)/$(PAK_NAME).pak
	@mkdir -p $(BUILD_DIR)/$(PLATFORM)/$(PAK_NAME).pak
	@cp $(BUILD_DIR)/$(PLATFORM)/$(APP_NAME) $(BUILD_DIR)/$(PLATFORM)/$(PAK_NAME).pak/
	@cp launch.sh pak.json $(BUILD_DIR)/$(PLATFORM)/$(PAK_NAME).pak/
	@if [ -f LICENSE ]; then cp LICENSE $(BUILD_DIR)/$(PLATFORM)/$(PAK_NAME).pak/; fi
	@mkdir -p $(DIST_DIR)/$(PLATFORM)
	@rm -f $(DIST_DIR)/$(PLATFORM)/$(PAK_NAME).pak.zip
	@cd $(BUILD_DIR)/$(PLATFORM) && zip -r "$(CURDIR)/$(DIST_DIR)/$(PLATFORM)/$(PAK_NAME).pak.zip" "$(PAK_NAME).pak" -x '.*'

package: package-tg5040 package-tg5050 package-my355
	@rm -rf $(STAGING_DIR)
	@mkdir -p $(STAGING_DIR)/Tools/tg5040 $(STAGING_DIR)/Tools/tg5050 $(STAGING_DIR)/Tools/my355
	@cp -a $(BUILD_DIR)/tg5040/$(PAK_NAME).pak $(STAGING_DIR)/Tools/tg5040/
	@cp -a $(BUILD_DIR)/tg5050/$(PAK_NAME).pak $(STAGING_DIR)/Tools/tg5050/
	@cp -a $(BUILD_DIR)/my355/$(PAK_NAME).pak $(STAGING_DIR)/Tools/my355/
	@mkdir -p $(DIST_DIR)/all
	@rm -f $(DIST_DIR)/all/$(PAK_NAME).pakz
	@cd $(STAGING_DIR) && zip -9 -r "$(CURDIR)/$(DIST_DIR)/all/$(PAK_NAME).pakz" . -x '.*'

# ── ADB deploy ──────────────────────────────────────────────

deploy:
	@echo "Detecting platform..."
	@SERIAL="$(ADB_SERIAL)"; \
	if [ -z "$$SERIAL" ]; then \
		SERIAL=$$($(ADB) devices | awk 'NR>1 && $$2=="device" {print $$1; exit}'); \
	fi; \
	if [ -z "$$SERIAL" ]; then \
		echo "Error: No online adb device found."; \
		exit 1; \
	fi; \
	ADB_CMD="$(ADB) -s $$SERIAL"; \
	FINGERPRINT=$$($$ADB_CMD shell ' \
		cat /proc/device-tree/compatible 2>/dev/null; \
		echo; \
		cat /proc/device-tree/model 2>/dev/null; \
		echo; \
		uname -a 2>/dev/null' 2>/dev/null | tr '\000' '\n' | tr -d '\r'); \
	case "$$FINGERPRINT" in \
		*rk3566*|*miyoo-355*) PLATFORM=my355 ;; \
		*allwinner,a523*|*sun55iw3*) PLATFORM=tg5050 ;; \
		*allwinner,a133*|*sun50iw*) PLATFORM=tg5040 ;; \
		*allwinner*) \
			if printf '%s' "$$FINGERPRINT" | grep -qi 'a523'; then \
				PLATFORM=tg5050; \
			else \
				PLATFORM=tg5040; \
			fi \
			;; \
		*) \
			echo "Error: Could not detect a supported platform from adb fingerprint."; \
			echo "  Serial: $$SERIAL"; \
			echo "  Fingerprint snippet: $$(printf '%s' "$$FINGERPRINT" | head -c 240)"; \
			exit 1; \
			;; \
	esac; \
	echo "Detected adb serial: $$SERIAL"; \
	echo "Detected platform: $$PLATFORM"; \
	$(MAKE) deploy-platform PLATFORM=$$PLATFORM SERIAL=$$SERIAL

deploy-platform:
	@if [ -z "$(PLATFORM)" ] || [ -z "$(SERIAL)" ]; then \
		echo "Error: deploy-platform requires PLATFORM and SERIAL."; \
		exit 1; \
	fi
	@$(MAKE) package-$(PLATFORM)
	@ADB_CMD="$(ADB) -s $(SERIAL)"; \
	TOOLS_ROOT="/mnt/SDCARD/Tools/$(PLATFORM)"; \
	TOOLS_DIR="$$TOOLS_ROOT/$(PAK_NAME).pak"; \
	echo "Deploying $(PAK_NAME).pak to $$TOOLS_DIR..."; \
	$$ADB_CMD shell "rm -rf '$$TOOLS_DIR' && mkdir -p '$$TOOLS_ROOT'"; \
	$$ADB_CMD push "$(BUILD_DIR)/$(PLATFORM)/$(PAK_NAME).pak" "$$TOOLS_ROOT/"; \
	echo "Deploy complete."

# ── Testing ─────────────────────────────────────────────────

test:
	@mkdir -p $(TEST_BUILD_DIR)
	cc -std=gnu11 -O0 -g -Wall -Wextra -Wno-unused-parameter \
		-o $(TEST_BUILD_DIR)/test_parsers \
		tests/test_parsers.c \
		-lm
	./$(TEST_BUILD_DIR)/test_parsers

# ── Linting ─────────────────────────────────────────────────

lint:
	@echo "════════ cppcheck ════════"
	cppcheck --enable=warning,performance,portability \
		--suppress=missingIncludeSystem \
		--suppress='*:third_party/*' \
		-I src -I $(APOSTROPHE_DIR)/include \
		--error-exitcode=1 \
		src/
	@echo ""
	@echo "════════ clang-tidy ════════"
	$(LLVM_BIN)/clang-tidy \
		--checks='clang-analyzer-*,bugprone-*,performance-*,-bugprone-easily-swappable-parameters,-bugprone-unchecked-string-to-number-conversion,-bugprone-command-processor,-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling' \
		--warnings-as-errors='*' \
		--header-filter='src/.*' \
		src/*.c \
		-- -I src -I $(APOSTROPHE_DIR)/include \
		$$(pkg-config --cflags sdl2 SDL2_ttf SDL2_image) \
		-DPLATFORM_MAC

check: lint test
	@echo ""
	@echo "All checks passed."

# ── Cleanup ─────────────────────────────────────────────────

clean:
	rm -rf $(BUILD_DIR)

# ── Help ────────────────────────────────────────────────────

help:
	@echo "Targets:"
	@echo "  native        Build the mac development binary"
	@echo "  run-native    Build and run the mac binary"
	@echo "  all           Build tg5040, tg5050, and my355"
	@echo "  mac           Build for macOS (native)"
	@echo "  run-mac       Build and run for macOS"
	@echo "  tg5040        Build for TG5040 (Docker cross-compile)"
	@echo "  tg5050        Build for TG5050 (Docker cross-compile)"
	@echo "  my355         Build for Miyoo Flip (Docker cross-compile)"
	@echo "  update-apostrophe  Pin Apostrophe submodule to origin/main"
	@echo "  package       Package all platforms (.pak.zip + .pakz)"
	@echo "  deploy        Detect adb platform, package, and push"
	@echo "  test          Run unit tests"
	@echo "  lint          Run cppcheck and clang-tidy"
	@echo "  check         Run lint + test"
	@echo "  clean         Remove build artifacts"

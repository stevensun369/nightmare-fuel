#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

dependency_available() {
    local brew_name="$1"
    local pkg_config_name="$2"

    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists "$pkg_config_name"; then
        return 0
    fi

    if command -v brew >/dev/null 2>&1 && brew list --versions "$brew_name" >/dev/null 2>&1; then
        return 0
    fi

    return 1
}

check_dependencies() {
    MISSING=()

    if ! command -v cmake >/dev/null 2>&1; then
        MISSING+=(cmake)
    fi

    if ! dependency_available glfw glfw3; then
        MISSING+=(glfw)
    fi
}

check_dependencies

if ((${#MISSING[@]} > 0)); then
    echo "Missing dependencies: ${MISSING[*]}"

    if command -v brew >/dev/null 2>&1; then
        read -r -p "Run 'brew install ${MISSING[*]}' now? [y/N] " INSTALL_DEPS
        case "$INSTALL_DEPS" in
            y|Y|yes|YES|Yes)
                brew install "${MISSING[@]}"
                ;;
            *)
                echo "Dependencies were not installed. Exiting."
                exit 1
                ;;
        esac

        check_dependencies
    else
        echo "Homebrew is not installed or is not on PATH."
        echo "On macOS, install with: brew install ${MISSING[*]}"
        echo "On Debian/Ubuntu, install with: sudo apt install cmake libglfw3-dev"
        exit 1
    fi

    if ((${#MISSING[@]} > 0)); then
        echo "The following dependencies are still missing: ${MISSING[*]}"
        exit 1
    fi
fi

CONFIG=Debug
BUILD_DIR=build
BIN_DIR="$BUILD_DIR/Bin"
EXE_NAME="ACG_Lab4" 

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG"
cmake --build "$BUILD_DIR" --config "$CONFIG"

cd "$BIN_DIR"

if [[ "$(uname -s)" == "Linux" && "${XDG_SESSION_TYPE:-}" == "wayland" ]]; then
    unset WAYLAND_DISPLAY
    export XDG_SESSION_TYPE=x11
fi

./"$EXE_NAME"

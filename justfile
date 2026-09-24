# Helpers for this private fork of MEGAsync.
# No machine-specific paths: VCPKG_ROOT defaults to the sibling checkout,
# overridable via the environment.

set shell := ["bash", "-cu"]

vcpkg_root := env_var_or_default("VCPKG_ROOT", "../vcpkg")
build_dir  := "build/dev"
build_type := "Debug"

# Tests need ENABLE_DESKTOP_APP_TESTS=ON at configure time
test_flag := "-DENABLE_DESKTOP_APP_TESTS=ON"

# Known-broken upstream test cases, excluded by default (see ticket MEGA-1):
# - "setScaleFactorEnvironmentVariable()" and "getLogMessages()" abort (SIGABRT
#   via the Qt fatal-message handler; marked "Failing since the beginning" in the
#   test file itself).
# - "getTimeString()" expects <span>-decorated units that prod
#   Utilities::filledTimeString() never produces — test is out of sync with prod.
# Override: extra args pass straight to the binary, e.g. just test '"class TransferBatchTests*"'
broken_tests := '"~setScaleFactorEnvironmentVariable()" "~getLogMessages()" "~getTimeString()"'

default:
    @just --list

# Configure (first time / after CMake or vcpkg changes) + build the desktop app (incremental)
build *args:
    cmake -DVCPKG_ROOT={{vcpkg_root}} -S . -B {{build_dir}} -DCMAKE_BUILD_TYPE={{build_type}} {{test_flag}}
    cmake --build {{build_dir}} --target MEGAsync -j $(nproc) {{args}}

# Build and run unit tests (Catch2 binary, no CTest wiring in this repo).
# Known-broken upstream cases are excluded by default (always).
# Extra args are passed straight to the binary: just test '"class TransferBatchTests*"'
# or just test --success
test *args:
    cmake --build {{build_dir}} --target UnitTests -j $(nproc)
    {{build_dir}}/src/MEGAAutoTests/UnitTests/UnitTests {{broken_tests}} {{args}}

# Run the dev-built app. NOTE: uses the prod settings/data dir directly —
# quit the installed prod MEGAsync first (single-instance lock, same data).
run *args:
    {{build_dir}}/src/MEGASync/megasync {{args}}

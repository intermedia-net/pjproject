#!/bin/bash
#
# Builds the PJSIP Android release (libpjsua2.so per ABI, headers and Java
# bindings). Runs inside ghcr.io/intermedia-net/pjproject-android-builder,
# which provides the toolchain (NDK, SDK, SWIG, JDK) and the prebuilt
# OpenSSL, Opus and bcg729. Everything that decides what goes into the
# library (config_site.h, configure flags, ABIs, API level) lives here.
#
# Replaces the builder image's /home/build script.

set -Eeuo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"

abis="armeabi-v7a x86 arm64-v8a x86_64"
api=28

# Provided by the builder image (see its Dockerfile and config.conf).
download_dir="${DOWNLOAD_DIR:-/home/tools}"
deps_dir="${BUILD_DIR:-/home/output}"
ndk_dir="$download_dir/android-ndk-r28c"

for i in "$@"; do
  case $i in
    --source-dir=*) source_dir="${i#*=}" ;;
    --output-dir=*) output_dir="${i#*=}" ;;
    --abis=*)       abis="${i#*=}" ;;
    --api=*)        api="${i#*=}" ;;
    --ndk-dir=*)    ndk_dir="${i#*=}" ;;
    --deps-dir=*)   deps_dir="${i#*=}" ;;
    *) echo "Unknown argument: $i"; exit 1 ;;
  esac
done

function check_or_exit() {
    if [ -z "${2:-}" ]; then
        echo "Please specify $1"
        exit 1
    fi
}

check_or_exit "source directory using --source-dir" "${source_dir:-}"
check_or_exit "output directory using --output-dir" "${output_dir:-}"

# APPS-30898: codecs not used by the app. Both speex flags are needed so the
# Speex library itself is not built/linked (the app uses WebRTC AEC3, not
# Speex AEC). Keep in sync with config_site.h.
configure_args=(
    --use-ndk-cflags
    --enable-libwebrtc-aec3
    --disable-g7221-codec
    --disable-gsm-codec
    --disable-ilbc-codec
    --disable-speex-codec
    --disable-speex-aec
)

# Codec registration symbols that must not appear in the library.
removed_codecs="g7221 gsm ilbc speex"

export ANDROID_NDK_ROOT="$ndk_dir"
export PATH="$ANDROID_NDK_ROOT:$PATH"

nm_tool="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm"
[ -x "$nm_tool" ] || nm_tool=nm

tmp_dir="/tmp/pjsip"
swig_dir="$tmp_dir/pjsip-apps/src/swig"
swig_android_dir="$swig_dir/java/android/pjsua2/src/main"

out_lib="$output_dir/lib"
out_includes="$output_dir/includes"
out_logs="$output_dir/logs"
out_java="$output_dir/java"

function on_error() {
    local log="$out_logs/${current_abi:-unknown}.log"
    echo "Build failed for ABI ${current_abi:-unknown}. Last lines of $log:"
    [ -f "$log" ] && tail -n 60 "$log"
}
trap on_error ERR

function prepare_output() {
    mkdir -p "$output_dir"
    rm -rf "$out_lib" "$out_includes" "$out_logs" "$out_java"
    mkdir -p "$out_lib" "$out_includes" "$out_logs"
}

function copy_source() {
    rm -rf "$tmp_dir"
    mkdir -p "$tmp_dir"
    tar -C "$source_dir" \
        --exclude=./.git --exclude=./.build --exclude=./.swiftpm \
        --exclude=./ios_framework --exclude=./android_framework \
        -cf - . | tar -C "$tmp_dir" -xf -
    cp "$script_dir/config_site.h" "$tmp_dir/pjlib/include/pj/config_site.h"
}

function build_abi() {
    local abi=$1
    local log="$out_logs/$abi.log"
    current_abi=$abi

    echo "Building PJSIP for $abi (API $api) ..."
    copy_source
    cd "$tmp_dir"

    APP_PLATFORM="android-$api" TARGET_ABI="$abi" ./configure-android \
        "${configure_args[@]}" \
        --with-ssl="$deps_dir/openssl-build-output/libs/$abi" \
        --with-opus="$deps_dir/opus-build-output/libs/$abi" \
        --with-bcg729="$deps_dir/bcg729-build-output/libs/$abi" \
        >> "$log" 2>&1

    make dep >> "$log" 2>&1
    make clean >> "$log" 2>&1
    make >> "$log" 2>&1

    echo "Building pjsua2 SWIG bindings for $abi ..."
    cd "$swig_dir"
    make >> "$log" 2>&1

    mkdir -p "$out_lib/$abi"
    cp -r "$swig_android_dir/jniLibs/$abi"/* "$out_lib/$abi"

    local dir
    for dir in pjlib pjlib-util pjmedia pjnath pjnat64 pjsip; do
        mkdir -p "$out_includes/$abi/$dir"
        cp -r "$tmp_dir/$dir/include"/* "$out_includes/$abi/$dir"
    done

    check_removed_codecs "$abi"
}

function check_removed_codecs() {
    local abi=$1
    local so="$out_lib/$abi/libpjsua2.so"
    local codec found="" symbols

    symbols="$("$nm_tool" -D --defined-only "$so")"
    for codec in $removed_codecs; do
        if grep -q " pjmedia_codec_${codec}_init$" <<< "$symbols"; then
            found="$found $codec"
        fi
    done
    if [ -n "$found" ]; then
        echo "Removed codecs are still in $so:$found"
        exit 1
    fi
}

function copy_java() {
    echo "Copying pjsua2 Java bindings ..."
    cp -r "$swig_android_dir/java" "$output_dir"
}

prepare_output
for abi in $abis; do
    build_abi "$abi"
done
copy_java
rm -rf "$tmp_dir"

echo "Finished. Output in $output_dir"

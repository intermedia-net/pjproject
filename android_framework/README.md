# Android build

Builds the Android release used by `unite-mobile_android_usip`: `libpjsua2.so` for each ABI, the headers, and the pjsua2 Java bindings.

## What lives where

**This directory** decides what goes into the library:
- `config_site.h`
- the configure flags (in `build_android.sh`)
- the ABIs and the Android API level

**The image** [`ghcr.io/intermedia-net/pjproject-android-builder`](https://github.com/intermedia-net/pjproject-android-builder) only provides:
- the toolchain: JDK, NDK r28c, SDK and SWIG;
- the prebuilt OpenSSL, Opus and bcg729.

When you change `config_site.h`, keep the matching `--disable-*` flags in `build_android.sh` in sync. Without the flags, third-party codec libraries are still built and linked.

## Output

The output layout is consumed as-is by `unite-mobile_android_usip`, so don't change it:

```
lib/<abi>/libpjsua2.so, libc++_shared.so
includes/<abi>/{pjlib,pjlib-util,pjmedia,pjnath,pjnat64,pjsip}/
java/org/pjsip/pjsua2/*.java
logs/<abi>.log
```

## Local build

From the repo root. On Apple Silicon this runs under amd64 emulation and is slow.

```sh
docker run --rm --platform linux/amd64 \
  -v "$PWD":/home/tools/pjproject \
  -v "$PWD/android_framework/artifact":/home/output/pjsip-build-output \
  ghcr.io/intermedia-net/pjproject-android-builder:master \
  /home/tools/pjproject/android_framework/build_android.sh \
    --source-dir=/home/tools/pjproject \
    --output-dir=/home/output/pjsip-build-output
```

Add `--abis=arm64-v8a` to build a single ABI.

The build works on a copy of the source in `/tmp/pjsip` inside the container. Even so, don't run two builds against the same checkout at the same time.

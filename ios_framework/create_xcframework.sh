#!/bin/sh
PLISTBUDDY_EXEC="/usr/libexec/PlistBuddy"

BASE_DIR=$1
SOURCE_DIR=$2
XCFRAMEWORK_DIR=$3
INFO_PLIST="${XCFRAMEWORK_DIR}/Info.plist"

plist_add_library() {
    local index=$1
    local identifier=$2
    local platform=$3
    local platform_variant=$4
    "$PLISTBUDDY_EXEC" -c "Add :AvailableLibraries: dict"  "${INFO_PLIST}"
    "$PLISTBUDDY_EXEC" -c "Add :AvailableLibraries:${index}:LibraryIdentifier string ${identifier}"  "${INFO_PLIST}"
    "$PLISTBUDDY_EXEC" -c "Add :AvailableLibraries:${index}:LibraryPath string PJSipIOS.framework"  "${INFO_PLIST}"
    "$PLISTBUDDY_EXEC" -c "Add :AvailableLibraries:${index}:SupportedArchitectures array"  "${INFO_PLIST}"
    "$PLISTBUDDY_EXEC" -c "Add :AvailableLibraries:${index}:SupportedPlatform string ${platform}"  "${INFO_PLIST}"
    if [ ! -z "$platform_variant" ]; then
        "$PLISTBUDDY_EXEC" -c "Add :AvailableLibraries:${index}:SupportedPlatformVariant string ${platform_variant}" "${INFO_PLIST}"
    fi
}

plist_add_architecture() {
    local index=$1
    local arch=$2
    "$PLISTBUDDY_EXEC" -c "Add :AvailableLibraries:${index}:SupportedArchitectures: string ${arch}"  "${INFO_PLIST}"
}

rm -rf "${XCFRAMEWORK_DIR}"
mkdir -p "${XCFRAMEWORK_DIR}"

"$PLISTBUDDY_EXEC" -c "Add :CFBundlePackageType string XFWK"  "${INFO_PLIST}"
"$PLISTBUDDY_EXEC" -c "Add :XCFrameworkFormatVersion string 1.0"  "${INFO_PLIST}"
"$PLISTBUDDY_EXEC" -c "Add :AvailableLibraries array" "${INFO_PLIST}"

# Add iOS libs to XCFramework
IOS_LIB_IDENTIFIER="ios-arm64"
IOS_SIM_LIB_IDENTIFIER="ios-arm64_x86_64-simulator"

IOS_LIB_DIR="${XCFRAMEWORK_DIR}/${IOS_LIB_IDENTIFIER}/PJSipIOS.framework"
IOS_SIM_DIR="${XCFRAMEWORK_DIR}/${IOS_SIM_LIB_IDENTIFIER}/PJSipIOS.framework"

mkdir -p "$IOS_LIB_DIR/lib"
mkdir -p "$IOS_SIM_DIR/lib"

LIB_IOS_INDEX=0
LIB_IOS_SIMULATOR_INDEX=1
plist_add_library $LIB_IOS_INDEX $IOS_LIB_IDENTIFIER "ios"
plist_add_library $LIB_IOS_SIMULATOR_INDEX $IOS_SIM_LIB_IDENTIFIER "ios" "simulator"

plist_add_architecture $LIB_IOS_INDEX "arm64"
plist_add_architecture $LIB_IOS_SIMULATOR_INDEX "arm64"
plist_add_architecture $LIB_IOS_SIMULATOR_INDEX "x86_64"

cp -r $SOURCE_DIR/arm64/* "$IOS_LIB_DIR/lib"

cp -r $BASE_DIR/Sources/* "$IOS_LIB_DIR/" 
cp -r $BASE_DIR/Sources/* "$IOS_SIM_DIR/" 

ARM_SIM_DIR="$SOURCE_DIR/arm64-simulator"
X86_SIM_DIR="$SOURCE_DIR/x86_64-simulator"

for file in "$ARM_SIM_DIR"/*; do
    filename=$(basename "$file")
    lipo -create -output "$IOS_SIM_DIR/lib/$filename" "$ARM_SIM_DIR/$filename" "$X86_SIM_DIR/$filename"   
done

libtool -static -o $IOS_LIB_DIR/PJSipIOS $IOS_LIB_DIR/lib/* 
libtool -static -o $IOS_SIM_DIR/PJSipIOS $IOS_SIM_DIR/lib/*

rm -rf $IOS_LIB_DIR/lib 
rm -rf $IOS_SIM_DIR/lib 
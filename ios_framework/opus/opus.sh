#!/bin/bash

#  Automatic build script for libopus
#  for iPhoneOS and iPhoneSimulator

###########################################################################
#  Change values here                                                     #
#                                                                         #
VERSION="1.3.1"   
if test "x${MIN_IOS_VERSION}" = "x"; then
  MIN_IOS_VERSION="14.0"
  echo "$F: MIN_IOS_VERSION is not specified, using ${MIN_IOS_VERSION}"
fi                                                   #
#                                                                         #
###########################################################################
#                                                                         #
# Don't change anything under this line!                                  #
#                                                                         #
###########################################################################

#configure options
OPUS_CONFIGURE_OPTIONS="--enable-float-approx \
                        --disable-shared \
                        --enable-static \
                        --with-pic \
                        --disable-extra-programs \
                        --disable-doc"

CURRENTPATH=`pwd`
DEVELOPER=`xcode-select -print-path`

if [ ! -d "$DEVELOPER" ]; then
    echo "xcode path is not set correctly $DEVELOPER does not exist (most likely because of xcode > 4.3)"
    echo "run"
    echo "sudo xcode-select -switch <xcode path>"
    echo "for default installation:"
    echo "sudo xcode-select -switch /Applications/Xcode.app/Contents/Developer"
    exit 1
fi

case $DEVELOPER in
    *\ * )
    echo "Your Xcode path contains whitespaces, which is not supported."
    exit 1
    ;;
esac

case $CURRENTPATH in
    *\ * )
    echo "Your path contains whitespaces, which is not supported by 'make install'."
    exit 1
    ;;
esac

# packLibrary()
# {
#     LIBRARY=$1
#     lipo \
#         "${CURRENTPATH}/bin/iPhoneSimulator-x86_64.sdk/lib/lib${LIBRARY}.a" \
#         "${CURRENTPATH}/bin/iPhoneOS-arm64.sdk/lib/lib${LIBRARY}.a" \
#         "${CURRENTPATH}/bin/iPhoneOS-arm64e.sdk/lib/lib${LIBRARY}.a" \
#         -create -output ${CURRENTPATH}/build/lib/lib${LIBRARY}.a

#     lipo -info ${CURRENTPATH}/build/lib/lib${LIBRARY}.a
# }

set -e

if [ ! -e opus-${VERSION}.tar.gz ]; then
    echo "Downloading opus-${VERSION}.tar.gz"
    curl -O -L -s https://archive.mozilla.org/pub/opus/opus-${VERSION}.tar.gz
else
    echo "Using opus-${VERSION}.tar.gz"
fi

rm -rf "${CURRENTPATH}/bin"
rm -rf "${CURRENTPATH}/build"
rm -rf "${CURRENTPATH}/src"

mkdir -p "${CURRENTPATH}/bin"
mkdir -p "${CURRENTPATH}/build/lib"
mkdir -p "${CURRENTPATH}/build/include/opus"
mkdir -p "${CURRENTPATH}/src"

tar zxf opus-${VERSION}.tar.gz -C "${CURRENTPATH}/src"
cd "${CURRENTPATH}/src/opus-${VERSION}"

ARCH=$1
SDK=$2

if [[ "${ARCH}" == "x86_64" ]];
then
    HOST=x86_64-apple-darwin
else 
    HOST=arm-apple-darwin 
fi

if [[ "${SDK}" == "iphonesimulator" ]];
then
    PLATFORM="iPhoneSimulator"
    ios_version_flag="-mios-simulator-version-min=$MIN_IOS_VERSION"
else 
    PLATFORM="iPhoneOS"
    ios_version_flag="-miphoneos-version-min=$MIN_IOS_VERSION"
fi


export CROSS_TOP=`xcrun -sdk $SDK --show-sdk-platform-path`/Developer
export CROSS_SDK="${PLATFORM}.sdk"
export BUILD_TOOLS="${DEVELOPER}"

echo "Building opus-${VERSION} for ${PLATFORM} ${ARCH}"
echo "Please stand by..."

mkdir -p "${CURRENTPATH}/bin/${PLATFORM}-${ARCH}.sdk"
LOG="${CURRENTPATH}/bin/${PLATFORM}-${ARCH}.sdk/build-opus-${VERSION}.log"

export CC="xcrun -sdk ${SDK} clang $ios_version_flag -arch ${ARCH}"
CFLAGS="-arch ${ARCH} -D__OPTIMIZE__ -fembed-bitcode"

set +e
INSTALL_DIR="${CURRENTPATH}/bin/${PLATFORM}-${ARCH}.sdk"

./configure --host=${HOST} ${OPUS_CONFIGURE_OPTIONS} --prefix="${INSTALL_DIR}" CFLAGS="${CFLAGS}" > "${LOG}" 2>&1

if [ $? != 0 ];
then
    echo "Problem while configure - Please check ${LOG}"
    exit 1
fi

# add -isysroot to CC=
sed -ie "s!^CFLAG=!CFLAG=-isysroot ${CROSS_TOP}/SDKs/${CROSS_SDK} $ios_version_flag !" "Makefile"

if [ "$1" == "verbose" ];
then
    make -j$(sysctl -n hw.ncpu)
else
    make -j$(sysctl -n hw.ncpu) >> "${LOG}" 2>&1
fi

if [ $? != 0 ];
then
    echo "Problem while make - Please check ${LOG}"
    exit 1
fi

set -e
make install >> "${LOG}" 2>&1
make clean >> "${LOG}" 2>&1
cp $INSTALL_DIR/lib/*.a $CURRENTPATH/build/lib
cp $INSTALL_DIR/include/opus/*.h $CURRENTPATH/build/include/opus 

# packLibrary "opus"

cd ${CURRENTPATH}
echo "Building done."

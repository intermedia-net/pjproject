#!/bin/sh

MIN_IOS_VERSION="14.0"
BASE_DIR=`pwd -P`

for i in "$@"; do
  case $i in
    --source-dir=*)
      source_dir="${i#*=}"
      shift
      ;;
    --output-dir=*)
      output_dir="${i#*=}"
      shift
      ;;
    --name=*)
      framework_name="${i#*=}"
      shift
      ;;
    *)
      ;;
  esac
done

function check_or_exit() {
    if [ -z "$2" ]; then
        echo Please specify "$1"
        exit 1
    fi
}

check_or_exit "output directory using --output-dir" $output_dir
check_or_exit "source directory using --source-dir" $source_dir
check_or_exit "framework name using --name" $framework_name

library_dir=$BASE_DIR/Sources/lib
framework_dir="$BASE_DIR/Sources"
build_dir="$PWD/tmp"
xcframework_name="PJSipIOS.xcframework"
xcframework_path="$BASE_DIR/$xcframework_name"

# prerequisites
function checker() {
    which "$1" | grep -o "$1"  > /dev/null &&  return 0 || return 1
}

function check_and_install() {
    if ! checker $1 == 1; then
        echo $1 not installed. Installing it...
        brew install $1
    fi
}

function check_prerequisites () {
    echo "Verifying that we have everything that's needed for building pjsip"

    if checker xcodebuild == 1 ; then
        echo "xcodebuild: Installed";
    else 
        echo "Please install Xcode or Xcode CLI to be able to continue."
        echo "Exiting script"
        exit
    fi
    check_and_install make
    check_and_install cmake
    check_and_install automake
    check_and_install autoconf
}

function build_pjsip() {
    set -e

    arch=$1
    sdk=$2

    output_dir="$build_dir/$arch"
    if [ "$sdk" == "iphonesimulator" ]; then
        output_dir="${output_dir}-simulator"
    fi
    mkdir -p $output_dir

    echo output dir $output_dir

    arguments="--min-ios-version=$MIN_IOS_VERSION"
    arguments="$arguments --output-dir=$output_dir"
    arguments="$arguments --source-dir=$source_dir"
    arguments="$arguments --arch=$arch"
    arguments="$arguments --sdk=$sdk"
    arguments="$arguments --opus=$BASE_DIR/opus"
    arguments="$arguments --openssl=$BASE_DIR/openssl"
    arguments="$arguments --bcg729=$BASE_DIR/bcg729"

    sh ./build_pjsip_and_dependencies.sh $arguments
}

function archive_xcframework() {
    echo "Starting final assembly..."
    
    artefact_file_name="$framework_name.zip"
    zip -r "$artefact_file_name" ./$xcframework_name
    mkdir -p $output_dir
    mv $artefact_file_name $output_dir

    echo "Final assembly has finished and moved to $output_dir"
}

function copy_headers() {
    #copy headers
    local headers_dir=$1

    mkdir -p $headers_dir

    cp -r ${source_dir}/pjsip/include/* $headers_dir
    cp -r ${source_dir}/pjlib/include/pj $headers_dir
    cp -r ${source_dir}/pjlib/include/pjlib.h $headers_dir
    cp -r ${source_dir}/pjlib-util/include/* $headers_dir
    cp -r ${source_dir}/pjmedia/include/* $headers_dir
    cp -r ${source_dir}/pjnath/include/* $headers_dir
    #remove unneeded headers that was copied from "pjsip/include"
    prev_dir=`pwd -P`
    cd $headers_dir
    if [ -d "pjsua2" ]; then rm -Rf "pjsua2"; fi
    if [ -f "pjsua2.hpp" ]; then rm -Rf "pjsua2.hpp"; fi
    #replace namespace in headers
    #replace "include <p" with "include <PJSipIOS/p"
    find . -type f -name "*.h" -exec sed -i '.bak' 's/include \<p/include \<PJSipIOS\/p/gI' {} +
    #replace "pjsip_auth/" with "pjsip/" in pjsip_auth.h
    sed -i '' 's/pjsip_auth\//pjsip\//g' pjsip_auth.h
    #cleanup backup files
    find . -name '*.bak' -type f -delete
    cd $prev_dir
}

check_prerequisites

rm -rf $library_dir
mkdir -p $library_dir
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR
rm -rf $build_dir

cp $BASE_DIR/config_site.h "$source_dir/pjlib/include/pj"

build_pjsip arm64 iphoneos
build_pjsip arm64 iphonesimulator
build_pjsip x86_64 iphonesimulator

sh ./create_xcframework.sh $BASE_DIR $build_dir $xcframework_path
copy_headers $xcframework_path/ios-arm64/PJSipIOS.framework/Headers 
copy_headers $xcframework_path/ios-arm64_x86_64-simulator/PJSipIOS.framework/Headers

archive_xcframework
rm -rf $library_dir
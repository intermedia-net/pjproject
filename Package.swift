// swift-tools-version: 5.6
import PackageDescription

let package = Package(
    name: "PJSipIOS",
    platforms: [
        .iOS(.v15)
    ],
    products: [
        // This is what consumers will add to their targets
        .library(
            name: "PJSipIOS",
            targets: ["PJSipIOSWrapper"]
        )
    ],
    targets: [
        // The actual XCFramework downloaded from Releases
        .binaryTarget(
            name: "PJSipIOSBinary",
            url: "https://github.com/intermedia-net/pjproject/releases/download/2.14.1-24/pjsip-ios-2.14.1-24.zip",
            checksum: "ad0ad514b481430065608f161eb69bc31d819d53e91cfb5baa45d0ca074f2fb6"
        ),

        // Wrapper: lets you express linker settings (CocoaPods spec.framework equivalent)
        .target(
            name: "PJSipIOSWrapper",
            dependencies: [
                "PJSipIOSBinary"
            ],
            path: "SPMWrapper",
            linkerSettings: [
                .linkedFramework("AudioToolbox"),
                .linkedFramework("AVFoundation"),
                .linkedFramework("CFNetwork"),
                .linkedFramework("CoreMedia"),
                .linkedFramework("UIKit")
            ]
        )
    ]
)

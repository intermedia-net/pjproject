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
            url: "https://github.com/intermedia-net/pjproject/releases/download/2.14.1-27/pjsip-ios-2.14.1-27.zip",
            checksum: "f6c1d78196f9d939dd7c4832e6bde43ff86f5e0c3d728286605049be96b8872a"
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

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
            url: "https://github.com/intermedia-net/pjproject/releases/download/2.14.1-28/pjsip-ios-2.14.1-28.zip",
            checksum: "48d3c9500d10f62e102688b275cccb50f2b2c2c94ec8d7398793c1fb97fcb58e"
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

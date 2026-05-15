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
            url: "https://github.com/intermedia-net/pjproject/releases/download/2.14.1-26/pjsip-ios-2.14.1-26.zip",
            checksum: "2ab0371e922827bbb2d3b1f6236d67ec3f1837c9464a78f117d5e07a323904f6"
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

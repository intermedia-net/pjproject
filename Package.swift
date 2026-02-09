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
            url: "https://github.com/intermedia-net/pjproject/releases/download/2.14.1-23/pjsip-ios-2.14.1-23.zip",
            checksum: "1b6c25d38b526721f4ab5ec5dd66d747e5df40d863990295372d7e1ca25c5f52"
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

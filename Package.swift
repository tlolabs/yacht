// swift-tools-version: 6.0
import PackageDescription
import Foundation
let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().path
let package = Package(
    name: "Yacht", platforms: [.macOS(.v14)],
    products: [.library(name: "YachtCore", targets: ["YachtCore"]), .executable(name: "YachtApp", targets: ["YachtApp"])],
    dependencies: [.package(url: "https://github.com/sparkle-project/Sparkle", exact: "2.9.6")],
    targets: [
        .systemLibrary(name: "CYacht", path: "bindings/c"),
        .target(name: "YachtCore", dependencies: ["CYacht"], path: "platform/macos/Sources/YachtCore", linkerSettings: [.unsafeFlags(["-L", root + "/target/swift"]), .linkedLibrary("yacht_ffi"), .linkedLibrary("iconv")]),
        .executableTarget(name: "YachtApp", dependencies: ["YachtCore", .product(name: "Sparkle", package: "Sparkle")], path: "platform/macos/Sources/YachtApp"),
        .testTarget(name: "YachtAppTests", dependencies: ["YachtApp"], path: "platform/macos/Tests/YachtAppTests"),
        .testTarget(name: "YachtCoreTests", dependencies: ["YachtCore"], path: "platform/macos/Tests/YachtCoreTests")
    ]
)

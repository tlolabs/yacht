// swift-tools-version: 6.0
import PackageDescription
import Foundation
let root = URL(fileURLWithPath: #filePath).deletingLastPathComponent().path
let package = Package(
    name: "Yacht", platforms: [.macOS(.v14)],
    products: [.library(name: "YachtCore", targets: ["YachtCore"]), .executable(name: "YachtApp", targets: ["YachtApp"])],
    targets: [
        .systemLibrary(name: "CYacht", path: "bindings/c"),
        .target(name: "YachtCore", dependencies: ["CYacht"], path: "platform/macos/Sources/YachtCore", linkerSettings: [.unsafeFlags(["-L", root + "/target/swift"]), .linkedLibrary("yacht_ffi"), .linkedLibrary("iconv")]),
        .executableTarget(name: "YachtApp", dependencies: ["YachtCore"], path: "platform/macos/Sources/YachtApp"),
        .testTarget(name: "YachtCoreTests", dependencies: ["YachtCore"], path: "platform/macos/Tests/YachtCoreTests")
    ]
)

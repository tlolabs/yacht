// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "Yacht",
    platforms: [.macOS(.v14)],
    products: [
        .library(name: "YachtCore", targets: ["YachtCore"]),
        .executable(name: "yacht", targets: ["YachtCLI"]),
        .executable(name: "YachtApp", targets: ["YachtApp"])
    ],
    targets: [
        .target(name: "YachtCore"),
        .executableTarget(name: "YachtCLI", dependencies: ["YachtCore"]),
        .executableTarget(name: "YachtApp", dependencies: ["YachtCore"]),
        .testTarget(name: "YachtCoreTests", dependencies: ["YachtCore"], resources: [.copy("Fixtures")])
    ]
)

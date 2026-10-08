// swift-tools-version:5.9
// DXVKit - MIT License. Copyright (c) 2026 Amrit Rosell.
import PackageDescription

let package = Package(
    name: "DXVKit",
    platforms: [.macOS(.v12), .iOS(.v15), .tvOS(.v15)],
    products: [
        .library(name: "DXVKit", targets: ["DXVKit"]),
    ],
    targets: [
        // The C99 core (src/ + include/dxvkit.h); the module map exposes the C header only.
        .target(
            name: "CDXVKit",
            path: ".",
            sources: ["src"],
            publicHeadersPath: "include"
        ),
        .target(
            name: "DXVKit",
            dependencies: ["CDXVKit"],
            path: "swift/Sources/DXVKit"
        ),
        .testTarget(
            name: "DXVKitTests",
            dependencies: ["DXVKit"],
            path: "swift/Tests/DXVKitTests"
        ),
    ]
)

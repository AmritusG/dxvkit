// DXVKit Swift tests: every decode row of test_vectors/vectors.tsv through the Swift API, block
// streams and RGBA compared by SHA-256; every reject row must throw.
//
// MIT License. Copyright (c) 2026 Amrit Rosell. See LICENSE.

import CryptoKit
import DXVKit
import Foundation
import XCTest

final class DXVKitTests: XCTestCase {
    private let vectors = URL(fileURLWithPath: #filePath)
        .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        .deletingLastPathComponent().appendingPathComponent("test_vectors")

    private func sha(_ p: UnsafeRawBufferPointer) -> String {
        SHA256.hash(data: p).map { String(format: "%02x", $0) }.joined()
    }

    func testVectors() throws {
        let tsv = try String(contentsOf: vectors.appendingPathComponent("vectors.tsv"), encoding: .utf8)
        var decoded = 0, rejected = 0, streams = 0
        for line in tsv.split(separator: "\n") where !line.hasPrefix("#") {
            let col = line.split(separator: "\t", omittingEmptySubsequences: false).map(String.init)
            guard col.count >= 9 else { continue }
            let url = vectors.appendingPathComponent("streams/\(col[1])/\(col[0]).mov")
            let index = Int(col[2])!
            let hashes = Dictionary(uniqueKeysWithValues: (col.count > 9 ? col[9] : "")
                .split(separator: " ").compactMap { kv -> (String, String)? in
                    let p = kv.split(separator: "=", maxSplits: 1)
                    return p.count == 2 ? (String(p[0]), String(p[1])) : nil
                })
            if col[8] == "reject" {
                XCTAssertThrowsError(try {
                    let movie = try DXVMovie(url: url)
                    _ = try movie.decodeRGBA(at: index)
                }(), col[0])
                rejected += 1
                continue
            }
            let movie = try DXVMovie(url: url)
            for parallel in [false, true] {
                let frame = DXVFrame()
                try movie.decodeBlocks(at: index, into: frame, options: DXVOptions(parallel: parallel))
                let layout = try XCTUnwrap(frame.layout)
                XCTAssertEqual("\(layout.codedWidth)x\(layout.codedHeight)", col[5])
                let key0 = layout.planes[0].format == .bc1 ? "bc1" : layout.planes[0].format == .bc3 ? "bc3" : "plane-a"
                for (i, key) in [key0, "plane-b"].prefix(layout.planes.count).enumerated() {
                    frame.withPlane(i) { XCTAssertEqual(sha($0), hashes[key], "\(col[0]) frame \(index) plane \(i)") }
                    streams += parallel ? 0 : 1
                }
                let rgba = try frame.rgba(options: DXVOptions(parallel: parallel))
                rgba.withUnsafeBytes { XCTAssertEqual(sha($0), hashes["rgba"], "\(col[0]) frame \(index) rgba") }
            }
            decoded += 1
        }
        XCTAssertEqual(decoded, 91)
        XCTAssertEqual(streams, 127)
        XCTAssertEqual(rejected, 11)
    }

    func testMovieProperties() throws {
        let movie = try DXVMovie(url: vectors.appendingPathComponent("streams/resolume/resolume-alley_yg10_testsrc2_1920x1080.mov"))
        XCTAssertEqual(movie.width, 1920)
        XCTAssertEqual(movie.height, 1080)
        XCTAssertEqual(movie.frameCount, 3)
        XCTAssertEqual(movie.variant, .yg10)
        XCTAssertTrue(movie.hasAlpha)
        let layout = try movie.layout
        XCTAssertEqual(layout.codedHeight, 1088)
        XCTAssertEqual(layout.planes.map(\.format), [.bc5, .bc5])
        XCTAssertEqual(layout.planes[1].width, 960)

        // In-memory open gives the same packets.
        let data = try Data(contentsOf: vectors.appendingPathComponent("streams/resolume/resolume-alley_yg10_testsrc2_1920x1080.mov"))
        let memory = try DXVMovie(data: data)
        XCTAssertEqual(try memory.packet(at: 2), try movie.packet(at: 2))

        // Premultiply only changes colour where alpha < 255.
        let straight = try movie.decodeRGBA(at: 0)
        let premul = try movie.decodeRGBA(at: 0, options: DXVOptions(premultiply: true))
        XCTAssertEqual(straight.count, premul.count)
        XCTAssertNotEqual(straight, premul)

        XCTAssertThrowsError(try movie.decodeRGBA(at: 3))
        XCTAssertThrowsError(try DXVMovie(url: vectors.appendingPathComponent("vectors.tsv")))
    }
}

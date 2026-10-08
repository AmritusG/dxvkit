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
        XCTAssertEqual(decoded, 287)
        XCTAssertEqual(streams, 366)
        XCTAssertEqual(rejected, 47)
    }

    func testVersionAndArenaRules() throws {
        XCTAssertEqual(DXVKitVersion, "1.0.0")
        XCTAssertEqual(DXVKitLibraryVersion, DXVKitVersion)
        // Arena 7.28 rules: DXD4 loads, the packet decides the generation, stored HQ plays.
        let dxd4 = try DXVMovie(url: vectors.appendingPathComponent("streams/arena/dxvkit-w5_23_fourcc-DXD4-dxt1_256.mov"))
        XCTAssertEqual(dxd4.variant, .dxt1)
        let cross = try DXVMovie(url: vectors.appendingPathComponent("streams/arena/dxvkit-w5_28_fourcc-DXD3-holding-legacy-type23_256.mov"))
        XCTAssertEqual(cross.variant, .legacyDXT1)
        let stored = try DXVMovie(url: vectors.appendingPathComponent("streams/arena/dxvkit-w5_21_dxv3-yg10-storedflag_256.mov"))
        let control = try DXVMovie(url: vectors.appendingPathComponent("streams/arena/dxvkit-w5_20_dxv3-yg10-control_256.mov"))
        XCTAssertEqual(try stored.decodeRGBA(at: 3), try control.decodeRGBA(at: 3))
        // Grey 128 through the HQ encoder plays as 128 (chroma midpoint 127.5).
        let grey = Data((0..<64 * 64).flatMap { _ in [UInt8(128), 128, 128, 255] })
        for variant in [DXVVariant.ycg6, .yg10] {
            let enc = try DXVEncoder(variant: variant, width: 64, height: 64)
            let packet = try enc.encode(rgba: grey)
            let frame = DXVFrame()
            try packet.withUnsafeBytes {
                try frame.decode(packet: $0, generation: .auto, width: 64, height: 64)
            }
            XCTAssertEqual(try frame.rgba(), grey, variant.name)
        }
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

    func testEncoderRoundTrip() throws {
        let source = try DXVMovie(url: vectors.appendingPathComponent("streams/glenc/glenc_yg10_pat333x221.mov"))
        let rgba = try source.decodeRGBA(at: 0)
        for variant in [DXVVariant.dxt1, .dxt5, .ycg6, .yg10] {
            for options in [DXVEncodeOptions(), DXVEncodeOptions(effort: .best, parallel: true),
                            DXVEncodeOptions(opcodes: .stored)] {
                let enc = try DXVEncoder(variant: variant, width: source.width, height: source.height, options: options)
                let packet = try enc.encode(rgba: rgba)
                let out = FileManager.default.temporaryDirectory.appendingPathComponent("dxvkit-swift-\(variant.name).mov")
                let writer = try DXVMovieWriter(url: out, width: source.width, height: source.height,
                                                timescale: 30000, frameDuration: 1001)
                try writer.append(packet)
                try writer.append(packet)
                try writer.finish()
                let movie = try DXVMovie(url: out)
                XCTAssertEqual(movie.frameCount, 2)
                XCTAssertEqual(movie.variant, variant)
                XCTAssertEqual(movie.frameRate, 30000.0 / 1001.0, accuracy: 1e-9)
                XCTAssertEqual(try movie.packet(at: 1), packet)
                let frame = DXVFrame()
                try movie.decodeBlocks(at: 0, into: frame)
                for i in 0..<enc.layout.planes.count {
                    let packed = enc.withPackedPlane(i) { Data($0) }
                    frame.withPlane(i) { XCTAssertEqual(Data($0), packed, "\(variant.name) plane \(i)") }
                }
                // blocks-in path gives the same packet
                let repacked: Data = try frame.withPlane(0) { p0 in
                    if enc.layout.planes.count == 2 {
                        return try frame.withPlane(1) { p1 in try enc.pack(plane0: p0.baseAddress!, plane1: p1.baseAddress!) }
                    }
                    return try enc.pack(plane0: p0.baseAddress!)
                }
                XCTAssertEqual(repacked, packet)
                try? FileManager.default.removeItem(at: out)
            }
        }
        XCTAssertThrowsError(try DXVEncoder(variant: .ycg6, width: 64, height: 64, options: DXVEncodeOptions(storedPayload: true)))
        XCTAssertThrowsError(try DXVEncoder(variant: .legacyDXT1, width: 64, height: 64))
    }
}

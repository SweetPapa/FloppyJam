// Reject blank launch screens before recording or publishing native media.
import Foundation
import CoreGraphics
import ImageIO
import Vision

guard CommandLine.arguments.count == 2,
      let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: CommandLine.arguments[1]) as CFURL, nil),
      let image = CGImageSourceCreateImageAtIndex(source, 0, nil),
      let crop = image.cropping(to: CGRect(x: Double(image.width) * 0.1,
                                          y: Double(image.height) * 0.2,
                                          width: Double(image.width) * 0.8,
                                          height: Double(image.height) * 0.55)) else { exit(2) }
let side = 128
var pixels = [UInt8](repeating: 0, count: side * side * 4)
let fraction = pixels.withUnsafeMutableBytes { bytes -> Double in
    guard let context = CGContext(data: bytes.baseAddress, width: side, height: side,
                                  bitsPerComponent: 8, bytesPerRow: side * 4,
                                  space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue) else { return 0 }
    context.draw(crop, in: CGRect(x: 0, y: 0, width: side, height: side))
    let values = bytes.bindMemory(to: UInt8.self)
    var colorful = 0
    for i in stride(from: 0, to: values.count, by: 4) {
        let high = max(values[i], values[i + 1], values[i + 2])
        let low = min(values[i], values[i + 1], values[i + 2])
        if high > 75 && high - low > 35 { colorful += 1 }
    }
    return Double(colorful) / Double(side * side)
}
// Dark neon stages can have only 2–3% colorful pixels; launch blanks have 0%.
print("Visible scene color fraction: \(fraction)")
if CommandLine.arguments[1].contains("-stage-") {
    let request = VNRecognizeTextRequest()
    request.recognitionLevel = .fast
    try VNImageRequestHandler(cgImage: image).perform([request])
    let words = (request.results ?? []).compactMap { $0.topCandidates(1).first?.string.uppercased() }.joined(separator: " ")
    if words.contains("LEVEL COMPLETE") || words.contains("PLAY AGAIN") {
        print("Rejected completion-menu capture; active gameplay is required.")
        exit(1)
    }
}
exit(fraction >= 0.01 ? 0 : 1)

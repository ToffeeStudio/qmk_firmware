// Read-only OCR of an explicitly supplied meter image/normalized ROI.
// No camera ownership, USB access, image rewriting or inferred units.
import Foundation
import Vision
import ImageIO

let args = CommandLine.arguments
guard args.count == 6, let x = Double(args[2]), let y = Double(args[3]),
      let w = Double(args[4]), let h = Double(args[5]),
      x >= 0, y >= 0, w > 0, h > 0, x + w <= 1, y + h <= 1 else {
    fatalError("Required: IMAGE X Y WIDTH HEIGHT; normalized lower-left ROI coordinates")
}
let request = VNRecognizeTextRequest()
request.recognitionLevel = .accurate
request.usesLanguageCorrection = false
request.recognitionLanguages = ["en-US"]
request.regionOfInterest = CGRect(x: x, y: y, width: w, height: h)
let handler = VNImageRequestHandler(url: URL(fileURLWithPath: args[1]), options: [:])
try handler.perform([request])
let rows = (request.results ?? []).map { observation -> [String: Any] in
    let candidate = observation.topCandidates(1)[0]
    return ["text": candidate.string, "confidence": candidate.confidence,
            "bounds": [observation.boundingBox.minX, observation.boundingBox.minY,
                       observation.boundingBox.width, observation.boundingBox.height]]
}
let data = try JSONSerialization.data(withJSONObject: rows, options: [.sortedKeys])
print(String(data: data, encoding: .utf8)!)

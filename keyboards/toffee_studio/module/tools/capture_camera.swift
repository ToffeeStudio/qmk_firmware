import AVFoundation
import CoreImage
import Foundation
import ImageIO
import UniformTypeIdentifiers

struct CaptureError: Error, CustomStringConvertible {
    let description: String
}

final class Snapshot: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate {
    let destination: URL
    let finished = DispatchSemaphore(value: 0)
    var error: Error?
    private var firstFrame: Date?
    private var captured = false

    init(destination: URL) {
        self.destination = destination
    }

    func captureOutput(_ output: AVCaptureOutput, didOutput sampleBuffer: CMSampleBuffer, from connection: AVCaptureConnection) {
        if captured { return }
        if firstFrame == nil {
            firstFrame = Date()
            return
        }
        if Date().timeIntervalSince(firstFrame!) < 2 { return }
        captured = true
        defer { finished.signal() }
        do {
            guard let pixels = CMSampleBufferGetImageBuffer(sampleBuffer) else {
                throw CaptureError(description: "Camera frame has no image buffer.")
            }
            let image = CIImage(cvPixelBuffer: pixels)
            guard let rendered = CIContext().createCGImage(image, from: image.extent),
                  let file = CGImageDestinationCreateWithURL(destination as CFURL, UTType.png.identifier as CFString, 1, nil) else {
                throw CaptureError(description: "Cannot create the requested PNG image.")
            }
            CGImageDestinationAddImage(file, rendered, nil)
            guard CGImageDestinationFinalize(file) else {
                throw CaptureError(description: "Cannot write the requested PNG image.")
            }
        } catch {
            self.error = error
        }
    }
}

func capture() throws {
    let discovery = AVCaptureDevice.DiscoverySession(deviceTypes: [.builtInWideAngleCamera, .external], mediaType: .video, position: .unspecified)
    let args = Array(CommandLine.arguments.dropFirst())
    if args == ["--list"] {
        print("Camera authorization: \(AVCaptureDevice.authorizationStatus(for: .video).rawValue) (0=unrequested, 1=restricted, 2=denied, 3=authorized)")
        for device in discovery.devices { print("\(device.uniqueID) | \(device.localizedName)") }
        return
    }
    guard args.count == 2 else {
        throw CaptureError(description: "Usage: module_camera --list OR module_camera DEVICE_ID OUTPUT.png")
    }
    guard let device = discovery.devices.first(where: { $0.uniqueID == args[0] }) else {
        throw CaptureError(description: "The required camera ID is not available; run --list.")
    }
    let destination = URL(fileURLWithPath: args[1])
    guard destination.pathExtension == "png", FileManager.default.fileExists(atPath: destination.deletingLastPathComponent().path) else {
        throw CaptureError(description: "Provide a .png output path in an existing directory.")
    }
    if AVCaptureDevice.authorizationStatus(for: .video) == .notDetermined {
        var allowed: Bool?
        AVCaptureDevice.requestAccess(for: .video) { granted in
            DispatchQueue.main.async { allowed = granted }
        }
        let deadline = Date().addingTimeInterval(60)
        while allowed == nil && Date() < deadline {
            RunLoop.current.run(until: Date().addingTimeInterval(0.1))
        }
        guard allowed == true else {
            throw CaptureError(description: "Camera access was not granted. Allow the camera in macOS Privacy & Security, then retry.")
        }
    }
    guard AVCaptureDevice.authorizationStatus(for: .video) == .authorized else {
        throw CaptureError(description: "Camera access is denied or restricted in macOS Privacy & Security.")
    }
    let session = AVCaptureSession()
    session.sessionPreset = .high
    let input = try AVCaptureDeviceInput(device: device)
    guard session.canAddInput(input) else { throw CaptureError(description: "Cannot attach the selected camera.") }
    session.addInput(input)
    let output = AVCaptureVideoDataOutput()
    output.videoSettings = [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA]
    guard session.canAddOutput(output) else { throw CaptureError(description: "Cannot attach the camera frame output.") }
    session.addOutput(output)
    let snapshot = Snapshot(destination: destination)
    output.setSampleBufferDelegate(snapshot, queue: DispatchQueue(label: "studio.toffee.led-camera"))
    session.startRunning()
    defer { session.stopRunning() }
    guard snapshot.finished.wait(timeout: .now() + 20) == .success else {
        throw CaptureError(description: "Camera did not deliver a settled frame within 20 seconds.")
    }
    if let error = snapshot.error { throw error }
    print("Captured \(device.localizedName): \(destination.path)")
}

do {
    try capture()
} catch {
    FileHandle.standardError.write(Data("Module camera: \(error)\n".utf8))
    exit(1)
}

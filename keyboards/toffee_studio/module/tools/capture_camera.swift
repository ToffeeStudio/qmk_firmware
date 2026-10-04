import AVFoundation
import CoreImage
import Foundation
import ImageIO
import UniformTypeIdentifiers

struct CaptureError: Error, CustomStringConvertible {
    let description: String
}

let logLock = NSLock()
func emit(_ event: String, _ fields: [String: Any] = [:]) {
    var record = fields
    record["event"] = event
    record["time"] = ISO8601DateFormatter().string(from: Date())
    record["pid"] = ProcessInfo.processInfo.processIdentifier
    logLock.lock()
    defer { logLock.unlock() }
    let data = try! JSONSerialization.data(withJSONObject: record, options: [.sortedKeys])
    FileHandle.standardOutput.write(data + Data("\n".utf8))
}

func destinationURL(_ path: String) throws -> URL {
    let destination = URL(fileURLWithPath: path)
    guard destination.pathExtension == "png", FileManager.default.fileExists(atPath: destination.deletingLastPathComponent().path) else {
        throw CaptureError(description: "Provide a .png output path in an existing directory.")
    }
    return destination
}

enum FrameDestination {
    case pngFile(URL)
    case jpegMemory
}

final class Snapshot: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate {
    let queue = DispatchQueue(label: "studio.toffee.led-camera")
    let ready = DispatchSemaphore(value: 0)
    private let finished = DispatchSemaphore(value: 0)
    private let context = CIContext()
    private var destination: FrameDestination?
    private var result: Result<[String: Any], Error>?
    private var firstFrame: TimeInterval?
    private var lastFrame: TimeInterval?
    private var settled = false
    private var frames = 0
    private var snapshots = 0
    private var starts = 0
    private var stops = 0
    private var interruptions = 0

    func event(_ name: String, detail: String) {
        queue.async {
            if name == "session_started" { self.starts += 1 }
            if name == "session_stopped" { self.stops += 1 }
            if name == "session_interrupted" { self.interruptions += 1 }
            emit(name, ["detail": detail])
        }
    }

    func status() -> [String: Any] {
        queue.sync {
            var info: [String: Any] = ["frames": frames, "snapshots": snapshots,
                "session_starts": starts, "session_stops": stops, "interruptions": interruptions]
            if let lastFrame { info["frame_age_ms"] = Int((ProcessInfo.processInfo.systemUptime - lastFrame) * 1000) }
            return info
        }
    }

    // Each request waits for a NEW frame, rather than returning a cached image.
    func save(to destination: FrameDestination) throws -> [String: Any] {
        queue.sync {
            self.result = nil
            self.destination = destination
        }
        guard finished.wait(timeout: .now() + 5) == .success else {
            throw CaptureError(description: "No fresh snapshot within 5 seconds; camera session must be inspected.")
        }
        return try queue.sync { try result!.get() }
    }

    func captureOutput(_ output: AVCaptureOutput, didOutput sampleBuffer: CMSampleBuffer, from connection: AVCaptureConnection) {
        let now = ProcessInfo.processInfo.systemUptime
        frames += 1
        lastFrame = now
        if firstFrame == nil { firstFrame = now }
        if !settled {
            if now - firstFrame! < 2 { return }
            settled = true
            ready.signal()
        }
        guard let destination else { return }
        self.destination = nil
        defer { finished.signal() }
        do {
            guard let pixels = CMSampleBufferGetImageBuffer(sampleBuffer) else {
                throw CaptureError(description: "Camera frame has no image buffer.")
            }
            let image = CIImage(cvPixelBuffer: pixels)
            let data = NSMutableData()
            let format: UTType
            switch destination {
            case .pngFile: format = .png
            case .jpegMemory: format = .jpeg
            }
            guard let rendered = context.createCGImage(image, from: image.extent),
                  let file = CGImageDestinationCreateWithData(data, format.identifier as CFString, 1, nil) else {
                throw CaptureError(description: "Cannot create the requested image.")
            }
            CGImageDestinationAddImage(file, rendered, [kCGImageDestinationLossyCompressionQuality: 0.85] as CFDictionary)
            guard CGImageDestinationFinalize(file) else {
                throw CaptureError(description: "Cannot encode the requested image.")
            }
            snapshots += 1
            var info: [String: Any] = ["frame": frames,
                "frame_uptime_seconds": now,
                "camera_pts_seconds": CMSampleBufferGetPresentationTimeStamp(sampleBuffer).seconds,
                "width": rendered.width, "height": rendered.height, "snapshot": snapshots]
            switch destination {
            case .pngFile(let path):
                try (data as Data).write(to: path, options: .atomic)
                info["path"] = path.path
            case .jpegMemory:
                info["jpeg_base64"] = (data as Data).base64EncodedString()
            }
            result = .success(info)
        } catch {
            result = .failure(error)
        }
    }
}

func capture() throws {
    let discovery = AVCaptureDevice.DiscoverySession(deviceTypes: [.builtInWideAngleCamera, .external, .continuityCamera], mediaType: .video, position: .unspecified)
    let args = Array(CommandLine.arguments.dropFirst())
    if args == ["--list"] {
        print("Camera authorization: \(AVCaptureDevice.authorizationStatus(for: .video).rawValue) (0=unrequested, 1=restricted, 2=denied, 3=authorized)")
        for device in discovery.devices { print("\(device.uniqueID) | \(device.localizedName)") }
        return
    }
    guard args.count == 2 else {
        throw CaptureError(description: "Usage: module_camera --list OR module_camera DEVICE_ID OUTPUT.png OR module_camera --serve DEVICE_ID")
    }
    let serving = args[0] == "--serve"
    let deviceID = serving ? args[1] : args[0]
    guard let device = discovery.devices.first(where: { $0.uniqueID == deviceID }) else {
        throw CaptureError(description: "The required camera ID is not available; run --list.")
    }
    let destination = serving ? nil : try destinationURL(args[1])
    if AVCaptureDevice.authorizationStatus(for: .video) == .notDetermined {
        let permission = DispatchSemaphore(value: 0)
        AVCaptureDevice.requestAccess(for: .video) { granted in
            permission.signal()
        }
        guard permission.wait(timeout: .now() + 60) == .success else {
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
    output.alwaysDiscardsLateVideoFrames = true
    output.videoSettings = [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA]
    guard session.canAddOutput(output) else { throw CaptureError(description: "Cannot attach the camera frame output.") }
    session.addOutput(output)
    let snapshot = Snapshot()
    output.setSampleBufferDelegate(snapshot, queue: snapshot.queue)
    let notifications: [(Notification.Name, String)] = [
        (AVCaptureSession.didStartRunningNotification, "session_started"),
        (AVCaptureSession.didStopRunningNotification, "session_stopped"),
        (AVCaptureSession.wasInterruptedNotification, "session_interrupted"),
        (AVCaptureSession.interruptionEndedNotification, "interruption_ended"),
        (AVCaptureSession.runtimeErrorNotification, "session_error"),
    ]
    let observers = notifications.map { name, event in
        NotificationCenter.default.addObserver(forName: name, object: session, queue: nil) { notification in
            snapshot.event(event, detail: notification.userInfo.map { String(describing: $0) } ?? "")
        }
    }
    defer { observers.forEach { NotificationCenter.default.removeObserver($0) } }
    session.startRunning()
    defer {
        session.stopRunning()
        emit("closed", snapshot.status())
    }
    guard snapshot.ready.wait(timeout: .now() + 20) == .success else {
        throw CaptureError(description: "Camera did not deliver a settled frame within 20 seconds.")
    }
    if let destination {
        emit("snapshot", try snapshot.save(to: .pngFile(destination)))
        return
    }
    emit("ready", ["camera": device.localizedName, "device_id": device.uniqueID,
        "commands": "snapshot /absolute/path.png | frame | status | quit"])
    while let command = readLine() {
        if command == "quit" { return }
        if command == "frame" {
            emit("frame", try snapshot.save(to: .jpegMemory))
            continue
        }
        if command == "status" {
            var info = snapshot.status()
            info["running"] = session.isRunning
            emit("status", info)
            continue
        }
        let destination: URL
        do {
            guard command.hasPrefix("snapshot ") else {
                throw CaptureError(description: "Expected snapshot /absolute/path.png, frame, status or quit.")
            }
            destination = try destinationURL(String(command.dropFirst("snapshot ".count)))
        } catch {
            emit("request_error", ["message": String(describing: error)])
            continue
        }
        // A capture failure terminates; there is no hidden restart or stale-frame fallback.
        emit("snapshot", try snapshot.save(to: .pngFile(destination)))
    }
}

// Session work/stdin can block; leave the main run loop free for AVFoundation
// lifecycle notifications and permission callbacks throughout the connection.
DispatchQueue.global(qos: .userInitiated).async {
    do {
        try capture()
        exit(0)
    } catch {
        FileHandle.standardError.write(Data("Module camera: \(error)\n".utf8))
        exit(1)
    }
}
RunLoop.main.run()

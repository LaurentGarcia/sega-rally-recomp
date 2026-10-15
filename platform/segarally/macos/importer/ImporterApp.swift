import AppKit
import SwiftUI
import UniformTypeIdentifiers

@MainActor final class ImportModel: ObservableObject {
    @Published var status = "Build your own local copy"
    @Published var detail = "Choose your Sega Rally Revision B ZIP. The game is prepared on this Mac; your archive is never uploaded."
    @Published var log = ""
    @Published var busy = false
    @Published var progress: Double? = nil
    @Published var session: String? = nil
    @Published var folder: String? = nil
    private var process: Process?
    private var pending = Data()
    private var terminationReply = false

    init() {
        if let saved = UserDefaults.standard.string(forKey: "CompletedImport") {
            session = saved; folder = saved
            detail = "A previous local import is available. Launch it below, or choose a ZIP to build another copy. The app is verified again before launch."
        }
    }
    func choose() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.zip]
        panel.canChooseDirectories = false
        panel.allowsMultipleSelection = false
        if panel.runModal() == .OK, let url = panel.url { start(["--import-rom", url.path]) }
    }
    func python() -> String? {
        let candidates = ["/opt/homebrew/bin/python3", "/usr/local/bin/python3", "/usr/bin/python3"]
        for candidate in candidates where FileManager.default.isExecutableFile(atPath: candidate) {
            let p = Process(); let pipe = Pipe()
            p.executableURL = URL(fileURLWithPath: candidate)
            p.arguments = ["-c", "import sys; raise SystemExit(0 if sys.version_info >= (3,10) else 1)"]
            p.standardOutput = pipe; p.standardError = pipe
            do { try p.run(); p.waitUntilExit(); if p.terminationStatus == 0 { return candidate } } catch { }
        }
        return nil
    }
    func start(_ arguments: [String]) {
        guard !busy else { return }
        // Avoid invoking Apple's Python/Git launcher stubs before tools exist.
        let tools = Process(); let toolsPipe = Pipe()
        tools.executableURL = URL(fileURLWithPath: "/usr/bin/xcode-select")
        tools.arguments = ["-p"]; tools.standardOutput = toolsPipe; tools.standardError = toolsPipe
        do {
            try tools.run(); tools.waitUntilExit()
            if tools.terminationStatus != 0 {
                status = "Apple Command Line Tools are needed"
                detail = "In Terminal, run xcode-select --install, complete Apple's setup, then reopen this importer. No tools have been installed automatically."
                return
            }
        } catch { status = "Could not check build tools"; detail = error.localizedDescription; return }
        guard let interpreter = python(), let script = Bundle.main.url(forResource: "coordinator", withExtension: "py") else {
            status = "Python is needed first"
            detail = "Install Python 3.10 or newer from python.org, then reopen this importer. Nothing has been changed."
            return
        }
        if arguments.first == "--import-rom" { session = nil; folder = nil; log = "" }
        let p = Process(); let pipe = Pipe()
        p.executableURL = URL(fileURLWithPath: interpreter)
        p.arguments = [script.path] + arguments
        p.standardOutput = pipe; p.standardError = pipe
        pending = Data(); busy = true; progress = nil
        status = arguments.first == "--launch" ? "Opening your app" : "Checking your Mac"
        detail = "The first import downloads the pinned source and three build dependencies. Git, CMake and Apple Command Line Tools must already be installed."
        process = p
        do {
            try p.run()
            // One reader owns the pipe. Main-queue delivery preserves event
            // order and drains completion before changing the busy state.
            DispatchQueue.global(qos: .utility).async { [weak self] in
                while true {
                    let data = pipe.fileHandleForReading.readData(ofLength: 8192)
                    if data.isEmpty { break }
                    DispatchQueue.main.async { self?.consume(data) }
                }
                p.waitUntilExit()
                pipe.fileHandleForReading.closeFile()
                DispatchQueue.main.async {
                    guard let self else { return }
                    self.busy = false; self.process = nil
                    if p.terminationStatus != 0 && self.status != "Cancelled" {
                        self.status = "Import did not finish"
                        if self.detail.isEmpty { self.detail = "Your files are preserved. Review the log and retry to create a separate build." }
                    }
                    if self.terminationReply { NSApp.reply(toApplicationShouldTerminate: true) }
                }
            }
        } catch {
            busy = false; process = nil; status = "Could not start"; detail = error.localizedDescription
        }
    }
    func consume(_ data: Data) {
        pending.append(data)
        while let newline = pending.firstIndex(of: 10) {
            let line = pending.prefix(upTo: newline); pending.removeSubrange(...newline)
            guard let value = try? JSONSerialization.jsonObject(with: line) as? [String: Any], let kind = value["kind"] as? String else {
                appendLog(String(decoding: line, as: UTF8.self) + "\n"); continue
            }
            switch kind {
            case "log": appendLog(value["text"] as? String ?? "")
            case "phase":
                status = value["text"] as? String ?? "Preparing your app"
                if let step = value["step"] as? Double, let steps = value["steps"] as? Double, steps > 0 { progress = step / steps }
            case "workspace": folder = value["path"] as? String
            case "complete":
                session = value["session"] as? String; progress = 1
                if let session { UserDefaults.standard.set(session, forKey: "CompletedImport") }
                status = "Your app is ready"; detail = "Launch your local copy below. Existing apps and their saves were not changed."
            case "launched": status = "App opened"; detail = "Your local app uses its own save folder. You can close this importer."
            case "cancelled": status = "Cancelled"; detail = value["text"] as? String ?? "Files are preserved. Retry starts a new build."
            case "error": detail = value["text"] as? String ?? "See the log for details."; appendLog(detail + "\n")
            default: break
            }
        }
    }
    func appendLog(_ text: String) {
        log += text
        if log.utf8.count > 200_000 { log = String(log.suffix(100_000)) }
    }
    func cancel() { status = "Cancelling safely…"; process?.terminate() }
    func quit() -> NSApplication.TerminateReply {
        if !busy { return .terminateNow }
        terminationReply = true; cancel(); return .terminateLater
    }
}

struct ImportView: View {
    @ObservedObject var model: ImportModel
    @State private var showLog = false
    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Sega Rally Native").font(.largeTitle.bold())
            Text("Local app builder").font(.title3).foregroundStyle(.secondary)
            Divider()
            Text(model.status).font(.headline)
            Text(model.detail).textSelection(.enabled).fixedSize(horizontal: false, vertical: true)
            if model.busy {
                if let value = model.progress { ProgressView(value: value) } else { ProgressView().controlSize(.small) }
            }
            HStack {
                Button(model.folder == nil ? "Choose ROM ZIP…" : "Choose ZIP / Retry…") { model.choose() }.disabled(model.busy)
                if model.busy { Button("Cancel") { model.cancel() } }
                if let session = model.session { Button("Launch Sega Rally") { model.start(["--launch", session]) }.disabled(model.busy) }
                if let folder = model.folder { Button("Show Files") { NSWorkspace.shared.open(URL(fileURLWithPath: folder)) } }
            }
            DisclosureGroup("Build log", isExpanded: $showLog) {
                ScrollView { Text(model.log.isEmpty ? "No build has started." : model.log).font(.system(.caption, design: .monospaced)).textSelection(.enabled).frame(maxWidth: .infinity, alignment: .leading) }.frame(minHeight: 120, maxHeight: 260)
            }
            Spacer(minLength: 0)
            Text("No game data is included. Use only game files you are entitled to use. First setup needs internet access and several GB of free space. This Apple Silicon preview is ad-hoc signed and is not notarized.").font(.caption).foregroundStyle(.secondary)
        }.padding(24).frame(minWidth: 620, idealWidth: 720, minHeight: 440)
    }
}

@MainActor final class AppDelegate: NSObject, NSApplicationDelegate {
    let model = ImportModel()
    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply { model.quit() }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}

@main struct ImporterApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var delegate
    var body: some Scene {
        WindowGroup("Sega Rally Native — Local Builder") { ImportView(model: delegate.model) }
        .windowStyle(.titleBar)
        .commands { CommandGroup(replacing: .newItem) {} }
    }
}

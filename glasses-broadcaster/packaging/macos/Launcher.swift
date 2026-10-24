import AppKit

@main
enum Launcher {
    @MainActor
    static func main() {
        let headless = CommandLine.arguments.contains("--check-bundle")
        guard let command = Bundle.main.url(forResource: "launch", withExtension: "command"),
              FileManager.default.isExecutableFile(atPath: command.path) else {
            fail("The bundled terminal command is missing. Reinstall Glasses Broadcaster.", headless: headless)
        }
        if headless {
            print(command.path)
            return
        }

        let process = Process()
        process.executableURL = URL(fileURLWithPath: "/usr/bin/open")
        process.arguments = ["-b", "com.apple.Terminal", command.path]
        do {
            try process.run()
            process.waitUntilExit()
            if process.terminationStatus != 0 {
                fail("Terminal could not be opened (exit \(process.terminationStatus)).", headless: false)
            }
        } catch {
            fail("Terminal could not be opened: \(error.localizedDescription)", headless: false)
        }
    }

    @MainActor
    private static func fail(_ message: String, headless: Bool) -> Never {
        FileHandle.standardError.write(Data("\(message)\n".utf8))
        if !headless {
            let alert = NSAlert()
            alert.messageText = "Glasses Broadcaster could not start"
            alert.informativeText = message
            alert.alertStyle = .critical
            alert.runModal()
        }
        exit(1)
    }
}
import AppKit

// 1. Draw the app's original lettermark at each standard macOS icon size.
let outputDirectory = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
try FileManager.default.createDirectory(at: outputDirectory, withIntermediateDirectories: true)
for size in [16, 32, 128, 256, 512] {
    for scale in [1, 2] {
        let pixels = size * scale
        let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: pixels, pixelsHigh: pixels,
            bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
            colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
        NSGraphicsContext.saveGraphicsState()
        NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: bitmap)
        let inset = CGFloat(pixels) * 0.08
        let rect = NSRect(x: inset, y: inset, width: CGFloat(pixels) - 2 * inset, height: CGFloat(pixels) - 2 * inset)
        NSColor(calibratedRed: 0.714, green: 0.898, blue: 0.788, alpha: 1).setFill()
        NSBezierPath(roundedRect: rect, xRadius: CGFloat(pixels) * 0.19, yRadius: CGFloat(pixels) * 0.19).fill()
        let attributes: [NSAttributedString.Key: Any] = [
            .font: NSFont.systemFont(ofSize: CGFloat(pixels) * 0.67, weight: .semibold),
            .foregroundColor: NSColor(calibratedRed: 0.063, green: 0.078, blue: 0.086, alpha: 1)
        ]
        let mark = "n" as NSString
        let textSize = mark.size(withAttributes: attributes)
        mark.draw(at: NSPoint(x: (CGFloat(pixels) - textSize.width) / 2,
            y: (CGFloat(pixels) - textSize.height) / 2 + CGFloat(pixels) * 0.025), withAttributes: attributes)
        NSGraphicsContext.restoreGraphicsState()

        // 2. Write lossless PNGs for iconutil; no network or third-party image assets are used.
        let suffix = scale == 2 ? "@2x" : ""
        let filename = "icon_\(size)x\(size)\(suffix).png"
        try bitmap.representation(using: .png, properties: [:])!.write(to: outputDirectory.appendingPathComponent(filename))
    }
}

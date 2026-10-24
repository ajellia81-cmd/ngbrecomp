// Reuse the upstream app icon; only rasterize it to Apple's packaging sizes.
import AppKit
import Foundation

guard CommandLine.arguments.count == 4,
      let source = NSImage(contentsOfFile: CommandLine.arguments[1]) else {
    fatalError("Expected upstream icon, app bundle and scratch directory")
}
let bundle = URL(fileURLWithPath: CommandLine.arguments[2])
let scratch = URL(fileURLWithPath: CommandLine.arguments[3])
let catalog = scratch.appendingPathComponent("Icons.xcassets")
let icons = catalog.appendingPathComponent("AppIcon.appiconset")
try FileManager.default.createDirectory(at: icons, withIntermediateDirectories: true)
let sizes: [(String, Int, String)] = [
    ("20x20", 2, "iphone"), ("20x20", 3, "iphone"),
    ("29x29", 2, "iphone"), ("29x29", 3, "iphone"),
    ("40x40", 2, "iphone"), ("40x40", 3, "iphone"),
    ("60x60", 2, "iphone"), ("60x60", 3, "iphone"),
    ("1024x1024", 1, "ios-marketing")]
var images: [[String: String]] = []
for (size, scale, idiom) in sizes {
    let side = Int(size.split(separator: "x")[0])! * scale
    let name = "icon-\(size)-\(scale).png"
    guard let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: side,
        pixelsHigh: side, bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true,
        isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: side * 4,
        bitsPerPixel: 32), let context = NSGraphicsContext(bitmapImageRep: bitmap) else {
        fatalError("Cannot rasterize app icon")
    }
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = context
    context.imageInterpolation = .high
    source.draw(in: NSRect(x: 0, y: 0, width: CGFloat(side), height: CGFloat(side)),
                from: .zero, operation: .copy, fraction: 1)
    NSGraphicsContext.restoreGraphicsState()
    bitmap.hasAlpha = false
    guard let png = bitmap.representation(using: .png, properties: [:]) else {
        fatalError("Cannot encode app icon")
    }
    try png.write(to: icons.appendingPathComponent(name))
    images.append(["size": size, "scale": "\(scale)x", "idiom": idiom, "filename": name])
}
let info: [String: Any] = ["version": 1, "author": "ngbrecomp"]
try JSONSerialization.data(withJSONObject: ["info": info]).write(to: catalog.appendingPathComponent("Contents.json"))
try JSONSerialization.data(withJSONObject: ["info": info, "images": images]).write(to: icons.appendingPathComponent("Contents.json"))
let partial = scratch.appendingPathComponent("icon-info.plist")
let process = Process()
process.executableURL = URL(fileURLWithPath: "/usr/bin/xcrun")
process.arguments = ["actool", catalog.path, "--compile", bundle.path,
    "--platform", "iphoneos", "--minimum-deployment-target", "15.0",
    "--target-device", "iphone", "--app-icon", "AppIcon",
    "--output-partial-info-plist", partial.path]
try process.run()
process.waitUntilExit()
guard process.terminationStatus == 0 else { fatalError("Asset catalog compilation failed") }
let plist = bundle.appendingPathComponent("Info.plist")
var metadata = try PropertyListSerialization.propertyList(from: Data(contentsOf: plist), format: nil) as! [String: Any]
let extra = try PropertyListSerialization.propertyList(from: Data(contentsOf: partial), format: nil) as! [String: Any]
metadata.merge(extra) { _, iconValue in iconValue }
try PropertyListSerialization.data(fromPropertyList: metadata, format: .xml, options: 0).write(to: plist)
print("Upstream NGB icon packaged in iPhone asset catalog")

// SPDX-License-Identifier: AGPL-3.0-or-later
// Run: swift generate-icon.swift /path/to/RitualCodec.png
import AppKit

let size = 1024
let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: size, pixelsHigh: size,
                              bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true,
                              isPlanar: false, colorSpaceName: .deviceRGB,
                              bytesPerRow: 0, bitsPerPixel: 0)!
NSGraphicsContext.saveGraphicsState()
NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: bitmap)
NSGraphicsContext.current?.imageInterpolation = .high
func colour(_ red: CGFloat, _ green: CGFloat, _ blue: CGFloat) -> NSColor {
    NSColor(srgbRed: red / 255, green: green / 255, blue: blue / 255, alpha: 1)
}
let tile = NSBezierPath(roundedRect: NSRect(x: 64, y: 64, width: 896, height: 896),
                        xRadius: 200, yRadius: 200)
NSGradient(starting: colour(13, 9, 17), ending: colour(43, 23, 56))!.draw(in: tile, angle: 90)
colour(117, 65, 131).setStroke()
tile.lineWidth = 3
tile.stroke()
// Preserve the header emblem's double ring and seven-point waveform.
for (diameter, width) in [(600.0, 14.0), (504.0, 8.0)] {
    let ring = NSBezierPath(ovalIn: NSRect(x: 512 - diameter / 2, y: 512 - diameter / 2,
                                          width: diameter, height: diameter))
    colour(173, 98, 196).setStroke()
    ring.lineWidth = width
    ring.stroke()
}
let pulse = NSBezierPath()
let points: [(Double, Double)] = [(-20, 0), (-11, 0), (-5, 13), (1, -14), (7, 7), (12, 0), (20, 0)]
for (index, point) in points.enumerated() {
    let position = NSPoint(x: 512 + point.0 * 12, y: 512 + point.1 * 12)
    if index == 0 { pulse.move(to: position) } else { pulse.line(to: position) }
}
pulse.lineWidth = 18
pulse.lineJoinStyle = .miter
pulse.lineCapStyle = .butt
colour(230, 175, 234).setStroke()
pulse.stroke()
NSGraphicsContext.restoreGraphicsState()

// Deterministic grain at several scales gives the tile and ink a printed texture.
func grain(_ x: Int, _ y: Int, _ seed: UInt64) -> Double {
    var value = UInt64(x) &* 0x9e3779b185ebca87 ^ UInt64(y) &* 0xc2b2ae3d27d4eb4f ^ seed
    value = (value ^ (value >> 30)) &* 0xbf58476d1ce4e5b9
    value = (value ^ (value >> 27)) &* 0x94d049bb133111eb
    value ^= value >> 31
    return Double(value & 65535) / 32767.5 - 1.0
}
let pixels = bitmap.bitmapData!
for y in 0..<size {
    for x in 0..<size {
        let offset = y * bitmap.bytesPerRow + x * 4
        let alpha = Double(pixels[offset + 3])
        if alpha == 0 { continue }
        let texture = grain(x, y, 17) * 18 + grain(x / 4, y / 4, 89) * 11
                      + grain(x / 19, y / 19, 137) * 5
        let wear = grain(x, y, 233) > 0.94 ? -38.0 : 0.0
        for (channel, tint) in [1.0, 0.62, 1.2].enumerated() {
            let value = Double(pixels[offset + channel]) + (texture * tint + wear) * alpha / 255
            pixels[offset + channel] = UInt8(max(0, min(alpha, value)).rounded())
        }
    }
}
try bitmap.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: CommandLine.arguments[1]))

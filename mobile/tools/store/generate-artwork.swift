import AppKit
import ImageIO

// Store artwork uses the game's existing icon and palette; screenshots are captured separately.
let output=URL(fileURLWithPath:CommandLine.arguments[1],isDirectory:true)
try FileManager.default.createDirectory(at:output,withIntermediateDirectories:true)
let icon=NSImage(contentsOfFile:"mobile/ios/Maglava/Assets.xcassets/AppIcon.appiconset/AppIcon.png")!
func color(_ hex:Int)->NSColor { NSColor(red:CGFloat((hex>>16)&255)/255,green:CGFloat((hex>>8)&255)/255,blue:CGFloat(hex&255)/255,alpha:1) }
func render(_ name:String,_ width:Int,_ height:Int,_ draw:()->Void) throws {
    let image=NSImage(size:NSSize(width:width,height:height))
    image.lockFocus(); draw(); image.unlockFocus()
    let context=CGContext(data:nil,width:width,height:height,bitsPerComponent:8,bytesPerRow:0,space:CGColorSpaceCreateDeviceRGB(),bitmapInfo:CGImageAlphaInfo.noneSkipLast.rawValue)!
    NSGraphicsContext.saveGraphicsState(); NSGraphicsContext.current=NSGraphicsContext(cgContext:context,flipped:false)
    image.draw(in:NSRect(x:0,y:0,width:width,height:height)); NSGraphicsContext.restoreGraphicsState()
    let destination=CGImageDestinationCreateWithURL(output.appendingPathComponent(name) as CFURL,"public.png" as CFString,1,nil)!
    CGImageDestinationAddImage(destination,context.makeImage()!,nil)
    precondition(CGImageDestinationFinalize(destination))
}
try render("icon.png",512,512) { icon.draw(in:NSRect(x:0,y:0,width:512,height:512)) }
try render("feature-graphic.png",1024,500) {
    color(0x101C2A).setFill(); NSBezierPath(rect:NSRect(x:0,y:0,width:1024,height:500)).fill()
    icon.draw(in:NSRect(x:564,y:0,width:500,height:500))
    func text(_ value:String,_ size:CGFloat,_ weight:NSFont.Weight,_ hex:Int,_ y:CGFloat) {
        (value as NSString).draw(at:NSPoint(x:58,y:y),withAttributes:[.font:NSFont.systemFont(ofSize:size,weight:weight),.foregroundColor:color(hex)])
    }
    text("RISE OR BURN",18,.semibold,0x65E0B1,355)
    text("MAGLAVA",72,.heavy,0xFFFFFF,256)
    text("Swing higher.",30,.medium,0xDCE7F2,194)
    text("Outrun the lava.",30,.medium,0xDCE7F2,151)
    color(0xF2633E).setFill(); NSBezierPath(rect:NSRect(x:58,y:91,width:70,height:5)).fill()
}

import AppKit
import ImageIO
let size=1024
let image=NSImage(size:NSSize(width:size,height:size))
image.lockFocus()
func color(_ n:Int)->NSColor { NSColor(red:CGFloat((n>>16)&255)/255,green:CGFloat((n>>8)&255)/255,blue:CGFloat(n&255)/255,alpha:1) }
color(0x101C2A).setFill(); NSBezierPath(rect:NSRect(x:0,y:0,width:size,height:size)).fill()
color(0x1C3043).setFill()
for x in [90,890] { NSBezierPath(roundedRect:NSRect(x:x,y:0,width:44,height:1024),xRadius:10,yRadius:10).fill() }
color(0xF2633E).setFill(); let lava=NSBezierPath(); lava.move(to:NSPoint(x:0,y:180)); lava.curve(to:NSPoint(x:1024,y:180),controlPoint1:NSPoint(x:320,y:290),controlPoint2:NSPoint(x:660,y:90)); lava.line(to:NSPoint(x:1024,y:0));lava.line(to:.zero);lava.close();lava.fill()
color(0x65E0B1).setStroke();let tether=NSBezierPath();tether.move(to:NSPoint(x:390,y:680));tether.line(to:NSPoint(x:610,y:420));tether.lineWidth=15;tether.stroke()
let node=NSBezierPath(roundedRect:NSRect(x:280,y:570,width:220,height:220),xRadius:55,yRadius:55);color(0x182C3D).setFill();node.fill();color(0x65E0B1).setStroke();node.lineWidth=20;node.stroke()
let ring=NSBezierPath(ovalIn:NSRect(x:335,y:625,width:110,height:110));ring.lineWidth=14;ring.stroke()
NSColor.white.setFill();NSBezierPath(ovalIn:NSRect(x:540,y:350,width:140,height:140)).fill()
color(0x112137).setFill();NSBezierPath(ovalIn:NSRect(x:595,y:410,width:40,height:40)).fill()
image.unlockFocus()
// Use a supported 32-bit RGB context; a 24-bit NSBitmap context silently renders black.
let context=CGContext(data:nil,width:size,height:size,bitsPerComponent:8,bytesPerRow:0,space:CGColorSpaceCreateDeviceRGB(),bitmapInfo:CGImageAlphaInfo.noneSkipLast.rawValue)!
NSGraphicsContext.saveGraphicsState()
NSGraphicsContext.current=NSGraphicsContext(cgContext:context,flipped:false)
image.draw(in:NSRect(x:0,y:0,width:size,height:size))
NSGraphicsContext.restoreGraphicsState()
let destination=CGImageDestinationCreateWithURL(URL(fileURLWithPath:CommandLine.arguments[1]) as CFURL,"public.png" as CFString,1,nil)!
CGImageDestinationAddImage(destination,context.makeImage()!,nil)
precondition(CGImageDestinationFinalize(destination))

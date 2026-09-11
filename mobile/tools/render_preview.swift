// Offscreen native Metal preview. Opens no window and boots no simulator.
// Build from repository root:
// swiftc -import-objc-header mobile/shared/presentation.h mobile/tools/render_preview.swift \
//   mobile/.build/core/libmaglava_core.a -o mobile/.build/render-preview
// Run: mobile/.build/render-preview output.png [level | gallery] [reduced]
import Foundation
import Metal
import AppKit

let args=CommandLine.arguments
precondition(args.count>=2,"Supply an output PNG path and optional stage number or gallery")
let width=900,height=1100
let game=ml_create()!,scene=ml_scene_create()!
defer { ml_scene_destroy(scene);ml_destroy(game) }
var s=[Float](repeating:0,count:Int(ML_SNAPSHOT_SIZE))
let level=args.count>2 ? Int(args[2]) ?? 1 : 1
ml_start(game,Int32(level));ml_snapshot(game,&s)
if args.count>2 && args[2]=="gallery" {
    // Snapshot fixture for inspecting every magnet color and the roaming enemy.
    s[0]=270;s[1]=2560;s[2]=2730;s[3]=2450;s[4]=0;s[9]=3.2;s[11]=0
    s[13]=4;s[14]=1;s[15]=0;s[18]=0;s[23]=3;s[24] = -1;s[25]=3;s[28]=0
    for i in 0..<4 {
        let k=40+i*6;s[k]=i%2==0 ? 125:415;s[k+1]=i<2 ? 2270:2490
        s[k+2]=Float(i);s[k+3]=1;s[k+4]=i==2 ? 1:0;s[k+5]=0;s[30+i]=Float(i)
    }
    s[424]=3;s[425]=270;s[426]=2370;s[427]=1
} else {
    // Show an authored roamer location when the selected stage contains one.
    if let i=(0..<Int(s[14])).first(where:{s[424+$0*10]==3}) { s[3]=s[426+i*10] }
}
let reduced=args.contains("reduced")
if args.contains("menu") { ml_menu_snapshot(3.2,Float(width)/Float(height),reduced ? 1:0,&s) }
if args.contains("hit") { s[8]=1;s[4]=3;s[35]=s[0];s[36]=s[1];s[37]=0.24;s[38]=3.44 }
ml_scene_build(scene,s,Float(width)/Float(height),reduced ? 1:0)
precondition(ml_scene_overflowed(scene)==0,"Scene exceeded its vertex budget")
let count=Int(ml_scene_vertex_count(scene)),opaque=Int(ml_scene_opaque_count(scene))
guard let gpu=MTLCreateSystemDefaultDevice(),let queue=gpu.makeCommandQueue() else { fatalError("Metal unavailable") }
// Compile the production Apple shader, keeping preview and app rendering aligned.
let app=try String(contentsOfFile:"mobile/ios/Maglava/GameScene.swift",encoding:.utf8)
let source=app.components(separatedBy:"let source=\"\"\"")[1].components(separatedBy:"\"\"\"")[0]
let library=try gpu.makeLibrary(source:source,options:nil)
let pipeline=MTLRenderPipelineDescriptor()
pipeline.vertexFunction=library.makeFunction(name:"scene_vertex")
pipeline.fragmentFunction=library.makeFunction(name:"scene_fragment")
pipeline.colorAttachments[0].pixelFormat = .rgba8Unorm
pipeline.depthAttachmentPixelFormat = .depth32Float
let solid=try gpu.makeRenderPipelineState(descriptor:pipeline)
let blend=pipeline.colorAttachments[0]!
blend.isBlendingEnabled=true;blend.sourceRGBBlendFactor = .sourceAlpha;blend.destinationRGBBlendFactor = .one
blend.sourceAlphaBlendFactor = .zero;blend.destinationAlphaBlendFactor = .one
let glow=try gpu.makeRenderPipelineState(descriptor:pipeline)
let depthDesc=MTLDepthStencilDescriptor();depthDesc.depthCompareFunction = .lessEqual;depthDesc.isDepthWriteEnabled=true
let depth=gpu.makeDepthStencilState(descriptor:depthDesc)!
depthDesc.isDepthWriteEnabled=false
let glowDepth=gpu.makeDepthStencilState(descriptor:depthDesc)!
let colorDesc=MTLTextureDescriptor.texture2DDescriptor(pixelFormat:.rgba8Unorm,width:width,height:height,mipmapped:false)
colorDesc.usage = .renderTarget;colorDesc.storageMode = .shared
let color=gpu.makeTexture(descriptor:colorDesc)!
let zDesc=MTLTextureDescriptor.texture2DDescriptor(pixelFormat:.depth32Float,width:width,height:height,mipmapped:false)
zDesc.usage = .renderTarget;zDesc.storageMode = .private
let z=gpu.makeTexture(descriptor:zDesc)!
let pass=MTLRenderPassDescriptor();pass.colorAttachments[0].texture=color
pass.colorAttachments[0].loadAction = .clear;pass.colorAttachments[0].storeAction = .store
pass.colorAttachments[0].clearColor=MTLClearColorMake(0.018,0.027,0.052,1)
pass.depthAttachment.texture=z;pass.depthAttachment.loadAction = .clear;pass.depthAttachment.clearDepth=1
let buffer=gpu.makeBuffer(bytes:ml_scene_vertices(scene)!,length:count*MemoryLayout<MLVertex>.stride,options:.storageModeShared)!
let command=queue.makeCommandBuffer()!,encoder=command.makeRenderCommandEncoder(descriptor:pass)!
encoder.setCullMode(.none);encoder.setVertexBuffer(buffer,offset:0,index:0)
encoder.setRenderPipelineState(solid);encoder.setDepthStencilState(depth)
encoder.drawPrimitives(type:.triangle,vertexStart:0,vertexCount:opaque)
encoder.setRenderPipelineState(glow);encoder.setDepthStencilState(glowDepth)
encoder.drawPrimitives(type:.triangle,vertexStart:opaque,vertexCount:count-opaque)
encoder.endEncoding();command.commit();command.waitUntilCompleted()
if let error=command.error { throw error }
var pixels=[UInt8](repeating:0,count:width*height*4)
color.getBytes(&pixels,bytesPerRow:width*4,from:MTLRegionMake2D(0,0,width,height),mipmapLevel:0)
let provider=CGDataProvider(data:Data(pixels) as CFData)!
let cg=CGImage(width:width,height:height,bitsPerComponent:8,bitsPerPixel:32,bytesPerRow:width*4,space:CGColorSpace(name:CGColorSpace.sRGB)!,bitmapInfo:CGBitmapInfo(rawValue:CGImageAlphaInfo.last.rawValue),provider:provider,decode:nil,shouldInterpolate:false,intent:.defaultIntent)!
let png=NSBitmapImageRep(cgImage:cg).representation(using:.png,properties:[:])!
try png.write(to:URL(fileURLWithPath:args[1]))
print("Rendered \(count) vertices (\(opaque) solid) to \(args[1]); no emulator required.")

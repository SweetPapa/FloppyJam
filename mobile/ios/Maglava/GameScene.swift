import MetalKit
import UIKit

let magneticColors: [UIColor] = (0..<4).map { UIColor(rgb:Int(ml_magnet_color(Int32($0)))) }
extension UIColor {
    convenience init(rgb: Int, alpha: CGFloat = 1) {
        self.init(red:CGFloat((rgb>>16)&255)/255,green:CGFloat((rgb>>8)&255)/255,blue:CGFloat(rgb&255)/255,alpha:alpha)
    }
}

/// Metal draws the shared C scene's perspective triangles with real depth.
/// The display callback, simulation, input and UIKit all belong to the main thread.
final class GameScene: MTKView, MTKViewDelegate {
    let core: OpaquePointer
    var snapshot = [Float](repeating:0,count:Int(ML_SNAPSHOT_SIZE))
    var onFrame: (([Float])->Void)?
    var running = false
    var menuBackdrop = false
    private var menuTime: Float = 0
    var reducedMotion = false
    private var lastTime: TimeInterval = 0
    private let mesh = ml_scene_create()!
    private var queue: MTLCommandQueue!
    private var solid: MTLRenderPipelineState!
    private var glow: MTLRenderPipelineState!
    private var depth: MTLDepthStencilState!
    private var glowDepth: MTLDepthStencilState!
    private var buffers: [MTLBuffer] = []
    private var bufferIndex = 0
    private let available = DispatchSemaphore(value:3)
    private(set) var renderedFrames = 0
    init(core: OpaquePointer) {
        self.core=core
        super.init(frame:.zero,device:MTLCreateSystemDefaultDevice())
        guard let gpu=device else { fatalError("MagLava requires Metal") }
        colorPixelFormat = .bgra8Unorm; depthStencilPixelFormat = .depth32Float
        sampleCount = gpu.supportsTextureSampleCount(4) ? 4 : 1
        clearColor=MTLClearColorMake(0.018,0.027,0.052,1)
        preferredFramesPerSecond=UIScreen.main.maximumFramesPerSecond
        isPaused=true; enableSetNeedsDisplay=false; isUserInteractionEnabled=false
        queue=gpu.makeCommandQueue()
        let source="""
        #include <metal_stdlib>
        using namespace metal;
        struct Vertex { float4 position; float4 color; };
        struct Output { float4 position [[position]]; float4 color; };
        vertex Output scene_vertex(const device Vertex *v [[buffer(0)]], uint id [[vertex_id]]) {
            Output o; o.position=v[id].position;
            o.position.z=(o.position.z+o.position.w)*0.5;
            o.color=v[id].color; return o;
        }
        fragment float4 scene_fragment(Output in [[stage_in]]) { return in.color; }
        """
        do {
            let library=try gpu.makeLibrary(source:source,options:nil)
            let descriptor=MTLRenderPipelineDescriptor()
            descriptor.vertexFunction=library.makeFunction(name:"scene_vertex")
            descriptor.fragmentFunction=library.makeFunction(name:"scene_fragment")
            descriptor.colorAttachments[0].pixelFormat=colorPixelFormat
            descriptor.depthAttachmentPixelFormat=depthStencilPixelFormat
            descriptor.rasterSampleCount=sampleCount
            solid=try gpu.makeRenderPipelineState(descriptor:descriptor)
            let blend=descriptor.colorAttachments[0]!
            blend.isBlendingEnabled=true; blend.sourceRGBBlendFactor = .sourceAlpha; blend.destinationRGBBlendFactor = .one
            blend.sourceAlphaBlendFactor = .zero; blend.destinationAlphaBlendFactor = .one
            glow=try gpu.makeRenderPipelineState(descriptor:descriptor)
        } catch { fatalError("3D shader compilation failed: \(error)") }
        let z=MTLDepthStencilDescriptor(); z.depthCompareFunction = .lessEqual; z.isDepthWriteEnabled=true
        depth=gpu.makeDepthStencilState(descriptor:z); z.isDepthWriteEnabled=false; glowDepth=gpu.makeDepthStencilState(descriptor:z)
        for _ in 0..<3 { buffers.append(gpu.makeBuffer(length:196608*MemoryLayout<MLVertex>.stride,options:.storageModeShared)!) }
        delegate=self
        ml_snapshot(core,&snapshot)
    }
    required init(coder: NSCoder) { fatalError("Programmatic scene") }
    deinit { ml_scene_destroy(mesh) }
    func resetClock() { lastTime=0; ml_snapshot(core,&snapshot) }
    func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) { if isPaused { draw() } }
    func draw(in view: MTKView) {
        let now=CACurrentMediaTime(),dt=lastTime==0 ? 0 : min(0.1,now-lastTime)
        lastTime=now
        if running { ml_advance(core,dt); ml_snapshot(core,&snapshot) }
        guard drawableSize.width>0,drawableSize.height>0,
              available.wait(timeout:.now()) == .success else { if running { onFrame?(snapshot) }; return }
        guard let pass=currentRenderPassDescriptor,let drawable=currentDrawable,
              let command=queue.makeCommandBuffer(),let encoder=command.makeRenderCommandEncoder(descriptor:pass) else { available.signal(); if running { onFrame?(snapshot) }; return }
        if menuBackdrop {
            menuTime += Float(dt)
            var backdrop=[Float](repeating:0,count:Int(ML_SNAPSHOT_SIZE))
            ml_menu_snapshot(menuTime,Float(drawableSize.width/drawableSize.height),reducedMotion ? 1 : 0,&backdrop)
            ml_scene_build(mesh,backdrop,Float(drawableSize.width/drawableSize.height),reducedMotion ? 1 : 0)
        } else { ml_scene_build(mesh,snapshot,Float(drawableSize.width/drawableSize.height),reducedMotion ? 1 : 0) }
        let count=Int(ml_scene_vertex_count(mesh)),opaque=Int(ml_scene_opaque_count(mesh))
        assert(ml_scene_overflowed(mesh)==0,"Scene exceeded vertex budget")
        let buffer=buffers[bufferIndex]; bufferIndex=(bufferIndex+1)%buffers.count
        memcpy(buffer.contents(),ml_scene_vertices(mesh),count*MemoryLayout<MLVertex>.stride)
        encoder.setCullMode(.none); encoder.setVertexBuffer(buffer,offset:0,index:0)
        encoder.setRenderPipelineState(solid); encoder.setDepthStencilState(depth)
        if opaque>0 { encoder.drawPrimitives(type:.triangle,vertexStart:0,vertexCount:opaque) }
        encoder.setRenderPipelineState(glow); encoder.setDepthStencilState(glowDepth)
        if count>opaque { encoder.drawPrimitives(type:.triangle,vertexStart:opaque,vertexCount:count-opaque) }
        encoder.endEncoding(); command.present(drawable)
        let semaphore=available
        command.addCompletedHandler { _ in semaphore.signal() }; command.commit(); renderedFrames += 1
        if running { onFrame?(snapshot) }
    }
}

/// Layout is computed in shared C so device aspect cannot rearrange the colors.
final class ColorControls: UIView {
    var buttons: [UIButton] = []
    override func layoutSubviews() {
        super.layoutSubviews()
        var rects=[Float](repeating:0,count:16)
        ml_control_layout(Float(bounds.width),Float(bounds.height),&rects)
        for (i,b) in buttons.enumerated() { let k=i*4
            b.frame=CGRect(x:CGFloat(rects[k]),y:CGFloat(rects[k+1]),width:CGFloat(rects[k+2]),height:CGFloat(rects[k+3]))
        }
    }
}

package dev.fofo.maglava

import android.content.Context
import android.graphics.Color
import android.opengl.GLES20.*
import android.opengl.GLSurfaceView
import android.view.Choreographer
import android.widget.FrameLayout
import java.nio.ByteBuffer
import java.nio.ByteOrder
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10
import kotlin.math.min

val magnetColors=IntArray(4) { rgb(Native.magnetColor(it)) }
fun rgb(value:Int)=value or 0xFF000000.toInt()
fun alpha(color:Int,value:Float)=(color and 0xFFFFFF) or ((value*255).toInt().coerceIn(0,255) shl 24)

/** UI-thread physics publishes immutable snapshots to the GLES render thread.
 * Both native rasterizers consume the same C mesh, camera, lighting and effects. */
class GameView(context:Context,val core:Long):FrameLayout(context),Choreographer.FrameCallback {
    val data=FloatArray(1256)
    var onFrame:((FloatArray)->Unit)?=null
    var reducedMotion=false
    var menuBackdrop=false
    private var menuTime=0f
    private var running=false
    private var lastTime=0L
    private val lock=Any()
    private val published=FloatArray(1256)
    private var publishedReduced=false
    private val renderer=SceneRenderer()
    private val surface=GLSurfaceView(context).apply {
        setEGLContextClientVersion(2)
        setEGLConfigChooser(8,8,8,8,24,0)
        preserveEGLContextOnPause=true
        setRenderer(renderer); renderMode=GLSurfaceView.RENDERMODE_WHEN_DIRTY
    }
    val renderedFrames get()=renderer.frames
    init { isFocusable=false; setBackgroundColor(rgb(0x05070D)); addView(surface,LayoutParams(-1,-1)); reset() }
    fun startFrames() { if(running)return; running=true;lastTime=0;Choreographer.getInstance().postFrameCallback(this) }
    fun stopFrames() { running=false;lastTime=0;Choreographer.getInstance().removeFrameCallback(this) }
    fun reset() { lastTime=0;Native.frame(core,0.0,data);publish() }
    fun pauseSurface()=surface.onPause()
    fun resumeSurface()=surface.onResume()
    fun release() { stopFrames();surface.queueEvent { renderer.release() } }
    override fun onDetachedFromWindow() { stopFrames();super.onDetachedFromWindow() }
    override fun doFrame(time:Long) {
        if(!running)return
        val dt=if(lastTime==0L)0.0 else min(0.1,(time-lastTime)/1e9)
        if(menuBackdrop && lastTime!=0L && time-lastTime<33_000_000L) {
            Choreographer.getInstance().postFrameCallback(this);return
        }
        lastTime=time
        if(menuBackdrop) {
            menuTime+=dt.toFloat()
            Native.menuFrame(menuTime,width.toFloat()/height.coerceAtLeast(1),reducedMotion,data)
            publish()
        } else { Native.frame(core,dt,data);publish();onFrame?.invoke(data) }
        if(running)Choreographer.getInstance().postFrameCallback(this)
    }
    private fun publish() { synchronized(lock) { data.copyInto(published);publishedReduced=reducedMotion };surface.requestRender() }
    private inner class SceneRenderer:GLSurfaceView.Renderer {
        private var mesh=0L
        private var program=0
        private var position=0
        private var color=0
        private var vbo=0
        private var aspect=1f
        private var closed=false
        private val frame=FloatArray(1256)
        private val counts=IntArray(2)
        private val vertices=ByteBuffer.allocateDirect(196608*32).order(ByteOrder.nativeOrder())
        @Volatile var frames=0
        private fun shader(type:Int,source:String):Int {
            val id=glCreateShader(type);glShaderSource(id,source);glCompileShader(id)
            val ok=IntArray(1);glGetShaderiv(id,GL_COMPILE_STATUS,ok,0)
            check(ok[0]!=0) { "3D shader: ${glGetShaderInfoLog(id)}" };return id
        }
        override fun onSurfaceCreated(gl:GL10?,config:EGLConfig?) {
            if(closed)return
            if(mesh==0L)mesh=Native.sceneCreate()
            check(mesh!=0L)
            val vs=shader(GL_VERTEX_SHADER,"attribute vec4 position; attribute vec4 color; varying vec4 tint; void main(){gl_Position=position;tint=color;}")
            val fs=shader(GL_FRAGMENT_SHADER,"precision mediump float; varying vec4 tint; void main(){gl_FragColor=tint;}")
            program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program)
            val ok=IntArray(1);glGetProgramiv(program,GL_LINK_STATUS,ok,0);check(ok[0]!=0) { glGetProgramInfoLog(program) }
            glDeleteShader(vs);glDeleteShader(fs)
            position=glGetAttribLocation(program,"position");color=glGetAttribLocation(program,"color")
            val ids=IntArray(1);glGenBuffers(1,ids,0);vbo=ids[0]
            glClearColor(.018f,.027f,.052f,1f);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDisable(GL_CULL_FACE)
        }
        override fun onSurfaceChanged(gl:GL10?,w:Int,h:Int) { glViewport(0,0,w,h);aspect=w.toFloat()/h.coerceAtLeast(1) }
        override fun onDrawFrame(gl:GL10?) {
            if(closed||mesh==0L)return
            val reduced=synchronized(lock) { published.copyInto(frame);publishedReduced }
            Native.sceneBuild(mesh,frame,aspect,reduced,vertices,counts)
            glDepthMask(true);glClear(GL_COLOR_BUFFER_BIT or GL_DEPTH_BUFFER_BIT);glUseProgram(program)
            glBindBuffer(GL_ARRAY_BUFFER,vbo);vertices.position(0)
            glBufferData(GL_ARRAY_BUFFER,counts[1]*32,vertices,GL_DYNAMIC_DRAW)
            glEnableVertexAttribArray(position);glEnableVertexAttribArray(color)
            glVertexAttribPointer(position,4,GL_FLOAT,false,32,0);glVertexAttribPointer(color,4,GL_FLOAT,false,32,16)
            glDisable(GL_BLEND);glDrawArrays(GL_TRIANGLES,0,counts[0])
            glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE);glDepthMask(false)
            glDrawArrays(GL_TRIANGLES,counts[0],counts[1]-counts[0]);glDepthMask(true);glDisable(GL_BLEND)
            check(glGetError()==GL_NO_ERROR) { "3D frame failed" };frames++
        }
        fun release() { closed=true;if(mesh!=0L)Native.sceneDestroy(mesh);mesh=0;glDeleteProgram(program);glDeleteBuffers(1,intArrayOf(vbo),0) }
    }
}

class ColorControls(context:Context):FrameLayout(context) {
    private val rects=FloatArray(16)
    override fun onLayout(changed:Boolean,l:Int,t:Int,r:Int,b:Int) {
        val density=resources.displayMetrics.density
        Native.controlLayout(width/density,height/density,rects)
        for(i in 0 until childCount) { val k=i*4
            getChildAt(i).layout((rects[k]*density).toInt(),(rects[k+1]*density).toInt(),((rects[k]+rects[k+2])*density).toInt(),((rects[k+1]+rects[k+3])*density).toInt())
        }
    }
}

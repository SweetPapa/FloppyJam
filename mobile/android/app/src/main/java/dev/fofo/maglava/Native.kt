package dev.fofo.maglava

object Native {
    init { System.loadLibrary("maglava") }
    external fun create(): Long
    external fun destroy(handle: Long)
    external fun start(handle: Long, level: Int)
    external fun pause(handle: Long, paused: Boolean)
    external fun input(handle: Long, color: Int)
    external fun frame(handle: Long, dt: Double, snapshot: FloatArray)
    external fun sceneCreate(): Long
    external fun sceneDestroy(handle: Long)
    external fun sceneBuild(handle: Long, data: FloatArray, aspect: Float, reduced: Boolean, vertices: java.nio.ByteBuffer, counts: IntArray)
    external fun controlLayout(width: Float, height: Float, rects: FloatArray)
    external fun magnetColor(color: Int): Int
    external fun accentColor(level: Int): Int
    external fun chapterName(level: Int): String
    external fun musicTrack(index: Int): String
    external fun musicCount(): Int
    external fun menuFrame(seconds: Float, aspect: Float, reduced: Boolean, snapshot: FloatArray)
    external fun count(): Int
    external fun metadata(level: Int, field: Int): String
}

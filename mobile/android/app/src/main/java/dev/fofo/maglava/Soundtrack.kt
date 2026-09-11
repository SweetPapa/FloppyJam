package dev.fofo.maglava

import android.content.Context
import android.media.AudioAttributes
import android.media.MediaPlayer

/** Prebuffer the next track for native, gapless handoff. Owned by the UI thread. */
class Soundtrack(private val context:Context) {
    private var index=0
    private var enabled=false
    private var closed=false
    private var current:MediaPlayer=prepare(0)
    private var next:MediaPlayer=prepare(1)
    init { current.setNextMediaPlayer(next) }
    private fun prepare(n:Int):MediaPlayer {
        val player=MediaPlayer()
        try {
            player.setAudioAttributes(AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
            context.assets.openFd("Audio/${Native.musicTrack(n)}.m4a").use { player.setDataSource(it.fileDescriptor,it.startOffset,it.length) }
            player.isLooping=false;player.setVolume(.65f,.65f);player.prepare()
            player.setOnCompletionListener { finished ->
                if(!closed && finished===current) {
                    current=next;index=(index+1)%Native.musicCount()
                    if(!enabled && current.isPlaying)current.pause()
                    next=prepare(index+1);current.setNextMediaPlayer(next)
                    finished.release()
                }
            }
            return player
        } catch(error:Exception) { player.release();throw error }
    }
    fun setPlaying(value:Boolean) {
        enabled=value
        if(closed)return
        if(value)current.start() else if(current.isPlaying)current.pause()
    }
    val name get()=Native.musicTrack(index)
    val position get()=current.currentPosition
    val duration get()=current.duration
    fun seekNearEnd() { current.seekTo((duration-400).coerceAtLeast(0).toLong(),MediaPlayer.SEEK_CLOSEST) }
    fun release() { if(closed)return;closed=true;current.setNextMediaPlayer(null);current.release();next.release() }
}

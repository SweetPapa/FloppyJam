package dev.fofo.maglava

import android.app.Instrumentation
import android.content.Intent
import android.os.Bundle
import android.os.SystemClock
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import kotlin.math.abs

/** Real UI-thread/JNI/touch/lifecycle smoke test, using only platform test APIs. */
class SmokeRunner : Instrumentation() {
    override fun onCreate(arguments:Bundle?) { super.onCreate(arguments); start() }
    override fun onStart() {
        val result=Bundle()
        try {
            coreContract()
            targetContext.getSharedPreferences("maglava",0).edit().putBoolean("musicMuted",false).commit()
            val activity=startActivitySync(Intent(targetContext,MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK).putExtra("level",1)) as MainActivity
            waitForIdleSync()
            lateinit var game:GameView
            runOnMainSync { game=descendants(activity.window.decorView).filterIsInstance<GameView>().first() }
            fun state():FloatArray { var copy=FloatArray(0); runOnMainSync { copy=game.data.copyOf() }; return copy }
            fun click(title:String) { runOnMainSync { descendants(activity.window.decorView).filterIsInstance<Button>().first { it.text.toString()==title || it.contentDescription?.toString()==title }.performClick() }; waitForIdleSync() }
            SystemClock.sleep(250)
            check(game.renderedFrames>0) { "GLES must submit a 3D frame" }
            runOnMainSync {
                val buttons=descendants(activity.window.decorView).filterIsInstance<Button>().toList()
                fun control(name:String)=buttons.first { it.contentDescription=="Tether to $name magnet" }
                val red=control("red");val blue=control("blue");val yellow=control("yellow");val green=control("green")
                check(red.left==blue.left && red.bottom<blue.top)
                check(yellow.right<blue.left && blue.right<green.left)
                check(yellow.top==blue.top && blue.top==green.top)
            }
            for(n in 0..3) {
                val name=Native.musicTrack(n)
                val player=android.media.MediaPlayer()
                try { targetContext.assets.openFd("Audio/$name.m4a").use { player.setDataSource(it.fileDescriptor,it.startOffset,it.length);player.prepare();check(player.duration>90000) } }
                finally { player.release() }
            }
            click("Pause game"); val elapsed=state()[9]
            var song="";var musicBefore=0
            runOnMainSync {song=activity.debugMusicName;musicBefore=activity.debugMusicPosition}
            SystemClock.sleep(350)
            runOnMainSync {check(activity.debugMusicName==song && activity.debugMusicPosition>musicBefore+100) {"Pause interrupted music"}}
            check(state()[9]==elapsed) { "Paused simulation advanced" }
            click("How to Play"); SystemClock.sleep(150); check(state()[9]==elapsed)
            click("Back"); click("Settings"); click("Back")
            click("Resume")
            // Codec preparation and first-boot setup take variable time on CI.
            // Begin the touch trial at a fresh level, as a player pressing Retry.
            click("Pause game"); click("Restart Level")
            var captures=0
            val deadline=SystemClock.uptimeMillis()+90000
            while(SystemClock.uptimeMillis()<deadline) {
                val s=state()
                if(captures<2 && s[9]>(captures+1)*2.5f) { capture("maglava-game-${captures+1}.png"); captures++ }
                if(s[5]>0)break
                if(s[4]==0f) {
                    var color=-1; var y=s[1]
                    for(c in 0..3) { val index=s[30+c].toInt(); if(index>=0 && s[40+index*6+1]<y) { y=s[40+index*6+1]; color=c } }
                    if(color>=0) {
                        val names=arrayOf("red","blue","yellow","green"); val location=IntArray(2); var w=0; var h=0
                        runOnMainSync { val b=descendants(activity.window.decorView).filterIsInstance<Button>().first { it.contentDescription=="Tether to ${names[color]} magnet" }; b.getLocationOnScreen(location); w=b.width; h=b.height }
                        val now=SystemClock.uptimeMillis(); val x=location[0]+w/2f; val yTouch=location[1]+h/2f
                        val down=MotionEvent.obtain(now,now,MotionEvent.ACTION_DOWN,x,yTouch,0)
                        val up=MotionEvent.obtain(now,now+8,MotionEvent.ACTION_UP,x,yTouch,0)
                        sendPointerSync(down); sendPointerSync(up); down.recycle(); up.recycle()
                    }
                }
                SystemClock.sleep(100)
            }
            val finalState=state()
            check(finalState[5]>0) { "Touch-controlled first stage did not complete: elapsed=${finalState[9]}, state=${finalState[4]}, retries=${finalState[8]}, height=${finalState[1]}, frames=${game.renderedFrames}" }
            capture("maglava-complete.png")
            val prefs=targetContext.getSharedPreferences("maglava",0)
            check(prefs.getInt("stars.level-1a",0)>0) { "Completion was not persisted by stable key" }
            click("Next Level"); check(state()[34]==2f) { "Next Level did not load" }
            click("Pause game"); click("Restart Level"); check(state()[9]<0.2f) { "Retry did not reset clock" }
            click("Pause game");click("Home")
            val menuFrames=game.renderedFrames;SystemClock.sleep(300);check(game.renderedFrames>menuFrames)
            capture("maglava-home.png")
            runOnMainSync { check(descendants(activity.window.decorView).filterIsInstance<Button>().none { it.text.toString().startsWith("01   ") }) }
            click("Level Select")
            runOnMainSync { check(descendants(activity.window.decorView).filterIsInstance<Button>().count { (it.tag as? String)?.startsWith("level.")==true }==40) }
            capture("maglava-levels.png")
            runOnMainSync { descendants(activity.window.decorView).filterIsInstance<android.widget.ScrollView>().first().fullScroll(View.FOCUS_DOWN) }
            waitForIdleSync(); SystemClock.sleep(100)
            runOnMainSync { check(descendants(activity.window.decorView).filterIsInstance<Button>().first { it.text=="Back" }.getGlobalVisibleRect(android.graphics.Rect())) }
            click("Back")
            click("How to Play"); capture("maglava-help.png"); click("Back")
            click("Settings")
            runOnMainSync { check(descendants(activity.window.decorView).filterIsInstance<android.widget.Switch>().count()==4) }
            click("Back")
            runOnMainSync {check(activity.debugMusicName==song && activity.debugMusicPosition>musicBefore)}
            for(n in 1..4) {
                runOnMainSync {activity.debugSeekMusicEnd()}
                val end=SystemClock.uptimeMillis()+6000
                var advanced=false
                while(SystemClock.uptimeMillis()<end) {
                    runOnMainSync {advanced=activity.debugMusicName==Native.musicTrack(n)}
                    if(advanced)break
                    SystemClock.sleep(100)
                }
                check(advanced) {"Playlist did not advance to ${Native.musicTrack(n)}"}
                SystemClock.sleep(300)
                runOnMainSync {check(activity.debugMusicPosition>0)}
            }
            runOnMainSync {musicBefore=activity.debugMusicPosition}
            runOnMainSync { activity.moveTaskToBack(true) }; SystemClock.sleep(300)
            val before=state()[9]; SystemClock.sleep(250); check(state()[9]==before) { "Background simulation advanced" }
            runOnMainSync {check(abs(activity.debugMusicPosition-musicBefore)<150) {"Music continued in background"};activity.finish() }
            result.putString("stream","PASS: JNI rates, native GLES 3D, triangular controls, original soundtrack, touch completion, stable progress, next stage, retry, pause and background lifecycle.\n")
            finish(-1,result)
        } catch(error:Throwable) {
            try { capture("maglava-failure.png") } catch(_:Throwable) {}
            result.putString("stream","FAIL: ${error.stackTraceToString()}\n"); finish(1,result)
        }
    }
    private fun descendants(view:View):Sequence<View> = sequence { yield(view); if(view is ViewGroup)for(i in 0 until view.childCount)yieldAll(descendants(view.getChildAt(i))) }
    private fun capture(name:String) {
        waitForIdleSync(); SystemClock.sleep(250)
        val bitmap=uiAutomation.takeScreenshot() ?: error("Screenshot unavailable")
        java.io.FileOutputStream(java.io.File(targetContext.cacheDir,name)).use { bitmap.compress(android.graphics.Bitmap.CompressFormat.PNG,100,it) }
        bitmap.recycle()
    }
    private fun coreContract() {
        fun run(hz:Int):FloatArray {
            val core=Native.create(); check(core!=0L); val s=FloatArray(1256)
            try { for(i in 0 until hz*2)Native.frame(core,1.0/hz,s); return s } finally { Native.destroy(core) }
        }
        val a=run(60); val b=run(120)
        for(i in a.indices)if(i!=22)check(abs(a[i]-b[i])<0.002f) { "JNI rate mismatch at $i" }
        check(Native.count()==40)
    }
}

package dev.fofo.maglava

import android.app.Activity
import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.media.*
import android.os.Bundle
import android.view.*
import android.widget.*
import kotlin.math.max

class MainActivity : Activity() {
    private var core=0L
    private lateinit var game: GameView
    private lateinit var root: FrameLayout
    private lateinit var layout: LinearLayout
    private lateinit var hud: TextView
    private lateinit var subhud: TextView
    private lateinit var progressBar: ProgressBar
    private val colors=mutableListOf<Button>()
    private val prefs by lazy { getSharedPreferences("maglava",Context.MODE_PRIVATE) }
    private val font by lazy { Typeface.createFromAsset(assets,"Barlow-SemiBold.ttf") }
    private var overlay: View?=null
    private var panelMode="menu"
    private var playing=false
    private var completed=false
    private var level=1
    private var hudTick=-1f
    private var mediaReady=false
    private var music: Soundtrack?=null
    private var audioActive=false
    private val musicMuted get()=prefs.getBoolean("musicMuted",muted)
    private var audio: SoundPool?=null
    private val effects=mutableMapOf<String,Int>()
    private val loaded=mutableSetOf<Int>()
    private lateinit var audioManager: AudioManager
    private lateinit var focus: AudioFocusRequest
    private var hasFocus=false
    private fun t(text:String)=Native.text(text)
    private val language get()=prefs.getString("language","") ?: ""
    private fun applyLanguage() { Native.language(language.ifEmpty { resources.configuration.locales[0].toLanguageTag() }) }
    private fun dp(n:Int)=(n*resources.displayMetrics.density).toInt()
    private fun key(n:Int)=Native.metadata(n,0)
    private fun stars(n:Int)=prefs.getInt("stars.${key(n)}",0).coerceIn(0,3)
    private fun best(n:Int)=prefs.getFloat("time.${key(n)}",0f)
    private fun unlocked(n:Int)=n==1 || (n-1..Native.count()).any { stars(it)>0 }
    private val muted get()=prefs.getBoolean("muted",false)
    private val haptics get()=prefs.getBoolean("haptics",true)
    override fun onCreate(savedInstanceState:Bundle?) {
        super.onCreate(savedInstanceState)
        applyLanguage()
        if(!prefs.contains("musicMuted"))prefs.edit().putBoolean("musicMuted",muted).apply()
        requestedOrientation=if(resources.configuration.smallestScreenWidthDp>=600) android.content.pm.ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR else android.content.pm.ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
        core=Native.create(); Native.lavaRate(core,prefs.getFloat("lavaRate",1.5f)); check(core!=0L)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        val display=windowManager.defaultDisplay
        val mode=display.mode
        display.supportedModes.filter { it.physicalWidth==mode.physicalWidth && it.physicalHeight==mode.physicalHeight }
            .maxByOrNull { it.refreshRate }?.let { preferred -> window.attributes=window.attributes.apply { preferredDisplayModeId=preferred.modeId } }
        root=FrameLayout(this); root.setBackgroundColor(rgb(0x090F19)); setContentView(root)
        window.decorView.systemUiVisibility=View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
        if(android.os.Build.VERSION.SDK_INT>=30) {
            window.insetsController?.apply {
                hide(WindowInsets.Type.statusBars() or WindowInsets.Type.navigationBars())
                systemBarsBehavior=WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        } else {
            window.decorView.systemUiVisibility=window.decorView.systemUiVisibility or View.SYSTEM_UI_FLAG_FULLSCREEN or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        }
        root.setOnApplyWindowInsetsListener { _, insets ->
            if(android.os.Build.VERSION.SDK_INT>=30) {
                val safe=insets.getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout())
                root.setPadding(safe.left,safe.top,safe.right,safe.bottom)
            } else { root.setPadding(insets.systemWindowInsetLeft,insets.systemWindowInsetTop,insets.systemWindowInsetRight,insets.systemWindowInsetBottom) }
            insets
        }
        layout=LinearLayout(this); layout.orientation=LinearLayout.VERTICAL; root.addView(layout,FrameLayout.LayoutParams(-1,-1))
        val header=LinearLayout(this); header.orientation=LinearLayout.VERTICAL; header.setPadding(dp(20),dp(10),dp(20),dp(10)); layout.addView(header)
        val row=LinearLayout(this); row.gravity=Gravity.CENTER_VERTICAL
        hud=label("",21,Color.WHITE); hud.maxLines=1; row.addView(hud,LinearLayout.LayoutParams(0,-2,1f))
        val pause=button("Ⅱ") { pauseGame() }; pause.contentDescription=t("Pause game"); pause.tag="game.pause"; row.addView(pause,LinearLayout.LayoutParams(dp(48),dp(48)))
        header.addView(row); subhud=label("",12); subhud.maxLines=2; subhud.typeface=Typeface.createFromAsset(assets,"Barlow-Medium.ttf"); subhud.gravity=Gravity.CENTER_VERTICAL; header.addView(subhud,LinearLayout.LayoutParams(-1,dp(34)).apply { topMargin=dp(7) })
        progressBar=ProgressBar(this,null,android.R.attr.progressBarStyleHorizontal); progressBar.max=1000
        progressBar.progressTintList=android.content.res.ColorStateList.valueOf(rgb(0x65E0B1)); header.addView(progressBar,LinearLayout.LayoutParams(-1,dp(3)).apply { topMargin=dp(7) })
        game=GameView(this,core); layout.addView(game,LinearLayout.LayoutParams(-1,0,1f))
        val controls=ColorControls(this); layout.addView(controls,LinearLayout.LayoutParams(-1,dp(144)).apply { topMargin=dp(10); bottomMargin=dp(14) })
        for(i in 0..3) {
            val names=arrayOf("RED","BLUE","YELLOW","GREEN")
            val b=button("${"RBYG"[i]}\n${t(names[i])}",alpha(magnetColors[i],0.18f)) { if(playing) Native.input(core,i) }
            b.setAutoSizeTextTypeUniformWithConfiguration(10,14,1,android.util.TypedValue.COMPLEX_UNIT_SP)
            b.setTextColor(magnetColors[i]); b.textSize=14f; b.contentDescription=t("Tether to ${names[i].lowercase(java.util.Locale.ROOT)} magnet")
            // Fire on touch-down for low latency; performClick remains available to accessibility.
            b.setOnTouchListener { v,event ->
                when(event.actionMasked) {
                    MotionEvent.ACTION_DOWN -> { v.isPressed=true; v.performClick(); true }
                    MotionEvent.ACTION_UP,MotionEvent.ACTION_CANCEL -> { v.isPressed=false; true }
                    else -> true
                }
            }
            b.setPadding(dp(4),dp(4),dp(4),dp(4)); b.minimumWidth=0; b.minimumHeight=0
            (b.background as GradientDrawable).setStroke(dp(2),alpha(magnetColors[i],0.75f))
            controls.addView(b,FrameLayout.LayoutParams(dp(68),dp(68))); colors.add(b)
        }
        game.onFrame={ frame(it) }
        val attributes=AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build()
        audioManager=getSystemService(AudioManager::class.java)
        focus=AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN).setAudioAttributes(attributes)
            .setOnAudioFocusChangeListener { change ->
                if(change<0) { hasFocus=false; pauseGame(); music?.setPlaying(false); audio?.autoPause() }
                else if(change==AudioManager.AUDIOFOCUS_GAIN) { hasFocus=true; if(audioActive)startAudio() }
            }.build()
        audio=SoundPool.Builder().setMaxStreams(6).setAudioAttributes(attributes).build()
        audio?.setOnLoadCompleteListener { _,id,status -> if(status==0)loaded.add(id) }
        for(name in listOf("attach","checkpoint","death","complete","swing")) assets.openFd("Audio/$name.wav").use { effects[name]=audio!!.load(it,1) }
        if(android.os.Build.VERSION.SDK_INT>=33) onBackInvokedDispatcher.registerOnBackInvokedCallback(android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT) { handleBack() }
        music=Soundtrack(this)
        showMenu()
        if(applicationInfo.flags and android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE!=0 && intent.hasExtra("level")) start(intent.getIntExtra("level",1).coerceIn(1,Native.count()))
    }
    private fun label(text:String,size:Int=17,color:Int=rgb(0xA0B4C7))=TextView(this).apply { this.text=t(text); textSize=size.toFloat(); setTextColor(color); typeface=font; setLineSpacing(dp(3).toFloat(),1f) }
    private fun button(text:String,color:Int=rgb(0x263D52),action:()->Unit)=Button(this).apply {
        this.text=t(text); isAllCaps=false; typeface=font; textSize=17f; setTextColor(Color.WHITE)
        background=GradientDrawable().apply { setColor(color); cornerRadius=dp(14).toFloat() }
        setPadding(dp(16),dp(14),dp(16),dp(14)); minHeight=dp(52); setOnClickListener { action() }
    }
    private fun add(stack:LinearLayout,view:View) { val lp=LinearLayout.LayoutParams(-1,-2); lp.bottomMargin=dp(14); stack.addView(view,lp) }
    private fun panel(eyebrow:String,title:String,detail:String,onBack:(()->Unit)?=null):LinearLayout {
        overlay?.let { root.removeView(it) }; layout.importantForAccessibility=View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
        val cover=ScrollView(this); cover.setBackgroundColor(alpha(rgb(0x090F19),if(title=="MAGLAVA")0.18f else 0.94f)); cover.isFillViewport=true
        val stack=LinearLayout(this); stack.orientation=LinearLayout.VERTICAL; stack.setPadding(0,dp(24),0,dp(16))
        val content=FrameLayout(this); cover.addView(content); content.addView(stack,FrameLayout.LayoutParams(dp(320),-2,Gravity.TOP or Gravity.CENTER_HORIZONTAL))
        cover.addOnLayoutChangeListener { v,_,_,_,_,_,_,_,_ ->
            val width=kotlin.math.min((v.width*0.88f).toInt(),dp(560))
            if(stack.layoutParams.width!=width) stack.layoutParams=FrameLayout.LayoutParams(width,-2,Gravity.TOP or Gravity.CENTER_HORIZONTAL)
        }
        val shell=FrameLayout(this); shell.addView(cover,FrameLayout.LayoutParams(-1,-1).apply { if(onBack!=null)topMargin=dp(66) })
        shell.setBackgroundColor(alpha(rgb(0x090F19),if(title=="MAGLAVA")0f else 0.7f))
        if(onBack!=null) {
            val nav=button("Back",action=onBack)
            shell.addView(nav,FrameLayout.LayoutParams(dp(96),dp(50)).apply { topMargin=dp(16);leftMargin=dp(24) })
        }
        root.addView(shell,FrameLayout.LayoutParams(-1,-1)); overlay=shell
        if(eyebrow.isNotEmpty())add(stack,label(eyebrow,12,rgb(0x65E0B1))); add(stack,label(title,if(title=="MAGLAVA")48 else 44,if(title=="MAGLAVA")rgb(0xFF8756) else Color.WHITE)); if(detail.isNotEmpty())add(stack,label(detail))
        cover.announceForAccessibility(t(title)); return stack
    }
    private fun showMenu() {
        panelMode="menu"
        playing=false; game.stopFrames(); Native.pause(core,true); window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        setMenuBackdrop(true); startAudio()
        val stack=panel("","MAGLAVA","Swing. Climb. Survive.")
        for(i in 0 until stack.childCount)(stack.getChildAt(i) as? TextView)?.gravity=Gravity.CENTER
        val showcase=View(this)
        stack.addView(showcase,LinearLayout.LayoutParams(-1,dp(140)))
        overlay?.addOnLayoutChangeListener { v,_,_,_,_,_,_,_,_ ->
            val height=(v.height*0.32f-dp(90)).toInt().coerceIn(dp(140),dp(340))
            if(showcase.layoutParams.height!=height)showcase.layoutParams=LinearLayout.LayoutParams(-1,height)
        }
        val frontier=(1..Native.count()).last { unlocked(it) }
        val hasProgress=(1..Native.count()).any { stars(it)>0 }
        add(stack,button(if(hasProgress)"Continue" else "Play",rgb(0x346858)) { start(frontier) })
        add(stack,button("Level Select") { showLevels() })
        add(stack,button("How to Play") { howToPlay(false) })
        add(stack,button("Settings") { settings(false) })
        add(stack,button("Exit") { stopAudio(); finishAndRemoveTask() })
    }
    private fun returnToMenu(fromPause:Boolean) { if(fromPause)showPause() else showMenu() }
    private fun showLevels(fromPause:Boolean=false) {
        panelMode=if(fromPause)"levelsPause" else "levels"
        val stack=panel("","Level Select","") { returnToMenu(fromPause) }
        for(i in 1..Native.count()) {
            if((i-1)%4==0)add(stack,label("%02d  /  %s".format((i-1)/4+1,t(Native.chapterName(i))),12,rgb(Native.accentColor(i))))
            val open=unlocked(i); val rating=stars(i); val time=best(i)
            val detail=if(open) "★".repeat(rating)+"☆".repeat(3-rating)+(if(time>0)"   %.1fs".format(time) else "") else t("Locked")
            val b=button("%02d   %s\n%s".format(i,Native.metadata(i,1),detail),rgb(if(open)0x182A3B else 0x111B27)) { start(i) }
            b.tag="level.$i"
            b.gravity=Gravity.START or Gravity.CENTER_VERTICAL; b.textSize=16f; b.isEnabled=open; add(stack,b)
        }
    }
    private fun start(n:Int) {
        setMenuBackdrop(false)
        level=n; completed=false; hudTick=-1f; Native.start(core,n); game.reset()
        game.reducedMotion=prefs.getBoolean("reduced",false) || !android.animation.ValueAnimator.areAnimatorsEnabled()
        overlay?.let { root.removeView(it) }; overlay=null; layout.importantForAccessibility=View.IMPORTANT_FOR_ACCESSIBILITY_AUTO
        hud.text="%02d  %s".format(n,Native.metadata(n,1)); subhud.text=Native.metadata(n,2)
        playing=true; game.startFrames(); startAudio(); window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }
    private fun setMenuBackdrop(enabled:Boolean) {
        layout.getChildAt(0).visibility=if(enabled)View.GONE else View.VISIBLE
        layout.getChildAt(2).visibility=if(enabled)View.GONE else View.VISIBLE
        game.menuBackdrop=enabled
        game.reducedMotion=prefs.getBoolean("reduced",false) || !android.animation.ValueAnimator.areAnimatorsEnabled()
        if(enabled)game.startFrames()
    }
    internal val debugMusicName get()=music?.name ?: ""
    internal val debugMusicPosition get()=music?.position ?: 0
    internal fun debugSeekMusicEnd() { if(applicationInfo.flags and android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE!=0)music?.seekNearEnd() }
    private fun startAudio() {
        if(!audioActive)return
        if(!muted || !musicMuted) {
            if(!hasFocus)hasFocus=audioManager.requestAudioFocus(focus)==AudioManager.AUDIOFOCUS_REQUEST_GRANTED
            music?.setPlaying(hasFocus && !musicMuted)
            if(hasFocus && !muted)audio?.autoResume() else audio?.autoPause()
        } else stopAudio()
    }
    private fun stopAudio() { music?.setPlaying(false); audio?.autoPause(); if(hasFocus)audioManager.abandonAudioFocusRequest(focus); hasFocus=false }
    private fun resume() {
        overlay?.let { root.removeView(it) }; overlay=null; layout.importantForAccessibility=View.IMPORTANT_FOR_ACCESSIBILITY_AUTO
        Native.pause(core,false); game.reset(); playing=true; game.startFrames(); startAudio(); window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }
    private fun pauseGame() {
        if(!playing)return
        playing=false; game.stopFrames(); Native.pause(core,true); window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        showPause()
    }
    private fun showPause() {
        panelMode="pause"
        val stack=panel("","Paused",Native.metadata(level,1))
        add(stack,button("Resume",rgb(0x346858)) { resume() }); add(stack,button("Restart Level") { start(level) })
        add(stack,button("How to Play") { howToPlay(true) }); add(stack,button("Settings") { settings(true) }); add(stack,button("Home") { showMenu() })
    }
    private fun settings(fromPause:Boolean) {
        panelMode=if(fromPause)"settingsPause" else "settings"
        val stack=panel("","Settings","") { returnToMenu(fromPause) }
        fun toggle(title:String,key:String,value:Boolean,invert:Boolean=false) {
            val control=Switch(this); control.text=t(title); control.textSize=17f; control.setTextColor(Color.WHITE); control.isChecked=value; control.minHeight=dp(56)
            control.setOnCheckedChangeListener { _,v -> prefs.edit().putBoolean(key,if(invert)!v else v).apply(); game.reducedMotion=prefs.getBoolean("reduced",false) || !android.animation.ValueAnimator.areAnimatorsEnabled(); startAudio() }
            add(stack,control)
        }
        toggle("Music","musicMuted",!musicMuted,true); toggle("Sound effects","muted",!muted,true); toggle("Haptics","haptics",haptics); toggle("Reduced motion","reduced",prefs.getBoolean("reduced",false))
        fun choose(title:String,values:List<String>,selected:Int,onChoose:(Int)->Unit) {
            add(stack,label(title,17,Color.WHITE))
            add(stack,button(values[selected]) {
                android.app.AlertDialog.Builder(this).setTitle(t(title)).setSingleChoiceItems(values.toTypedArray(),selected) { dialog,index ->
                    dialog.dismiss(); onChoose(index); settings(fromPause)
                }.setNegativeButton(t("Back"),null).show()
            }.apply { tag=if(title=="Language")"settings.language" else "settings.lavaRate" })
        }
        val rates=listOf(0.5f,0.75f,1f,1.25f,1.5f,2f,3f)
        choose("Lava rise rate",rates.map { "${it}x" },rates.indexOf(prefs.getFloat("lavaRate",1.5f)).let { if(it<0)4 else it }) {
            prefs.edit().putFloat("lavaRate",rates[it]).apply(); Native.lavaRate(core,rates[it])
        }
        add(stack,label("Multiplies each level's lava speed. 1x is the original rate; default is 1.5x.",14))
        choose("Language",listOf(t("System language"))+(0..6).map { Native.languageName(it) },if(language.isEmpty())0 else Native.languageIndex()+1) {
            prefs.edit().putString("language",if(it==0)"" else Native.languageCode(it-1)).apply(); applyLanguage()
            colors.forEachIndexed { i,b ->
                b.text="${"RBYG"[i]}\n${t(arrayOf("RED","BLUE","YELLOW","GREEN")[i])}"
                b.contentDescription=t("Tether to ${arrayOf("red","blue","yellow","green")[i]} magnet")
            }
            root.findViewWithTag<Button>("game.pause")?.contentDescription=t("Pause game")
            hud.text="%02d  %s".format(level,Native.metadata(level,1)); hudTick=-1f
        }
        add(stack,label("Changes apply immediately and are saved.",14))
        add(stack,label("Barlow typeface by Jeremy Tribby · SIL Open Font License. See the bundled OFL.txt.",12))
    }
    private fun howToPlay(fromPause:Boolean) {
        panelMode=if(fromPause)"helpPause" else "help"
        val stack=panel("","How to Play","Reach the top before the lava catches you.") { returnToMenu(fromPause) }
        val guide=ColorControls(this)
        for(i in 0..3) {
            val badge=label("${"RBYG"[i]}\n${t(arrayOf("RED","BLUE","YELLOW","GREEN")[i])}",16,magnetColors[i])
            badge.gravity=Gravity.CENTER; badge.background=GradientDrawable().apply { setColor(alpha(magnetColors[i],0.18f)); cornerRadius=dp(14).toFloat() }
            guide.addView(badge,FrameLayout.LayoutParams(dp(68),dp(68)))
        }
        stack.addView(guide,LinearLayout.LayoutParams(-1,dp(144)).apply { bottomMargin=dp(14) })
        for((title,detail) in listOf(
            "1  Match a color" to "Tap a color to grab a matching magnet. Glowing outlines show which magnets are in reach.",
            "2  Keep swinging" to "Tap another color while swinging to catch the next magnet and climb higher.",
            "3  Stay above the lava" to "Avoid hazards and cross checkpoints. If you fall, you restart at your last checkpoint."
        )) { add(stack,label(title,19,Color.WHITE)); add(stack,label(detail,16)) }
        add(stack,label("Stars: finish the level, beat the target time, and finish without a death.",15))
    }
    private fun frame(s:FloatArray) {
        // Replay via the normal restart path so recorder startup cannot leave
        // short-stage marketing captures sitting on the completion menu.
        if(applicationInfo.flags and android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE!=0 && intent.getBooleanExtra("media_tour",false) && playing && s[5]>0) {
            start(level)
            return
        }
        if(!mediaReady && applicationInfo.flags and android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE!=0 && intent.getBooleanExtra("media_tour",false) && hasWindowFocus() && game.renderedFrames>=3) {
            mediaReady=true
            android.util.Log.i("MagLavaCapture","READY stage=$level")
        }
        if(applicationInfo.flags and android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE!=0 && intent.getBooleanExtra("media_tour",false) && playing && s[4]==0f) {
            var color=-1;var y=s[1]
            for(c in 0..3) {val i=s[30+c].toInt();if(i>=0 && s[40+i*6+1]<y) {y=s[40+i*6+1];color=c}}
            if(color>=0)Native.input(core,color)
        }
        if(s[9]-hudTick>0.1f || hudTick<0) {
            hudTick=s[9]; progressBar.progress=(s[26]*1000).toInt()
            subhud.text=if(s[4]==3f) { if(s[21]>0)t("Rival wins. Try again.") else t("Respawning…") } else if(s[9]<7) Native.metadata(level,2) else t("%.1fs / %.0fs   ·   %d pts   ·   %d retries").format(s[9],s[27],s[6].toInt(),s[8].toInt())
            subhud.setTextColor(rgb(if(s[29]<200)0xFFAF75 else 0xA0B4C7))
            for(i in 0..3) { colors[i].alpha=if(s[30+i]>=0)1f else 0.65f; if(android.os.Build.VERSION.SDK_INT>=30) colors[i].stateDescription=if(s[30+i]>=0)t("Target in range") else t("No target in range") }
        }
        val events=s[22].toInt()
        val effect=when { events and 16!=0 -> "complete"; events and 8!=0 -> "death"; events and 2!=0 -> "checkpoint"; events and 1!=0 -> "attach"; events and 4!=0 -> "swing"; else -> null }
        if(effect!=null) {
            val id=effects[effect]
            if(!muted && hasFocus && id!=null && loaded.contains(id)) audio?.play(id,0.6f,0.6f,1,0,1f)
            if(haptics && events and 27!=0) root.performHapticFeedback(if(events and 24!=0)HapticFeedbackConstants.LONG_PRESS else HapticFeedbackConstants.KEYBOARD_TAP)
        }
        if(s[5]>0 && !completed) {
            completed=true; playing=false; game.stopFrames(); Native.pause(core,true); window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
            val k=key(level); val old=best(level)
            val editor=prefs.edit().putInt("stars.$k",max(stars(level),s[16].toInt())).putInt("score.$k",max(prefs.getInt("score.$k",0),s[6].toInt()))
            if(s[9].isFinite() && s[9]>0 && (old<=0 || s[9]<old))editor.putFloat("time.$k",s[9])
            // One atomic preferences edit stores completion and unlocks the next stage by stable key.
            editor.apply()
            panelMode="complete"
            val stack=panel("LEVEL COMPLETE","★".repeat(s[16].toInt())+"☆".repeat(3-s[16].toInt()),t("%.1f seconds · %d points · %d retries\nPersonal best: %.1fs").format(s[9],s[6].toInt(),s[8].toInt(),best(level)))
            if(level<Native.count())add(stack,button("Next Level",rgb(0x346858)) { start(level+1) })
            else add(stack,label("All 40 levels complete!",17,Color.WHITE))
            add(stack,button("Play Again") { start(level) }); add(stack,button("Home") { showMenu() })
        }
    }
    private fun handleBack() {
        when {
            playing -> pauseGame()
            panelMode=="menu" -> finish()
            panelMode in listOf("settingsPause","helpPause","levelsPause") -> showPause()
            panelMode=="pause" -> resume()
            else -> showMenu()
        }
    }
    // API 33+ uses the native dispatcher registered above; retain key back on API 26–32.
    @android.annotation.SuppressLint("GestureBackNavigation")
    @Deprecated("Fallback for Android 8–12") override fun onBackPressed() { handleBack() }
    override fun onPause() { audioActive=false; if(::game.isInitialized) { pauseGame(); game.stopFrames(); game.pauseSurface(); stopAudio() }; super.onPause() }
    override fun onWindowFocusChanged(hasFocus:Boolean) { super.onWindowFocusChanged(hasFocus); if(!hasFocus && ::game.isInitialized)pauseGame() }
    override fun onResume() { super.onResume(); if(::game.isInitialized) { audioActive=true; game.resumeSurface(); if(game.menuBackdrop)game.startFrames(); startAudio() } }
    override fun onDestroy() { if(::game.isInitialized)game.release(); music?.release(); audio?.release(); if(core!=0L)Native.destroy(core); core=0; super.onDestroy() }
}

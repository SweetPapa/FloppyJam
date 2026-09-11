import UIKit
import MetalKit
import AVFoundation

@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
    var window: UIWindow?
    func application(_ application: UIApplication, didFinishLaunchingWithOptions options: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        let window = UIWindow(frame: UIScreen.main.bounds)
        window.rootViewController = GameController(); window.makeKeyAndVisible(); self.window = window
        return true
    }
    func applicationWillResignActive(_ application: UIApplication) { (window?.rootViewController as? GameController)?.suspend() }
    func applicationDidBecomeActive(_ application: UIApplication) { (window?.rootViewController as? GameController)?.resumeMenuMusic() }
}

final class Progress {
    private let defaults = UserDefaults.standard
    init() { if defaults.object(forKey:"musicMuted")==nil { defaults.set(defaults.bool(forKey:"muted"),forKey:"musicMuted") } }
    func stars(_ level: Int) -> Int { min(3,max(0,defaults.integer(forKey: "stars.\(key(level))"))) }
    func best(_ level: Int) -> Double { defaults.double(forKey: "time.\(key(level))") }
    func key(_ level: Int) -> String { String(cString: ml_level_key(Int32(level))) }
    func unlocked(_ level: Int) -> Bool {
        // Derive frontier from stable completed keys; inserted stages remain reachable.
        level <= 1 || (level-1...Int(ml_level_count())).contains { stars($0)>0 }
    }
    func record(_ level: Int, _ s: [Float]) {
        let k=key(level), time=Double(s[9]), old=best(level)
        defaults.set(max(stars(level),Int(s[16])),forKey:"stars.\(k)")
        if time.isFinite && time>0 && (old<=0 || time<old) { defaults.set(time,forKey:"time.\(k)") }
        defaults.set(max(defaults.integer(forKey:"score.\(k)"),Int(s[6])),forKey:"score.\(k)")
    }
    var muted: Bool { get { defaults.bool(forKey:"muted") } set { defaults.set(newValue,forKey:"muted") } }
    var musicMuted: Bool { get { defaults.object(forKey:"musicMuted") as? Bool ?? muted } set { defaults.set(newValue,forKey:"musicMuted") } }
    var reduced: Bool { get { defaults.bool(forKey:"reduced") } set { defaults.set(newValue,forKey:"reduced") } }
    var haptics: Bool { get { defaults.object(forKey:"haptics") as? Bool ?? true } set { defaults.set(newValue,forKey:"haptics") } }
}

final class GameController: UIViewController {
    private let core = ml_create()!
    private lazy var scene = GameScene(core: core)
    private let progress = Progress()
    private lazy var sk = scene
    private let hud = UILabel(), subhud = UILabel(), bar = UIProgressView(progressViewStyle: .bar)
    private let header = UIStackView()
    private let controls = ColorControls()
    private var colorButtons: [UIButton] = []
    private var overlay: UIView?
    private var menuBack: (() -> Void)?
    private var level = 1, playing = false, completed = false
    private var hudTick: Float = -1
    private lazy var music = Soundtrack()
    private var audioActive=true
    private var audioInterrupted=false
    private var gameConstraints:[NSLayoutConstraint]=[]
    private var menuConstraints:[NSLayoutConstraint]=[]
    private var sounds: [String:AVAudioPlayer] = [:]
    private let impact = UIImpactFeedbackGenerator(style: .light)
    private let notification = UINotificationFeedbackGenerator()
    override var prefersStatusBarHidden: Bool { true }
    override var preferredScreenEdgesDeferringSystemGestures: UIRectEdge { .bottom }
    deinit { ml_destroy(core) }
    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor=UIColor(rgb:0x090F19)
        sk.translatesAutoresizingMaskIntoConstraints=false; view.addSubview(sk)
        header.axis = .vertical; header.spacing=7; header.translatesAutoresizingMaskIntoConstraints=false; view.addSubview(header)
        let row=UIStackView(); row.axis = .horizontal; row.alignment = .center
        hud.font=UIFont(name:"Barlow-SemiBold",size:21) ?? .boldSystemFont(ofSize:21); hud.textColor = .white
        hud.adjustsFontSizeToFitWidth=true; hud.minimumScaleFactor=0.6
        row.addArrangedSubview(hud)
        let pause=button("Ⅱ", color:UIColor(rgb:0x23374C)) { [weak self] in self?.pauseGame() }
        pause.accessibilityLabel="Pause game"; pause.widthAnchor.constraint(equalToConstant:48).isActive=true; pause.heightAnchor.constraint(equalToConstant:48).isActive=true
        row.addArrangedSubview(pause); header.addArrangedSubview(row)
        subhud.numberOfLines=2
        subhud.heightAnchor.constraint(equalToConstant:34).isActive=true
        subhud.font = UIFont(name:"Barlow-Medium",size:12); subhud.textColor=UIColor(rgb:0xA0B4C7)
        header.addArrangedSubview(subhud); bar.progressTintColor=UIColor(rgb:0x65E0B1); bar.trackTintColor=UIColor(rgb:0x203145); bar.heightAnchor.constraint(equalToConstant:3).isActive=true; header.addArrangedSubview(bar)
        controls.translatesAutoresizingMaskIntoConstraints=false; view.addSubview(controls)
        for i in 0..<4 {
            let b=button(["R\nRED","B\nBLUE","Y\nYELLOW","G\nGREEN"][i],color:magneticColors[i].withAlphaComponent(0.18),onDown:true) { [weak self] in
                guard let self=self, self.playing else { return }; ml_input(self.core,Int32(i))
            }
            b.contentEdgeInsets=UIEdgeInsets(top:8,left:4,bottom:8,right:4); b.titleLabel?.font=UIFont(name:"Barlow-SemiBold",size:14)
            b.setTitleColor(magneticColors[i],for:.normal); b.titleLabel?.numberOfLines=2; b.titleLabel?.textAlignment = .center
            b.accessibilityLabel="Tether to \(["red","blue","yellow","green"][i]) magnet"
            b.layer.borderColor=magneticColors[i].withAlphaComponent(0.75).cgColor; b.layer.borderWidth=1.5
            b.layer.shadowColor=magneticColors[i].cgColor; b.layer.shadowOpacity=0.22; b.layer.shadowRadius=10; b.layer.shadowOffset = .zero
            controls.addSubview(b); colorButtons.append(b)
        }
        controls.buttons=colorButtons
        controls.heightAnchor.constraint(equalToConstant:144).isActive=true
        let safe=view.safeAreaLayoutGuide
        NSLayoutConstraint.activate([
            header.topAnchor.constraint(equalTo:safe.topAnchor,constant:10),header.leadingAnchor.constraint(equalTo:safe.leadingAnchor,constant:20),header.trailingAnchor.constraint(equalTo:safe.trailingAnchor,constant:-20),
            controls.bottomAnchor.constraint(equalTo:safe.bottomAnchor,constant:-14), controls.leadingAnchor.constraint(equalTo:safe.leadingAnchor,constant:18),controls.trailingAnchor.constraint(equalTo:safe.trailingAnchor,constant:-18),
            sk.leadingAnchor.constraint(equalTo:safe.leadingAnchor),sk.trailingAnchor.constraint(equalTo:safe.trailingAnchor)])
        gameConstraints=[sk.topAnchor.constraint(equalTo:header.bottomAnchor,constant:10),sk.bottomAnchor.constraint(equalTo:controls.topAnchor,constant:-10)]
        menuConstraints=[sk.topAnchor.constraint(equalTo:view.topAnchor),sk.bottomAnchor.constraint(equalTo:view.bottomAnchor)]
        NSLayoutConstraint.activate(gameConstraints)
        scene.onFrame={ [weak self] s in self?.frame(s) }
        scene.reducedMotion=progress.reduced || UIAccessibility.isReduceMotionEnabled
        NotificationCenter.default.addObserver(self,selector:#selector(audioInterrupted(_:)),name:AVAudioSession.interruptionNotification,object:nil)
        NotificationCenter.default.addObserver(self,selector:#selector(motionChanged),name:UIAccessibility.reduceMotionStatusDidChangeNotification,object:nil)
        try? AVAudioSession.sharedInstance().setCategory(.ambient,mode:.default)
        for name in ["attach","checkpoint","death","complete","swing"] {
            if let url=Bundle.main.url(forResource:name,withExtension:"wav",subdirectory:"Audio") { sounds[name]=try? AVAudioPlayer(contentsOf:url); sounds[name]?.prepareToPlay() }
        }
        showMenu()
        #if DEBUG
        // Deterministic UI smoke entry points, never enabled in release builds.
        if let i=ProcessInfo.processInfo.arguments.firstIndex(of:"--menu"), ProcessInfo.processInfo.arguments.count>i+1 {
            switch ProcessInfo.processInfo.arguments[i+1] {
            case "levels": showLevels()
            case "help": howToPlay()
            case "settings": settings()
            default: break
            }
        }
        if let i=ProcessInfo.processInfo.arguments.firstIndex(of:"--level"), ProcessInfo.processInfo.arguments.count>i+1, let n=Int(ProcessInfo.processInfo.arguments[i+1]) { start(min(Int(ml_level_count()),max(1,n))) }
        #endif
    }
    @objc private func audioInterrupted(_ note: Notification) {
        guard let raw=note.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt,
              let type=AVAudioSession.InterruptionType(rawValue:raw) else { return }
        if type == .began { audioInterrupted=true; pauseGame(); updateMusic() }
        else {
            let options=AVAudioSession.InterruptionOptions(rawValue:note.userInfo?[AVAudioSessionInterruptionOptionKey] as? UInt ?? 0)
            if options.contains(.shouldResume) { audioInterrupted=false; updateMusic() }
        }
    }
    func suspend() { audioActive=false; pauseGame(); sk.isPaused=true; updateMusic(); sounds.values.forEach { $0.stop() } }
    func resumeMenuMusic() { audioActive=true; if scene.menuBackdrop { sk.isPaused=false }; updateMusic() }
    private func updateMusic() { music.setPlaying(audioActive && !audioInterrupted && !progress.musicMuted) }
    private func menuBackdrop(_ enabled:Bool) {
        NSLayoutConstraint.deactivate(enabled ? gameConstraints : menuConstraints)
        NSLayoutConstraint.activate(enabled ? menuConstraints : gameConstraints)
        scene.menuBackdrop=enabled
        scene.preferredFramesPerSecond=enabled ? 30 : UIScreen.main.maximumFramesPerSecond
    }
    @objc private func motionChanged() { scene.reducedMotion=progress.reduced || UIAccessibility.isReduceMotionEnabled }
    private func button(_ title:String,color:UIColor=UIColor(rgb:0x263D52),onDown:Bool=false,action:@escaping ()->Void)->UIButton {
        let b=TouchButton(type:.system); b.setTitle(title,for:.normal); b.setTitleColor(.white,for:.normal)
        b.titleLabel?.font=UIFont(name:"Barlow-SemiBold",size:17) ?? .boldSystemFont(ofSize:17)
        b.backgroundColor=color; b.layer.cornerRadius=14; b.contentEdgeInsets=UIEdgeInsets(top:14,left:16,bottom:14,right:16)
        b.addAction(UIAction { _ in action() },for:onDown ? .touchDown : .touchUpInside)
        if onDown { b.accessibleAction=action }
        return b
    }
    private func label(_ text:String,size:CGFloat=17,color:UIColor=UIColor(rgb:0xA0B4C7))->UILabel {
        let l=UILabel(); l.text=text; l.textColor=color; l.numberOfLines=0
        l.font=UIFont(name:"Barlow-Medium",size:size) ?? .systemFont(ofSize:size); return l
    }
    private func panel(_ eyebrow:String,_ title:String,_ detail:String, back:Bool=false)->UIStackView {
        overlay?.removeFromSuperview()
        let cover=UIView(); cover.backgroundColor=UIColor(rgb:0x090F19,alpha:title=="MAGLAVA" ? 0.18 : 0.94); cover.translatesAutoresizingMaskIntoConstraints=false
        view.addSubview(cover); overlay=cover
        NSLayoutConstraint.activate([cover.topAnchor.constraint(equalTo:view.topAnchor),cover.bottomAnchor.constraint(equalTo:view.bottomAnchor),cover.leadingAnchor.constraint(equalTo:view.leadingAnchor),cover.trailingAnchor.constraint(equalTo:view.trailingAnchor)])
        let scroll=UIScrollView(); scroll.translatesAutoresizingMaskIntoConstraints=false; cover.addSubview(scroll)
        let stack=UIStackView(); stack.axis = .vertical; stack.spacing=14; stack.translatesAutoresizingMaskIntoConstraints=false; scroll.addSubview(stack)
        let preferredWidth=scroll.widthAnchor.constraint(equalTo:cover.widthAnchor,multiplier:0.88); preferredWidth.priority = .defaultHigh; preferredWidth.isActive=true
        scroll.widthAnchor.constraint(lessThanOrEqualTo:cover.widthAnchor,multiplier:0.88).isActive=true
        NSLayoutConstraint.activate([scroll.widthAnchor.constraint(lessThanOrEqualToConstant:560),scroll.topAnchor.constraint(equalTo:cover.safeAreaLayoutGuide.topAnchor,constant:back ? 82 : 24),scroll.bottomAnchor.constraint(equalTo:cover.safeAreaLayoutGuide.bottomAnchor,constant:-16),scroll.centerXAnchor.constraint(equalTo:cover.centerXAnchor),stack.topAnchor.constraint(equalTo:scroll.contentLayoutGuide.topAnchor),stack.bottomAnchor.constraint(equalTo:scroll.contentLayoutGuide.bottomAnchor),stack.leadingAnchor.constraint(equalTo:scroll.contentLayoutGuide.leadingAnchor),stack.trailingAnchor.constraint(equalTo:scroll.contentLayoutGuide.trailingAnchor),stack.widthAnchor.constraint(equalTo:scroll.frameLayoutGuide.widthAnchor)])
        if back {
            let nav=button("Back") { [weak self] in self?.menuBack?() }
            nav.translatesAutoresizingMaskIntoConstraints=false; cover.addSubview(nav)
            NSLayoutConstraint.activate([nav.leadingAnchor.constraint(equalTo:scroll.leadingAnchor),nav.topAnchor.constraint(equalTo:cover.safeAreaLayoutGuide.topAnchor,constant:16),nav.widthAnchor.constraint(equalToConstant:96),nav.heightAnchor.constraint(equalToConstant:50)])
        }
        if !eyebrow.isEmpty { stack.addArrangedSubview(label(eyebrow,size:12,color:UIColor(rgb:0x65E0B1))) }
        stack.addArrangedSubview(label(title,size:title=="MAGLAVA" ? 48 : 44,color:title=="MAGLAVA" ? UIColor(rgb:0xFF8756) : .white))
        if !detail.isEmpty { stack.addArrangedSubview(label(detail)) }
        cover.accessibilityViewIsModal=true; UIAccessibility.post(notification:.screenChanged,argument:stack)
        return stack
    }
    private func showMenu() {
        playing=false; scene.running=false; sk.isPaused=true; UIApplication.shared.isIdleTimerDisabled=false; ml_pause(core,1); header.isHidden=true; controls.isHidden=true
        menuBackdrop(true); sk.isPaused = !audioActive; updateMusic()
        menuBack=nil
        let stack=panel("", "MAGLAVA", "Swing. Climb. Survive.")
        for case let text as UILabel in stack.arrangedSubviews { text.textAlignment = .center }
        let showcase=UIView()
        stack.addArrangedSubview(showcase)
        let showcaseHeight=showcase.heightAnchor.constraint(equalTo:view.heightAnchor,multiplier:0.32,constant:-90)
        showcaseHeight.priority = .defaultHigh
        NSLayoutConstraint.activate([showcaseHeight,showcase.heightAnchor.constraint(greaterThanOrEqualToConstant:140),showcase.heightAnchor.constraint(lessThanOrEqualToConstant:340)])
        showcase.isAccessibilityElement=false
        let frontier=(1...Int(ml_level_count())).last { progress.unlocked($0) } ?? 1
        let hasProgress=(1...Int(ml_level_count())).contains { progress.stars($0)>0 }
        stack.addArrangedSubview(button(hasProgress ? "Continue" : "Play",color:UIColor(rgb:0x346858)) { [weak self] in self?.start(frontier) })
        stack.addArrangedSubview(button("Level Select") { [weak self] in self?.showLevels() })
        stack.addArrangedSubview(button("How to Play") { [weak self] in self?.howToPlay() })
        stack.addArrangedSubview(button("Settings") { [weak self] in self?.settings() })
    }
    private func returnToMenu(_ fromPause:Bool) { if fromPause { showPause() } else { showMenu() } }
    private func showLevels(fromPause:Bool=false) {
        menuBack={ [weak self] in self?.returnToMenu(fromPause) }
        let stack=panel("", "Level Select", "", back:true)
        for i in 1...Int(ml_level_count()) {
            if (i-1)%4==0 { stack.addArrangedSubview(label(String(format:"%02d  /  ",(i-1)/4+1)+String(cString:ml_chapter_name(Int32(i))),size:12,color:UIColor(rgb:Int(ml_accent_color(Int32(i)))))) }
            let unlocked=progress.unlocked(i), stars=progress.stars(i), time=progress.best(i)
            let title=String(format:"%02d",i)+"   "+String(cString:ml_level_name(Int32(i)))+"\n"+(unlocked ? String(repeating:"★",count:stars)+String(repeating:"☆",count:3-stars)+(time>0 ? String(format:"   %.1fs",time) : "") : "Locked")
            let b=button(title,color:UIColor(rgb:unlocked ? 0x182A3B : 0x111B27)) { [weak self] in self?.start(i) }
            b.accessibilityIdentifier="level.\(i)"
            b.isEnabled=unlocked; b.titleLabel?.numberOfLines=2; b.contentHorizontalAlignment = .left
            b.titleLabel?.font=UIFont(name:"Barlow-SemiBold",size:16); stack.addArrangedSubview(b)
        }
    }
    #if DEBUG
    var debugSnapshot: [Float] { scene.snapshot }
    var debugRenderedFrames: Int { scene.renderedFrames }
    var debugMusicName: String { music.name }
    var debugMusicDuration: TimeInterval { music.duration }
    var debugMusicPosition: Double { music.position }
    func debugSeekMusicEnd() { music.seekNearEnd() }
    func debugStart(_ n:Int) { start(n) }
    #endif
    private func start(_ n:Int) {
        menuBack=nil
        menuBackdrop(false)
        level=n; completed=false; hudTick = -1; ml_start(core,Int32(n)); scene.resetClock()
        scene.reducedMotion=progress.reduced || UIAccessibility.isReduceMotionEnabled
        overlay?.removeFromSuperview(); overlay=nil; header.isHidden=false; controls.isHidden=false
        hud.text=String(format:"%02d  ",n)+String(cString:ml_level_name(Int32(n)))
        subhud.text=String(cString:ml_level_hint(Int32(n)))
        playing=true; scene.running=true; sk.isPaused=false; UIApplication.shared.isIdleTimerDisabled=true; impact.prepare()
        updateMusic()
    }
    func pauseGame() {
        guard playing else { return }
        playing=false; scene.running=false; sk.isPaused=true; UIApplication.shared.isIdleTimerDisabled=false; ml_pause(core,1); scene.resetClock()
        showPause()
    }
    private func showPause() {
        menuBack={ [weak self] in self?.resume() }
        let stack=panel("", "Paused", String(cString:ml_level_name(Int32(level))))
        stack.addArrangedSubview(button("Resume",color:UIColor(rgb:0x346858)) { [weak self] in self?.resume() })
        stack.addArrangedSubview(button("Restart Level") { [weak self] in guard let self=self else{return}; self.start(self.level) })
        stack.addArrangedSubview(button("How to Play") { [weak self] in self?.howToPlay(fromPause:true) })
        stack.addArrangedSubview(button("Settings") { [weak self] in self?.settings(fromPause:true) })
        stack.addArrangedSubview(button("Home") { [weak self] in self?.showMenu() })
    }
    override var canBecomeFirstResponder: Bool { true }
    override func pressesBegan(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        let mapping: [String: Int32] = ["r":0,"b":1,"y":2,"g":3,"w":0,"s":1,"a":2,"d":3]
        for press in presses {
            guard let key=press.key else { continue }
            if key.keyCode == .keyboardEscape { if playing { pauseGame() } else { menuBack?() }; return }
            if playing, let color=mapping[key.charactersIgnoringModifiers.lowercased()] { ml_input(core,color); return }
        }
        super.pressesBegan(presses, with:event)
    }
    private func resume() {
        overlay?.removeFromSuperview(); overlay=nil; ml_pause(core,0); scene.resetClock(); playing=true; scene.running=true; sk.isPaused=false; UIApplication.shared.isIdleTimerDisabled=true
        updateMusic()
    }
    private func settings(fromPause:Bool=false) {
        menuBack={ [weak self] in self?.returnToMenu(fromPause) }
        let stack=panel("", "Settings", "", back:true)
        func toggle(_ title:String,_ value:Bool,_ set:@escaping (Bool)->Void) {
            let row=UIStackView(); row.addArrangedSubview(label(title,color:.white)); let control=UISwitch(); control.isOn=value
            control.accessibilityLabel=title; control.addAction(UIAction { _ in set(control.isOn) },for:.valueChanged); row.addArrangedSubview(control); stack.addArrangedSubview(row)
        }
        toggle("Music",!progress.musicMuted) { [weak self] in
            guard let self=self else{return}; self.progress.musicMuted = !$0
            self.updateMusic()
        }
        toggle("Sound effects",!progress.muted) { [weak self] in self?.progress.muted = !$0 }
        toggle("Haptics",progress.haptics) { [weak self] in self?.progress.haptics = $0 }
        toggle("Reduced motion",progress.reduced) { [weak self] in self?.progress.reduced=$0; self?.motionChanged() }
        stack.addArrangedSubview(label("Barlow typeface by Jeremy Tribby · SIL Open Font License. See the bundled OFL.txt.",size:12))
    }
    private func howToPlay(fromPause:Bool=false) {
        menuBack={ [weak self] in self?.returnToMenu(fromPause) }
        let stack=panel("", "How to Play", "Reach the top before the lava catches you.", back:true)
        let guide=ColorControls(); guide.heightAnchor.constraint(equalToConstant:144).isActive=true
        for i in 0..<4 {
            let badge=button(["R\nRED","B\nBLUE","Y\nYELLOW","G\nGREEN"][i],color:magneticColors[i].withAlphaComponent(0.18)) {}
            badge.isUserInteractionEnabled=false; badge.accessibilityTraits = .staticText
            badge.contentEdgeInsets=UIEdgeInsets(top:4,left:4,bottom:4,right:4)
            badge.titleLabel?.font=UIFont(name:"Barlow-SemiBold",size:14)
            badge.setTitleColor(magneticColors[i],for:.normal); badge.titleLabel?.numberOfLines=2; badge.titleLabel?.textAlignment = .center
            guide.addSubview(badge); guide.buttons.append(badge)
        }
        stack.addArrangedSubview(guide)
        for (title,detail) in [
            ("1  Match a color", "Tap a color to grab a matching magnet. Glowing outlines show which magnets are in reach."),
            ("2  Keep swinging", "Tap another color while swinging to catch the next magnet and climb higher."),
            ("3  Stay above the lava", "Avoid hazards and cross checkpoints. If you fall, you restart at your last checkpoint.")
        ] {
            stack.addArrangedSubview(label(title,size:19,color:.white)); stack.addArrangedSubview(label(detail,size:16))
        }
        #if targetEnvironment(macCatalyst)
        stack.addArrangedSubview(label("Keyboard: R/B/Y/G or W/S/A/D. Escape pauses.",size:15))
        #endif
        stack.addArrangedSubview(label("Stars: finish the level, beat the target time, and finish without a death.",size:15))
    }
    private func frame(_ s:[Float]) {
        if s[9]-hudTick>0.1 || hudTick<0 {
            hudTick=s[9]; bar.progress=s[26]
            subhud.text=s[4]==3 ? (s[21]>0 ? "Rival wins. Try again." : "Respawning…") : s[9]<7 ? String(cString:ml_level_hint(Int32(level))) : String(format:"%.1fs / %.0fs   ·   %d pts   ·   %d retries",s[9],s[27],Int(s[6]),Int(s[8]))
            subhud.textColor=UIColor(rgb:s[29]<200 ? 0xFFAF75 : 0xA0B4C7)
            for c in 0..<4 { colorButtons[c].alpha=s[30+c]>=0 ? 1 : 0.65; colorButtons[c].accessibilityValue=s[30+c]>=0 ? "Target in range" : "No target in range" }
        }
        let events=Int(s[22])
        let effect=events & 16 != 0 ? "complete" : events & 8 != 0 ? "death" : events & 2 != 0 ? "checkpoint" : events & 1 != 0 ? "attach" : events & 4 != 0 ? "swing" : nil
        if let effect=effect {
            if !progress.muted { sounds[effect]?.currentTime=0; sounds[effect]?.play() }
            if progress.haptics {
                if events & 24 != 0 { notification.notificationOccurred(events & 16 != 0 ? .success : .error) }
                else if events & 3 != 0 { impact.impactOccurred(intensity:events & 2 != 0 ? 1 : 0.5) }
            }
        }
        if s[5]>0 && !completed {
            menuBack={ [weak self] in self?.showMenu() }
            completed=true; playing=false; scene.running=false; sk.isPaused=true; UIApplication.shared.isIdleTimerDisabled=false; ml_pause(core,1); progress.record(level,s)
            let stack=panel("LEVEL COMPLETE",String(repeating:"★",count:Int(s[16]))+String(repeating:"☆",count:3-Int(s[16])),String(format:"%.1f seconds · %d points · %d retries\nPersonal best: %.1fs",s[9],Int(s[6]),Int(s[8]),progress.best(level)))
            if level<Int(ml_level_count()) { stack.addArrangedSubview(button("Next Level",color:UIColor(rgb:0x346858)) { [weak self] in guard let self=self else{return}; self.start(self.level+1) }) }
            else { stack.addArrangedSubview(label("All 40 levels complete!",color:.white)) }
            stack.addArrangedSubview(button("Play Again") { [weak self] in guard let self=self else{return}; self.start(self.level) })
            stack.addArrangedSubview(button("Home") { [weak self] in self?.showMenu() })
        }
    }
}

private final class TouchButton: UIButton {
    var accessibleAction: (() -> Void)?
    override func accessibilityActivate() -> Bool {
        if let action=accessibleAction { action(); return true }; return super.accessibilityActivate()
    }
}

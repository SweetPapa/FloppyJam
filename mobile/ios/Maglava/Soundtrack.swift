import AVFoundation

/// Queue the complete original playlist once; UI navigation never replaces it.
final class Soundtrack {
    private let player = AVQueuePlayer()
    private var observer: NSObjectProtocol?
    private var tracks: [URL] = []
    private var items: [AVPlayerItem] = []
    init() {
        tracks=(0..<Int(ml_music_count())).map {
            guard let url=Bundle.main.url(forResource:String(cString:ml_music_track(Int32($0))),withExtension:"m4a",subdirectory:"Audio") else { fatalError("Missing soundtrack") }
            return url
        }
        items=tracks.map { AVPlayerItem(url:$0) }
        for item in items { player.insert(item,after:nil) }
        player.volume=0.65
        observer=NotificationCenter.default.addObserver(forName:.AVPlayerItemDidPlayToEndTime,object:nil,queue:.main) { [weak self] note in
            guard let self=self,let finished=note.object as? AVPlayerItem,
                  let index=self.items.firstIndex(where: { $0 === finished }) else { return }
            let next=AVPlayerItem(url:self.tracks[index])
            self.items[index]=next; self.player.insert(next,after:nil)
        }
    }
    deinit { if let observer=observer { NotificationCenter.default.removeObserver(observer) }; player.pause() }
    func setPlaying(_ enabled:Bool) { if enabled { player.play() } else { player.pause() } }
    var name:String { (player.currentItem?.asset as? AVURLAsset)?.url.deletingPathExtension().lastPathComponent ?? "" }
    var position:Double { player.currentTime().seconds }
    var duration:Double { player.currentItem?.duration.seconds ?? 0 }
    #if DEBUG
    func seekNearEnd() { guard duration.isFinite else{return};player.seek(to:CMTime(seconds:max(0,duration-0.4),preferredTimescale:600),toleranceBefore:.zero,toleranceAfter:.zero) }
    #endif
}

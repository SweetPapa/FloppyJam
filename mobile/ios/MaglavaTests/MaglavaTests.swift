import XCTest
import UIKit
@testable import Maglava

@MainActor
final class MaglavaTests: XCTestCase {
    private func descendants(_ view: UIView) -> [UIView] { [view]+view.subviews.flatMap { descendants($0) } }
    private func button(_ controller: GameController, _ name: String) throws -> UIButton {
        try XCTUnwrap(descendants(controller.view).compactMap { $0 as? UIButton }.first { $0.accessibilityLabel==name || $0.title(for:.normal)==name })
    }
    private func wait(_ seconds: Double) async throws { try await Task.sleep(nanoseconds:UInt64(seconds*1_000_000_000)) }
    func testNativeTouchCompletionProgressAndPause() async throws {
        let app=try XCTUnwrap(UIApplication.shared.delegate as? AppDelegate)
        let controller=try XCTUnwrap(app.window?.rootViewController as? GameController)
        Progress().musicMuted=false
        app.applicationDidBecomeActive(UIApplication.shared)
        controller.debugStart(1)
        try await wait(0.3)
        XCTAssertGreaterThan(controller.debugRenderedFrames,0,"Metal must submit a 3D frame")
        XCTAssertEqual(controller.debugMusicName,"magLava-main-theme")
        XCTAssertGreaterThan(controller.debugMusicDuration,90,"The original song must be bundled")
        let red=try button(controller,"Tether to red magnet").frame
        let blue=try button(controller,"Tether to blue magnet").frame
        let yellow=try button(controller,"Tether to yellow magnet").frame
        let green=try button(controller,"Tether to green magnet").frame
        XCTAssertEqual(red.midX,blue.midX,accuracy:1)
        XCTAssertLessThan(red.maxY,blue.minY)
        XCTAssertLessThan(yellow.maxX,blue.minX)
        XCTAssertLessThan(blue.maxX,green.minX)
        for name in ["magLava-main-theme","magLava-game-bg-1","magLava-game-bg-2","magLava-game-bg-3"] {
            XCTAssertNotNil(Bundle.main.url(forResource:name,withExtension:"m4a",subdirectory:"Audio"))
        }
        try button(controller,"Pause game").sendActions(for:.touchUpInside)
        let paused=controller.debugSnapshot[9],musicBefore=controller.debugMusicPosition
        try await wait(0.25)
        XCTAssertEqual(controller.debugSnapshot[9],paused)
        XCTAssertGreaterThan(controller.debugMusicPosition,musicBefore+0.1,"Pause menu must keep music playing")
        try button(controller,"How to Play").sendActions(for:.touchUpInside)
        try await wait(0.15)
        XCTAssertEqual(controller.debugSnapshot[9],paused)
        try button(controller,"Back").sendActions(for:.touchUpInside)
        try button(controller,"Settings").sendActions(for:.touchUpInside)
        try button(controller,"Back").sendActions(for:.touchUpInside)
        try button(controller,"Resume").sendActions(for:.touchUpInside)
        let deadline=Date().addingTimeInterval(45)
        while Date()<deadline {
            let s=controller.debugSnapshot
            if s[5]>0 { break }
            if s[4]==0 {
                var color = -1, y=s[1]
                for c in 0..<4 { let index=Int(s[30+c]); if index>=0 && s[40+index*6+1]<y { y=s[40+index*6+1]; color=c } }
                if color>=0 {
                    let name=["red","blue","yellow","green"][color]
                    try button(controller,"Tether to \(name) magnet").sendActions(for:.touchDown)
                }
            }
            try await wait(0.1)
        }
        XCTAssertEqual(controller.debugSnapshot[5],1,"Native touch-input route did not finish")
        XCTAssertGreaterThan(Progress().stars(1),0)
        XCTAssertTrue(Progress().unlocked(2))
        try button(controller,"Next Level").sendActions(for:.touchUpInside)
        try await wait(0.1)
        XCTAssertEqual(controller.debugSnapshot[34],2)
        let trackBefore=controller.debugMusicName,musicBeforeInactive=controller.debugMusicPosition
        XCTAssertEqual(trackBefore,"magLava-main-theme","Stages must never replace the song")
        // UIApplication's interruption callback must pause before another frame advances.
        app.applicationWillResignActive(UIApplication.shared)
        let inactive=controller.debugSnapshot[9]
        try await wait(0.25)
        XCTAssertEqual(controller.debugSnapshot[9],inactive)
        XCTAssertEqual(controller.debugMusicPosition,musicBeforeInactive,accuracy:0.1)
        app.applicationDidBecomeActive(UIApplication.shared)
        try button(controller,"Restart Level").sendActions(for:.touchUpInside)
        try await wait(0.1)
        XCTAssertLessThan(controller.debugSnapshot[9],0.25)
        controller.pauseGame()
        try button(controller,"Home").sendActions(for:.touchUpInside)
        let frames=controller.debugRenderedFrames
        try await wait(0.25)
        XCTAssertGreaterThan(controller.debugRenderedFrames,frames,"Home lava must animate")
        XCTAssertFalse(descendants(controller.view).compactMap { $0 as? UIButton }.contains { $0.title(for:.normal)?.hasPrefix("01   ")==true })
        try button(controller,"Level Select").sendActions(for:.touchUpInside)
        XCTAssertEqual(descendants(controller.view).compactMap { $0 as? UIButton }.filter { $0.accessibilityIdentifier?.hasPrefix("level.")==true }.count,40)
        controller.view.layoutIfNeeded()
        let levelScroll=try XCTUnwrap(descendants(controller.view).compactMap { $0 as? UIScrollView }.first)
        levelScroll.setContentOffset(CGPoint(x:0,y:max(0,levelScroll.contentSize.height-levelScroll.bounds.height)),animated:false)
        let back=try button(controller,"Back")
        XCTAssertLessThan(back.convert(back.bounds,to:controller.view).maxY,controller.view.bounds.height)
        XCTAssertNil(back.superview as? UIStackView,"Back must stay outside the scrolling level list")
        try button(controller,"Back").sendActions(for:.touchUpInside)
        try button(controller,"How to Play").sendActions(for:.touchUpInside)
        XCTAssertTrue(descendants(controller.view).compactMap { $0 as? UILabel }.contains { $0.text=="1  Match a color" })
        controller.view.layoutIfNeeded()
        let helpScroll=try XCTUnwrap(descendants(controller.view).compactMap { $0 as? UIScrollView }.first)
        let helpFrame=helpScroll.convert(helpScroll.bounds,to:controller.view)
        XCTAssertGreaterThanOrEqual(helpFrame.minX,0)
        XCTAssertLessThanOrEqual(helpFrame.maxX,controller.view.bounds.width)
        XCTAssertLessThanOrEqual(helpScroll.contentSize.width,helpScroll.bounds.width+1,"Help should wrap, never scroll sideways")
        try button(controller,"Back").sendActions(for:.touchUpInside)
        try button(controller,"Settings").sendActions(for:.touchUpInside)
        XCTAssertEqual(descendants(controller.view).filter { $0 is UISwitch }.count,4)
        let selectors=descendants(controller.view).compactMap { $0 as? UIButton }
        let lava=try XCTUnwrap(selectors.first { $0.accessibilityIdentifier=="settings.lavaRate" })
        XCTAssertEqual(lava.menu?.children.count,7)
        XCTAssertEqual(lava.title(for:.normal),"1.5x")
        let language=try XCTUnwrap(selectors.first { $0.accessibilityIdentifier=="settings.language" })
        XCTAssertEqual(language.menu?.children.count,8)
        XCTAssertTrue(language.showsMenuAsPrimaryAction)
        XCTAssertTrue(language.menu?.children.contains { $0.title=="日本語" } ?? false)
        try button(controller,"Back").sendActions(for:.touchUpInside)
        XCTAssertEqual(controller.debugMusicName,trackBefore)
        for name in ["magLava-game-bg-1","magLava-game-bg-2","magLava-game-bg-3","magLava-main-theme"] {
            controller.debugSeekMusicEnd()
            let deadline=Date().addingTimeInterval(6)
            while controller.debugMusicName != name && Date()<deadline { try await wait(0.1) }
            XCTAssertEqual(controller.debugMusicName,name,"Playlist must advance and wrap")
            try await wait(0.3)
            XCTAssertGreaterThan(controller.debugMusicPosition,0)
        }
    }
}

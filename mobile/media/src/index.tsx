import React from 'react';
import {AbsoluteFill,Composition,Img,Sequence,interpolate,registerRoot,staticFile,useCurrentFrame,useVideoConfig} from 'remotion';
import {Audio,Video} from '@remotion/media';
const ink='#080e18',white='#eff4f8',muted='#a4b2c1',orange='#ff8856';
const font='Barlow, sans-serif';
const Font=()=> <style>{`@font-face{font-family:Barlow;src:url('${staticFile('fonts/Barlow-SemiBold.ttf')}')}*{box-sizing:border-box}`}</style>;
const clamp={extrapolateLeft:'clamp' as const,extrapolateRight:'clamp' as const};
const scenes=[
 {from:0,to:3,title:['RISE','OR BURN.'],tag:'MAGLAVA',detail:'A magnetic climb above rising lava.',stage:1,start:0},
 {from:3,to:10,title:['MATCH','YOUR COLOR.'],tag:'01 / CATCH',detail:'Four colors. One way up.',stage:1,start:1},
 {from:10,to:17,title:['CARRY','YOUR SWING.'],tag:'02 / CLIMB',detail:'Find your rhythm. Launch again.',stage:1,start:1},
 {from:17,to:24,title:['DODGE','THE DANGER.'],tag:'03 / SURVIVE',detail:'Ghosts. Lasers. Rising lava.',stage:6,start:1},
 {from:24,to:30,title:['FORTY','WAYS UP.'],tag:'40 HANDCRAFTED STAGES',detail:'Checkpoints. Personal bests. One more try.',stage:38,start:1},
 {from:30,to:36,title:['ONE MORE','LAUNCH.'],tag:'MAGLAVA',detail:'Swing. Climb. Survive.  •  maglava.io',stage:38,start:5},
];
function Scene({scene}:{scene:typeof scenes[number]}) {
 const frame=useCurrentFrame(),{fps}=useVideoConfig();
 const entrance=interpolate(frame,[0,14],[0,1],clamp),y=interpolate(frame,[0,18],[45,0],clamp);
 return <AbsoluteFill style={{background:ink,color:white,fontFamily:font}}>
  <Video muted src={staticFile(`captures/apple/iphone-stage-${scene.stage}.mp4`)} trimBefore={scene.start*fps} style={{position:'absolute',inset:0,width:'100%',height:'100%',objectFit:'cover',filter:'blur(36px)',opacity:.16}}/>
  <AbsoluteFill style={{background:'linear-gradient(110deg,#080e18 20%,transparent 90%)'}}/>
  <div style={{position:'absolute',width:1000,height:1000,borderRadius:1000,background:'radial-gradient(circle,#ff612322,transparent 68%)',left:-230,top:450}}/>
  <div style={{position:'absolute',left:112,top:165,width:920,opacity:entrance,transform:`translateY(${y}px)`}}>
   <div style={{fontSize:25,letterSpacing:7,color:'#73e7bc',marginBottom:55}}>{scene.tag}</div>
   {scene.title.map((line,i)=><div key={line} style={{fontSize:i===0?116:108,lineHeight:1.02,letterSpacing:-4,color:i===0?white:orange}}>{line}</div>)}
   <div style={{height:5,width:140,background:orange,margin:'44px 0'}}/>
   <div style={{fontSize:33,color:muted,lineHeight:1.45,maxWidth:740}}>{scene.detail}</div>
   <div style={{display:'flex',gap:18,marginTop:60}}>{['#ff526c','#ffd34d','#499fff','#46edaa'].map((c,i)=><div key={c} style={{width:20,height:20,borderRadius:20,background:c,boxShadow:`0 0 ${18+8*Math.sin(frame*.1+i)}px ${c}77`}}/>)}</div>
  </div>
  <div style={{position:'absolute',right:132,top:34,height:1012,width:466,borderRadius:34,overflow:'hidden',border:'2px solid #4e6175',boxShadow:'0 25px 90px #000c'}}>
   <Video muted src={staticFile(`captures/apple/iphone-stage-${scene.stage}.mp4`)} trimBefore={scene.start*fps} style={{width:'100%',height:'100%',objectFit:'cover'}}/>
  </div>
  <div style={{position:'absolute',bottom:58,left:112,fontSize:19,letterSpacing:3,color:'#768698'}}>ACTUAL NATIVE GAMEPLAY</div>
 </AbsoluteFill>;
}
function Trailer() {
 const frame=useCurrentFrame(),{fps,durationInFrames}=useVideoConfig();
 return <AbsoluteFill style={{background:ink}}><Font/>
  {scenes.map(scene=><Sequence key={scene.from} from={scene.from*fps} durationInFrames={(scene.to-scene.from)*fps}><Scene scene={scene}/></Sequence>)}
  <Sequence from={2*fps}><Audio src={staticFile('narration.wav')} volume={.95}/></Sequence>
  <Audio src={staticFile('music.m4a')} volume={f=>interpolate(f,[0,2*fps,33*fps,durationInFrames],[0,.18,.18,0],clamp)}/>
  <AbsoluteFill style={{background:'#000',opacity:interpolate(frame,[0,10,durationInFrames-20,durationInFrames],[1,0,0,1],clamp)}}/>
 </AbsoluteFill>;
}
function Preview() {
 const frame=useCurrentFrame(),{fps}=useVideoConfig();
 const clips=[{stage:1,from:0,length:9,start:0,label:'MATCH A COLOR. CATCH A MAGNET.'},{stage:6,from:9,length:9,start:1,label:'CARRY YOUR SWING.'},{stage:38,from:18,length:10,start:1,label:'CLIMB. DODGE. SURVIVE.'}];
 return <AbsoluteFill style={{background:ink,fontFamily:font}}><Font/>
  {clips.map(c=><Sequence key={c.stage} from={c.from*fps} durationInFrames={c.length*fps}>
   <AbsoluteFill><Video muted src={staticFile(`captures/apple/iphone-stage-${c.stage}.mp4`)} trimBefore={c.start*fps} style={{width:'100%',height:'100%',objectFit:'contain'}}/>
   <div style={{position:'absolute',top:235,left:20,right:20,padding:'20px 18px',background:'#080e18e8',color:white,textAlign:'center',fontSize:29,letterSpacing:1}}>{c.label}</div></AbsoluteFill>
  </Sequence>)}
  <Audio src={staticFile('music.m4a')} volume={f=>interpolate(f,[0,fps,26*fps,28*fps],[0,.65,.65,0],clamp)}/>
  <AbsoluteFill style={{background:'#000',opacity:interpolate(frame,[0,8,27*fps,28*fps],[1,0,0,1],clamp)}}/>
 </AbsoluteFill>;
}
type CardProps={platform:'iphone'|'ipad'|'android';image:string;title:string;subtitle:string;index:string};
function Card(p:CardProps) {
 const {width,height}=useVideoConfig(),pad=p.platform==='ipad';
 const margin=width*.065,top=height*.24;
 return <AbsoluteFill style={{background:ink,color:white,fontFamily:font}}><Font/>
  <AbsoluteFill style={{background:'radial-gradient(ellipse at 80% 12%,#70301b88,transparent 65%)'}}/>
  <div style={{position:'absolute',left:margin,top:height*.034,fontSize:width*.027,letterSpacing:width*.005,color:orange}}>MAGLAVA  /  {p.index}</div>
  <div style={{position:'absolute',left:margin,right:margin,top:height*.075,fontSize:width*(pad?.060:.079),letterSpacing:-width*.002,lineHeight:1.03}}>{p.title}</div>
  <div style={{position:'absolute',left:margin,right:margin,top:height*.173,fontSize:width*.034,color:muted}}>{p.subtitle}</div>
  <div style={{position:'absolute',top,left:margin,right:margin,bottom:height*.025,borderRadius:width*.038,overflow:'hidden',boxShadow:'0 25px 90px #000',background:'#050810',border:'2px solid #37465b'}}>
   <Img src={staticFile(p.image)} style={{width:'100%',height:'100%',objectFit:'contain'}}/>
  </div>
 </AbsoluteFill>;
}
const defaults:CardProps={platform:'iphone',image:'captures/apple/iphone-stage-1.png',title:'Catch a color.\nCarry your swing.',subtitle:'A magnetic climb above rising lava.',index:'01'};
function FeatureGraphic() {return <AbsoluteFill style={{background:ink,color:white,fontFamily:font}}><Font/>
 <AbsoluteFill style={{background:'radial-gradient(ellipse at 60% 100%,#84311988,transparent 70%)'}}/>
 <div style={{position:'absolute',left:65,top:110,fontSize:18,letterSpacing:4,color:'#73e7bc'}}>A MAGNETIC CLIMB</div>
 <div style={{position:'absolute',left:60,top:146,fontSize:105,letterSpacing:-4,color:orange}}>MAGLAVA</div>
 <div style={{position:'absolute',left:65,top:274,fontSize:30}}>Swing. Climb. Survive.</div>
 <div style={{position:'absolute',left:65,top:333,fontSize:19,color:muted}}>40 stages above rising lava.</div>
 <Img src={staticFile('captures/android/stage-6.png')} style={{position:'absolute',right:78,top:24,height:452,borderRadius:18,border:'1px solid #536476'}}/>
</AbsoluteFill>}
function Root(){return <>
 <Composition id="MagLavaTrailer" component={Trailer} durationInFrames={36*30} fps={30} width={1920} height={1080}/>
 <Composition id="AppPreview" component={Preview} durationInFrames={28*30} fps={30} width={886} height={1920}/>
 <Composition id="iPhoneCard" component={Card} defaultProps={defaults} durationInFrames={1} fps={30} width={1290} height={2796}/>
 <Composition id="iPadCard" component={Card} defaultProps={{...defaults,platform:'ipad'}} durationInFrames={1} fps={30} width={2064} height={2752}/>
 <Composition id="AndroidCard" component={Card} defaultProps={{...defaults,platform:'android'}} durationInFrames={1} fps={30} width={1080} height={2400}/>
 <Composition id="FeatureGraphic" component={FeatureGraphic} durationInFrames={1} fps={30} width={1024} height={500}/>
</>};
registerRoot(Root);

import React from 'react';
import {AbsoluteFill,Composition,Img,Sequence,interpolate,registerRoot,staticFile,useCurrentFrame,useVideoConfig} from 'remotion';
import {Audio,Video} from '@remotion/media';
const ink='#080e18',white='#eff4f8',muted='#a4b2c1',orange='#ff8856';
const font='Barlow, sans-serif';
const Font=()=> <style>{`@font-face{font-family:Barlow;src:url('${staticFile('fonts/Barlow-SemiBold.ttf')}')}@font-face{font-family:NotoJP;src:url('${staticFile('fonts/NotoJapanese.otf')}')}@font-face{font-family:NotoSC;src:url('${staticFile('fonts/NotoChinese.otf')}')}*{box-sizing:border-box}`}</style>;
const clamp={extrapolateLeft:'clamp' as const,extrapolateRight:'clamp' as const};
const stages=[1,6,12,20,30,38];
function Gameplay({portrait=false,preview=false}:{portrait?:boolean;preview?:boolean}) {
 const frame=useCurrentFrame(),{fps,durationInFrames}=useVideoConfig();
 const clips=preview?[{stage:1,from:0,length:8},{stage:6,from:8,length:8},{stage:12,from:16,length:6},{stage:38,from:22,length:6}]:[{stage:1,from:0,length:6},{stage:6,from:6,length:8},{stage:12,from:14,length:8},{stage:20,from:22,length:2},{stage:30,from:24,length:4},{stage:38,from:28,length:8}];
 return <AbsoluteFill style={{background:ink}}>
  {clips.map(({stage,from,length})=><Sequence key={stage} from={from*fps} durationInFrames={length*fps}>
   <AbsoluteFill>
    {!portrait&&<Video muted src={staticFile(`captures/apple/ipad-stage-${stage}.mp4`)} trimBefore={fps} style={{position:'absolute',inset:0,width:'100%',height:'100%',objectFit:'cover',filter:'blur(38px)',opacity:.25}}/>}
    <Video muted src={staticFile(`captures/apple/${portrait?'iphone':'ipad'}-stage-${stage}.mp4`)} trimBefore={fps} style={{position:'absolute',inset:0,width:'100%',height:'100%',objectFit:'contain'}}/>
   </AbsoluteFill>
  </Sequence>)}
  <Audio src={staticFile('music.m4a')} volume={f=>interpolate(f,[0,fps,durationInFrames-fps,durationInFrames],[0,.65,.65,0],clamp)}/>
  <AbsoluteFill style={{background:'#000',opacity:interpolate(frame,[0,8,durationInFrames-12,durationInFrames],[1,0,0,1],clamp)}}/>
 </AbsoluteFill>;
}
const Trailer=()=> <Gameplay/>;
const Portrait=()=> <Gameplay portrait/>;
const Preview=()=> <Gameplay portrait preview/>;
type CardProps={platform:'iphone'|'ipad'|'android';image:string;title:string;subtitle:string;index:string;locale:string};
function Card(p:CardProps) {
 const {width,height}=useVideoConfig(),pad=p.platform==='ipad';
 const font=p.locale==='ja'?'NotoJP, sans-serif':p.locale==='zh-CN'?'NotoSC, sans-serif':'Barlow, sans-serif';
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
const defaults:CardProps={platform:'iphone',image:'captures/apple/iphone-stage-1.png',title:'Catch a color.\nCarry your swing.',subtitle:'A magnetic climb above rising lava.',index:'01',locale:'en'};
type FeatureProps={locale:string;subtitle:string;detail:string};
function FeatureGraphic(p:FeatureProps) {const font=p.locale==='ja'?'NotoJP, sans-serif':p.locale==='zh-CN'?'NotoSC, sans-serif':'Barlow, sans-serif';return <AbsoluteFill style={{background:ink,color:white,fontFamily:font}}><Font/>
 <AbsoluteFill style={{background:'radial-gradient(ellipse at 60% 100%,#84311988,transparent 70%)'}}/>
 <div style={{position:'absolute',left:65,top:110,fontSize:18,letterSpacing:4,color:'#73e7bc'}}>MAGLAVA.IO</div>
 <div style={{position:'absolute',left:60,top:146,fontSize:105,letterSpacing:-4,color:orange}}>MAGLAVA</div>
 <div style={{position:'absolute',left:65,top:274,fontSize:30}}>{p.subtitle}</div>
 <div style={{position:'absolute',left:65,top:333,fontSize:19,color:muted}}>{p.detail}</div>
 <Img src={staticFile('captures/android/stage-6.png')} style={{position:'absolute',right:78,top:24,height:452,borderRadius:18,border:'1px solid #536476'}}/>
</AbsoluteFill>}
function Root(){return <>
 <Composition id="MagLavaTrailer" component={Trailer} durationInFrames={36*30} fps={30} width={1920} height={1080}/>
 <Composition id="GameplayPortrait" component={Portrait} durationInFrames={36*30} fps={30} width={1080} height={1920}/>
 <Composition id="AppPreview" component={Preview} durationInFrames={28*30} fps={30} width={886} height={1920}/>
 <Composition id="iPhoneCard" component={Card} defaultProps={defaults} durationInFrames={1} fps={30} width={1290} height={2796}/>
 <Composition id="iPadCard" component={Card} defaultProps={{...defaults,platform:'ipad'}} durationInFrames={1} fps={30} width={2064} height={2752}/>
 <Composition id="AndroidCard" component={Card} defaultProps={{...defaults,platform:'android'}} durationInFrames={1} fps={30} width={1080} height={2400}/>
 <Composition id="FeatureGraphic" component={FeatureGraphic} defaultProps={{locale:"en",subtitle:"Swing. Climb. Survive.",detail:"40 stages above rising lava."}} durationInFrames={1} fps={30} width={1024} height={500}/>
</>};
registerRoot(Root);

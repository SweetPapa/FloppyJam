import {readFile,writeFile} from 'node:fs/promises';
import {api,packageName} from './play-api.mjs';
const edit=await api('edits','POST',{});let committed=false;const images=[];
try {
 const details=await api(`edits/${edit.id}/details`);
 await api(`edits/${edit.id}/details`,'PUT',{...details,contactWebsite:'https://sweetpapa-games.web.app/maglava/support'});
 for(const [type,files] of [['icon',['icon.png']],['featureGraphic',['feature-graphic.png']],['phoneScreenshots',['android-game-1.png','android-game-2.png','android-complete.png']]]) {
  const existing=await api(`edits/${edit.id}/listings/en-US/${type}`);
  if(existing.images?.length)throw Error(`Existing ${type}: inspect before replacing artwork.`);
  for(const file of files) {
   const result=await api(`edits/${edit.id}/listings/en-US/${type}?uploadType=media`,'POST',await readFile(`mobile/.build/store/screenshots/${file}`),true,'image/png');
   images.push({type,file,id:result.image.id});
  }
 }
 const track=await api(`edits/${edit.id}/tracks/alpha`);
 for(const release of track.releases||[])for(const note of release.releaseNotes||[])note.text=note.text.replace(/ On Mac,.*$/s,'');
 await api(`edits/${edit.id}/tracks/alpha`,'PUT',track);
 await api(`edits/${edit.id}:validate`,'POST');
 await api(`edits/${edit.id}:commit`,'POST');committed=true;
 const receipt={packageName,images,track,committed:true};
 await writeFile('mobile/.build/store/play-artwork.json',JSON.stringify(receipt,null,2));console.log(JSON.stringify(receipt,null,2));
}finally{if(!committed)await api(`edits/${edit.id}`,'DELETE').catch(()=>{});}

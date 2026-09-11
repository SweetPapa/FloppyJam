import {readFile,writeFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {parseArgs} from 'node:util';

// Explicit build selection prevents accidentally uploading a stale signed bundle.
// This prepares the existing closed-testing draft; Console submission is separate.
const {values}=parseArgs({options:{build:{type:'string'}}});
if(!values.build || !/^[1-9][0-9]*$/.test(values.build))throw Error('Usage: node mobile/tools/store/upload-play.mjs --build 2');
const build=Number(values.build);
const signed=JSON.parse(await readFile(`mobile/.build/store/android-signing-${build}.json`,'utf8'));
if(signed.build!==build || signed.package!=='dev.fofo.maglava')throw Error('Signing receipt does not match the requested game build.');
const filename=`MagLava-${signed.version}-${build}.aab`;
const bytes=await readFile(`mobile/.build/store/${filename}`);
if(createHash('sha256').update(bytes).digest('hex')!==signed.files[filename])throw Error('Bundle changed since signing.');
const listing=JSON.parse(await readFile(new URL('../../store/en-US.json',import.meta.url),'utf8'));
const {api,packageName}=await import('./play-api.mjs');
const edit=await api('edits','POST',{});let committed=false;
try {
 const tracks=(await api(`edits/${edit.id}/tracks`)).tracks;
 const bundles=(await api(`edits/${edit.id}/bundles`)).bundles ?? [];
 if(bundles.some(b=>Number(b.versionCode)>=build))throw Error('This or a newer bundle already exists; inspect the saved release before uploading.');
 const alpha=tracks.find(t=>t.track==='alpha');
 if(alpha?.releases?.some(r=>r.status!=='draft'))throw Error('Closed testing has an active release. Review it before replacing the track.');
 const bundle=await api(`edits/${edit.id}/bundles?uploadType=media`,'POST',bytes,true);
 if(Number(bundle.versionCode)!==build)throw Error('Uploaded bundle has an unexpected version code.');
 const {language,title,shortDescription,fullDescription}=listing;
 await api(`edits/${edit.id}/listings/en-US`,'PUT',{language,title,shortDescription,fullDescription});
 const release={name:`${signed.version} (${build}) — Rise or burn`,versionCodes:[String(build)],status:'draft',releaseNotes:[{language:'en-US',text:listing.whatsNew}]};
 await api(`edits/${edit.id}/tracks/alpha`,'PUT',{track:'alpha',releases:[release]});
 await api(`edits/${edit.id}:validate`,'POST');
 await api(`edits/${edit.id}:commit`,'POST');committed=true;
 const receipt={packageName,track:'alpha',release,bundle,committed:true};
 await writeFile(`mobile/.build/store/play-upload-${build}.json`,JSON.stringify(receipt,null,2)+'\n');console.log(JSON.stringify(receipt,null,2));
}finally{if(!committed)await api(`edits/${edit.id}`,'DELETE').catch(()=>{});}

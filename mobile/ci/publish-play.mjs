import {publishPlayListings,releaseNotes} from './store-localizations.mjs';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {parseArgs} from 'node:util';
import {api,packageName} from '../tools/store/play-api.mjs';
const {values}=parseArgs({options:{bundle:{type:'string'},build:{type:'string'}}});
if(!values.bundle||!/^\d+$/.test(values.build??''))throw Error('Provide --bundle and --build');
const bytes=await readFile(values.bundle),sha=createHash('sha256').update(bytes).digest('hex');
const edit=await api('edits','POST',{});let committed=false;
try {
 const bundles=(await api(`edits/${edit.id}/bundles`)).bundles??[];
 if(bundles.some(b=>Number(b.versionCode)>Number(values.build)))throw Error('A newer Play build exists; refusing to roll back closed testing.');
 let bundle=bundles.find(b=>String(b.versionCode)===values.build);
 if(bundle && bundle.sha256!==sha)throw Error('That version code belongs to a different signed bundle. Start a new UAT run.');
 if(!bundle)bundle=await api(`edits/${edit.id}/bundles?uploadType=media`,'POST',bytes,true);
 if(String(bundle.versionCode)!==values.build)throw Error('Bundle version does not match the UAT run.');
 await publishPlayListings(api,edit.id);
 const release={name:`MagLava 1.0 (${values.build}) UAT`,versionCodes:[values.build],status:'completed',releaseNotes};
 let blocker=null;
 try {await api(`edits/${edit.id}/tracks/alpha`,'PUT',{track:'alpha',releases:[release]});await api(`edits/${edit.id}:validate`,'POST');}
 catch(error) {
  if(!error.message.includes('Only releases with status draft'))throw error;
  blocker=error.message;release.status='draft';await api(`edits/${edit.id}/tracks/alpha`,'PUT',{track:'alpha',releases:[release]});
 }
 // Preserve the public test once its first promotion has been explicitly made.
 const beta=await api(`edits/${edit.id}/tracks/beta`);
 if(!blocker&&beta.releases?.some(r=>r.status==='completed'))await api(`edits/${edit.id}/tracks/beta`,'PUT',{track:'beta',releases:[{...release,name:`MagLava 1.0 (${values.build}) Public Beta`}]});
 await api(`edits/${edit.id}:validate`,'POST');await api(`edits/${edit.id}:commit`,'POST');committed=true;
 const receipt={packageName,track:'alpha',release,blocker,sha256:sha};
 await mkdir('mobile/.build/uat/receipts',{recursive:true});await writeFile('mobile/.build/uat/receipts/google-play.json',JSON.stringify(receipt,null,2)+'\n');
 console.log(JSON.stringify(receipt,null,2));
 if(blocker)console.log('::warning::Bundle saved as a closed-testing draft; Play Console setup still prevents rollout.');
}finally{if(!committed)await api(`edits/${edit.id}`,'DELETE').catch(()=>{});}

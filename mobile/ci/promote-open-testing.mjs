// Promote an already uploaded, verified closed-test build. Never touches production.
import {api,packageName} from '../tools/store/play-api.mjs';
import {parseArgs} from 'node:util';
import {mkdir,writeFile} from 'node:fs/promises';
const {values}=parseArgs({options:{build:{type:'string'},apply:{type:'boolean',default:false}}});
if(!/^\d+$/.test(values.build??''))throw Error('Provide --build');
const edit=await api('edits','POST',{});let committed=false;
try {
 const source=await api(`edits/${edit.id}/tracks/alpha`);
 const release=source.releases?.find(r=>r.versionCodes.includes(values.build)&&r.status==='completed');
 if(!release)throw Error('The selected build must already be completed on closed testing.');
 const current=await api(`edits/${edit.id}/tracks/beta`);
 if(current.releases?.some(r=>r.versionCodes.some(v=>Number(v)>Number(values.build))))throw Error('Refusing to replace a newer open-test build.');
 const countries=await api(`edits/${edit.id}/countryAvailability/beta`);
 if(!countries?.countries?.length&&!countries?.restOfWorld)throw Error('Select countries in Play Console → Testing → Open testing → Countries/regions, then rerun.');
 const target={track:'beta',releases:[{...release,name:`MagLava 1.0 (${values.build}) Public Beta`}]};
 if(values.apply){await api(`edits/${edit.id}/tracks/beta`,'PUT',target);await api(`edits/${edit.id}:validate`,'POST');await api(`edits/${edit.id}:commit`,'POST');committed=true;}
 const receipt={packageName,build:values.build,applied:committed,...target,countries};
 await mkdir('mobile/.build/uat/receipts',{recursive:true});await writeFile('mobile/.build/uat/receipts/open-testing.json',JSON.stringify(receipt,null,2)+'\n');console.log(JSON.stringify(receipt,null,2));
}finally{if(!committed)await api(`edits/${edit.id}`,'DELETE').catch(()=>{});}

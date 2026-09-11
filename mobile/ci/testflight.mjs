import {apple} from '../tools/store/apple-api.mjs';
import {mkdir,writeFile,readFile} from 'node:fs/promises';
import {parseArgs} from 'node:util';
const {values}=parseArgs({options:{build:{type:'string'},inspect:{type:'boolean',default:false}}});
if(!/^\d+$/.test(values.build??''))throw Error('Provide --build');
const appId='6809634261';
if(values.inspect) {const rows=(await apple(`builds?filter[app]=${appId}&filter[version]=${values.build}`)).data;console.log(JSON.stringify(rows.map(x=>({id:x.id,version:x.attributes.version,state:x.attributes.processingState}))));process.exit(0);}
let build;
for(let attempt=0;attempt<50;attempt++) {
 const rows=(await apple(`builds?filter[app]=${appId}&filter[version]=${values.build}`)).data;
 build=rows.find(x=>x.attributes.processingState==='VALID');
 if(build)break;
 if(rows.some(x=>['FAILED','INVALID'].includes(x.attributes.processingState)))throw Error('Apple rejected binary processing.');
 console.log('Waiting for Apple build processing...');await new Promise(r=>setTimeout(r,30000));
}
if(!build)throw Error('Apple build processing is still pending; rerun this step after processing completes.');
const listing=JSON.parse(await readFile(new URL('../store/en-US.json',import.meta.url)));
const locales=(await apple(`builds/${build.id}/betaBuildLocalizations`)).data;
const existing=locales.find(x=>x.attributes.locale==='en-US');
const whatsNew=listing.whatsNew+'\nPlease test targeting, long swings, checkpoint recovery and audio across menus. Build '+values.build+'.';
if(existing)await apple(`betaBuildLocalizations/${existing.id}`,'PATCH',{data:{type:'betaBuildLocalizations',id:existing.id,attributes:{whatsNew}}});
else await apple('betaBuildLocalizations','POST',{data:{type:'betaBuildLocalizations',attributes:{locale:'en-US',whatsNew},relationships:{build:{data:{type:'builds',id:build.id}}}}});
const groups=(await apple(`apps/${appId}/betaGroups`)).data;
let publicLink=null;
for(const group of groups.filter(g=>['MagLava Playtest','MagLava Closed Beta'].includes(g.attributes.name))) {
 const members=(await apple(`betaGroups/${group.id}/builds`)).data;
 if(!members.some(b=>b.id===build.id))await apple(`betaGroups/${group.id}/relationships/builds`,'POST',{data:[{type:'builds',id:build.id}]});
 if(!group.attributes.isInternalGroup) {
  const updated=(await apple(`betaGroups/${group.id}`,'PATCH',{data:{type:'betaGroups',id:group.id,attributes:{publicLinkEnabled:true,publicLinkLimitEnabled:true,publicLinkLimit:1000}}})).data;
  publicLink=updated.attributes.publicLink;
 }
}
let review='pending';
try {
 const detail=(await apple(`builds/${build.id}/buildBetaDetail`)).data;
 review=detail.attributes.externalBuildState;
 if(review==='READY_FOR_BETA_SUBMISSION') {
  await apple('betaAppReviewSubmissions','POST',{data:{type:'betaAppReviewSubmissions',relationships:{build:{data:{type:'builds',id:build.id}}}}});review='SUBMITTED_FOR_BETA_REVIEW';
 }
} catch(error) {review='Beta review needs attention: '+error.message;console.log('::warning::'+review);}
const receipt={appId,buildId:build.id,version:values.build,processingState:build.attributes.processingState,review,publicLink};
await mkdir('mobile/.build/uat/receipts',{recursive:true});await writeFile('mobile/.build/uat/receipts/testflight.json',JSON.stringify(receipt,null,2)+'\n');console.log(JSON.stringify(receipt,null,2));

import { readFile,writeFile } from 'node:fs/promises';
import { apple,bundleId } from './apple-api.mjs';
const appId='6809634261';
const listing=JSON.parse(await readFile(new URL('../../store/en-US.json',import.meta.url),'utf8'));
const app=(await apple(`apps/${appId}`)).data;
if(app.attributes.bundleId!==bundleId)throw Error('Unexpected app');
const savedReview=(await apple(`betaAppReviewDetails/${appId}`)).data;
const reviewPhone=process.env.MAGLAVA_REVIEW_PHONE || savedReview.attributes.contactPhone;
const reviewNotes='No login, purchases, ads or external services. Tap Play or Continue to start. Color buttons select a matching node, not direction. Levels unlock sequentially. Test all controls, checkpoints, hazards and pause/resume. On Mac, the same onscreen controls work with the mouse; keyboard R/B/Y/G or W/S/A/D selects colors and Escape pauses.';
const reviewContact={contactPhone:reviewPhone,contactFirstName:'Forrester',contactLastName:'Terry',contactEmail:listing.feedbackEmail,demoAccountRequired:false,notes:reviewNotes};
const infos=(await apple(`apps/${appId}/appInfos`)).data;
const info=infos.find(x=>x.attributes.state==='PREPARE_FOR_SUBMISSION');
const age=(await apple(`appInfos/${info.id}/ageRatingDeclaration`)).data;
const ageAttributes=JSON.parse(await readFile(new URL('../../store/apple-age-rating.json',import.meta.url),'utf8'));
await apple(`ageRatingDeclarations/${age.id}`,'PATCH',{data:{type:'ageRatingDeclarations',id:age.id,attributes:ageAttributes}});
const locales=(await apple(`appInfos/${info.id}/appInfoLocalizations`)).data;
const en=locales.find(x=>x.attributes.locale==='en-US');
await apple(`appInfoLocalizations/${en.id}`,'PATCH',{data:{type:'appInfoLocalizations',id:en.id,attributes:{name:listing.title,subtitle:listing.subtitle,privacyPolicyUrl:'https://sweetpapa-games.web.app/maglava/privacy'}}});
await apple(`appInfos/${info.id}`,'PATCH',{data:{type:'appInfos',id:info.id,relationships:{primaryCategory:{data:{type:'appCategories',id:'GAMES'}},primarySubcategoryOne:{data:{type:'appCategories',id:'GAMES_ACTION'}},primarySubcategoryTwo:{data:{type:'appCategories',id:'GAMES_CASUAL'}}}}});
const versions=(await apple(`apps/${appId}/appStoreVersions`)).data;
for(const v of versions){
 if(v.attributes.appStoreState!=='PREPARE_FOR_SUBMISSION')continue;
 const locales=(await apple(`appStoreVersions/${v.id}/appStoreVersionLocalizations`)).data;
 const en=locales.find(x=>x.attributes.locale==='en-US');
 await apple(`appStoreVersionLocalizations/${en.id}`,'PATCH',{data:{type:'appStoreVersionLocalizations',id:en.id,attributes:{description:listing.fullDescription,keywords:listing.keywords,supportUrl:'https://sweetpapa-games.web.app/maglava/support',promotionalText:'40 magnetic climbs. Carry your swing, outpace the lava and reach the summit.'}}});
 await apple(`appStoreVersions/${v.id}`,'PATCH',{data:{type:'appStoreVersions',id:v.id,attributes:{copyright:'2026 Forrester Terry',usesIdfa:false}}});
 if(reviewPhone) {
  const detail=(await apple(`appStoreVersions/${v.id}/appStoreReviewDetail`)).data;
  if(detail)await apple(`appStoreReviewDetails/${detail.id}`,'PATCH',{data:{type:'appStoreReviewDetails',id:detail.id,attributes:reviewContact}});
  else await apple('appStoreReviewDetails','POST',{data:{type:'appStoreReviewDetails',attributes:reviewContact,relationships:{appStoreVersion:{data:{type:'appStoreVersions',id:v.id}}}}});
 }
}
const groups=(await apple(`apps/${appId}/betaGroups`)).data;
for(const [name,internal] of [['MagLava Playtest',true],['MagLava Public Beta',false]]){
 if(!groups.some(g=>g.attributes.name===name)) await apple('betaGroups','POST',{data:{type:'betaGroups',attributes:{name,isInternalGroup:internal,publicLinkEnabled:false},relationships:{app:{data:{type:'apps',id:appId}}}}});
}
const beta=(await apple(`apps/${appId}/betaAppLocalizations`)).data;
const attributes={locale:'en-US',description:listing.fullDescription,feedbackEmail:listing.feedbackEmail};
if(beta.some(x=>x.attributes.locale==='en-US')){const b=beta.find(x=>x.attributes.locale==='en-US');await apple(`betaAppLocalizations/${b.id}`,'PATCH',{data:{type:'betaAppLocalizations',id:b.id,attributes:{description:attributes.description,feedbackEmail:attributes.feedbackEmail}}});}
else await apple('betaAppLocalizations','POST',{data:{type:'betaAppLocalizations',attributes,relationships:{app:{data:{type:'apps',id:appId}}}}});
if(reviewPhone) await apple(`betaAppReviewDetails/${appId}`,'PATCH',{data:{type:'betaAppReviewDetails',id:appId,attributes:reviewContact}});
const summary={appId,bundleId,versions:versions.map(v=>({id:v.id,platform:v.attributes.platform})),groups:(await apple(`apps/${appId}/betaGroups`)).data.map(g=>({id:g.id,...g.attributes})),status:'Metadata saved; consult TestFlight receipt for build processing and beta review status'};
summary.reviewContactPhonePending=!reviewPhone;
await writeFile('mobile/.build/store/apple-setup.json',JSON.stringify(summary,null,2));console.log(JSON.stringify(summary,null,2));

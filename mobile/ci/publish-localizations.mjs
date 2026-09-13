// Localized beta/store metadata for the existing editable version. No production release.
import {localizations,publishPlayListings,releaseNotes} from './store-localizations.mjs';
import {mkdir,writeFile} from 'node:fs/promises';
import {parseArgs} from 'node:util';
const {values}=parseArgs({options:{platform:{type:'string',default:'all'},build:{type:'string',default:'1016'}}});
if(!['all','ios','android'].includes(values.platform)||!/^\d+$/.test(values.build))throw Error('Invalid platform/build');
const receipt=[];
if(values.platform!=='android') {
 const {apple}=await import('../tools/store/apple-api.mjs');
 const app='6809634261';
 const info=(await apple(`apps/${app}/appInfos`)).data.find(x=>x.attributes.state==='PREPARE_FOR_SUBMISSION');
 if(!info)throw Error('No editable app information');
 const infoLocales=(await apple(`appInfos/${info.id}/appInfoLocalizations`)).data;
 const betaLocales=(await apple(`apps/${app}/betaAppLocalizations`)).data;
 const versions=(await apple(`apps/${app}/appStoreVersions`)).data.filter(x=>['PREPARE_FOR_SUBMISSION','DEVELOPER_REJECTED','REJECTED'].includes(x.attributes.appStoreState));
 const build=(await apple(`builds?filter[app]=${app}&filter[version]=${values.build}`)).data.find(x=>x.attributes.processingState==='VALID');
 const buildLocales=build?(await apple(`builds/${build.id}/betaBuildLocalizations`)).data:[];
 async function upsert(type,existing,attrs,relationship,parentType,parentId) {
  if(existing){const {locale,...update}=attrs;return apple(`${type}/${existing.id}`,'PATCH',{data:{type,id:existing.id,attributes:update}});}
  return apple(type,'POST',{data:{type,attributes:attrs,relationships:{[relationship]:{data:{type:parentType,id:parentId}}}}});
 }
 for(const r of localizations) {
  await upsert('appInfoLocalizations',infoLocales.find(x=>x.attributes.locale===r.appleLocale),{locale:r.appleLocale,name:r.title,subtitle:r.subtitle,privacyPolicyUrl:'https://sweetpapa-games.web.app/maglava/privacy'},'appInfo','appInfos',info.id);
  await upsert('betaAppLocalizations',betaLocales.find(x=>x.attributes.locale===r.appleLocale),{locale:r.appleLocale,description:r.fullDescription,feedbackEmail:r.feedbackEmail},'app','apps',app);
  if(build)await upsert('betaBuildLocalizations',buildLocales.find(x=>x.attributes.locale===r.appleLocale),{locale:r.appleLocale,whatsNew:r.whatsNew},'build','builds',build.id);
 }
 for(const v of versions) {
  const existing=(await apple(`appStoreVersions/${v.id}/appStoreVersionLocalizations`)).data;
  for(const r of localizations)await upsert('appStoreVersionLocalizations',existing.find(x=>x.attributes.locale===r.appleLocale),{locale:r.appleLocale,description:r.fullDescription,keywords:r.keywords,promotionalText:r.promotionalText,supportUrl:'https://sweetpapa-games.web.app/maglava/support'},'appStoreVersion','appStoreVersions',v.id);
 }
 receipt.push({platform:'apple',locales:localizations.map(x=>x.appleLocale),versions:versions.map(x=>x.id),build:build?.id});
}
if(values.platform!=='ios') {
 const {api}=await import('../tools/store/play-api.mjs');const edit=await api('edits','POST',{});let committed=false;
 try {
  await publishPlayListings(api,edit.id);
  for(const track of ['alpha','beta']) {
   const current=await api(`edits/${edit.id}/tracks/${track}`);
   if(current.releases?.some(x=>x.versionCodes.includes(values.build))) {
    for(const r of current.releases)if(r.versionCodes.includes(values.build))r.releaseNotes=releaseNotes;
    await api(`edits/${edit.id}/tracks/${track}`,'PUT',current);
   }
  }
  await api(`edits/${edit.id}:validate`,'POST');await api(`edits/${edit.id}:commit`,'POST');committed=true;
  receipt.push({platform:'google',locales:localizations.map(x=>x.playLocale)});
 }finally{if(!committed)await api(`edits/${edit.id}`,'DELETE').catch(()=>{});}
}
await mkdir('mobile/.build/uat/receipts',{recursive:true});await writeFile(`mobile/.build/uat/receipts/localizations-${values.platform}.json`,JSON.stringify(receipt,null,2)+'\n');console.log(JSON.stringify(receipt,null,2));

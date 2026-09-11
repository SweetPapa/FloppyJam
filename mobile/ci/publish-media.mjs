import {readFile,writeFile,mkdir,readdir} from 'node:fs/promises';
import {join} from 'node:path';
import {createHash} from 'node:crypto';
import {parseArgs} from 'node:util';
const {values}=parseArgs({options:{directory:{type:'string'},platform:{type:'string',default:'all'}}});
if(!values.directory)throw Error('Provide --directory with rendered store cards');
const dir=values.directory;const receipt=[];
await mkdir('mobile/.build/uat/receipts',{recursive:true});
async function uploadAsset(apple,type,setType,setId,file,extra={}) {
 const content=await readFile(file),checksum=createHash('md5').update(content).digest('hex');
 const fileName=`maglava-${createHash('sha256').update(content).digest('hex').slice(0,16)}.${type==='appPreviews'?'mp4':'png'}`;
 const asset=(await apple(type,'POST',{data:{type,attributes:{fileName,fileSize:content.length,...extra},relationships:{[setType]:{data:{type:setType+'s',id:setId}}}}})).data;
 try {
  for(const op of asset.attributes.uploadOperations) {
   const response=await fetch(op.url,{method:op.method,headers:Object.fromEntries(op.requestHeaders.map(h=>[h.name,h.value])),body:content.subarray(op.offset,op.offset+op.length)});
   if(!response.ok)throw Error(`Asset upload returned ${response.status}`);
  }
  await apple(`${type}/${asset.id}`,'PATCH',{data:{type,id:asset.id,attributes:{uploaded:true,sourceFileChecksum:checksum}}});
  for(let n=0;n<40;n++) {
   const current=(await apple(`${type}/${asset.id}`)).data;
   const state=current.attributes.assetDeliveryState?.state;
   if(state==='COMPLETE')return current;
   if(state==='FAILED')throw Error(JSON.stringify(current.attributes.assetDeliveryState));
   await new Promise(r=>setTimeout(r,3000));
  }
  throw Error('Apple media processing still pending; old screenshots have been retained.');
 }catch(error){await apple(`${type}/${asset.id}`,'DELETE').catch(()=>{});throw error;}
}
if(values.platform!=='android') {
 const {apple}=await import('../tools/store/apple-api.mjs');
 const version=(await apple('apps/6809634261/appStoreVersions')).data.find(v=>v.attributes.platform==='IOS'&&['PREPARE_FOR_SUBMISSION','DEVELOPER_REJECTED','REJECTED'].includes(v.attributes.appStoreState));
 if(!version)throw Error('No editable iOS listing; refusing to modify an in-review or live version.');
 const locale=(await apple(`appStoreVersions/${version.id}/appStoreVersionLocalizations`)).data.find(x=>x.attributes.locale==='en-US');
 const sets=(await apple(`appStoreVersionLocalizations/${locale.id}/appScreenshotSets`)).data;
 for(const [platform,display] of [['iphone','APP_IPHONE_67'],['ipad','APP_IPAD_PRO_3GEN_129']]) {
  let set=sets.find(x=>x.attributes.screenshotDisplayType===display);
  if(!set)set=(await apple('appScreenshotSets','POST',{data:{type:'appScreenshotSets',attributes:{screenshotDisplayType:display},relationships:{appStoreVersionLocalization:{data:{type:'appStoreVersionLocalizations',id:locale.id}}}}})).data;
  const previous=(await apple(`appScreenshotSets/${set.id}/appScreenshots`)).data;
  const keep=[];
  for(let i=1;i<=3;i++) {
   const file=join(dir,'store',`${platform}-${i}.png`),content=await readFile(file);
   const name=`maglava-${createHash('sha256').update(content).digest('hex').slice(0,16)}.png`;
   const existing=previous.find(x=>x.attributes.fileName===name&&x.attributes.assetDeliveryState?.state==='COMPLETE');
   const asset=existing??await uploadAsset(apple,'appScreenshots','appScreenshotSet',set.id,file);
   keep.push(asset.id);receipt.push({platform,file:name,id:asset.id});
  }
  // New images are all complete before any old screenshot is removed.
  for(const old of previous.filter(x=>!keep.includes(x.id)))await apple(`appScreenshots/${old.id}`,'DELETE');
  await apple(`appScreenshotSets/${set.id}/relationships/appScreenshots`,'PATCH',{data:keep.map(id=>({type:'appScreenshots',id}))});
 }
 const previewFile=join(dir,'MagLava-AppPreview.mp4');
 try {
  await readFile(previewFile);
  let set=(await apple(`appStoreVersionLocalizations/${locale.id}/appPreviewSets`)).data.find(x=>x.attributes.previewType==='IPHONE_67');
  if(!set)set=(await apple('appPreviewSets','POST',{data:{type:'appPreviewSets',attributes:{previewType:'IPHONE_67'},relationships:{appStoreVersionLocalization:{data:{type:'appStoreVersionLocalizations',id:locale.id}}}}})).data;
  const previous=(await apple(`appPreviewSets/${set.id}/appPreviews`)).data;
  const content=await readFile(previewFile),name=`maglava-${createHash('sha256').update(content).digest('hex').slice(0,16)}.mp4`;
  let asset=previous.find(x=>x.attributes.fileName===name&&x.attributes.assetDeliveryState?.state==='COMPLETE');
  if(!asset)asset=await uploadAsset(apple,'appPreviews','appPreviewSet',set.id,previewFile,{mimeType:'video/mp4',previewFrameTimeCode:'00:00:05:00'});
  for(const old of previous.filter(x=>x.id!==asset.id))await apple(`appPreviews/${old.id}`,'DELETE');
  receipt.push({platform:'iphone-preview',id:asset.id});
 }catch(error) {if(error.code!=='ENOENT')throw error;}
}
if(values.platform!=='ios') {
 const {api}=await import('../tools/store/play-api.mjs');const edit=await api('edits','POST',{});let committed=false;
 try {
  const path=`edits/${edit.id}/listings/en-US/phoneScreenshots`;
  const previous=(await api(path)).images??[];
  const keep=[];
  for(let i=1;i<=3;i++) {
   const content=await readFile(join(dir,'store',`android-${i}.png`));
   const sha=createHash('sha256').update(content).digest('hex');
   let asset=previous.find(x=>x.sha256===sha);
   if(!asset)asset=(await api(path+'?uploadType=media','POST',content,true,'image/png')).image;
   keep.push(asset.id);receipt.push({platform:'android',id:asset.id,sha256:sha});
  }
  for(const old of previous.filter(x=>!keep.includes(x.id)))await api(path+'/'+old.id,'DELETE');
  const feature=await readFile(join(dir,'store','feature-graphic.png'));
  const featurePath=`edits/${edit.id}/listings/en-US/featureGraphic`;
  const featureSha=createHash('sha256').update(feature).digest('hex');
  const oldFeature=(await api(featurePath)).images??[];
  if(!oldFeature.some(x=>x.sha256===featureSha)) {
   // Play allows a single feature graphic. This is atomic within the edit.
   await api(featurePath,'DELETE');
   const asset=(await api(featurePath+'?uploadType=media','POST',feature,true,'image/png')).image;
   receipt.push({platform:'android-feature',id:asset.id,sha256:featureSha});
  }
  await api(`edits/${edit.id}:validate`,'POST');await api(`edits/${edit.id}:commit`,'POST');committed=true;
 }finally{if(!committed)await api(`edits/${edit.id}`,'DELETE').catch(()=>{});}
}
await writeFile(`mobile/.build/uat/receipts/store-media-${values.platform}.json`,JSON.stringify(receipt,null,2)+'\n');console.log(JSON.stringify(receipt,null,2));

import {mkdir,writeFile} from 'node:fs/promises';
import {apple,bundleId} from './apple-api.mjs';
const dir='mobile/.build/store';await mkdir(dir,{recursive:true});
const bundle=(await apple(`bundleIds?filter[identifier]=${bundleId}`)).data[0];
if(!bundle)throw Error('Expected the registered MagLava bundle');
const certificate=(await apple('certificates?filter[certificateType]=DISTRIBUTION')).data.find(c=>c.attributes.serialNumber==='3C1FB41089D400C864A5282761E16504');
if(!certificate||Date.parse(certificate.attributes.expirationDate)<=Date.now())throw Error('Expected the existing valid SPT Apple Distribution identity');
for(const [platform,profileType] of [['iOS','IOS_APP_STORE'],['macOS','MAC_CATALYST_APP_STORE']]){
 const name=`MagLava ${platform} App Store`;
 let profile=(await apple(`profiles?filter[name]=${encodeURIComponent(name)}`)).data.find(p=>p.attributes.profileState==='ACTIVE'&&Date.parse(p.attributes.expirationDate)>Date.now());
 if(!profile)profile=(await apple('profiles','POST',{data:{type:'profiles',attributes:{name,profileType},relationships:{bundleId:{data:{type:'bundleIds',id:bundle.id}},certificates:{data:[{type:'certificates',id:certificate.id}]}}}})).data;
 await writeFile(`${dir}/MagLava-${platform}.mobileprovision`,Buffer.from(profile.attributes.profileContent,'base64'),{mode:0o600});
 console.log(JSON.stringify({platform,name,uuid:profile.attributes.uuid,expires:profile.attributes.expirationDate}));
}

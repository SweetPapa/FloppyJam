// Add only MagLava support pages to the current live hosting version; preserve every existing file/config.
import {readFile,writeFile} from 'node:fs/promises';
import {homedir} from 'node:os';
import {createRequire} from 'node:module';
import {createHash} from 'node:crypto';
import {gzipSync} from 'node:zlib';
import {execFileSync} from 'node:child_process';
const npmRoot=execFileSync('npm',['root','-g'],{encoding:'utf8'}).trim();
const require=createRequire(`${npmRoot}/firebase-tools/package.json`);
const {getAccessToken}=require('./lib/auth.js');
const config=JSON.parse(await readFile(`${homedir()}/.config/configstore/firebase-tools.json`,'utf8'));
const token=await getAccessToken(config.tokens.refresh_token,['https://www.googleapis.com/auth/firebase']);
async function request(path,method='GET',body){const r=await fetch(`https://firebasehosting.googleapis.com/v1beta1/${path}`,{method,headers:{Authorization:`Bearer ${token.access_token}`,'Content-Type':'application/json'},...(body?{body:JSON.stringify(body)}:{})});const d=await r.json();if(!r.ok)throw Error(d.error?.message||`Hosting ${r.status}`);return d;}
const site='sites/sweetpapa-games';
const current=(await request(`${site}/releases?pageSize=1`)).releases[0].version;
const files={};let pageToken;
do {const page=await request(`${current.name}/files?pageSize=1000${pageToken?`&pageToken=${encodeURIComponent(pageToken)}`:''}`);for(const f of page.files||[])files[f.path]=f.hash;pageToken=page.nextPageToken;}while(pageToken);
const uploads=new Map();const additions=[];
for(const name of ['privacy','support']){
 const path=`/maglava/${name}.html`;const data=gzipSync(await readFile(`promo-site/public${path}`));
 const hash=createHash('sha256').update(data).digest('hex');files[path]=hash;uploads.set(hash,data);additions.push(path);
}
console.log(JSON.stringify({base:current.name,preservedFiles:Object.keys(files).length-additions.length,additions},null,2));
if(!process.argv.includes('--publish'))process.exit(0);
const version=await request(`${site}/versions`,'POST',{config:current.config});
const populated=await request(`${version.name}:populateFiles`,'POST',{files});
for(const hash of populated.uploadRequiredHashes||[]){
 if(!uploads.has(hash))throw Error('An existing live file unexpectedly needs uploading; refusing to publish an incomplete site.');
 const url=new URL(`${populated.uploadUrl}/${hash}`);if(!url.hostname.endsWith('.googleapis.com'))throw Error('Unexpected hosting upload host');
 const response=await fetch(url,{method:'POST',headers:{Authorization:`Bearer ${token.access_token}`,'Content-Type':'application/octet-stream'},body:uploads.get(hash)});
 if(!response.ok)throw Error(`File upload ${response.status}`);
}
await request(`${version.name}?updateMask=status`,'PATCH',{status:'FINALIZED'});
const latest=(await request(`${site}/releases?pageSize=1`)).releases[0].version;
if(latest.name!==current.name)throw Error('Live site changed during preparation; refusing to overwrite a concurrent deployment.');
const released=await request(`${site}/releases?versionName=${encodeURIComponent(version.name)}`,'POST',{message:'Add MagLava store support and privacy pages'});
await writeFile('mobile/.build/store/support-release.json',JSON.stringify({previous:current.name,release:released,additions},null,2));
console.log('Published MagLava support and privacy pages; existing live content preserved.');

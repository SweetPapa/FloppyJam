import {readFile} from 'node:fs/promises';
export const localizations=Object.values(JSON.parse(await readFile(new URL('../store/localizations.json',import.meta.url),'utf8')));
for(const row of localizations) {
 for(const [field,limit] of Object.entries({title:30,subtitle:30,shortDescription:80,fullDescription:4000,keywords:100,whatsNew:500,promotionalText:170})) {
  if(typeof row[field]!=='string'||!row[field].length||[...row[field]].length>limit)throw Error(`Invalid ${row.siteLocale} ${field}`);
 }
 if(row.cardTitles.length!==6||row.cardSubtitles.length!==6)throw Error('Six screenshot captions required');
}
if(localizations.length!==7)throw Error('All seven game languages are required');
export async function publishPlayListings(api,editId) {
 for(const r of localizations)await api(`edits/${editId}/listings/${r.playLocale}`,'PUT',{language:r.playLocale,title:r.title,shortDescription:r.shortDescription,fullDescription:r.fullDescription});
}
export const releaseNotes=localizations.map(r=>({language:r.playLocale,text:r.whatsNew}));

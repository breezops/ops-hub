import {test} from 'node:test';
import assert from 'node:assert/strict';
import {handleHitman} from './hitman-publisher.mjs';
const digest=async b=>Buffer.from(await crypto.subtle.digest('SHA-256',b)).toString('hex');
const token='b'.repeat(64),zip=Buffer.from('PK\x03\x04a test fixture');
const manifest=(v,sha)=>({schema:1,project:'hitman',version:v,mod_build:'preview.2',game_version:'3.290.1',runtime:1,protocol:2,path:`/downloads/hitman/HITMAN-Coop-${v}.zip`,sha256:sha,bytes:zip.length});
async function fixture(){
 const files=new Map();let sequence=0;
 const bucket={async get(k){return files.get(k)||null;},async head(k){return files.get(k)||null;},async put(k,b,o={}){
  const old=files.get(k);
  if(o.onlyIf?.etagDoesNotMatch==='*'&&old||o.onlyIf?.etagMatches&&o.onlyIf.etagMatches!==old?.etag)return null;
  const data=Buffer.from(b),etag=String(++sequence),metadata=o.httpMetadata||{};
  const value={body:data,size:data.length,etag,httpEtag:`"${etag}"`,customMetadata:o.customMetadata,
   json:async()=>JSON.parse(data),writeHttpMetadata(h){if(metadata.contentType)h.set('Content-Type',metadata.contentType);}};
  files.set(k,value);return value;
 }};
 const sha=await digest(zip),fallback=manifest('0.1.0',sha),env={BUCKET:bucket,HITMAN_PUBLISH_TOKEN_SHA256:await digest(Buffer.from(token))};
 const request=(path,body,headers={})=>new Request('https://example.test'+path,{method:'PUT',headers:{Authorization:'Bearer '+token,...headers},body});
 const run=r=>handleHitman(r,env,fallback);
 const upload=v=>run(request('/api/publish/hitman/package?version='+v,zip,{'X-Content-Sha256':sha}));
 const publish=m=>run(request('/api/publish/hitman/manifest',JSON.stringify(m)));
 return {files,env,fallback,request,run,upload,publish,sha};
}
test('unauthorized writes fail before storage; unrelated Stray routes are untouched',async()=>{
 const f=await fixture();assert.equal((await f.run(new Request('https://example.test/api/publish/hitman/manifest',{method:'PUT',body:'{}'}))).status,401);
 assert.equal(await f.run(new Request('https://example.test/release.json')),null);
 assert.equal(await f.run(new Request('https://example.test/api/health')),null);assert.equal(f.files.size,0);
});
test('manifest cannot precede ZIP or name a different game, protocol, hash or size',async()=>{
 const f=await fixture(),m=manifest('0.1.1',f.sha);
 assert.equal((await f.publish(m)).status,409);await f.upload(m.version);
 for(const patch of [{project:'stray'},{protocol:6},{path:'/downloads/Stray.zip'},{bytes:999},{sha256:'a'.repeat(64)}])assert.notEqual((await f.publish({...m,...patch})).status,200);
 assert.equal(f.files.has('hitman/release.json'),false);
});
test('verified ZIP then manifest exposes correct file and latest links',async()=>{
 const f=await fixture();assert.equal((await f.upload('0.1.1')).status,200);
 assert.equal((await f.publish(manifest('0.1.1',f.sha))).status,200);
 const m=await (await f.run(new Request('https://example.test/hitman/release.json'))).json();assert.equal(m.version,'0.1.1');
 const response=await f.run(new Request('https://example.test'+m.path));assert.deepEqual(Buffer.from(await response.arrayBuffer()),zip);
 assert.equal((await f.run(new Request('https://example.test/hitman/download'))).headers.get('Location'),m.path);
 assert.equal((await f.run(new Request('https://example.test/hitman/checksum'))).headers.get('Location'),m.path+'.sha256');
});
test('retries are idempotent; immutable files and published versions reject changes and rollback',async()=>{
 const f=await fixture();await f.upload('0.1.2');await f.publish(manifest('0.1.2',f.sha));
 assert.equal((await f.upload('0.1.2')).status,200);assert.equal((await f.publish(manifest('0.1.2',f.sha))).status,200);
 const other=Buffer.from('PK\x03\x04different');
 assert.equal((await f.run(f.request('/api/publish/hitman/package?version=0.1.2',other,{'X-Content-Sha256':await digest(other)}))).status,409);
 assert.equal((await f.publish({...manifest('0.1.2',f.sha),mod_build:'different'})).status,409);
 await f.upload('0.1.1');assert.equal((await f.publish(manifest('0.1.1',f.sha))).status,409);
});
test('oversized, malformed and bad-checksum uploads cannot change the channel',async()=>{
 const f=await fixture();assert.equal((await f.run(f.request('/api/publish/hitman/manifest','{'))).status,400);
 assert.equal((await f.run(f.request('/api/publish/hitman/package?version=../stray',zip))).status,400);
 assert.equal((await f.run(f.request('/api/publish/hitman/package?version=0.1.1',zip,{'X-Content-Sha256':'a'.repeat(64)}))).status,400);
 assert.equal((await f.run(f.request('/api/publish/hitman/manifest','{}',{'Content-Length':'999999'}))).status,400);
 assert.equal(f.files.size,0);
});

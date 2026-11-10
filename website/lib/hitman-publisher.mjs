// HITMAN-only release storage. No route can write Stray assets or lobby data.
const VERSION='(0|[1-9]\\d{0,4})\\.(0|[1-9]\\d{0,4})\\.(0|[1-9]\\d{0,4})';
const validVersion=v=>typeof v==='string'&&new RegExp(`^${VERSION}$`).test(v);
const MAX=32*1024*1024;
const reply=(data,status=200)=>Response.json(data,{status,headers:{'Cache-Control':'no-store','X-Content-Type-Options':'nosniff'}});
const hash=async bytes=>[...new Uint8Array(await crypto.subtle.digest('SHA-256',bytes))].map(v=>v.toString(16).padStart(2,'0')).join('');
async function bounded(request,max){
  if(Number(request.headers.get('content-length'))>max)throw Error('size');
  const reader=request.body?.getReader();const chunks=[];let total=0;
  if(reader)while(true){const {done,value}=await reader.read();if(done)break;total+=value.length;if(total>max){await reader.cancel();throw Error('size');}chunks.push(value);}
  const bytes=new Uint8Array(total);let offset=0;for(const c of chunks){bytes.set(c,offset);offset+=c.length;}return bytes;
}
export function validateManifest(m){
  if(!m||m.schema!==1||m.project!=='hitman'||!validVersion(m.version)||m.runtime!==1||m.protocol!==2||m.game_version!=='3.290.1'||
    typeof m.mod_build!=='string'||!m.mod_build.length||m.mod_build.length>100||!Number.isSafeInteger(m.bytes)||m.bytes<1||m.bytes>MAX||
    !/^[a-f0-9]{64}$/.test(m.sha256)||m.path!==`/downloads/hitman/HITMAN-Coop-${m.version}.zip`)throw Error('manifest');
  return m;
}
function compare(a,b){const x=a.split('.').map(Number),y=b.split('.').map(Number);for(let i=0;i<3;i++)if(x[i]!==y[i])return x[i]-y[i];return 0;}
export async function handleHitman(request,env,fallback){
 const url=new URL(request.url),p=url.pathname,bucket=env.BUCKET;
 const publishing=p.startsWith('/api/publish/hitman/');
 if(publishing){
  if(request.method!=='PUT')return reply({error:'Method not allowed'},405);
  const token=request.headers.get('authorization')?.match(/^Bearer ([a-f0-9]{64})$/)?.[1];
  if(!env.HITMAN_PUBLISH_TOKEN_SHA256||!token||await hash(new TextEncoder().encode(token))!==env.HITMAN_PUBLISH_TOKEN_SHA256)return reply({error:'Unauthorized'},401);
  if(!bucket)return reply({error:'Release storage unavailable'},503);
  try{
   if(p==='/api/publish/hitman/manifest'){
    const bytes=await bounded(request,8192),m=validateManifest(JSON.parse(new TextDecoder().decode(bytes)));
    const object=await bucket.head(m.path.slice(1));
    if(!object||object.size!==m.bytes||object.customMetadata?.sha256!==m.sha256)return reply({error:'Upload and verify the matching ZIP first'},409);
    const old=await bucket.get('hitman/release.json');
    const current=old?validateManifest(await old.json()):fallback;
    if(compare(m.version,current.version)<0)return reply({error:'Release rollback is not allowed'},409);
    if(m.version===current.version&&(m.sha256!==current.sha256||m.bytes!==current.bytes||m.mod_build!==current.mod_build))return reply({error:'Published versions are immutable'},409);
    // Compare-and-set prevents concurrent manifests from overwriting newer ones.
    const put=await bucket.put('hitman/release.json',bytes,{httpMetadata:{contentType:'application/json',cacheControl:'no-store'},
      onlyIf:old?{etagMatches:old.etag}:{etagDoesNotMatch:'*'}});
    if(!put)return reply({error:'Another publication completed; retry'},409);
    return reply({published:m.version,sha256:m.sha256,bytes:m.bytes});
   }
   const v=url.searchParams.get('version');if(!validVersion(v))return reply({error:'Invalid version'},400);
   const kind=p.slice('/api/publish/hitman/'.length);
   if(!['package','notes','checksum'].includes(kind))return reply({error:'Unknown publish route'},404);
   const bytes=await bounded(request,kind==='package'?MAX:65536);
   let key,type;
   if(kind==='package'){
    const digest=await hash(bytes);
    if(!bytes.length||digest!==request.headers.get('x-content-sha256'))return reply({error:'ZIP checksum mismatch'},400);
    if(bytes[0]!==80||bytes[1]!==75||bytes[2]!==3||bytes[3]!==4)return reply({error:'Expected a ZIP archive'},400);
    key=`downloads/hitman/HITMAN-Coop-${v}.zip`;type='application/zip';
    if(v===fallback.version&&digest!==fallback.sha256)return reply({error:'Existing static release is immutable'},409);
   }else if(kind==='notes'){key=`downloads/hitman/CHANGELOG-${v}.md`;type='text/plain; charset=utf-8';}
   else{
    key=`downloads/hitman/HITMAN-Coop-${v}.zip.sha256`;type='text/plain; charset=utf-8';
    const object=await bucket.head(`downloads/hitman/HITMAN-Coop-${v}.zip`);
    if(!object||new TextDecoder().decode(bytes).trim()!==`${object.customMetadata.sha256}  HITMAN-Coop-${v}.zip`)return reply({error:'Checksum must match uploaded package'},409);
   }
   const digest=await hash(bytes),old=await bucket.head(key);
   if(old)return old.customMetadata?.sha256===digest?reply({unchanged:true,sha256:digest,bytes:old.size}):reply({error:'Versioned files are immutable'},409);
   const put=await bucket.put(key,bytes,{onlyIf:{etagDoesNotMatch:'*'},customMetadata:{sha256:digest},httpMetadata:{contentType:type,cacheControl:'public, max-age=31536000, immutable'}});
   return put?reply({uploaded:key,sha256:digest,bytes:bytes.length}):reply({error:'Concurrent upload; retry'},409);
  }catch(error){if(['size','manifest'].includes(error.message)||error instanceof SyntaxError)return reply({error:'Invalid or oversized publication'},400);console.error('HITMAN publication failed');return reply({error:'Publication failed'},503);}
 }
 if(!['GET','HEAD'].includes(request.method))return null;
 if(['/hitman/release.json','/hitman/download','/hitman/changelog','/hitman/checksum'].includes(p)){
  const object=bucket&&await bucket.get('hitman/release.json');const m=object?validateManifest(await object.json()):fallback;
  if(p!=='/hitman/release.json')return new Response(null,{status:302,headers:{Location:p.endsWith('download')?m.path:p.endsWith('checksum')?m.path+'.sha256':`/downloads/hitman/CHANGELOG-${m.version}.md`,'Cache-Control':'no-store'}});
  return new Response(request.method==='HEAD'?null:JSON.stringify(m),{headers:{'Content-Type':'application/json','Cache-Control':'no-store','X-Content-Type-Options':'nosniff'}});
 }
 if(p.startsWith('/downloads/hitman/')&&bucket){
  const object=await bucket.get(p.slice(1));if(!object)return null;
  const headers=new Headers({'X-Content-Type-Options':'nosniff','Content-Length':String(object.size),ETag:object.httpEtag});object.writeHttpMetadata(headers);
  if(p.endsWith('.zip'))headers.set('Content-Disposition',`attachment; filename="${p.split('/').at(-1)}"`);
  return new Response(request.method==='HEAD'?null:object.body,{headers});
 }
 return null;
}

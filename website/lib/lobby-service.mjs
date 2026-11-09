// Ephemeral join-code directory. No gameplay relay or public release-upload route.
const ALPHABET='23456789ABCDEFGHJKLMNPQRSTUVWXYZ';
export const LIFETIME=10*60*1000;
const CODE=/^[23456789ABCDEFGHJKLMNPQRSTUVWXYZ]{8}$/;
const json=(data,status=200)=>Response.json(data,{status,headers:{'Cache-Control':'no-store','X-Content-Type-Options':'nosniff'}});
async function hash(value){return [...new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(value)))].map(v=>v.toString(16).padStart(2,'0')).join('');}
function randomHex(){return [...crypto.getRandomValues(new Uint8Array(32))].map(v=>v.toString(16).padStart(2,'0')).join('');}
function randomCode(){return [...crypto.getRandomValues(new Uint8Array(8))].map(v=>ALPHABET[v%32]).join('');}
function ipv4(value){
 if(typeof value!=='string'||!/^\d{1,3}(\.\d{1,3}){3}$/.test(value)||value.split('.').some(v=>+v>255))return false;
 const [a,b]=value.split('.').map(Number);
 return a!==0&&a!==10&&a!==127&&a<224&&!(a===192&&b===168)&&!(a===172&&b>=16&&b<=31)&&!(a===169&&b===254)&&!(a===100&&b>=64&&b<=127);
}
async function body(request){
 if(Number(request.headers.get('content-length')||0)>2048)throw Error('Request is too large.');
 const reader=request.body?.getReader();let bytes=0;const chunks=[];
 if(reader)while(true){const {value,done}=await reader.read();if(done)break;bytes+=value.length;if(bytes>2048){await reader.cancel();throw Error('Request is too large.');}chunks.push(value);}
 const raw=chunks.map(v=>new TextDecoder().decode(v)).join('');
 const value=JSON.parse(raw);if(!value||Array.isArray(value)||typeof value!=='object')throw Error('Invalid request.');return value;
}
async function limit(db,ip,scope,maximum,now,salt){
 const identity=await hash(salt+'|'+ip);const bucket=Math.floor(now/60000);
 const id=scope+':'+identity+':'+bucket;
 const r=await db.prepare('INSERT INTO rate_limits (id,count,expires) VALUES (?,1,?) ON CONFLICT(id) DO UPDATE SET count=count+1 RETURNING count').bind(id,now+120000).first();
 return {allowed:r.count<=maximum,identity};
}
export async function handleService(request,env,release,clock=Date.now){
 const url=new URL(request.url);if(!url.pathname.startsWith('/api/'))return null;
 if(url.pathname==='/api/release'&&request.method==='GET')return json(release);
 if(url.pathname==='/api/health'&&request.method==='GET')return json({service:'Stray Co-op',ready:!!env.DB,version:release.version});
 if(!env.DB||!env.LOBBY_RATE_SALT)return json({error:'Lobby service is being configured. Try again shortly.'},503);
 const db=env.DB;const now=clock();
 const ip=request.headers.get('cf-connecting-ip')||'';
 if(!ip)return json({error:'Could not determine the client address.'},400);
 const route=url.pathname.match(/^\/api\/lobbies(?:\/([A-Z2-9]{8}))?$/);
 if(!route||route[1]&&!CODE.test(route[1]))return json({error:'Lobby not found or expired.'},404);
 const code=route[1];
 try{
  const creating=!code&&request.method==='POST';
  const rl=await limit(db,ip,creating?'create':'lookup',creating?6:90,now,env.LOBBY_RATE_SALT);
  if(!rl.allowed)return json({error:'Too many attempts. Wait a minute and try again.'},429);
  if(creating){
   const data=await body(request);
   if(data.version!==release.version||data.protocol!==release.protocol)return json({error:'An update is available. Update both launchers before hosting.'},409);
   if(!Number.isInteger(data.port)||data.port<1024||data.port>65535)return json({error:'Invalid game port.'},400);
   if(!ipv4(ip)&&!(env.DEV_ALLOW_LOOPBACK==='1'&&ip==='127.0.0.1'))return json({error:'Hosting requires an IPv4 connection to the lobby service.'},400);
   await db.batch([db.prepare('DELETE FROM lobbies WHERE expires < ?').bind(now),db.prepare('DELETE FROM rate_limits WHERE expires < ?').bind(now)]);
   const active=await db.prepare('SELECT count(*) AS n FROM lobbies WHERE ip_hash=? AND expires>?').bind(rl.identity,now).first();
   if(active.n>=3)return json({error:'You already have active lobbies. Close an old session or wait ten minutes.'},429);
   const token=randomHex();const key=randomHex();const ownerHash=await hash(token);
   let created;
   for(let i=0;i<4;i++){
    const candidate=randomCode();
    const result=await db.prepare('INSERT OR IGNORE INTO lobbies (code,owner_hash,ip,ip_hash,port,session_key,version,protocol,expires,created) VALUES (?,?,?,?,?,?,?,?,?,?)').bind(candidate,ownerHash,ip,rl.identity,data.port,key,release.version,release.protocol,now+LIFETIME,now).run();
    if(result.meta.changes){created=candidate;break;}
   }
   if(!created)return json({error:'Could not create a lobby. Please try again.'},503);
   return json({code:created,owner_token:token,key,host:ip,port:data.port,version:release.version,protocol:release.protocol,expires:now+LIFETIME},201);
  }
  if(!code)return json({error:'Method not allowed.'},405);
  const lobby=await db.prepare('SELECT * FROM lobbies WHERE code=? AND expires>?').bind(code,now).first();
  if(!lobby)return json({error:'Lobby not found or expired. Ask the host for their current code.'},404);
  if(request.method==='GET')return json({code,host:lobby.ip,port:lobby.port,key:lobby.session_key,version:lobby.version,protocol:lobby.protocol,expires:lobby.expires});
  if(!['POST','DELETE'].includes(request.method))return json({error:'Method not allowed.'},405);
  const token=request.headers.get('authorization')?.match(/^Bearer ([0-9a-f]{64})$/)?.[1];
  if(!token||await hash(token)!==lobby.owner_hash)return json({error:'This launcher does not own that lobby.'},403);
  if(request.method==='DELETE'){await db.prepare('DELETE FROM lobbies WHERE code=? AND owner_hash=?').bind(code,lobby.owner_hash).run();return json({closed:true});}
  await db.prepare('UPDATE lobbies SET expires=? WHERE code=? AND owner_hash=?').bind(now+LIFETIME,code,lobby.owner_hash).run();
  return json({expires:now+LIFETIME});
 }catch(e){
  if(e instanceof SyntaxError||e.message==='Request is too large.'||e.message==='Invalid request.')return json({error:'Invalid lobby request.'},400);
  // Never log request bodies, session keys, join codes, or database rows.
  console.error('Lobby operation failed:',e?.name||'Error');
  return json({error:'Lobby service could not complete the request. Please retry.'},503);
 }
}

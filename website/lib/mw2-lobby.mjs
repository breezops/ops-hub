// MW2 launcher lobby only. This service never relays or starts the game.
export const MW2_PROTOCOL = 1;
export const ROOM_TTL = 90_000;
export const MEMBER_TTL = 45_000;
const PREFIX = '/mw2/api';
const ALPHABET = 'ABCDEFGHJKLMNPQRSTUVWXYZ';
const CODE = /^[A-HJ-NP-Z]{4,8}$/;
const TOKEN = /^[a-f0-9]{64}$/;
export const MISSIONS = ['overkill-concept','the-pit','sniper-fi','o-cristo-redentor','evasion','suspension','overwatch','body-count','bomb-squad','race','big-brother','hidden','breach-and-clear','time-trial','homeland-security','snatch-and-grab','wardriving','wreckage','acceptable-losses','terminal','estate-takedown','wetwork','high-explosive','armor-piercing'];
export const DIFFICULTIES = ['recruit','regular','hardened','veteran'];
const json = (value, status=200) => Response.json(value, {status, headers:{'Cache-Control':'no-store','X-Content-Type-Options':'nosniff'}});
class Failure extends Error { constructor(status,message) { super(message); this.status=status; } }
const fail = (status,message) => { throw new Failure(status,message); };
async function sha(value) { return [...new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(value)))].map(b=>b.toString(16).padStart(2,'0')).join(''); }
function code() {
  let value='';
  while(value.length<6) for(const b of crypto.getRandomValues(new Uint8Array(8))) { if(b<240 && value.length<6) value+=ALPHABET[b%24]; }
  return value;
}
async function payload(request) {
  if(!request.headers.get('content-type')?.startsWith('application/json')) fail(415,'Use an application/json request.');
  const reader=request.body?.getReader(); if(!reader) fail(400,'A request body is required.');
  const chunks=[]; let total=0;
  while(true) { const {value,done}=await reader.read(); if(done)break; total+=value.length; if(total>4096){await reader.cancel(); fail(413,'Request is too large.');} chunks.push(value); }
  const bytes=new Uint8Array(total); let offset=0; for(const chunk of chunks){bytes.set(chunk,offset);offset+=chunk.length;}
  try { const data=JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(bytes)); if(!data||Array.isArray(data)||typeof data!=='object')throw Error(); return data; }
  catch { fail(400,'Invalid JSON request.'); }
}
function identity(data) {
  if(data.protocol!==MW2_PROTOCOL) fail(409,'Update your launcher to join this lobby protocol.');
  if(typeof data.name!=='string') fail(400,'Enter a player name.');
  const name=data.name.trim().normalize('NFC');
  if(!/^[\p{L}\p{N} _.-]{1,24}$/u.test(name)) fail(400,'Use 1-24 letters, numbers, spaces, dots, underscores or hyphens.');
  if(typeof data.member_token!=='string'||!TOKEN.test(data.member_token)) fail(400,'Invalid launcher identity.');
  return {name,token:data.member_token};
}
const schemas=new WeakMap();
async function schema(db) {
  if(schemas.has(db))return schemas.get(db);
  const pending=(async()=>{
    await db.prepare('CREATE TABLE IF NOT EXISTS mw2_launcher_rooms (code TEXT PRIMARY KEY, state TEXT NOT NULL, revision INTEGER NOT NULL DEFAULT 0, expires INTEGER NOT NULL, creator_hash TEXT NOT NULL)').run();
    await db.prepare('CREATE INDEX IF NOT EXISTS mw2_launcher_rooms_expiry ON mw2_launcher_rooms(expires)').run();
    await db.prepare('CREATE TABLE IF NOT EXISTS mw2_launcher_limits (id TEXT PRIMARY KEY, count INTEGER NOT NULL, expires INTEGER NOT NULL)').run();
  })();
  schemas.set(db,pending);
  try{await pending;}catch(error){schemas.delete(db);throw error;}
}
async function rate(db, ip, salt, operation, now) {
  const identity=await sha(salt+'|mw2|'+ip);
  const key=operation+':'+identity+':'+Math.floor(now/60000);
  const row=await db.prepare('INSERT INTO mw2_launcher_limits (id,count,expires) VALUES (?,1,?) ON CONFLICT(id) DO UPDATE SET count=count+1 RETURNING count').bind(key,now+120000).first();
  if(row.count>(operation==='create'?6:operation==='join'?24:300)) fail(429,'Too many attempts. Wait a minute before retrying.');
  return identity;
}
function prune(room,now) { room.players=room.players.map((p,i)=>p && (i===0||now-p.seen<MEMBER_TTL)?p:null); }
function view(room,hash,now) {
  return {code:room.code,protocol:MW2_PROTOCOL,mission:room.mission,difficulty:room.difficulty,
    you_slot:room.players.findIndex(p=>p?.hash===hash),gameplay_available:false,
    players:room.players.map((p,slot)=>p?{slot,name:p.name,host:slot===0,online:now-p.seen<MEMBER_TTL,service_rtt_ms:p.rtt}:null)};
}
export async function handleMw2Lobby(request,env,clock=Date.now) {
  const url=new URL(request.url); if(!url.pathname.startsWith(PREFIX+'/'))return null;
  if(url.pathname===PREFIX+'/health'&&request.method==='GET')return json({service:'MW2 launcher lobby',protocol:MW2_PROTOCOL,ready:!!(env.DB&&env.LOBBY_RATE_SALT),gameplay_available:false});
  const match=url.pathname.match(/^\/mw2\/api\/lobbies(?:\/([A-Za-z]{4,8})(?:\/(join|heartbeat))?)?$/);
  if(!match)return json({error:'Lobby endpoint not found.'},404);
  if(!env.DB||!env.LOBBY_RATE_SALT)return json({error:'Lobby service is unavailable. Try again shortly.'},503);
  const origin=request.headers.get('origin'); if(origin&&origin!==url.origin)return json({error:'Cross-site requests are not supported.'},403);
  const ip=request.headers.get('cf-connecting-ip')||(env.DEV_ALLOW_LOOPBACK==='1'?'127.0.0.1':'');
  if(!ip)return json({error:'Client address unavailable.'},400);
  const roomCode=match[1]?.toUpperCase(), action=match[2], method=request.method;
  if(roomCode&&!CODE.test(roomCode))return json({error:'Invalid lobby code.'},400);
  if((!roomCode&&method!=='POST')||(action&&method!=='POST')||(roomCode&&!action&&!['GET','PATCH','DELETE'].includes(method)))return json({error:'Method not allowed.'},405);
  const now=clock(), db=env.DB;
  try {
    await schema(db);
    const ipHash=await rate(db,ip,env.LOBBY_RATE_SALT,!roomCode?'create':action==='join'?'join':'member',now);
    const data=method==='POST'||method==='PATCH'?await payload(request):{};
    const bearer=request.headers.get('authorization')?.match(/^Bearer ([a-f0-9]{64})$/)?.[1];
    if(!roomCode) {
      const person=identity(data), hash=await sha(person.token);
      if(data.mission!==undefined&&!MISSIONS.includes(data.mission)||data.difficulty!==undefined&&!DIFFICULTIES.includes(data.difficulty))fail(400,'Choose a valid mission and difficulty.');
      await db.prepare('DELETE FROM mw2_launcher_rooms WHERE expires<=?').bind(now).run();
      await db.prepare('DELETE FROM mw2_launcher_limits WHERE expires<=?').bind(now).run();
      const active=await db.prepare('SELECT count(*) AS n FROM mw2_launcher_rooms WHERE creator_hash=? AND expires>?').bind(ipHash,now).first();
      if(active.n>=3)fail(429,'Close an old lobby before creating another.');
      for(let attempt=0;attempt<8;attempt++) {
        const candidate=code();
        const room={code:candidate,mission:data.mission||'overkill-concept',difficulty:data.difficulty||'veteran',players:[{name:person.name,hash,seen:now,rtt:null},null,null,null]};
        const inserted=await db.prepare('INSERT OR IGNORE INTO mw2_launcher_rooms (code,state,expires,creator_hash) VALUES (?,?,?,?)').bind(candidate,JSON.stringify(room),now+ROOM_TTL,ipHash).run();
        if(inserted.meta.changes)return json(view(room,hash,now),201);
      }
      fail(503,'Could not allocate a lobby code. Please retry.');
    }
    const person=action==='join'?identity(data):null;
    const token=person?.token||bearer; if(!token)fail(403,'A member token is required.');
    const hash=await sha(token);
    for(let attempt=0;attempt<12;attempt++) {
      const record=await db.prepare('SELECT state,revision,expires FROM mw2_launcher_rooms WHERE code=? AND expires>?').bind(roomCode,now).first();
      if(!record)fail(404,'Lobby expired or closed. Ask the host for a new code.');
      const room=JSON.parse(record.state); prune(room,now);
      let slot=room.players.findIndex(p=>p?.hash===hash);
      if(action==='join') {
        if(slot<0) { slot=room.players.findIndex((p,i)=>i>0&&!p); if(slot<0)fail(409,'Lobby is full (four players).'); room.players[slot]={name:person.name,hash,seen:now,rtt:null}; }
      } else if(slot<0)fail(403,'This launcher is not a member of the lobby.');
      if(method==='GET')return json(view(room,hash,now));
      if(method==='PATCH') {
        if(slot!==0)fail(403,'Only the host can change mission and difficulty.');
        if(!MISSIONS.includes(data.mission)||!DIFFICULTIES.includes(data.difficulty))fail(400,'Choose a valid mission and difficulty.');
        room.mission=data.mission; room.difficulty=data.difficulty;
      }
      if(method==='DELETE'&&slot===0) {
        const deleted=await db.prepare('DELETE FROM mw2_launcher_rooms WHERE code=? AND revision=?').bind(roomCode,record.revision).run();
        if(deleted.meta.changes)return json({closed:true}); else continue;
      }
      if(method==='DELETE')room.players[slot]=null;
      else { room.players[slot].seen=now; if(action==='heartbeat'&&Number.isInteger(data.service_rtt_ms)&&data.service_rtt_ms>=0&&data.service_rtt_ms<=30000)room.players[slot].rtt=data.service_rtt_ms; }
      const expires=slot===0?now+ROOM_TTL:record.expires;
      const updated=await db.prepare('UPDATE mw2_launcher_rooms SET state=?,revision=revision+1,expires=? WHERE code=? AND revision=?').bind(JSON.stringify(room),expires,roomCode,record.revision).run();
      if(updated.meta.changes)return method==='DELETE'?json({left:true}):json(view(room,hash,now));
    }
    fail(409,'Lobby changed while syncing. Please retry.');
  } catch(error) {
    if(error instanceof Failure)return json({error:error.message},error.status);
    // Never log names, join codes, member tokens or room state.
    console.error('MW2 lobby operation failed:',error?.name||'Error');
    return json({error:'Lobby service could not complete the request. Retry shortly.'},503);
  }
}

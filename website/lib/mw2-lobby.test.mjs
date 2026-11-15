import test from 'node:test';
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {handleMw2Lobby,ROOM_TTL,MEMBER_TTL} from './mw2-lobby.mjs';
import {MISSIONS,MW2_PROTOCOL} from './mw2-lobby.mjs';
import {readFileSync} from 'node:fs';

// Real SQLite statements, adapted only to the D1 method shape used by the Worker.
function environment() {
  const sql=new DatabaseSync(':memory:');
  const db={prepare(query){const statement=sql.prepare(query); const wrap=args=>({bind:(...values)=>wrap(values),first:async()=>statement.get(...args)||null,run:async()=>({meta:{changes:Number(statement.run(...args).changes)}})}); return wrap([]);}};
  return {env:{DB:db,LOBBY_RATE_SALT:'test-only',DEV_ALLOW_LOOPBACK:'1'},sql};
}
function client(env,clock,ip='203.0.113.2') {
  return async(method,path,data,token)=>{
    const headers={'cf-connecting-ip':ip}; if(data)headers['content-type']='application/json';if(token)headers.authorization='Bearer '+token;
    const result=await handleMw2Lobby(new Request('https://test.invalid/mw2/api'+path,{method,headers,body:data?JSON.stringify(data):undefined}),env,clock);
    return {status:result.status,data:await result.json()};
  };
}
const identity=(n)=>({name:'Player '+n,member_token:n.toString(16).padStart(64,'0'),protocol:1});

test('native catalog and protocol match the service',()=>{
  const catalog=JSON.parse(readFileSync(new URL('../../games/mw2/missions.json',import.meta.url),'utf8'));
  const version=JSON.parse(readFileSync(new URL('../../games/mw2/version.json',import.meta.url),'utf8'));
  assert.deepEqual(new Set(catalog.map(m=>m.id)),new Set(MISSIONS));
  assert.equal(version.lobby_protocol,MW2_PROTOCOL);
});

test('real SQL: host, four names, shared mission, host authority, leave and close',async()=>{
  const {env,sql}=environment(); let now=1000;const call=client(env,()=>now);
  const host=identity(1),guest=identity(2);
  const created=await call('POST','/lobbies',host);assert.equal(created.status,201);assert.match(created.data.code,/^[A-HJ-NP-Z]{6}$/);
  const path='/lobbies/'+created.data.code;
  assert.equal((await call('POST',path+'/join',guest)).data.players[1].name,'Player 2');
  await call('POST',path+'/join',identity(3));await call('POST',path+'/join',identity(4));
  assert.equal((await call('POST',path+'/join',identity(5))).status,409);
  assert.equal((await call('GET',path,undefined)).status,403);
  assert.equal((await call('PATCH',path,{mission:'the-pit',difficulty:'regular'},guest.member_token)).status,403);
  const updated=await call('PATCH',path,{mission:'the-pit',difficulty:'hardened'},host.member_token);assert.equal(updated.status,200);
  const shared=await call('POST',path+'/heartbeat',{service_rtt_ms:85},guest.member_token);
  assert.equal(shared.data.mission,'the-pit');assert.equal(shared.data.difficulty,'hardened');assert.equal(shared.data.players[1].service_rtt_ms,85);assert.equal(shared.data.gameplay_available,false);
  assert.equal((await call('POST',path+'/join',guest)).data.players.filter(Boolean).length,4,'join retry does not allocate another slot');
  assert.equal((await call('DELETE',path,undefined,guest.member_token)).status,200);
  assert.equal((await call('POST',path+'/join',identity(5))).data.you_slot,1);
  assert.equal((await call('DELETE',path,undefined,host.member_token)).data.closed,true);
  assert.equal((await call('GET',path,undefined,identity(3).member_token)).status,404);
  sql.close();
});
test('simultaneous joins cannot overfill four slots or lose accepted players',async()=>{
  const {env,sql}=environment(),call=client(env,()=>1000);
  const room=await call('POST','/lobbies',identity(1)),path='/lobbies/'+room.data.code;
  const results=await Promise.all([2,3,4,5,6,7].map(i=>call('POST',path+'/join',identity(i))));
  assert.equal(results.filter(r=>r.status===200).length,3);
  const snapshot=await call('GET',path,undefined,identity(1).member_token);
  assert.equal(snapshot.data.players.filter(Boolean).length,4);
  sql.close();
});
test('stale guests vacate slots; host heartbeat extends room; host expiry closes room',async()=>{
  const {env,sql}=environment();let now=1000;const call=client(env,()=>now);
  const host=identity(1),room=await call('POST','/lobbies',host),path='/lobbies/'+room.data.code;
  await call('POST',path+'/join',identity(2));now+=MEMBER_TTL+1;
  const refreshed=await call('POST',path+'/heartbeat',{},host.member_token);assert.equal(refreshed.data.players[1],null);
  now+=ROOM_TTL-1;assert.equal((await call('GET',path,undefined,host.member_token)).status,200);
  now+=2;assert.equal((await call('GET',path,undefined,host.member_token)).status,404);sql.close();
});
test('validation, cross-site protection, hashed credentials and expired-row cleanup',async()=>{
  const {env,sql}=environment(),call=client(env,()=>1000);
  assert.equal((await call('POST','/lobbies',{...identity(1),protocol:2})).status,409);
  assert.equal((await call('POST','/lobbies',{...identity(1),name:'<script>'})).status,400);
  const room=await call('POST','/lobbies',identity(1)),path='/lobbies/'+room.data.code;
  assert.equal((await call('PATCH',path,{mission:'invented',difficulty:'veteran'},identity(1).member_token)).status,400);
  const foreign=await handleMw2Lobby(new Request('https://test.invalid/mw2/api/lobbies',{method:'POST',headers:{origin:'https://evil.invalid'}}),env);
  assert.equal(foreign.status,403);
  const stored=sql.prepare('SELECT state FROM mw2_launcher_rooms').get().state;assert.ok(!stored.includes(identity(1).member_token));
  assert.ok(!JSON.stringify(room.data).includes('hash'));
  assert.equal((await call('GET','/health')).data.gameplay_available,false);sql.close();
});
test('missing configuration fails closed; creation rate is bounded',async()=>{
  const unavailable=await handleMw2Lobby(new Request('https://test.invalid/mw2/api/lobbies',{method:'POST'}),{});assert.equal(unavailable.status,503);
  const {env,sql}=environment(),call=client(env,()=>1000);
  for(let i=1;i<=6;i++)await call('POST','/lobbies',{...identity(i),protocol:2});
  assert.equal((await call('POST','/lobbies',identity(7))).status,429);sql.close();
});

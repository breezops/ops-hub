import test from 'node:test';
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {readFileSync} from 'node:fs';
import {handleService,LIFETIME} from './lobby-service.mjs';

function fixture(){
 const sql=new DatabaseSync(':memory:');sql.exec(readFileSync(new URL('../drizzle/0000_curvy_crystal.sql',import.meta.url),'utf8'));
 const db={prepare(query){let args=[];const p={bind(...v){args=v;return p;},async first(){return sql.prepare(query).get(...args)||null;},async run(){const r=sql.prepare(query).run(...args);return {meta:{changes:r.changes}};}};return p;},async batch(items){return Promise.all(items.map(x=>x.run()));}};
 let now=1000000;
 const env={DB:db,LOBBY_RATE_SALT:'local-test-salt'};
 const release={version:'0.2.0',protocol:5,runtime:1};
 const request=(path,method='GET',body,token,ip='203.0.113.11')=>handleService(new Request('https://test.example'+path,{method,headers:{'cf-connecting-ip':ip,...(token?{Authorization:'Bearer '+token}:{})},...(body!==undefined?{body:JSON.stringify(body)}:{})}),env,release,()=>now);
 return {request,advance(ms){now+=ms;},sql};
}
test('host receives a short code; friend resolves exact session without a login',async()=>{
 const f=fixture();const r=await f.request('/api/lobbies','POST',{version:'0.2.0',protocol:5,port:7777});assert.equal(r.status,201);
 const h=await r.json();assert.match(h.code,/^[23456789ABCDEFGHJKLMNPQRSTUVWXYZ]{8}$/);assert.equal(h.key.length,64);
 const j=await (await f.request('/api/lobbies/'+h.code)).json();assert.equal(j.host,'203.0.113.11');assert.equal(j.key,h.key);assert.equal(j.owner_token,undefined);
 const row=f.sql.prepare('SELECT owner_hash FROM lobbies').get();assert.notEqual(row.owner_hash,h.owner_token);
});
test('only the owner can renew or close; codes expire and cannot be reused',async()=>{
 const f=fixture();const h=await (await f.request('/api/lobbies','POST',{version:'0.2.0',protocol:5,port:7777})).json();
 assert.equal((await f.request('/api/lobbies/'+h.code,'DELETE',undefined,'0'.repeat(64))).status,403);
 f.advance(LIFETIME-1000);assert.equal((await f.request('/api/lobbies/'+h.code,'POST',{},h.owner_token)).status,200);
 f.advance(2000);assert.equal((await f.request('/api/lobbies/'+h.code)).status,200);
 assert.equal((await f.request('/api/lobbies/'+h.code,'DELETE',undefined,h.owner_token)).status,200);
 assert.equal((await f.request('/api/lobbies/'+h.code)).status,404);
});
test('expired, invalid, mismatched and rate-limited requests are rejected',async()=>{
 const f=fixture();assert.equal((await f.request('/api/lobbies','POST',{version:'0.1.4',protocol:5,port:7777})).status,409);
 assert.equal((await f.request('/api/lobbies','POST',{version:'0.2.0',protocol:5,port:7777},null,'127.0.0.1')).status,400);
 const h=await (await f.request('/api/lobbies','POST',{version:'0.2.0',protocol:5,port:7777})).json();
 f.advance(LIFETIME+1);assert.equal((await f.request('/api/lobbies/'+h.code)).status,404);
 for(let i=0;i<90;i++)await f.request('/api/lobbies/ABCDEFGH');
 assert.equal((await f.request('/api/lobbies/ABCDEFGH')).status,429);
});
test('oversized input is bounded and publisher has no public write route',async()=>{
 const f=fixture();assert.equal((await f.request('/api/lobbies','POST',{x:'a'.repeat(3000)})).status,400);
 assert.equal((await f.request('/api/release','POST',{})).status,404);
 const release=await (await f.request('/api/release')).json();assert.equal(release.version,'0.2.0');
});

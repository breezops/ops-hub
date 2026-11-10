"""Publish a prepared HITMAN release; no builds or game installers run here."""
import hashlib,json,os,re,time,urllib.request,urllib.parse,zipfile,io

SITE='https://ops-hub.breezops.chatgpt.site'
MAX=32*1024*1024

class SafeRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,req,fp,code,msg,headers,url):
        # GitHub asset redirects carry a signed URL; never forward either token.
        if req.get_method()=='PUT':raise RuntimeError('Publisher redirect refused')
        result=super().redirect_request(req,fp,code,msg,headers,url)
        if urllib.parse.urlparse(url).netloc!=urllib.parse.urlparse(req.full_url).netloc:
            result.remove_header('Authorization')
        return result

def fetch(url,limit=MAX,headers=None,body=None,method=None):
    req=urllib.request.Request(url,data=body,method=method,headers={'User-Agent':'OpsHub-Hitman-Launcher/1',**(headers or {})})
    with urllib.request.build_opener(SafeRedirect).open(req,timeout=90) as response:
        data=response.read(limit+1)
        if len(data)>limit:raise ValueError('Oversized response')
        return data

def sha(data):return hashlib.sha256(data).hexdigest()

def main():
    tag=os.environ['RELEASE_TAG'];repo=os.environ['GITHUB_REPOSITORY']
    match=re.fullmatch(r'hitman-v((?:0|[1-9]\d{0,4})\.(?:0|[1-9]\d{0,4})\.(?:0|[1-9]\d{0,4}))',tag)
    if not match:raise ValueError('Expected independent hitman-vX.Y.Z tag')
    version=match[1]
    gh={'Authorization':'Bearer '+os.environ['GITHUB_TOKEN'],'Accept':'application/vnd.github+json'}
    releases=json.loads(fetch(f'https://api.github.com/repos/{repo}/releases',1024*1024,gh))
    release=next(r for r in releases if r['tag_name']==tag)
    if release['draft']:raise ValueError('Drafts are not published automatically')
    assets={a['name']:a for a in release['assets']}
    name=f'HITMAN-Coop-{version}.zip'
    def asset(name,limit):return fetch(assets[name]['url'],limit,{**gh,'Accept':'application/octet-stream'})
    manifest_bytes=asset('hitman-release.json',8192);m=json.loads(manifest_bytes)
    assert m['schema']==1 and m['project']=='hitman' and m['version']==version
    assert m['runtime']==1 and m['protocol']==2 and m['game_version']=='3.290.1'
    assert m['path']==f'/downloads/hitman/{name}' and re.fullmatch('[a-f0-9]{64}',m['sha256'])
    assert 0<m['bytes']<=MAX and len(m['mod_build'])<=100
    archive=asset(name,MAX);checksum=asset(name+'.sha256',1024);notes=asset('CHANGELOG-OPS.md',65536)
    assert len(archive)==m['bytes'] and sha(archive)==m['sha256']
    assert checksum.decode().strip()==f"{m['sha256']}  {name}"
    with zipfile.ZipFile(io.BytesIO(archive)) as z:
        assert z.testzip() is None
        seen=set();expanded=0
        for item in z.infolist():
            n=item.filename
            assert n.startswith('HITMAN-Coop/') and '\\' not in n and ':' not in n
            assert not any(p in ('.','..','') for p in n.rstrip('/').split('/'))
            assert n.lower() not in seen and (item.external_attr>>16)&0xf000!=0xa000
            seen.add(n.lower());expanded+=item.file_size
            assert expanded<=250*1024*1024 and len(seen)<=3000
        cfg=json.loads(z.read('HITMAN-Coop/updater-config.json'))
        assert cfg['project']=='hitman' and cfg['release_version']==version
        assert cfg['game_version']==m['game_version'] and cfg['mod_build']==m['mod_build']
        assert cfg['site_url'].rstrip('/')==SITE and cfg['manifest_path']=='/hitman/release.json'
    auth={'Authorization':'Bearer '+os.environ['HITMAN_PUBLISH_TOKEN']}
    def put(kind,data,extra=None):
        return fetch(SITE+'/api/publish/hitman/'+kind+'?version='+version,8192,{**auth,**(extra or {})},data,'PUT')
    # Nothing changes the live channel until the exact ZIP is public and verified.
    put('package',archive,{'X-Content-Sha256':m['sha256'],'Content-Type':'application/zip'})
    put('checksum',checksum);put('notes',notes)
    public=fetch(SITE+m['path']);assert len(public)==m['bytes'] and sha(public)==m['sha256']
    put('manifest',manifest_bytes,{'Content-Type':'application/json'})
    live=json.loads(fetch(SITE+'/hitman/release.json',8192));assert live==m
    print(f"Published and verified HITMAN {version}: {m['bytes']} bytes, SHA-256 {m['sha256']}")
    print(SITE+m['path']);print(SITE+'/hitman/release.json')

if __name__=='__main__':main()

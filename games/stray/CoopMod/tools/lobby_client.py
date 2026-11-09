"""HTTPS join-code directory client; gameplay stays on the existing UDP bridge."""
import json
import re
import socket
from http.client import HTTPSConnection
from urllib.request import Request, urlopen, build_opener, HTTPSHandler, ProxyHandler
from urllib.error import HTTPError
from urllib.parse import urlparse

ALPHABET='23456789ABCDEFGHJKLMNPQRSTUVWXYZ'

def ipv4_connection(address,timeout=socket._GLOBAL_DEFAULT_TIMEOUT,source_address=None):
    error=None
    for family,kind,protocol,_,target in socket.getaddrinfo(address[0],address[1],socket.AF_INET,socket.SOCK_STREAM):
        sock=socket.socket(family,kind,protocol)
        try:
            if timeout is not socket._GLOBAL_DEFAULT_TIMEOUT:sock.settimeout(timeout)
            if source_address:sock.bind(source_address)
            sock.connect(target);return sock
        except OSError as e:error=e;sock.close()
    raise error or OSError('No IPv4 route to the lobby service.')

class IPv4HTTPSConnection(HTTPSConnection):
    def connect(self):
        self._create_connection=ipv4_connection
        super().connect()

class IPv4HTTPSHandler(HTTPSHandler):
    def https_open(self,request):return self.do_open(IPv4HTTPSConnection,request,context=self._context)

def normalize_code(code):
    code=re.sub(r'[\s-]','',code).upper()
    if len(code)!=8 or any(c not in ALPHABET for c in code): raise ValueError('Enter the eight-character join code.')
    return code

class LobbyClient:
    def __init__(self,base,version,protocol):
        p=urlparse(base)
        if p.scheme!='https' and not (p.scheme=='http' and p.hostname in ('127.0.0.1','localhost')):
            raise ValueError('The lobby service must use HTTPS.')
        if p.username or p.password or p.query or p.fragment or p.path not in ('','/'):
            raise ValueError('Invalid lobby service address.')
        self.base=base.rstrip('/');self.version=version;self.protocol=protocol
        # The directory must observe the same public IPv4 used by the UDP game,
        # even on systems that normally prefer IPv6 for websites.
        self.opener=build_opener(ProxyHandler({}),IPv4HTTPSHandler())

    def request(self,path,method='GET',data=None,token=None):
        body=json.dumps(data).encode() if data is not None else None
        headers={'Content-Type':'application/json','Accept':'application/json','User-Agent':'StrayCoopLauncher/'+self.version}
        if token: headers['Authorization']='Bearer '+token
        req=Request(self.base+path,data=body,headers=headers,method=method)
        try:
            with self.opener.open(req,timeout=20) as r:
                raw=r.read(16385)
                if len(raw)>16384: raise ValueError('Lobby service response is too large.')
        except HTTPError as e:
            try: message=json.loads(e.read(4096)).get('error','Lobby request failed.')
            except Exception: message='Lobby request failed. Check your connection and try again.'
            raise ValueError(message) from None
        result=json.loads(raw)
        if not isinstance(result,dict): raise ValueError('Invalid lobby response.')
        return result

    def latest(self): return self.request('/api/release')
    def host(self): return self.request('/api/lobbies','POST',{'version':self.version,'protocol':self.protocol,'port':7777})
    def join(self,code):
        r=self.request('/api/lobbies/'+normalize_code(code))
        if r.get('version')!=self.version or r.get('protocol')!=self.protocol:
            raise ValueError('Both players need the same version. Check for updates first.')
        return r
    def renew(self,code,token): return self.request('/api/lobbies/'+normalize_code(code),'POST',{},token)
    def close(self,code,token): return self.request('/api/lobbies/'+normalize_code(code),'DELETE',None,token)

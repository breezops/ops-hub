"""Package the HTML frontend and its pinned Microsoft WebView2 SDK bridge."""
from pathlib import Path
import hashlib,shutil,subprocess
ROOT=Path(__file__).resolve().parents[2];PROJECT=ROOT/'CoopMod'
SDK=PROJECT/'vendor/WebView2-1.0.4258.31'
def package_frontend(tools):
    tools=Path(tools)
    for name in ('Microsoft.Web.WebView2.Core.dll','Microsoft.Web.WebView2.WinForms.dll','WebView2Loader.dll'):
        shutil.copy2(SDK/name,tools/name)
    shutil.copy2(SDK/'LICENSE.txt',tools/'WEBVIEW2-LICENSE.txt')
    shutil.copy2(SDK/'SOURCE.json',tools/'WEBVIEW2-SOURCE.json')
    shutil.copytree(PROJECT/'assets/launcher-web',tools/'ui')
    page=tools/'ui/index.html';html=page.read_text(encoding='utf-8')
    for name in ('app.css','app.js'):
        tag=hashlib.sha256((tools/'ui'/name).read_bytes()).hexdigest()[:16]
        html=html.replace(name+'"',name+'?asset='+tag+'"')
    page.write_text(html,encoding='utf-8',newline='\n')
    subprocess.run(['C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe','/nologo','/target:winexe','/platform:x64','/optimize+',
        '/reference:System.Web.Extensions.dll','/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll',
        '/reference:'+str(SDK/'Microsoft.Web.WebView2.Core.dll'),'/reference:'+str(SDK/'Microsoft.Web.WebView2.WinForms.dll'),
        '/win32icon:'+str(PROJECT/'assets/launcher/launcher.ico'),'/out:'+str(tools/'StrayCoopUI.exe'),str(PROJECT/'tools/LauncherWebView.cs')],check=True)

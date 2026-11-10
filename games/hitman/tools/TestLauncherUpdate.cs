using System;
using System.IO;
using System.IO.Compression;
using System.Web.Script.Serialization;

public static class TestLauncherUpdate {
    static int passed;
    static readonly JavaScriptSerializer json=new JavaScriptSerializer();
    static void Check(bool result,string label) { if(!result) throw new Exception(label); ++passed;Console.WriteLine("PASS "+label); }
    static void Reject(Action action,string label) { bool rejected=false;try{action();}catch{rejected=true;}Check(rejected,label); }
    public static int Main(string[] args) {
        string root=Path.GetFullPath(args[0]);Directory.CreateDirectory(root);
        string config=Path.Combine(root,"config.json");
        var settings=new LauncherUpdate.Configuration{schema=1,project="hitman",site_url="https://ops-hub.breezops.chatgpt.site",manifest_path="/hitman/release.json",release_version="0.1.0",mod_build="preview.2",game_version="3.290.1"};
        File.WriteAllText(config,json.Serialize(settings));
        var updater=new LauncherUpdate(config,Path.Combine(root,"home"));
        var release=new UpdateRelease{schema=1,project="hitman",runtime=1,protocol=2,version="0.1.1",mod_build="preview.3",game_version="3.290.1",path="/downloads/hitman/update.zip",sha256=new string('a',64),bytes=100};
        Check(updater.ValidateManifest(json.Serialize(release)).version=="0.1.1","accept independent HITMAN manifest");
        release.project="stray";Reject(()=>updater.ValidateManifest(json.Serialize(release)),"reject another game's release");release.project="hitman";
        release.path="//evil.example/update.zip";Reject(()=>updater.ValidateManifest(json.Serialize(release)),"reject cross-origin URL");
        release.path="/downloads/hitman/../stray.zip";Reject(()=>updater.ValidateManifest(json.Serialize(release)),"reject path traversal URL");release.path="/downloads/hitman/update.zip";
        release.runtime=2;Reject(()=>updater.ValidateManifest(json.Serialize(release)),"reject unsupported runtime");release.runtime=1;
        release.protocol=19;Reject(()=>updater.ValidateManifest(json.Serialize(release)),"reject legacy/other protocol");release.protocol=2;
        release.game_version="3.280.0";Reject(()=>updater.ValidateManifest(json.Serialize(release)),"reject incompatible game");release.game_version="3.290.1";
        release.version="../unsafe";Reject(()=>updater.ValidateManifest(json.Serialize(release)),"reject unsafe release version");release.version="0.1.1";
        string zip=Path.Combine(root,"update.zip");
        Action<string,bool> archive=(extra,duplicate)=> {
            if(File.Exists(zip))File.Delete(zip);
            using(ZipArchive output=ZipFile.Open(zip,ZipArchiveMode.Create)) {
                foreach(string file in new[]{"Install-Friend.ps1","installer.ps1","payload-hashes.json","HITMAN-Coop-Party-Preview.exe"})
                    using(var writer=new StreamWriter(output.CreateEntry("HITMAN-Coop/"+file).Open()))writer.Write("fixture only");
                var next=new LauncherUpdate.Configuration{schema=1,project="hitman",site_url=settings.site_url,manifest_path=settings.manifest_path,release_version=release.version,mod_build=release.mod_build,game_version=release.game_version};
                using(var writer=new StreamWriter(output.CreateEntry("HITMAN-Coop/updater-config.json").Open()))writer.Write(json.Serialize(next));
                if(extra!=null)using(var writer=new StreamWriter(output.CreateEntry(extra).Open()))writer.Write("bad");
                if(duplicate)using(var writer=new StreamWriter(output.CreateEntry("HITMAN-Coop/INSTALLER.ps1").Open()))writer.Write("duplicate");
            }
            release.bytes=new FileInfo(zip).Length;release.sha256=LauncherUpdate.HashFile(zip);
        };
        archive(null,false);
        string stage=updater.Stage(zip,release);
        Check(File.Exists(Path.Combine(stage,"verified-archive.sha256")),"verify and stage a complete release");
        Check(!File.Exists(Path.Combine(root,"home/current.json")),"staging never activates an uninstalled release");
        updater.Activate(stage);
        string active=File.ReadAllText(Path.Combine(root,"home/current.json"));
        Check(active.Contains(release.version),"activate a release after installer success");
        archive("HITMAN-Coop/../escape.txt",false);Reject(()=>updater.Stage(zip,release),"reject ZIP traversal");
        archive("HITMAN-Coop/CON.txt",false);Reject(()=>updater.Stage(zip,release),"reject reserved Windows name");
        archive("HITMAN-Coop/nested/C:/escape",false);Reject(()=>updater.Stage(zip,release),"reject drive/alternate-stream path");
        archive(null,true);Reject(()=>updater.Stage(zip,release),"reject case-insensitive duplicate paths");
        archive(null,false);release.sha256=new string('0',64);Reject(()=>updater.Stage(zip,release),"reject wrong ZIP hash");
        release.sha256=LauncherUpdate.HashFile(zip);++release.bytes;Reject(()=>updater.Stage(zip,release),"reject wrong ZIP size");
        Check(File.ReadAllText(Path.Combine(root,"home/current.json"))==active,"failed updates leave active release unchanged");

        string setup=Path.Combine(root,"setup-fixture");Directory.CreateDirectory(setup);
        string script=Path.Combine(setup,"Install-Friend.ps1");
        File.WriteAllText(script,"param([string]$Mode)\r\nWrite-Host 'SETUP STOPPED: Fixture error remains visible.'\r\n[Console]::Error.WriteLine('fixture stderr retained')\r\nexit 9\r\n");
        string failure=null;
        try { updater.RunInstaller(setup,"Auto").GetAwaiter().GetResult(); }
        catch(Exception error) { failure=error.Message; }
        Check(failure!=null && failure.Contains("Fixture error remains visible."),"preserve actual installer failure in launcher message");
        Check(File.Exists(updater.LastSetupLog) && failure.Contains(updater.LastSetupLog),"save failure log and expose its location");
        Check(File.ReadAllText(updater.LastSetupLog).Contains("fixture stderr retained"),"retain installer stderr alongside stdout");
        File.WriteAllText(script,"param([string]$Mode)\r\n$ErrorActionPreference='Stop'\r\nGet-FileHash -LiteralPath $PSCommandPath | Out-Null\r\nWrite-Host ('SETUP OK '+$Mode)\r\nexit 0\r\n");
        string previousModules=Environment.GetEnvironmentVariable("PSModulePath");
        Environment.SetEnvironmentVariable("PSModulePath",Path.Combine(root,"missing-modules"));
        try {
            Check(updater.RunInstaller(setup,"Auto").GetAwaiter().GetResult()==0,"repair succeeds with inherited module path sanitized");
            Check(File.ReadAllText(updater.LastSetupLog).Contains("SETUP OK Auto"),"keep a successful setup log too");
            Check(updater.RunInstaller(setup,"Remove").GetAwaiter().GetResult()==0,"removal uses same captured setup path");
        } finally { Environment.SetEnvironmentVariable("PSModulePath",previousModules); }
        settings.release_version="0.1.2";
        string newerConfig=Path.Combine(root,"newer-config.json");File.WriteAllText(newerConfig,json.Serialize(settings));
        var newer=new LauncherUpdate(newerConfig,Path.Combine(root,"home"));
        Check(!newer.RedirectToCurrent(),"newer downloaded launcher does not reopen an older cached release");
        Console.WriteLine(passed+" updater checks passed.");return 0;
    }
}

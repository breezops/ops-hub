using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Net;
using System.Security.Cryptography;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

public sealed class UpdateRelease {
    public int schema, runtime, protocol;
    public string project, version, mod_build, game_version, path, sha256;
    public long bytes;
}
public sealed class LauncherUpdate {
    public sealed class Configuration {
        public int schema;
        public string project, site_url, manifest_path, release_version, mod_build, game_version;
    }
    const long MaxDownload=100*1024*1024, MaxExpanded=250*1024*1024;
    readonly Configuration config;
    readonly string home;
    readonly JavaScriptSerializer json=new JavaScriptSerializer();
    public string ModBuild { get { return config.mod_build; } }
    public string ReleaseVersion { get { return config.release_version; } }
    public string LastSetupLog { get; private set; }
    public LauncherUpdate() : this(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"updater-config.json"),
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"OpsHub","HitmanCoop")) {}
    public LauncherUpdate(string configFile,string storage) {
        if(!File.Exists(configFile)) throw new IOException("Extract the entire HITMAN launcher ZIP first. Its updater-config.json file is missing.");
        config=json.Deserialize<Configuration>(File.ReadAllText(configFile));
        home=Path.GetFullPath(storage);
        if(config==null || config.schema!=1 || config.project!="hitman" || !ValidVersion(config.release_version) || String.IsNullOrWhiteSpace(config.mod_build) || String.IsNullOrWhiteSpace(config.game_version))
            throw new InvalidDataException("The HITMAN launcher configuration is invalid. Extract the complete package.");
        Address(config.manifest_path);
    }
    static bool ValidVersion(string value) { return value!=null && Regex.IsMatch(value,@"^\d{1,5}\.\d{1,5}\.\d{1,5}$"); }
    static string Hash(Stream stream) {
        using(SHA256 hash=SHA256.Create()) return BitConverter.ToString(hash.ComputeHash(stream)).Replace("-","").ToLowerInvariant();
    }
    public static string HashFile(string file) { using(Stream input=File.OpenRead(file)) return Hash(input); }
    Uri Address(string path) {
        Uri origin;
        if(!Uri.TryCreate(config.site_url,UriKind.Absolute,out origin) || origin.Scheme!="https" ||
            origin.UserInfo.Length>0 || origin.AbsolutePath!="/" || origin.Query.Length>0 || origin.Fragment.Length>0)
            throw new InvalidDataException("The update site must be an HTTPS origin.");
        if(path==null || !path.StartsWith("/") || path.StartsWith("//") || path.Contains("\\") || path.Contains("..") || path.Contains("%"))
            throw new InvalidDataException("Invalid HITMAN update address.");
        Uri result=new Uri(origin,path);
        if(result.GetLeftPart(UriPartial.Authority)!=origin.GetLeftPart(UriPartial.Authority) || result.Query.Length>0 || result.Fragment.Length>0)
            throw new InvalidDataException("The update address changed origin.");
        return result;
    }
    // Follow a bounded number of redirects only within the configured HTTPS
    // origin. Never send GitHub tokens or website sign-in cookies to players.
    async Task Fetch(Uri address,Stream destination,long limit) {
        ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;
        Uri current=address;
        for(int redirects=0;redirects<4;++redirects) {
            HttpWebRequest request=(HttpWebRequest)WebRequest.Create(current);
            request.UserAgent="OpsHub-Hitman-Launcher/1";request.AllowAutoRedirect=false;
            request.Timeout=20000;request.ReadWriteTimeout=20000;
            using(System.Threading.CancellationTokenSource timeout=new System.Threading.CancellationTokenSource(30000))
            using(timeout.Token.Register(()=>request.Abort()))
            using(HttpWebResponse response=(HttpWebResponse)await request.GetResponseAsync()) {
                int status=(int)response.StatusCode;
                if(status>=300 && status<400) {
                    Uri next=new Uri(current,response.Headers["Location"]);
                    if(next.Scheme!="https" || next.UserInfo.Length>0 || next.GetLeftPart(UriPartial.Authority)!=address.GetLeftPart(UriPartial.Authority))
                        throw new InvalidDataException("The update site redirected outside its trusted origin.");
                    current=next;continue;
                }
                if(status!=200 || response.ContentLength>limit) throw new InvalidDataException("Invalid or oversized update response.");
                using(Stream input=response.GetResponseStream()) {
                    byte[] buffer=new byte[65536];long total=0;int count;
                    while((count=await input.ReadAsync(buffer,0,buffer.Length))>0) {
                        total+=count;if(total>limit) throw new InvalidDataException("Update download exceeds its size limit.");
                        await destination.WriteAsync(buffer,0,count);
                    }
                }
                return;
            }
        }
        throw new InvalidDataException("The update site redirected too many times.");
    }
    public UpdateRelease ValidateManifest(string text) {
        UpdateRelease release=json.Deserialize<UpdateRelease>(text);
        if(release==null || release.schema!=1 || release.project!="hitman" || release.runtime!=1 || release.protocol!=2 ||
            !ValidVersion(release.version) || release.game_version!=config.game_version ||
            String.IsNullOrEmpty(release.mod_build) || release.mod_build.Length>100 ||
            release.sha256==null || !Regex.IsMatch(release.sha256,"^[0-9a-f]{64}$") ||
            release.bytes<1 || release.bytes>MaxDownload || release.path==null || !release.path.StartsWith("/downloads/hitman/"))
            throw new InvalidDataException("This is not a compatible HITMAN release manifest.");
        Address(release.path);
        return release;
    }
    public async Task<UpdateRelease> Check() {
        using(MemoryStream data=new MemoryStream()) {
            try { await Fetch(Address(config.manifest_path),data,65536); }
            catch(WebException error) {
                var response=error.Response as HttpWebResponse;
                if(response!=null && response.StatusCode==HttpStatusCode.NotFound)
                    throw new IOException("The HITMAN download channel has not been published on OPS HUB yet.");
                throw new IOException("Could not reach the HITMAN update channel. Try again later.",error);
            }
            UpdateRelease release=ValidateManifest(System.Text.Encoding.UTF8.GetString(data.ToArray()));
            return new Version(release.version)>new Version(ReleaseVersion) ? release : null;
        }
    }
    public async Task<string> Download(UpdateRelease release) {
        ValidateManifest(json.Serialize(release));
        Directory.CreateDirectory(home);
        string archive=Path.Combine(home,"download-"+Guid.NewGuid().ToString("N")+".zip");
        try {
            using(Stream output=File.Create(archive)) await Fetch(Address(release.path),output,release.bytes);
            return await Task.Run(()=>Stage(archive,release));
        } finally { if(File.Exists(archive)) File.Delete(archive); }
    }
    public string Stage(string archive,UpdateRelease release) {
        ValidateManifest(json.Serialize(release));
        if(new FileInfo(archive).Length!=release.bytes || HashFile(archive)!=release.sha256)
            throw new InvalidDataException("The downloaded ZIP failed its SHA-256 or size check.");
        string stage=Path.Combine(home,"releases",release.version+"-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(stage);
        try {
            using(ZipArchive zip=ZipFile.OpenRead(archive)) {
                if(zip.Entries.Count>3000) throw new InvalidDataException("Too many files in update.");
                long expanded=0;var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                foreach(ZipArchiveEntry entry in zip.Entries) {
                    string name=entry.FullName;
                    if(!name.StartsWith("HITMAN-Coop/",StringComparison.Ordinal) || name.Contains("\\") || name.Contains(":"))
                        throw new InvalidDataException("Unexpected update folder.");
                    string relative=name.Substring("HITMAN-Coop/".Length);
                    if(relative.Length==0 && name.EndsWith("/")) continue;
                    foreach(string part in relative.TrimEnd('/').Split('/'))
                        if(part.Length==0 || part=="." || part==".." || part.EndsWith(".") || part.EndsWith(" ") ||
                            part.IndexOfAny(Path.GetInvalidFileNameChars())>=0 || Regex.IsMatch(part,@"^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)",RegexOptions.IgnoreCase))
                            throw new InvalidDataException("Unsafe path in update.");
                    if(!seen.Add(relative.TrimEnd('/')) || ((entry.ExternalAttributes>>16)&0xF000)==0xA000)
                        throw new InvalidDataException("Duplicate path or link in update.");
                    if(entry.Length>MaxExpanded-expanded) throw new InvalidDataException("Expanded update is too large.");
                    expanded+=entry.Length;
                    string target=Path.GetFullPath(Path.Combine(stage,relative.Replace('/',Path.DirectorySeparatorChar)));
                    if(!target.StartsWith(stage+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Unsafe extraction path.");
                    if(name.EndsWith("/")) Directory.CreateDirectory(target);
                    else { Directory.CreateDirectory(Path.GetDirectoryName(target));entry.ExtractToFile(target); }
                }
            }
            foreach(string file in new[]{"Install-Friend.ps1","installer.ps1","payload-hashes.json","HITMAN-Coop-Party-Preview.exe","updater-config.json"})
                if(!File.Exists(Path.Combine(stage,file))) throw new InvalidDataException("Update is missing "+file);
            var next=json.Deserialize<Configuration>(File.ReadAllText(Path.Combine(stage,"updater-config.json")));
            if(next==null || next.schema!=1 || next.project!="hitman" || next.release_version!=release.version || next.mod_build!=release.mod_build ||
                next.game_version!=release.game_version || next.site_url!=config.site_url || next.manifest_path!=config.manifest_path)
                throw new InvalidDataException("Release configuration disagrees with the update channel.");
            File.WriteAllText(Path.Combine(stage,"verified-archive.sha256"),release.sha256);
            return stage;
        } catch {
            string allowed=Path.GetFullPath(Path.Combine(home,"releases"))+Path.DirectorySeparatorChar;
            if(Path.GetFullPath(stage).StartsWith(allowed,StringComparison.OrdinalIgnoreCase)) Directory.Delete(stage,true);
            throw;
        }
    }
    public async Task<int> RunInstaller(string folder,string mode) {
        if(mode!="Auto" && mode!="Remove") throw new ArgumentException("Invalid setup mode.");
        if(Process.GetProcessesByName("HITMAN3").Length>0) throw new IOException("Close HITMAN before changing the mod.");
        string installer=Path.Combine(folder,"Install-Friend.ps1");
        if(!File.Exists(installer)) throw new IOException("Extract the complete launcher package before installing.");
        string powershell=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),@"WindowsPowerShell\v1.0\powershell.exe");
        var start=new ProcessStartInfo(powershell,"-NoProfile -STA -ExecutionPolicy Bypass -File \""+installer+"\" -Mode "+mode){
            UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true,
            WorkingDirectory=Path.GetFullPath(folder)
        };
        // Windows PowerShell must use its own modules, even when the launcher
        // inherits a PowerShell 7/custom environment from its parent process.
        start.EnvironmentVariables["PSModulePath"]=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),@"WindowsPowerShell\Modules")+
            ";"+Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),@"WindowsPowerShell\v1.0\Modules");
        string logs=Path.Combine(home,"logs");Directory.CreateDirectory(logs);
        LastSetupLog=Path.Combine(logs,"setup-"+DateTime.UtcNow.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N").Substring(0,8)+".txt");
        using(Process process=Process.Start(start)) {
            Task<string> stdout=process.StandardOutput.ReadToEndAsync(),stderr=process.StandardError.ReadToEndAsync();
            await Task.WhenAll(stdout,stderr,Task.Run(()=>process.WaitForExit()));
            string output=stdout.Result+Environment.NewLine+stderr.Result;
            File.WriteAllText(LastSetupLog,output);
            if(process.ExitCode!=0) {
                string detail="Setup returned exit code "+process.ExitCode+".";
                foreach(string line in output.Split(new[]{'\r','\n'},StringSplitOptions.RemoveEmptyEntries))
                    if(line.Trim().StartsWith("SETUP STOPPED:",StringComparison.OrdinalIgnoreCase)) detail=line.Trim();
                throw new IOException(detail+Environment.NewLine+"Setup log: "+LastSetupLog);
            }
            return process.ExitCode;
        }
    }
    public void Activate(string stage) {
        string full=Path.GetFullPath(stage),root=Path.Combine(home,"releases")+Path.DirectorySeparatorChar;
        if(!full.StartsWith(root,StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Invalid release activation path.");
        string launcher=Path.Combine(full,"HITMAN-Coop-Party-Preview.exe");
        Directory.CreateDirectory(home);
        string next=Path.Combine(home,"current.new.json"),pointer=Path.Combine(home,"current.json");
        File.WriteAllText(next,json.Serialize(new{folder=full,sha256=HashFile(launcher),previous=AppDomain.CurrentDomain.BaseDirectory}));
        if(File.Exists(pointer)) File.Replace(next,pointer,null);else File.Move(next,pointer);
    }
    public bool RedirectToCurrent() {
        string pointer=Path.Combine(home,"current.json");
        if(!File.Exists(pointer)) return false;
        var record=json.Deserialize<Dictionary<string,string>>(File.ReadAllText(pointer));
        string target=Path.GetFullPath(record["folder"]),root=Path.Combine(home,"releases")+Path.DirectorySeparatorChar;
        if(!target.StartsWith(root,StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Invalid saved launcher location.");
        if(String.Equals(target.TrimEnd('\\'),AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\'),StringComparison.OrdinalIgnoreCase)) return false;
        string launcher=Path.Combine(target,"HITMAN-Coop-Party-Preview.exe");
        if(HashFile(launcher)!=record["sha256"]) throw new InvalidDataException("The saved launcher failed its integrity check. Use Install / repair.");
        Process.Start(new ProcessStartInfo(launcher){UseShellExecute=true});return true;
    }
}

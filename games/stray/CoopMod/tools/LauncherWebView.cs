using System;
using System.IO;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Diagnostics;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Web.WebView2.Core;
using Microsoft.Web.WebView2.WinForms;

internal sealed class LauncherWebView : Form {
    const string Origin="https://launcher.straycoop.invalid/";
    [DllImport("shell32.dll",CharSet=CharSet.Unicode)] static extern int SetCurrentProcessExplicitAppUserModelID(string id);
    [DllImport("user32.dll")] static extern bool ReleaseCapture();
    [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr hwnd,int msg,IntPtr wparam,IntPtr lparam);
    [DllImport("user32.dll",EntryPoint="GetWindowLongW")] static extern int GetWindowLong(IntPtr hwnd,int index);
    readonly JavaScriptSerializer json=new JavaScriptSerializer {MaxJsonLength=4*1024*1024};
    readonly string tools=AppDomain.CurrentDomain.BaseDirectory;
    readonly string home;
    readonly string diagnostic;
    readonly bool liveTest;
    readonly WebView2 web=new WebView2();
    readonly List<string> testActions=new List<string>();
    Process backend;
    bool allowClose,frontendReady;
    string lastState;
    static void Trace(string text) {
        string path=Environment.GetEnvironmentVariable("STRAYCOOP_UI_TRACE");
        if(!String.IsNullOrEmpty(path))try {File.AppendAllText(path,DateTime.UtcNow.ToString("O")+" "+text+Environment.NewLine);}catch{}
    }

    [STAThread] static int Main(string[] args) {
        Trace("Main "+String.Join(" | ",args));
        if(args.Length>0 && args[0]=="--check-runtime") {
            try { return String.IsNullOrEmpty(CoreWebView2Environment.GetAvailableBrowserVersionString())?2:0; }
            catch { return 2; }
        }
        SetCurrentProcessExplicitAppUserModelID("StrayCoop.Launcher");
        Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        try { Application.Run(new LauncherWebView(args));return 0; }
        catch(Exception error) { MessageBox.Show(error.Message,"Stray Co-op",MessageBoxButtons.OK,MessageBoxIcon.Error);return 1; }
    }
    LauncherWebView(string[] args) {
        home=Environment.GetEnvironmentVariable("STRAYCOOP_LAUNCHER_HOME") ?? Path.GetFullPath(Path.Combine(tools,"..","..",".."));
        liveTest=args.Length==2 && args[0]=="--smoke-test";
        diagnostic=args.Length==2 && (args[0]=="--self-test" || liveTest)?Path.GetFullPath(args[1]):null;
        if(diagnostic!=null)Directory.CreateDirectory(diagnostic);
        Trace("constructed live="+liveTest);
        Text="Stray Co-op";FormBorderStyle=FormBorderStyle.None;ClientSize=new Size(720,760);
        MinimumSize=new Size(680,720);StartPosition=FormStartPosition.CenterScreen;ShowInTaskbar=true;
        BackColor=Color.FromArgb(8,10,15);Icon=new Icon(Path.Combine(tools,"assets","launcher.ico"));
        web.Dock=DockStyle.Fill;web.DefaultBackgroundColor=BackColor;Controls.Add(web);
        Shown+=async delegate {await Initialize();};
        Resize+=delegate {RoundCorners();};
        FormClosing+=delegate(object sender,FormClosingEventArgs e) {
            if(allowClose)return;e.Cancel=true;
            if(backend==null || backend.HasExited) {allowClose=true;BeginInvoke(new Action(Close));}
            else SendBackend(new Dictionary<string,object>{{"action","close"}});
        };
        FormClosed+=delegate { if(backend!=null)try {backend.StandardInput.Close();}catch{}; };
        RoundCorners();
    }
    void RoundCorners() {
        if(ClientSize.Width<32 || ClientSize.Height<32)return;
        using(var path=new GraphicsPath()) {
            int d=30,w=ClientSize.Width,h=ClientSize.Height;
            path.AddArc(0,0,d,d,180,90);path.AddArc(w-d,0,d,d,270,90);
            path.AddArc(w-d,h-d,d,d,0,90);path.AddArc(0,h-d,d,d,90,90);path.CloseFigure();
            var previous=Region;Region=new Region(path);if(previous!=null)previous.Dispose();
        }
    }
    async Task Initialize() {
        try {
            Trace("initializing");
            string profile=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"StrayCoopLauncher","WebView2");
            var environment=await CoreWebView2Environment.CreateAsync(null,profile,null);
            await web.EnsureCoreWebView2Async(environment);
            Trace("webview ready");
            web.CoreWebView2.Settings.AreDefaultContextMenusEnabled=false;
            web.CoreWebView2.Settings.AreDevToolsEnabled=false;
            web.CoreWebView2.SetVirtualHostNameToFolderMapping("launcher.straycoop.invalid",Path.Combine(tools,"ui"),CoreWebView2HostResourceAccessKind.DenyCors);
            web.CoreWebView2.NavigationStarting+=delegate(object sender,CoreWebView2NavigationStartingEventArgs e) {if(!e.Uri.StartsWith(Origin,StringComparison.Ordinal))e.Cancel=true;};
            web.CoreWebView2.NewWindowRequested+=delegate(object sender,CoreWebView2NewWindowRequestedEventArgs e) {e.Handled=true;};
            web.CoreWebView2.WebMessageReceived+=Message;
            if(diagnostic==null || liveTest)StartBackend();
            if(diagnostic!=null)web.CoreWebView2.NavigationCompleted+=async delegate(object sender,CoreWebView2NavigationCompletedEventArgs e) {if(e.IsSuccess) {if(liveTest)await SmokeTest();else await SelfTest();}};
            web.CoreWebView2.Navigate(Origin+"index.html?build="+Uri.EscapeDataString(new DirectoryInfo(tools).Parent.Name));
        } catch(Exception error) {
            if(diagnostic!=null) {Directory.CreateDirectory(diagnostic);File.WriteAllText(Path.Combine(diagnostic,"error.txt"),error.ToString());allowClose=true;Close();}
            else {MessageBox.Show(this,error.Message,"Stray Co-op could not open",MessageBoxButtons.OK,MessageBoxIcon.Error);allowClose=true;Close();}
        }
    }
    static string Quote(string value) {return "\""+value.Replace("\"","\\\"")+"\"";}
    void StartBackend() {
        var start=new ProcessStartInfo(Path.Combine(home,"runtime","python.exe"),"-B -u "+Quote(Path.Combine(tools,"launcher_backend.py")));
        start.WorkingDirectory=tools;start.UseShellExecute=false;start.CreateNoWindow=true;
        start.RedirectStandardInput=true;start.RedirectStandardOutput=true;start.RedirectStandardError=true;
        start.EnvironmentVariables["STRAYCOOP_LAUNCHER_HOME"]=home;
        backend=new Process {StartInfo=start,EnableRaisingEvents=true};
        backend.OutputDataReceived+=delegate(object sender,DataReceivedEventArgs e) {if(e.Data!=null)Dispatch(e.Data);};
        backend.ErrorDataReceived+=delegate(object sender,DataReceivedEventArgs e) {
            if(e.Data!=null)Trace("backend stderr "+e.Data);
            if(!String.IsNullOrWhiteSpace(e.Data))Dispatch(json.Serialize(new {kind="notice",title="Launcher diagnostics",message=e.Data}));
        };
        backend.Exited+=delegate {if(!allowClose)Dispatch(json.Serialize(new {kind="notice",title="Launcher stopped",message="The launcher service stopped. Close and reopen the launcher."}));};
        backend.Start();backend.BeginOutputReadLine();backend.BeginErrorReadLine();Trace("backend started");
    }
    void Dispatch(string data) {
        if(IsDisposed || !IsHandleCreated)return;
        try { BeginInvoke(new Action(delegate {
            try {
                var value=json.DeserializeObject(data) as Dictionary<string,object>;
                if(value==null || !value.ContainsKey("kind"))return;
                string kind=Convert.ToString(value["kind"]);
                if(kind=="exit") {allowClose=true;Close();return;}
                if(kind=="clipboard") {
                    Clipboard.SetText(Convert.ToString(value["value"]));
                    if(frontendReady)web.CoreWebView2.PostWebMessageAsJson("{\"kind\":\"copied\"}");
                    return;
                }
                if(kind=="state") {
                    lastState=data;Trace("backend state");
                    if(diagnostic!=null)File.WriteAllText(Path.Combine(diagnostic,"last-state.json"),data);
                }
                if(kind=="notice" && diagnostic!=null)File.WriteAllText(Path.Combine(diagnostic,"last-notice.json"),data);
                if(frontendReady)web.CoreWebView2.PostWebMessageAsJson(data);
            } catch(Exception error) {Debug.WriteLine(error.Message);}
        }));}catch(InvalidOperationException){}
    }
    void SendBackend(Dictionary<string,object> value) {
        if(backend==null || backend.HasExited)return;
        try {backend.StandardInput.WriteLine(json.Serialize(value));backend.StandardInput.Flush();}
        catch(IOException){}
    }
    void Message(object sender,CoreWebView2WebMessageReceivedEventArgs e) {
        if(!e.Source.StartsWith(Origin,StringComparison.Ordinal) || e.WebMessageAsJson.Length>16384)return;
        var value=json.DeserializeObject(e.WebMessageAsJson) as Dictionary<string,object>;
        if(value==null || !value.ContainsKey("action"))return;
        string action=Convert.ToString(value["action"]);
        if(diagnostic!=null)testActions.Add(action);
        if(action=="ready") {frontendReady=true;if(lastState!=null)web.CoreWebView2.PostWebMessageAsJson(lastState);}
        if(action=="minimize") {WindowState=FormWindowState.Minimized;return;}
        if(action=="drag") {ReleaseCapture();SendMessage(Handle,0xA1,new IntPtr(2),IntPtr.Zero);return;}
        if(diagnostic!=null && !liveTest)return;
        if(action=="browse") {
            using(var picker=new FolderBrowserDialog()) {
                picker.Description="Choose the Stray folder containing Hk_project";
                if(value.ContainsKey("root") && Directory.Exists(Convert.ToString(value["root"])))picker.SelectedPath=Convert.ToString(value["root"]);
                if(picker.ShowDialog(this)==DialogResult.OK)SendBackend(new Dictionary<string,object>{{"action","folder"},{"path",picker.SelectedPath}});
            }
            return;
        }
        if(action=="close" && (backend==null || backend.HasExited)) {allowClose=true;Close();return;}
        if(new HashSet<string>{"host","join","end","updates","copy","close","confirm_close","ready","directory"}.Contains(action))SendBackend(value);
    }
    object Fixture(bool active) {
        return new {kind="state",state=new {directory=@"F:\FitGirl\Stray",status="Ready. In cinematics, hold Space to skip for both players.",
            version="Version 0.2.5",updateState="Up to date",updateText="Check for updates",updateDisabled=false,stopDisabled=false,
            active=active,sessionCode=active?"ABCD-EFGH":"",error=false,logs=new []{"Stray starts automatically after your partner connects."}}};
    }
    async Task<Dictionary<string,object>> WaitState(Func<Dictionary<string,object>,bool> predicate) {
        for(int i=0;i<500;i++) {
            if(lastState!=null) {
                var message=json.DeserializeObject(lastState) as Dictionary<string,object>;
                var state=message["state"] as Dictionary<string,object>;
                if(predicate(state))return state;
                if(Convert.ToBoolean(state["error"]))throw new Exception(Convert.ToString(state["status"]));
            }
            await Task.Delay(100);
        }
        throw new TimeoutException("UI/backend state did not complete.");
    }
    async Task SmokeTest() {
        Directory.CreateDirectory(diagnostic);
        Trace("smoke running");
        bool failed=false;
        try {
            await WaitState(s=>Convert.ToString(s["updateState"])=="Up to date");
            await web.CoreWebView2.ExecuteScriptAsync("document.getElementById('host-btn').click()");
            await WaitState(s=>Convert.ToString(s["sessionCode"]).Length>0);
            await WaitState(s=>Convert.ToString(s["status"]).StartsWith("Waiting for your friend"));
            using(var output=File.Create(Path.Combine(diagnostic,"waiting.png")))await web.CoreWebView2.CapturePreviewAsync(CoreWebView2CapturePreviewImageFormat.Png,output);
            await web.CoreWebView2.ExecuteScriptAsync("document.getElementById('end-session-btn').click()");
            await WaitState(s=>!Convert.ToBoolean(s["active"]) && Convert.ToString(s["status"]).StartsWith("Session ended"));
            File.WriteAllText(Path.Combine(diagnostic,"live-checks.json"),json.Serialize(new {hosted=true,waited=true,ended=true,nativeCaption=(GetWindowLong(Handle,-16)&0xC00000)!=0,actions=testActions}));
            await web.CoreWebView2.ExecuteScriptAsync("document.getElementById('close-btn').click()");
        } catch(Exception error) {
            File.WriteAllText(Path.Combine(diagnostic,"error.txt"),error.ToString());
            SendBackend(new Dictionary<string,object>{{"action","end"}});failed=true;
        }
        if(failed) {
            using(var output=File.Create(Path.Combine(diagnostic,"failure.png")))await web.CoreWebView2.CapturePreviewAsync(CoreWebView2CapturePreviewImageFormat.Png,output);
            await Task.Delay(1000);
            SendBackend(new Dictionary<string,object>{{"action","close"}});allowClose=true;Close();
        }
    }
    async Task SelfTest() {
        try {
            Directory.CreateDirectory(diagnostic);await Task.Delay(900);
            web.CoreWebView2.PostWebMessageAsJson(json.Serialize(Fixture(false)));await Task.Delay(200);
            string result=await web.CoreWebView2.ExecuteScriptAsync("JSON.stringify({fonts:document.fonts.status,ids:['game-folder-input','browse-btn','host-btn','join-code-input','join-btn','status-text','log-output','version-text','check-updates-btn','min-btn','close-btn'].map(id=>{const e=document.getElementById(id),r=e.getBoundingClientRect();return {id,visible:r.width>0&&r.height>0,inside:r.left>=0&&r.top>=0&&r.right<=innerWidth+1&&r.bottom<=innerHeight+1}})})");
            File.WriteAllText(Path.Combine(diagnostic,"dom-layout.json"),Convert.ToString(json.DeserializeObject(result)));
            using(var output=File.Create(Path.Combine(diagnostic,"idle.png")))await web.CoreWebView2.CapturePreviewAsync(CoreWebView2CapturePreviewImageFormat.Png,output);
            await web.CoreWebView2.ExecuteScriptAsync("document.getElementById('host-btn').click();document.getElementById('join-code-input').value='ABCD-EFGH';document.getElementById('join-btn').click();document.getElementById('check-updates-btn').click()");
            web.CoreWebView2.PostWebMessageAsJson(json.Serialize(Fixture(true)));await Task.Delay(200);
            using(var output=File.Create(Path.Combine(diagnostic,"hosting.png")))await web.CoreWebView2.CapturePreviewAsync(CoreWebView2CapturePreviewImageFormat.Png,output);
            await web.CoreWebView2.ExecuteScriptAsync("document.getElementById('copy-code-btn').click();document.getElementById('end-session-btn').click();document.getElementById('min-btn').click()");
            await Task.Delay(150);bool minimized=WindowState==FormWindowState.Minimized;WindowState=FormWindowState.Normal;await Task.Delay(150);
            ClientSize=new Size(680,720);await Task.Delay(150);
            using(var output=File.Create(Path.Combine(diagnostic,"minimum.png")))await web.CoreWebView2.CapturePreviewAsync(CoreWebView2CapturePreviewImageFormat.Png,output);
            await web.CoreWebView2.ExecuteScriptAsync("document.getElementById('close-btn').click()");await Task.Delay(100);
            File.WriteAllText(Path.Combine(diagnostic,"host-checks.json"),json.Serialize(new {frameless=FormBorderStyle==FormBorderStyle.None,nativeCaption=(GetWindowLong(Handle,-16)&0xC00000)!=0,minimized=minimized,taskbar=ShowInTaskbar,actions=testActions}));
        } catch(Exception error) {File.WriteAllText(Path.Combine(diagnostic,"error.txt"),error.ToString());}
        allowClose=true;Close();
    }
}

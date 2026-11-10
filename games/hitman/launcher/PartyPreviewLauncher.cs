using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Markup;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

public sealed class PartyPreviewLauncher : Window {
    static LauncherUpdate Updates;
    static string Build;
    static string startupNotice;
    readonly Grid root;
    readonly TextBlock state, detail, roster, feedback, mission;
    readonly TextBox address;
    readonly DispatcherTimer timer = new DispatcherTimer();
    bool busy;
    int pid;
    bool updating;
    UpdateRelease available;
    [STAThread]
    public static int Main(string[] args) {
        try {
            Updates=new LauncherUpdate(); Build=Updates.ModBuild;
            if (args.Length == 4 && args[0] == "--request") {
                File.WriteAllText(args[3], CoopLauncher.Exchange(Int32.Parse(args[1]), args[2]).GetAwaiter().GetResult());
                return 0;
            }
            if(args.Length==0) {
                try { if(Updates.RedirectToCurrent()) return 0; }
                catch(Exception error) { startupNotice=error.Message; }
            }
            Application app = new Application();
            PartyPreviewLauncher window = new PartyPreviewLauncher();
            if (args.Length == 2 && args[0] == "--preview") {
                window.root.Measure(new Size(1120,820));
                window.root.Arrange(new Rect(0,0,1120,820));
                window.root.UpdateLayout();
                RenderTargetBitmap bitmap = new RenderTargetBitmap(1120,820,96,96,PixelFormats.Pbgra32);
                bitmap.Render(window.root);
                PngBitmapEncoder encoder = new PngBitmapEncoder();
                encoder.Frames.Add(BitmapFrame.Create(bitmap));
                using (FileStream file = File.Create(args[1])) encoder.Save(file);
                return 0;
            }
            app.Run(window);
            return 0;
        } catch (Exception error) {
            if (args.Length == 4) File.WriteAllText(args[3], "{\"ok\":false,\"message\":\"" + error.GetType().Name + "\"}");
            else MessageBox.Show(error.Message,"HITMAN Co-op preview");
            return 1;
        }
    }
    public PartyPreviewLauncher() {
        Title="HITMAN / CO-OP — OPS HUB";
        Width=1160; Height=880; MinWidth=1050; MinHeight=820;
        WindowStartupLocation=WindowStartupLocation.CenterScreen;
        Background=new SolidColorBrush(Color.FromRgb(15,17,20));
        using (Stream input=Assembly.GetExecutingAssembly().GetManifestResourceStream("LauncherLayout.xaml"))
        using (StreamReader reader=new StreamReader(input)) root=(Grid)XamlReader.Parse(reader.ReadToEnd());
        Content=root;
        state=Find<TextBlock>("State"); detail=Find<TextBlock>("Detail"); roster=Find<TextBlock>("Roster");
        feedback=Find<TextBlock>("Feedback"); mission=Find<TextBlock>("Mission"); address=Find<TextBox>("Address");
        Find<Button>("Launch").Click+=(s,e)=> {
            try {
                if (Process.GetProcessesByName("HITMAN3").Length==0)
                    Process.Start(new ProcessStartInfo("steam://rungameid/1659040"){UseShellExecute=true});
                feedback.Text="Load Freeform Training in HITMAN, then return here.";
            } catch { feedback.Text="Start HITMAN from your Steam library."; }
        };
        Find<Button>("Host").Click+=async(s,e)=>await Send("preview_host");
        Find<Button>("Join").Click+=async(s,e)=> {
            string value=address.Text.Trim();
            if (value.Length==0 || value.Length>200 || value.IndexOfAny(new char[]{'\"','\\','\r','\n'})>=0) {
                feedback.Text="Enter the host's address first."; return;
            }
            await Send("preview_join \""+value+"\"");
        };
        Find<Button>("Leave").Click+=async(s,e)=>await Send("preview_leave");
        Find<Button>("Ready").Click+=async(s,e)=>await Send("preview_ready");
        Find<Button>("Start").Click+=async(s,e)=>await Send("preview_start");
        Find<Button>("Stop").Click+=async(s,e)=>await Send("preview_lobby");
        Find<Button>("Reconnect").Click+=async(s,e)=>await Send("preview_reconnect");
        timer.Interval=TimeSpan.FromSeconds(1);
        timer.Tick+=async(s,e)=>await Send("preview_status");
        Loaded+=async(s,e)=>{timer.Start();await Send("preview_status");};
        Closed+=(s,e)=>timer.Stop();
        Find<Button>("Updates").Click+=async(s,e)=>await CheckUpdates();
        Find<Button>("Repair").Click+=async(s,e)=>await RunSetup("Auto");
        Find<Button>("Remove").Click+=async(s,e)=>await RunSetup("Remove");
        Find<TextBlock>("Release").Text="LAUNCHER "+Updates.ReleaseVersion+"  /  "+Build;
        Disable();
        if(!String.IsNullOrEmpty(startupNotice)) feedback.Text=startupNotice;
    }
    async Task RunSetup(string mode) {
        if (updating) return;
        if (Process.GetProcessesByName("HITMAN3").Length>0) { feedback.Text="Close HITMAN before changing the installed mod."; return; }
        updating=true; Disable();
        try {
            feedback.Text=mode=="Remove" ? "Removing the tracked co-op files…" : "Installing or repairing the current mod…";
            int result=await Updates.RunInstaller(AppDomain.CurrentDomain.BaseDirectory,mode);
            feedback.Text=result==0 ? (mode=="Remove" ? "Co-op removed. Your game saves are preserved." : "Installation verified. You can launch HITMAN.") : "Setup stopped. Read the setup window for details; the launcher has not switched releases.";
        } catch(Exception error) { feedback.Text=error.Message; }
        finally { updating=false; }
    }
    async Task CheckUpdates() {
        if (updating) return;
        updating=true;
        Button button=Find<Button>("Updates");button.IsEnabled=false;
        try {
            if (available==null) {
                Find<TextBlock>("UpdateState").Text="Checking the HITMAN channel…";
                available=await Updates.Check();
                Find<TextBlock>("UpdateState").Text=available==null ? "You have the current release." : "Release "+available.version+" is available.";
                button.Content=available==null ? "CHECK FOR UPDATES" : "UPDATE & RESTART";
            } else {
                if(Process.GetProcessesByName("HITMAN3").Length>0) { feedback.Text="Close HITMAN before installing the update.";return; }
                Disable();
                Find<TextBlock>("UpdateState").Text="Downloading and verifying the update…";
                string stage=await Updates.Download(available);
                Find<TextBlock>("UpdateState").Text="Installing the verified release…";
                int result=await Updates.RunInstaller(stage,"Auto");
                if(result!=0) throw new IOException("Setup stopped. The previous launcher remains selected; read the setup window.");
                Process.Start(new ProcessStartInfo(Path.Combine(stage,"HITMAN-Coop-Party-Preview.exe"),"--active"){UseShellExecute=true});
                Updates.Activate(stage);
                Close();
            }
        } catch(Exception error) {
            Find<TextBlock>("UpdateState").Text="Update unavailable — your installation is unchanged.";
            feedback.Text=error.Message;
        } finally { updating=false;button.IsEnabled=true; }
    }
    T Find<T>(string name) where T:FrameworkElement { return (T)root.FindName(name); }
    static string Text(Dictionary<string,object> data,string key) {
        object value; return data.TryGetValue(key,out value) && value!=null ? Convert.ToString(value):"";
    }
    static bool Flag(Dictionary<string,object> data,string key) {
        object value; return data.TryGetValue(key,out value) && value is bool && (bool)value;
    }
    void Disable() {
        foreach(string name in new[]{"Host","Join","Leave","Ready","Start","Stop","Reconnect"}) Find<Button>(name).IsEnabled=false;
    }
    async Task Send(string command) {
        if (busy || updating) return;
        busy=true;
        try {
            Process[] games=Process.GetProcessesByName("HITMAN3");
            if (games.Length!=1) {
                Disable(); state.Text=games.Length==0 ? "Start HITMAN to connect":"Close the extra HITMAN instance";
                detail.Text="This launcher connects to one running game on this PC."; return;
            }
            pid=games[0].Id;
            string response=await CoopLauncher.Exchange(pid,command);
            Dictionary<string,object> data=new JavaScriptSerializer().Deserialize<Dictionary<string,object>>(response);
            if (command!="preview_status" || Text(data,"presentation_error").Length>0) feedback.Text=Text(data,"message");
            if (!Flag(data,"preview") || Text(data,"version")!=Build) {
                Disable(); state.Text="Matching preview mod required";
                detail.Text="This launcher needs "+Build+". Your installed mod reports "+Text(data,"version")+"."; return;
            }
            state.Text=Text(data,"connection")+" / "+Text(data,"phase");
            detail.Text="You: "+Text(data,"role")+" | "+Build+" | "+Text(data,"room_message");
            mission.Text=Text(data,"presentation_error").Length>0 ? Text(data,"presentation_error") : Flag(data,"native_ready") ? "Freeform Training is loaded on this PC.":"Load Freeform Training on this PC before clicking Ready.";
            StringBuilder players=new StringBuilder(); int count=0;
            object raw;
            if (data.TryGetValue("players",out raw)) {
                IEnumerable rows=raw as IEnumerable;
                if (rows!=null) foreach(object item in rows) {
                    Dictionary<string,object> member=item as Dictionary<string,object>;
                    if (member==null) continue;
                    ++count;
                    players.Append("Player ").Append(Text(member,"id")).Append("   ").Append(Text(member,"name"));
                    if (Flag(member,"local")) players.Append(" (you)");
                    players.Append("   /   ").Append(Flag(member,"loaded")?"In preview":Flag(member,"ready")?"Ready":"Not ready").AppendLine().AppendLine();
                }
            }
            roster.Text=count+" / 4 players\n\n"+players.ToString();
            Find<Button>("Host").IsEnabled=Find<Button>("Join").IsEnabled=Flag(data,"can_connect");
            Find<Button>("Leave").IsEnabled=Flag(data,"can_leave");
            Find<Button>("Ready").IsEnabled=Flag(data,"can_ready");
            Find<Button>("Ready").Content=Flag(data,"local_ready")?"Cancel ready":"I'm ready";
            Find<Button>("Start").IsEnabled=Flag(data,"can_start");
            Find<Button>("Stop").IsEnabled=Flag(data,"can_stop");
            Find<Button>("Reconnect").IsEnabled=Flag(data,"can_reconnect");
        } catch {
            Disable(); state.Text="Waiting for the preview mod";
            detail.Text="Keep HITMAN running and the preview mod installed. The game may still be loading.";
        } finally { busy=false; }
    }
}

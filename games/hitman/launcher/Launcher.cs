using System;

using System.Collections.Generic;

using System.Collections;

using System.Diagnostics;

using System.IO;

using System.IO.Pipes;

using System.Text;

using System.Threading;

using System.Threading.Tasks;

using System.Web.Script.Serialization;

using System.Windows;

using System.Windows.Controls;

using System.Windows.Markup;

using System.Windows.Media;

using System.Windows.Media.Imaging;

using System.Windows.Threading;

public sealed class CoopLauncher : Window {

    const string Build = "0.13.94-four-player.preview.4";

    readonly SemaphoreSlim gate = new SemaphoreSlim(1, 1);

    readonly DispatcherTimer timer = new DispatcherTimer();

    Grid root;

    TextBlock badge, detail, action, version, partner, role, mission, prompt;

    TextBox address;

    Button host, join, leave, ready, countdown, replay;

    bool polling;

    int pid;

    [STAThread]

    public static int Main(string[] args) {

        try {

            if (args.Length == 4 && args[0] == "--request") {

                string response = Exchange(Int32.Parse(args[1]), args[2]).GetAwaiter().GetResult();

                File.WriteAllText(args[3], response);

                return 0;

            }

            Application app = new Application();

            CoopLauncher window = new CoopLauncher();

            if (args.Length == 2 && args[0] == "--preview") {

                window.root.Measure(new Size(1000, 800));

                window.root.Arrange(new Rect(0, 0, 1000, 800));

                window.root.UpdateLayout();

                RenderTargetBitmap bitmap = new RenderTargetBitmap(1000, 800, 96, 96, PixelFormats.Pbgra32);

                bitmap.Render(window.root);

                PngBitmapEncoder encoder = new PngBitmapEncoder();

                encoder.Frames.Add(BitmapFrame.Create(bitmap));

                using (FileStream file = File.Create(args[1])) encoder.Save(file);

                return 0;

            }

            app.Run(window);

            return 0;

        } catch (Exception error) {

            if (args.Length == 4) File.WriteAllText(args[3], "{\"error\":\"" + error.GetType().Name + "\"}");

            else if (args.Length == 2 && args[0] == "--preview") File.WriteAllText(args[1] + ".error", error.ToString());

            else MessageBox.Show(error.Message, "HITMAN Co-op launcher");

            return 1;

        }

    }

    public CoopLauncher() {

        Title = "HITMAN Co-op";

        Width = 1020; Height = 850; MinWidth = 920; MinHeight = 810;

        WindowStartupLocation = WindowStartupLocation.CenterScreen;

        Background = new SolidColorBrush(Color.FromRgb(15,17,20));

        root = (Grid)XamlReader.Parse(Layout);

        Content = root;

        badge = Find<TextBlock>("Badge"); detail = Find<TextBlock>("Detail"); action = Find<TextBlock>("Action");

        version = Find<TextBlock>("Version"); partner = Find<TextBlock>("Partner"); role = Find<TextBlock>("Role");

        mission = Find<TextBlock>("MissionState"); prompt = Find<TextBlock>("Prompt"); address = Find<TextBox>("Address");

        host = Find<Button>("Host"); join = Find<Button>("Join"); leave = Find<Button>("Leave");

        ready = Find<Button>("Ready"); countdown = Find<Button>("Countdown"); replay = Find<Button>("Replay");

        host.Click += async (s,e) => await Send("host");

        join.Click += async (s,e) => {

            string value = address.Text.Trim();

            if (value.Length == 0 || value.Length > 200 || value.IndexOfAny(new char[]{'"','\\','\r','\n'}) >= 0) {

                action.Text = "Enter the host's address first."; return;

            }

            await Send("join \"" + value + "\"");

        };

        leave.Click += async (s,e) => await Send("leave");

        ready.Click += async (s,e) => await Send("ready");

        countdown.Click += async (s,e) => await Send("countdown");

        replay.Click += async (s,e) => await Send("replay");

        Find<Button>("LaunchGame").Click += (s,e) => {

            try {

                if (Process.GetProcessesByName("HITMAN3").Length == 0) {

                    Process.Start(new ProcessStartInfo("steam://rungameid/1659040") { UseShellExecute = true });

                    action.Text = "Starting HITMAN through Steam. Keep this launcher open.";

                } else action.Text = "HITMAN is already running. Switch to the game to continue.";

            } catch { action.Text = "Could not open Steam. Start HITMAN from your Steam library."; }

        };

        timer.Interval = TimeSpan.FromSeconds(1);

        timer.Tick += async (s,e) => await Poll();

        Loaded += async (s,e) => { timer.Start(); await Poll(); };

        Closed += (s,e) => timer.Stop();

    }

    T Find<T>(string name) where T : class { return (T)root.FindName(name); }

    static string Text(Dictionary<string,object> data, string key) { object value; return data.TryGetValue(key,out value) && value != null ? Convert.ToString(value) : ""; }

    static bool Flag(Dictionary<string,object> data, string key) { object value; return data.TryGetValue(key,out value) && value is bool && (bool)value; }

    void Enable(bool online, bool connected, bool synced, bool canReplay) {

        host.IsEnabled = join.IsEnabled = online && !connected;

        leave.IsEnabled = online && connected;

        ready.IsEnabled = countdown.IsEnabled = online && synced;

        replay.IsEnabled = online && canReplay;

    }

    async Task Poll() {

        if (polling) return;

        polling = true;

        try {

            Process[] games = Process.GetProcessesByName("HITMAN3");

            if (games.Length != 1) {

                pid = 0; badge.Text = games.Length == 0 ? "GAME IS CLOSED" : "MULTIPLE GAMES FOUND";

                detail.Text = games.Length == 0 ? "Launch HITMAN, then host or join here." : "Keep one HITMAN instance running.";

                Enable(false,false,false,false);
                partner.Text = "Waiting for players"; role.Text = "None";
                mission.Text = "Open Freeform Training in HITMAN on both PCs.";
                return;

            }

            pid = games[0].Id;

            await Send("status");

        } finally { polling = false; }

    }

    async Task Send(string command) {

        if (pid == 0) return;

        await gate.WaitAsync();

        try {

            string response = await Exchange(pid, command);

            Dictionary<string,object> data = new JavaScriptSerializer().Deserialize<Dictionary<string,object>>(response);

            if (Text(data,"version") != Build || Text(data,"bridge_version") != "1") {

                badge.Text = "UPDATE NEEDED"; detail.Text = "Install the launcher and mod from the same Foundation 4 package.";

                Enable(false,false,false,false); return;

            }

            bool connected = Flag(data,"connected");

            bool active = connected || Text(data,"connection") == "Hosting, waiting for a friend" || Text(data,"connection") == "Connecting";

            badge.Text = Text(data,"connection").ToUpperInvariant();

            detail.Text = Text(data,"session_message");

            if (detail.Text.Length == 0) detail.Text = "Game connected to launcher. Host or join a friend.";

            version.Text = Text(data,"version") + "  /  protocol " + Text(data,"protocol") + "\nHITMAN " + Text(data,"game_version");

            List<string> roster = new List<string>();

            object rows;

            if (data.TryGetValue("players", out rows) && rows is IEnumerable)

                foreach (object row in (IEnumerable)rows) {

                    Dictionary<string,object> member = row as Dictionary<string,object>;

                    if (member != null) roster.Add((Flag(member,"local") ? "You" : Text(member,"name")) + "   /   player " + Text(member,"id"));

                }

            partner.Text = roster.Count == 0 ? "Waiting for players" : String.Join("\n", roster.ToArray());

            role.Text = Text(data,"role");

            mission.Text = Flag(data,"synced") && Flag(data,"freeform") ? "Both players are in Freeform Training." :

                Flag(data,"in_mission") ? "Load Freeform Training on both PCs for this test." : "Open Freeform Training in HITMAN on both PCs.";

            string readyState = Text(data,"ready_state");
            prompt.Text = readyState == "incoming" ? "Your friend asks if you are ready. Click I'm ready to reply." :
                readyState == "waiting" ? "Waiting for your friend to reply. Ready checks expire after 60 seconds." :
                readyState == "acknowledged" ? "Your friend replied: ready." :
                readyState == "replied" ? "Your ready reply was sent to your friend." :
                readyState == "expired" ? "The ready check expired. Click Ready check to ask again." :
                Flag(data,"synced") ? "Send a ready check when you are ready." : "Both players need to load the same mission first.";
            ready.Content = readyState == "incoming" ? "I'm ready" : readyState == "waiting" ? "Waiting for reply" : "Ready check";
            Enable(true,active,Flag(data,"synced"),Flag(data,"can_replay"));
            if (readyState == "waiting") ready.IsEnabled = false;
            leave.IsEnabled = Flag(data,"can_leave");
            if (command != "status") action.Text = Text(data,"message");
            else if (readyState == "waiting" || readyState == "acknowledged" || readyState == "replied" || readyState == "expired")
                action.Text = prompt.Text;

        } catch {

            badge.Text = "WAITING FOR CO-OP";

            detail.Text = "Let HITMAN finish loading. If this persists, install Foundation 4 and relaunch the game.";

            Enable(false,false,false,false);

            if (command != "status") action.Text = "The game did not acknowledge that action. Check its connection before trying again.";

        } finally { gate.Release(); }

    }

    public static async Task<string> Exchange(int processId, string command) {

        using (NamedPipeClientStream pipe = new NamedPipeClientStream(".", "HitmanCoop.Launcher." + processId,

                PipeDirection.InOut, PipeOptions.Asynchronous)) {

            await pipe.ConnectAsync(700);

            pipe.ReadMode = PipeTransmissionMode.Message;

            byte[] request = Encoding.UTF8.GetBytes(command);

            await pipe.WriteAsync(request,0,request.Length);

            byte[] buffer = new byte[16384]; int used = 0;

            do {

                Task<int> read = pipe.ReadAsync(buffer,used,buffer.Length-used);

                if (await Task.WhenAny(read,Task.Delay(3000)) != read) throw new TimeoutException();

                int count = await read;

                if (count == 0) throw new IOException();

                used += count;

                if (used == buffer.Length && !pipe.IsMessageComplete) throw new IOException();

            } while (!pipe.IsMessageComplete);

            await pipe.WriteAsync(new byte[]{1},0,1);

            return Encoding.UTF8.GetString(buffer,0,used);

        }

    }

    const string Layout = @"

<Grid xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml' Background='#0F1114'>

 <Grid.Resources>

  <Style TargetType='TextBlock'><Setter Property='Foreground' Value='#EFF0F2'/><Setter Property='FontFamily' Value='Segoe UI'/><Setter Property='FontSize' Value='14'/><Setter Property='TextWrapping' Value='Wrap'/></Style>

  <Style TargetType='Button'><Setter Property='Background' Value='#30343C'/><Setter Property='Foreground' Value='White'/><Setter Property='BorderThickness' Value='0'/><Setter Property='Padding' Value='16,11'/><Setter Property='Margin' Value='0,0,8,0'/><Setter Property='FontSize' Value='14'/><Setter Property='Cursor' Value='Hand'/><Style.Triggers><Trigger Property='IsEnabled' Value='False'><Setter Property='Opacity' Value='0.4'/></Trigger></Style.Triggers></Style>

  <Style TargetType='TextBox'><Setter Property='Background' Value='#111318'/><Setter Property='Foreground' Value='White'/><Setter Property='BorderBrush' Value='#424750'/><Setter Property='Padding' Value='10'/><Setter Property='FontSize' Value='15'/></Style>

 </Grid.Resources>

 <Grid Margin='32'>

  <Grid.RowDefinitions><RowDefinition Height='Auto'/><RowDefinition Height='Auto'/><RowDefinition Height='*'/><RowDefinition Height='Auto'/></Grid.RowDefinitions>

  <StackPanel Margin='0,0,0,24'><TextBlock Text='HITMAN / CO-OP' FontSize='30' FontWeight='Bold'/><TextBlock Text='Session launcher / Foundation 4.1 / target: 4 players' Foreground='#9DA6B6' Margin='0,6,0,0'/></StackPanel>

  <Border Grid.Row='1' Background='#1D222B' CornerRadius='8' Padding='18' Margin='0,0,0,20'><StackPanel><TextBlock x:Name='Badge' Text='GAME IS CLOSED' FontWeight='Bold' Foreground='#F67A7A'/><TextBlock x:Name='Detail' Text='Launch HITMAN, then host or join here.' Margin='0,6,0,0' Foreground='#BCC4D1'/></StackPanel></Border>

  <Grid Grid.Row='2'><Grid.ColumnDefinitions><ColumnDefinition Width='1.65*'/><ColumnDefinition Width='20'/><ColumnDefinition Width='*'/></Grid.ColumnDefinitions>

   <StackPanel>

    <Border Background='#191C22' CornerRadius='8' Padding='20' Margin='0,0,0,16'><StackPanel>

     <TextBlock Text='1  CONNECT' FontWeight='Bold' FontSize='17' Margin='0,0,0,14'/>

     <StackPanel Orientation='Horizontal' Margin='0,0,0,16'><Button x:Name='Host' Content='Host a session' Background='#B52C37' IsEnabled='False'/><Button x:Name='Leave' Content='Leave' IsEnabled='False'/></StackPanel>

     <TextBlock Text='Or enter the host address' Foreground='#A9B2C2' Margin='0,0,0,6'/>

     <DockPanel><Button x:Name='Join' Content='Join' DockPanel.Dock='Right' Margin='10,0,0,0' IsEnabled='False'/><TextBox x:Name='Address' MaxLength='200'/></DockPanel>

    </StackPanel></Border>

    <Border Background='#191C22' CornerRadius='8' Padding='20'><StackPanel>

     <TextBlock Text='2  MISSION' FontWeight='Bold' FontSize='17'/><TextBlock Text='Freeform Training' FontSize='24' Margin='0,12,0,3'/><TextBlock Text='ICA Facility  /  first tested mission' Foreground='#A9B2C2'/>

     <TextBlock x:Name='MissionState' Text='Open Freeform Training in HITMAN on both PCs.' Margin='0,14,0,12'/>

     <WrapPanel><Button x:Name='Ready' Content='Ready check' IsEnabled='False' Margin='0,0,8,8'/><Button x:Name='Countdown' Content='Countdown' IsEnabled='False' Margin='0,0,8,8'/><Button x:Name='Replay' Content='Replay / restart' IsEnabled='False' Margin='0,0,0,8'/></WrapPanel>

     <TextBlock x:Name='Prompt' Text='Ready checks are available once both players are in the mission.' FontSize='12' Foreground='#9DA6B6'/>

    </StackPanel></Border>

   </StackPanel>

   <StackPanel Grid.Column='2'>

    <Border Background='#191C22' CornerRadius='8' Padding='20'><StackPanel><TextBlock Text='PARTY' FontWeight='Bold' FontSize='17'/><TextBlock Text='Connected players' Foreground='#9DA6B6' Margin='0,18,0,4'/><TextBlock x:Name='Partner' Text='Waiting for a friend' FontSize='18'/><TextBlock Text='Your role' Foreground='#9DA6B6' Margin='0,16,0,4'/><TextBlock x:Name='Role' Text='None'/><TextBlock Text='Build' Foreground='#9DA6B6' Margin='0,16,0,4'/><TextBlock x:Name='Version' Text='four-player dev.1 / legacy protocol 19' FontSize='12'/></StackPanel></Border>

    <Border Background='#1D222B' CornerRadius='8' Padding='18' Margin='0,16,0,0'><StackPanel><TextBlock Text='DEVELOPMENT STATUS' FontWeight='Bold' FontSize='12' Foreground='#E3B975'/><TextBlock Text='This test build supports 2 players. Four-player networking, automatic mission launch and shared extraction are being integrated.' Margin='0,8,0,0' Foreground='#BCC4D1'/></StackPanel></Border>

   </StackPanel>

  </Grid>

  <Grid Grid.Row='3' Margin='0,20,0,0'><Grid.ColumnDefinitions><ColumnDefinition Width='*'/><ColumnDefinition Width='Auto'/></Grid.ColumnDefinitions><StackPanel><TextBlock x:Name='Action' Text='Co-op notification sounds are off.' Foreground='#BEC7D7'/><TextBlock Text='Keep this launcher open while you play.' FontSize='12' Foreground='#788395' Margin='0,4,0,0'/></StackPanel><Button x:Name='LaunchGame' Grid.Column='1' Content='Launch HITMAN' Margin='12,0,0,0'/></Grid>

 </Grid>

</Grid>";

}

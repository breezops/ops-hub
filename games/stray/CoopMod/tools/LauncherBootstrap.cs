using System;
using System.IO;
using System.Diagnostics;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;
using System.Collections.Generic;
using System.Windows.Forms;

internal static class LauncherBootstrap {
    [STAThread]
    static void Main() {
        try {
            string home=AppDomain.CurrentDomain.BaseDirectory;
            var current=new JavaScriptSerializer().Deserialize<Dictionary<string,object>>(File.ReadAllText(Path.Combine(home,"current.json")));
            string version=Convert.ToString(current["version"]);
            if(!Regex.IsMatch(version,@"^\d+\.\d+\.\d+$"))throw new Exception("The launcher release pointer is invalid.");
            string release=Path.Combine(home,"releases",version);
            string python=Path.Combine(home,"runtime","pythonw.exe");
            string script=Path.Combine(release,"tools","launcher.py");
            if(!File.Exists(python)||!File.Exists(script))throw new Exception("Extract the entire launcher ZIP before opening StrayCoop.exe.");
            var start=new ProcessStartInfo(python,"-B \""+script+"\"");
            start.WorkingDirectory=release;start.UseShellExecute=false;start.CreateNoWindow=true;
            start.EnvironmentVariables["STRAYCOOP_LAUNCHER_HOME"]=home;
            Process.Start(start);
        } catch(Exception error) {MessageBox.Show(error.Message,"Stray Co-op",MessageBoxButtons.OK,MessageBoxIcon.Error);}
    }
}

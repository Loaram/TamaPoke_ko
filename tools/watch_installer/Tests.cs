using System;
using System.IO;
using System.Drawing;
using System.Windows.Forms;
using TamaPokeWatchInstaller;

public static class Tests {
    static int count;
    static void Check(bool condition, string name) {
        if (!condition) throw new Exception("FAIL: " + name);
        count++; Console.WriteLine("PASS: " + name);
    }
    static void Reject(Action action, string name) {
        bool rejected = false;
        try { action(); } catch (Exception) { rejected = true; }
        Check(rejected, name);
    }
    [STAThread] public static int Main(string[] args) {
        if (args.Length == 2 && args[0] == "--verify-bundle") {
            Console.WriteLine("Verified release bundle: " + Adb.VerifyBundle(args[1])); return 0;
        }
        if (args.Length > 0 && args[0] == "fake-ok") { Console.WriteLine("Success"); return 0; }
        if (args.Length > 0 && args[0] == "fake-input") { Console.WriteLine(Console.ReadLine()); return 0; }
        if (args.Length > 0 && args[0] == "fake-fail") { Console.Error.WriteLine("INSTALL_FAILED_UPDATE_INCOMPATIBLE"); return 1; }
        if (args.Length > 0 && args[0] == "fake-slow") { System.Threading.Thread.Sleep(3000); return 0; }
        Check(Adb.Endpoint(" 192.168.0.12 ", " 37891 ") == "192.168.0.12:37891", "endpoint");
        foreach (var host in new [] { "host & calc", "192.168.0.1:1234", "999.2.3.4", "127.1", "-s", "" })
            Reject(() => Adb.Endpoint(host, "1234"), "invalid host " + host);
        foreach (var port in new [] { "0", "65536", "12 & calc", "" })
            Reject(() => Adb.Endpoint("192.168.0.1", port), "invalid port " + port);
        Check(Adb.Code("012345") == "012345", "leading zero pairing code");
        Reject(() => Adb.Code("12345"), "short code");
        Reject(() => Adb.Code("123456\nfoo"), "code injection");
        Check(Adb.Explain("INSTALL_FAILED_UPDATE_INCOMPATIBLE").Contains("서명"), "signature guidance");
        Check(Adb.Explain("INSTALL_FAILED_VERSION_DOWNGRADE").Contains("최신"), "downgrade guidance");
        Check(Adb.Explain("INSTALL_FAILED_INSUFFICIENT_STORAGE").Contains("공간"), "storage guidance");
        string exe = System.Reflection.Assembly.GetExecutingAssembly().Location;
        Check(Adb.Run(exe, "fake-ok", 3000) == "Success", "process output");
        Check(!Adb.Run(exe, "fake-input", 3000, "012345").Contains("012345"), "secret redacted");
        Reject(() => Adb.Run(exe, "fake-fail", 3000), "nonzero rejected");
        Reject(() => Adb.Run(exe, "fake-slow", 100), "timeout bounded");
        string folder = Path.Combine(Path.GetTempPath(), "tamapoke-installer-test-" + Guid.NewGuid());
        Directory.CreateDirectory(folder);
        try {
            var sums = "";
            foreach (string name in new [] { "adb.exe", "AdbWinApi.dll", "AdbWinUsbApi.dll", "TamaPoke-WearOS.apk" }) {
                File.WriteAllText(Path.Combine(folder, name), name);
                sums += Adb.Hash(Path.Combine(folder, name)) + "  " + name + "\n";
            }
            File.WriteAllText(Path.Combine(folder, "SHA256SUMS.txt"), sums);
            Check(File.Exists(Adb.VerifyBundle(folder)), "bundle verification");
            File.AppendAllText(Path.Combine(folder, "TamaPoke-WearOS.apk"), "bad");
            Reject(() => Adb.VerifyBundle(folder), "damaged APK rejected");
            File.WriteAllText(Path.Combine(folder, "TamaPoke-WearOS.apk"), "TamaPoke-WearOS.apk");
            File.WriteAllText(Path.Combine(folder, "SHA256SUMS.txt"), sums + sums);
            Reject(() => Adb.VerifyBundle(folder), "duplicate manifest entries rejected");
            File.WriteAllText(Path.Combine(folder, "SHA256SUMS.txt"), sums + new string('a', 64) + "  ../outside.apk\n");
            Reject(() => Adb.VerifyBundle(folder), "manifest traversal rejected");
            File.WriteAllText(Path.Combine(folder, "SHA256SUMS.txt"), sums);
            File.Delete(Path.Combine(folder, "AdbWinApi.dll"));
            Reject(() => Adb.VerifyBundle(folder), "missing DLL rejected");
            File.WriteAllText(Path.Combine(folder, "SHA256SUMS.txt"), "");
            Reject(() => Adb.VerifyBundle(folder), "empty manifest rejected");
        } finally { foreach (var file in Directory.GetFiles(folder)) File.Delete(file); Directory.Delete(folder); }
        Application.EnableVisualStyles();
        using (var form = new Installer(true, args.Length > 1 ? args[1] : null)) {
            form.ClientSize = new Size(780, 1100);
            form.CreateControl();
            Check(form.Controls.Count > 0, "GUI constructed");
            int disabledActions = 0;
            bool guideAvailable = false;
            foreach (Control child in form.Controls[0].Controls) {
                var button = child as Button;
                if (button == null) continue;
                if (button.Text.Contains("PDF")) guideAvailable = button.Enabled;
                else if (!button.Enabled) disabledActions++;
            }
            Check(disabledActions == 4, "ADB actions blocked before bundle verification");
            Check(guideAvailable, "PDF help available before bundle verification");
            if (args.Length >= 1) {
                form.ShowInTaskbar = false;
                form.Opacity = 0;
                form.Show();
                Application.DoEvents();
                using (var bitmap = new Bitmap(form.Controls[0].Width, form.Controls[0].Height)) {
                    form.Controls[0].DrawToBitmap(bitmap, new Rectangle(0, 0, bitmap.Width, bitmap.Height));
                    bitmap.Save(args[0]);
                }
            }
        }
        Console.WriteLine(count + " checks passed. Physical watch install not tested.");
        return 0;
    }
}

using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace TamaPokeWatchInstaller {
    public static class Adb {
        public const string Package = "com.loaram.tamapoke";
        public static string Endpoint(string host, string port) {
            IPAddress address;
            host = host.Trim(); port = port.Trim();
            if (!Regex.IsMatch(host, @"^\d{1,3}(\.\d{1,3}){3}$") || !IPAddress.TryParse(host, out address))
                throw new Exception("워치에 표시된 IP 주소를 입력하세요. 예: 192.168.0.12");
            int number;
            if (!int.TryParse(port, out number) || number < 1 || number > 65535)
                throw new Exception("포트는 1~65535 사이 숫자입니다. 워치 화면의 콜론(:) 뒤 숫자를 입력하세요.");
            return address.ToString() + ":" + number;
        }
        public static string Code(string code) {
            code = code.Trim();
            if (!Regex.IsMatch(code, @"^[0-9]{6}$")) throw new Exception("워치의 6자리 페어링 코드를 입력하세요.");
            return code;
        }
        public static string Explain(string output) {
            if (output.Contains("INSTALL_FAILED_UPDATE_INCOMPATIBLE"))
                return "기존 앱과 새 APK의 서명이 다릅니다. 앱을 삭제하면 세이브도 지워집니다. 기존 앱을 유지하고 제작자에게 같은 서명의 업데이트를 요청하세요.";
            if (output.Contains("INSTALL_FAILED_VERSION_DOWNGRADE"))
                return "워치에 더 최신 버전이 설치되어 있습니다. 최신 설치 도우미를 사용하세요.";
            if (output.Contains("INSTALL_FAILED_INSUFFICIENT_STORAGE"))
                return "워치 저장 공간이 부족합니다. 다른 불필요한 파일을 정리한 뒤 다시 시도하세요.";
            if (output.Contains("INSTALL_FAILED_NO_MATCHING_ABIS") || output.Contains("INSTALL_FAILED_OLDER_SDK"))
                return "이 워치의 운영체제 또는 CPU가 APK와 호환되지 않습니다. Wear OS 버전과 워치 모델을 확인하세요.";
            if (output.Contains("unauthorized")) return "워치 화면에서 이 PC의 디버깅 연결을 허용하세요. 필요하면 다시 페어링하세요.";
            if (output.Contains("offline")) return "워치 연결이 끊겼습니다. 화면을 켜고 현재 연결 포트를 확인한 뒤 다시 연결하세요.";
            return "작업에 실패했습니다. PC와 워치를 같은 Wi-Fi에 연결하고 워치 화면을 켜세요. 페어링 포트와 연결 포트는 서로 다릅니다. 무선 디버깅을 다시 켰다면 바뀐 포트를 입력하세요.";
        }
        public static string Run(string exe, string args, int timeout, string input = null) {
            var info = new ProcessStartInfo(exe, args) {
                UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true,
                RedirectStandardError = true, RedirectStandardInput = true,
                StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8
            };
            using (var process = Process.Start(info)) {
                var stdout = process.StandardOutput.ReadToEndAsync();
                var stderr = process.StandardError.ReadToEndAsync();
                if (input != null) process.StandardInput.WriteLine(input);
                process.StandardInput.Close();
                if (!process.WaitForExit(timeout)) {
                    try { process.Kill(); } catch (InvalidOperationException) { }
                    throw new Exception("응답 시간이 초과되었습니다. 설치 중이었다면 워치의 앱 목록부터 확인하세요. 현재 연결 포트를 확인하고 다시 연결할 수 있습니다.");
                }
                var output = (stdout.Result + "\n" + stderr.Result).Trim();
                if (input != null) output = output.Replace(input, "[코드 숨김]");
                if (process.ExitCode != 0) throw new Exception(Explain(output) + "\n\n" + output);
                return output;
            }
        }
        public static string Hash(string path) {
            using (var stream = File.OpenRead(path)) using (var sha = SHA256.Create())
                return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
        }
        public static string VerifyBundle(string root) {
            var manifest = Path.Combine(root, "SHA256SUMS.txt");
            if (!File.Exists(manifest)) throw new Exception("압축을 모두 풀어 주세요. SHA256SUMS.txt가 없습니다.");
            var files = new System.Collections.Generic.HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var line in File.ReadAllLines(manifest)) {
                var match = Regex.Match(line, @"^([a-fA-F0-9]{64})  ([a-zA-Z0-9_.-]+)$");
                if (!match.Success || !files.Add(match.Groups[2].Value)) throw new Exception("설치 파일 목록이 올바르지 않습니다. ZIP을 다시 받으세요.");
                var path = Path.Combine(root, match.Groups[2].Value);
                if (!File.Exists(path) || Hash(path) != match.Groups[1].Value.ToLowerInvariant())
                    throw new Exception("설치 파일이 없거나 손상되었습니다: " + match.Groups[2].Value + "\nZIP을 다시 받아 압축을 모두 풀어 주세요.");
            }
            foreach (var name in new [] { "adb.exe", "AdbWinApi.dll", "AdbWinUsbApi.dll", "TamaPoke-WearOS.apk" })
                if (!files.Contains(name)) throw new Exception("필수 설치 파일의 검사 정보가 없습니다: " + name);
            return Path.Combine(root, "TamaPoke-WearOS.apk");
        }
    }

    public sealed class Installer : Form {
        readonly string root = AppDomain.CurrentDomain.BaseDirectory;
        readonly TextBox ip = new TextBox(), pairPort = new TextBox(), code = new TextBox(), connectPort = new TextBox();
        readonly TextBox log = new TextBox();
        readonly Label status = new Label();
        readonly ProgressBar progress = new ProgressBar();
        readonly System.Collections.Generic.List<Button> buttons = new System.Collections.Generic.List<Button>();
        readonly Button install, launch;
        string serial, apk;
        bool busy;

        public Installer(bool preview = false, string previewVersion = null) {
            Text = "TamaPoke 워치 설치 도우미";
            Font = new Font("맑은 고딕", 10);
            ClientSize = new Size(780, 850); MinimumSize = new Size(730, 650);
            StartPosition = FormStartPosition.CenterScreen;
            AutoScaleMode = AutoScaleMode.Dpi;
            var page = new TableLayoutPanel { Dock = DockStyle.Fill, AutoScroll = true, ColumnCount = 1, Padding = new Padding(22) };
            page.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            Controls.Add(page);
            AddLabel(page, "TamaPoke  |  워치에 설치하기", 20, true);
            var version = Path.Combine(root, "version.txt");
            AddLabel(page, "Galaxy Watch · Wear OS   /   " + (previewVersion ?? (File.Exists(version) ? File.ReadAllText(version).Trim() : "개발용")), 10, false);
            var help = new Button { Text = "그림으로 보는 PDF 설명서", AutoSize = true, MinimumSize = new Size(220, 34) };
            help.Click += delegate {
                var guide = Path.Combine(root, "Watch-Installer-Guide-KO.pdf");
                try {
                    if (!File.Exists(guide)) throw new Exception("동봉된 PDF가 없습니다. ZIP을 모두 압축 풀거나 릴리스에서 설치 도우미 PDF를 받으세요.");
                    Process.Start(new ProcessStartInfo(guide) { UseShellExecute = true });
                } catch (Exception ex) { MessageBox.Show(this, ex.Message, "설명서 열기"); }
            };
            page.Controls.Add(help);
            AddLabel(page, "1. 워치 준비", 13, true);
            AddLabel(page, "① PC와 워치를 같은 Wi-Fi에 연결하고 워치 화면을 켜 두세요.\n② 워치 설정 → 워치 정보 → 소프트웨어 정보 → 소프트웨어 버전을 여러 번 눌러 개발자 모드를 켜세요.\n③ 설정 → 개발자 옵션 → ADB 디버깅 및 무선 디버깅을 켜고 연결을 허용하세요.\n※ 메뉴 이름은 워치의 소프트웨어 버전에 따라 다를 수 있습니다.", 10, false);
            AddLabel(page, "2. 처음 사용하는 PC 등록", 13, true);
            AddLabel(page, "워치의 무선 디버깅 → ‘새 기기 페어링’을 열어 아래 값을 입력하세요.\n이미 이 PC와 페어링했다면 3단계로 바로 이동할 수 있습니다.", 10, false);
            AddField(page, "워치 IP 주소", ip, "예: 192.168.0.12 (콜론 앞)");
            AddField(page, "페어링 포트", pairPort, "‘새 기기 페어링’ 화면의 콜론 뒤 숫자");
            code.UseSystemPasswordChar = true; code.MaxLength = 6;
            AddField(page, "페어링 코드", code, "같은 화면에 표시된 6자리 숫자");
            AddButton(page, "PC 등록 (페어링)", Pair);
            AddLabel(page, "3. 연결하고 설치", 13, true);
            AddLabel(page, "워치에서 한 화면 뒤로 돌아가 ‘무선 디버깅’의 IP 주소 및 포트를 확인하세요.\n위의 페어링 포트가 아닌, 이 화면에 표시된 연결 포트를 입력하세요.", 10, false);
            AddField(page, "연결 포트", connectPort, "무선 디버깅 기본 화면의 콜론 뒤 숫자");
            AddButton(page, "워치 연결 확인", ConnectWatch);
            install = AddButton(page, "설치 / 업데이트", Install);
            launch = AddButton(page, "워치에서 게임 열기", Launch);
            AddLabel(page, "업데이트 전 세이브를 백업하고 미완료 포켓몬 전송·교환을 먼저 끝내세요.\n자동 백업은 하지 않습니다. 설치 후 워치에서 무선 디버깅을 꺼 주세요.", 10, false);
            status.AutoSize = true; status.MaximumSize = new Size(670, 0); status.Text = "설치 파일 확인 중…"; page.Controls.Add(status);
            progress.Dock = DockStyle.Top; progress.Height = 8; page.Controls.Add(progress);
            log.Multiline = true; log.ReadOnly = true; log.ScrollBars = ScrollBars.Vertical;
            log.Height = 100; log.Dock = DockStyle.Top; page.Controls.Add(log);
            foreach (var button in buttons) button.Enabled = false;
            ip.TextChanged += delegate { InvalidateConnection(); };
            connectPort.TextChanged += delegate { InvalidateConnection(); };
            if (!preview) Shown += async delegate { await Work("동봉된 설치 파일을 확인하고 있습니다…", () => {
                apk = Adb.VerifyBundle(root);
                return "준비 완료. 워치의 값을 입력한 뒤 PC 등록 또는 워치 연결 확인을 누르세요.";
            }); };
            FormClosing += delegate(object sender, FormClosingEventArgs e) {
                if (busy) { e.Cancel = true; MessageBox.Show(this, "진행 중인 작업이 끝난 뒤 닫아 주세요. 연결 확인은 최대 30초, 설치는 최대 10분 기다립니다.", Text); }
            };
        }
        void InvalidateConnection() { serial = null; install.Enabled = launch.Enabled = false; }
        static void AddLabel(TableLayoutPanel panel, string text, int size, bool bold) {
            panel.Controls.Add(new Label { Text = text, AutoSize = true, MaximumSize = new Size(680, 0),
                Font = new Font("맑은 고딕", size, bold ? FontStyle.Bold : FontStyle.Regular), Margin = new Padding(0, 7, 0, 5) });
        }
        static void AddField(TableLayoutPanel panel, string title, TextBox entry, string hint) {
            var row = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Top, WrapContents = true };
            row.Controls.Add(new Label { Text = title, Width = 110, Padding = new Padding(0, 5, 0, 0) });
            entry.Width = 155; row.Controls.Add(entry);
            row.Controls.Add(new Label { Text = hint, AutoSize = true, Padding = new Padding(4, 5, 0, 0) }); panel.Controls.Add(row);
        }
        Button AddButton(TableLayoutPanel panel, string text, Func<Task> action) {
            var button = new Button { Text = text, AutoSize = true, MinimumSize = new Size(190, 34), Margin = new Padding(0, 4, 0, 4) };
            button.Click += async delegate { await action(); }; panel.Controls.Add(button); buttons.Add(button); return button;
        }
        async Task Work(string title, Func<string> job) {
            if (busy) return;
            busy = true; foreach (var button in buttons) button.Enabled = false;
            ip.Enabled = pairPort.Enabled = code.Enabled = connectPort.Enabled = false;
            status.Text = title; progress.Style = ProgressBarStyle.Marquee;
            try {
                string result = await Task.Run(job);
                status.Text = result; log.AppendText(result + Environment.NewLine);
            } catch (Exception ex) {
                serial = null; status.Text = "작업을 완료하지 못했습니다. 아래 안내를 확인하세요.";
                log.AppendText(ex.Message + Environment.NewLine);
                MessageBox.Show(this, ex.Message, "설치 안내", MessageBoxButtons.OK, MessageBoxIcon.Information);
            } finally {
                busy = false; progress.Style = ProgressBarStyle.Blocks;
                foreach (var button in buttons) button.Enabled = apk != null;
                install.Enabled = launch.Enabled = apk != null && serial != null;
                ip.Enabled = pairPort.Enabled = code.Enabled = connectPort.Enabled = true;
            }
        }
        string Run(string args, int timeout = 30000, string input = null) { return Adb.Run(Path.Combine(root, "adb.exe"), args, timeout, input); }
        async Task Pair() {
            string endpoint, secret;
            try { endpoint = Adb.Endpoint(ip.Text, pairPort.Text); secret = Adb.Code(code.Text); }
            catch (Exception ex) { MessageBox.Show(this, ex.Message, "입력 확인"); return; }
            code.Clear();
            await Work("워치와 PC를 등록하고 있습니다…", () => {
                string result = Run("pair " + endpoint, 30000, secret);
                if (!result.Contains("Successfully paired")) throw new Exception(Adb.Explain(result) + "\n" + result);
                return "PC 등록 완료. 워치에서 뒤로 돌아가 ‘무선 디버깅’ 화면의 연결 포트를 입력하세요.";
            });
        }
        void CheckWatch(string target) {
            if (Run("-s " + target + " get-state").Trim() != "device") throw new Exception("워치 연결을 확인할 수 없습니다. 다시 연결하세요.");
            var features = Run("-s " + target + " shell pm list features");
            if (!Regex.IsMatch(features, @"(?m)^feature:android.hardware.type.watch\s*$"))
                throw new Exception("연결된 기기가 Wear OS 워치로 확인되지 않았습니다. 워치의 IP 주소를 확인하세요.");
        }
        async Task ConnectWatch() {
            string endpoint;
            try { endpoint = Adb.Endpoint(ip.Text, connectPort.Text); }
            catch (Exception ex) { MessageBox.Show(this, ex.Message, "입력 확인"); return; }
            serial = null;
            await Work("워치 연결과 기기 종류를 확인하고 있습니다…", () => {
                string result = Run("connect " + endpoint);
                if (!result.Contains("connected to " + endpoint)) throw new Exception(Adb.Explain(result) + "\n" + result);
                CheckWatch(endpoint);
                var model = Run("-s " + endpoint + " shell getprop ro.product.model").Trim();
                serial = endpoint;
                return "연결 완료: " + model + " (" + endpoint + "). ‘설치 / 업데이트’를 누르세요.";
            });
        }
        async Task Install() {
            string target = serial;
            if (target == null) return;
            if (MessageBox.Show(this, "연결 대상: " + target + "\n\n기존 앱을 삭제하지 않고 설치 / 업데이트합니다.\n자동 백업은 하지 않습니다. 기존 사용자는 세이브를 백업하고\n미완료 포켓몬 전송·교환을 먼저 끝내세요.\n\n설치를 시작할까요?", "설치 전 확인", MessageBoxButtons.YesNo, MessageBoxIcon.Question, MessageBoxDefaultButton.Button2) != DialogResult.Yes) return;
            await Work("설치 중… 큰 파일을 전송하므로 몇 분 걸릴 수 있습니다. Wi-Fi 연결을 유지하세요.", () => {
                // Re-check before every write; never uninstall, clear data, or allow a downgrade.
                CheckWatch(target);
                string verified = Adb.VerifyBundle(root);
                var result = Run("-s " + target + " install -r \"" + verified + "\"", 600000);
                if (!Regex.IsMatch(result, @"(?m)^Success\s*$")) throw new Exception(Adb.Explain(result) + "\n" + result);
                var path = Run("-s " + target + " shell pm path " + Adb.Package);
                if (!path.Contains("package:")) throw new Exception("설치 명령은 끝났으나 앱 확인에 실패했습니다. 워치의 앱 목록을 확인하세요.");
                return "설치 완료! ‘워치에서 게임 열기’를 누르거나 워치 앱 목록에서 TamaPoke를 여세요.";
            });
        }
        async Task Launch() {
            string target = serial;
            if (target == null) return;
            await Work("워치에서 게임을 열고 있습니다…", () => {
                CheckWatch(target);
                var result = Run("-s " + target + " shell am start -n " + Adb.Package + "/.TamaPokeActivity");
                if (result.IndexOf("Error", StringComparison.OrdinalIgnoreCase) >= 0 || result.Contains("Exception"))
                    throw new Exception("게임을 열지 못했습니다. 설치를 완료한 뒤 다시 시도하세요.\n" + result);
                return "게임 실행 요청 완료. 워치를 확인하고 설치가 끝나면 무선 디버깅을 꺼 주세요.";
            });
        }
        [STAThread] public static void Main() {
            Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new Installer());
        }
    }
}

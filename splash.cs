using System;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;

class SplashApp {
    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    public static extern IntPtr LoadImage(IntPtr hinst, string lpszName, uint uType, int cxDesired, int cyDesired, uint fuLoad);

    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern int GetSystemMetrics(int nIndex);

    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    public const uint IMAGE_ICON = 1;
    public const uint LR_LOADFROMFILE = 0x00000010;
    public const uint WM_SETICON = 0x0080;
    public const int ICON_SMALL = 0;
    public const int ICON_BIG = 1;

    public const int SM_CXICON = 11;
    public const int SM_CYICON = 12;
    public const int SM_CXSMICON = 49;
    public const int SM_CYSMICON = 50;

    [STAThread]
    static void Main(string[] args) {
        string dir = AppDomain.CurrentDomain.BaseDirectory;
        string imgPath = Path.Combine(dir, "Splash_screen_hp71b.png");
        string icoPath = Path.Combine(dir, "icon.ico");

        if (args.Length > 0 && File.Exists(args[0])) {
            imgPath = args[0];
        }

        if (!File.Exists(imgPath)) return;

        Image img;
        try {
            img = Image.FromFile(imgPath);
        } catch {
            return;
        }

        Form form = new Form();
        form.FormBorderStyle = FormBorderStyle.None;
        form.StartPosition = FormStartPosition.CenterScreen;
        form.TopMost = true;
        form.ShowInTaskbar = false;
        form.Size = new Size(781, 525);
        form.BackColor = Color.Black;

        PictureBox pic = new PictureBox();
        pic.Dock = DockStyle.Fill;
        pic.SizeMode = PictureBoxSizeMode.StretchImage;
        pic.Image = img;
        form.Controls.Add(pic);

        // Background worker to set sharp icon on the calculator window as soon as it appears
        ThreadPool.QueueUserWorkItem((state) => {
            if (!File.Exists(icoPath)) return;

            int cxBig = Math.Max(32, GetSystemMetrics(SM_CXICON));
            int cyBig = Math.Max(32, GetSystemMetrics(SM_CYICON));
            int cxSmall = Math.Max(16, GetSystemMetrics(SM_CXSMICON));
            int cySmall = Math.Max(16, GetSystemMetrics(SM_CYSMICON));

            IntPtr hBig = LoadImage(IntPtr.Zero, icoPath, IMAGE_ICON, cxBig, cyBig, LR_LOADFROMFILE);
            IntPtr hSmall = LoadImage(IntPtr.Zero, icoPath, IMAGE_ICON, cxSmall, cySmall, LR_LOADFROMFILE);

            if (hBig == IntPtr.Zero) return;

            for (int i = 0; i < 40; i++) {
                Thread.Sleep(200);
                bool applied = false;
                EnumWindows((hWnd, lParam) => {
                    if (IsWindowVisible(hWnd)) {
                        StringBuilder sb = new StringBuilder(256);
                        GetWindowText(hWnd, sb, 256);
                        string title = sb.ToString();
                        if (title.Contains("HP71B") || title.Contains("Santa Barbara")) {
                            SendMessage(hWnd, WM_SETICON, (IntPtr)ICON_BIG, hBig);
                            SendMessage(hWnd, WM_SETICON, (IntPtr)ICON_SMALL, hSmall);
                            applied = true;
                            return false;
                        }
                    }
                    return true;
                }, IntPtr.Zero);

                if (applied) break;
            }
        });

        System.Windows.Forms.Timer timer = new System.Windows.Forms.Timer();
        timer.Interval = 1000;
        timer.Tick += (s, e) => {
            timer.Stop();
            form.Close();
        };

        form.Shown += (s, e) => {
            timer.Start();
            form.Activate();
        };

        Application.Run(form);
    }
}

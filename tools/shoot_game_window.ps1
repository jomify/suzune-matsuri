<#
    Launch the packaged game, screenshot only its window after it settles, then close it.

        powershell -ExecutionPolicy Bypass -File tools/shoot_game_window.ps1 -Exe Package\Windows\SuzuneMatsuri.exe -Out logs\vm\packaged_title.png

    SuzuneMatsuri.exe is a launcher: the window belongs to the SuzuneMatsuri-Win64-Shipping process it starts.
    Only a process of that name started after this script began is used, and the window draws itself into
    the bitmap (PrintWindow): whatever else is on screen, or covering it, is never captured.
#>
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [Parameter(Mandatory = $true)][string]$Out,
    [string]$GameProcess = "SuzuneMatsuri-Win64-Shipping",
    [int]$TimeoutSeconds = 120,
    [int]$SettleSeconds = 25,
    [string]$GameArgs = "-windowed -ResX=1600 -ResY=900"
)

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public class GameCap {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    public delegate bool EnumProc(IntPtr h, IntPtr p);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);

    /** The largest visible top-level window of process Pid (the game view, not a helper window). */
    public static IntPtr LargestWindowOf(uint Pid) {
        IntPtr best = IntPtr.Zero; long bestArea = 0;
        EnumWindows((h, p) => {
            uint owner; GetWindowThreadProcessId(h, out owner);
            RECT r;
            if (owner == Pid && IsWindowVisible(h) && GetWindowRect(h, out r)) {
                long area = (long)(r.Right - r.Left) * (r.Bottom - r.Top);
                if (area > bestArea) { bestArea = area; best = h; }
            }
            return true;
        }, IntPtr.Zero);
        return best;
    }
}
'@
[GameCap]::SetProcessDPIAware() | Out-Null

$started = Get-Date
$launcher = Start-Process -FilePath $Exe -ArgumentList $GameArgs -PassThru
$game = $null
$deadline = $started.AddSeconds($TimeoutSeconds)
while ((Get-Date) -lt $deadline) {
    $game = Get-Process -Name $GameProcess -ErrorAction SilentlyContinue |
        Where-Object { $_.StartTime -ge $started.AddSeconds(-1) -and $_.MainWindowHandle -ne [IntPtr]::Zero } |
        Select-Object -First 1
    if ($game) { break }
    Start-Sleep -Seconds 1
}
if (-not $game) {
    Write-Output "no game window within $TimeoutSeconds s"
    if (-not $launcher.HasExited) { Stop-Process -Id $launcher.Id -Force }
    exit 2
}
Write-Output "game pid $($game.Id), window '$($game.MainWindowTitle)'"
Start-Sleep -Seconds $SettleSeconds
$game.Refresh()
if ($game.HasExited) { Write-Output "the game exited during the settle time (code $($game.ExitCode))"; exit 3 }

$window = [GameCap]::LargestWindowOf([uint32]$game.Id)
if ($window -eq [IntPtr]::Zero) { Write-Output "no visible game window"; Stop-Process -Id $game.Id -Force; exit 5 }
$rect = New-Object GameCap+RECT
[GameCap]::GetWindowRect($window, [ref]$rect) | Out-Null
$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
$bitmap = New-Object System.Drawing.Bitmap $width, $height
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
# PW_RENDERFULLCONTENT (2): the window renders its own content (DirectX included), even when covered.
$hdc = $graphics.GetHdc()
$ok = [GameCap]::PrintWindow($window, $hdc, 2)
$graphics.ReleaseHdc($hdc)
if (-not $ok) { Write-Output "PrintWindow failed"; Stop-Process -Id $game.Id -Force; exit 4 }
New-Item -ItemType Directory -Force -Path (Split-Path $Out) | Out-Null
$bitmap.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
Write-Output "captured ${width}x${height} -> $Out"

Stop-Process -Id $game.Id -Force
if (-not $launcher.HasExited) { Stop-Process -Id $launcher.Id -Force }
exit 0

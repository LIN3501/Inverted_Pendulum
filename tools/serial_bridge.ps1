# serial_bridge.ps1 — 独占串口 ↔ 本地TCP 双向桥 + 日志
# 用法: powershell -ExecutionPolicy Bypass -File tools\serial_bridge.ps1
# 可选: -Port COM8 -Baud 115200 -TcpPort 9000
param(
    [string]$Port = "COM8",
    [int]$Baud = 115200,
    [int]$TcpPort = 9000
)

$ErrorActionPreference = "Stop"
$LogDir = Join-Path $PSScriptRoot "logs"
if (-not (Test-Path $LogDir)) { New-Item -ItemType Directory -Path $LogDir | Out-Null }
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$LogFile = Join-Path $LogDir "serial_$stamp.log"
$ErrFile = Join-Path $LogDir "bridge_error.log"

function Log-Err([string]$msg) {
    $line = "[{0}] {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $msg
    try { [System.IO.File]::AppendAllText($ErrFile, $line + "`r`n", [System.Text.UTF8Encoding]::new($false)) } catch {}
    Write-Host $line -ForegroundColor Red
}

# ---- 打开串口（独占），失败自动重试 ----
$sp = $null
function Open-Serial {
    $s = New-Object System.IO.Ports.SerialPort $Port, $Baud, "None", 8, "One"
    $s.ReadBufferSize = 65536
    $s.ReadTimeout = 200
    $s.DtrEnable = $false
    $s.RtsEnable = $false
    $s.Open()
    return $s
}
while ($null -eq $sp) {
    try { $sp = Open-Serial }
    catch {
        Log-Err "打开 $Port 失败: $($_.Exception.Message)；1秒后重试（请确认占用程序已关闭）"
        Start-Sleep -Seconds 1
    }
}

# ---- 本地TCP服务器 ----
$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $TcpPort)
$listener.Start()
$clients = New-Object System.Collections.ArrayList

# FileShare.ReadWrite：外部工具可同时读日志
$fs = [System.IO.FileStream]::new($LogFile, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::ReadWrite)
$logWriter = [System.IO.StreamWriter]::new($fs, [System.Text.Encoding]::UTF8)
$logWriter.AutoFlush = $true

function Remove-Client($idx) {
    $c = $clients[$idx]
    try { $c.Client.Close() } catch {}
    $clients.RemoveAt($idx)
}

Write-Host "======================================================" -ForegroundColor Green
Write-Host " 串口桥已启动（双向）"
Write-Host "   串口: $Port @ $Baud (独占)"
Write-Host "   日志: $LogFile"
Write-Host "   TCP:  127.0.0.1:$TcpPort  ← VOFA+ 数据接口选 TCP客户端"
Write-Host "   下行: TCP客户端数据 → 串口（握手/调参命令照常到单片机）"
Write-Host " 停止: 本窗口按 Ctrl+C ；错误: tools\logs\bridge_error.log" -ForegroundColor Yellow
Write-Host "======================================================" -ForegroundColor Green

$buf = New-Object byte[] 4096
$cbuf = New-Object byte[] 4096
$nextSerialReopen = [DateTime]::MinValue

while ($true) {
    try {
        # 1) 接受新TCP客户端
        while ($listener.Pending()) {
            $cli = $listener.AcceptTcpClient()
            $cli.NoDelay = $true
            [void]$clients.Add($cli)
            Write-Host ("[{0}] TCP接入: {1}" -f (Get-Date -Format "HH:mm:ss"), $cli.Client.RemoteEndPoint) -ForegroundColor Cyan
        }

        # 2) 串口 → 日志 + 广播TCP
        if ($sp.IsOpen -and $sp.BytesToRead -gt 0) {
            $toRead = [Math]::Min($buf.Length, $sp.BytesToRead)
            $n = $sp.Read($buf, 0, $toRead)
            if ($n -gt 0) {
                $ts = Get-Date -Format "HH:mm:ss.fff"
                $text = [System.Text.Encoding]::ASCII.GetString($buf, 0, $n)
                $logWriter.WriteLine("[$ts] " + $text.Replace("`r`n", "`n ").TrimEnd())
                for ($i = $clients.Count - 1; $i -ge 0; $i--) {
                    try { $clients[$i].GetStream().Write($buf, 0, $n) }
                    catch {
                        Write-Host ("[{0}] TCP客户端断开" -f (Get-Date -Format "HH:mm:ss")) -ForegroundColor DarkGray
                        Remove-Client $i
                    }
                }
            }
        }

        # 3) TCP → 串口（握手数据/调参命令下发给单片机）
        for ($i = 0; $i -lt $clients.Count; $i++) {
            try {
                $ns = $clients[$i].GetStream()
                while ($ns.DataAvailable) {
                    $cn = $ns.Read($cbuf, 0, $cbuf.Length)
                    if ($cn -gt 0 -and $sp.IsOpen) { $sp.Write($cbuf, 0, $cn) }
                }
            } catch {
                Write-Host ("[{0}] TCP客户端断开" -f (Get-Date -Format "HH:mm:ss")) -ForegroundColor DarkGray
                Remove-Client $i
                $i--
            }
        }

        Start-Sleep -Milliseconds 10
    }
    catch [System.IO.IOException] {
        # 串口设备消失/拔线：关闭后延时重开，进程不死
        Log-Err "串口IO异常: $($_.Exception.Message)"
        try { if ($sp) { $sp.Close() } } catch {}
        $sp = $null
        if ((Get-Date) -ge $nextSerialReopen) {
            $nextSerialReopen = (Get-Date).AddSeconds(2)
            try { $sp = Open-Serial; Write-Host "串口已重连" -ForegroundColor Green }
            catch { Log-Err "串口重连失败: $($_.Exception.Message)"; $nextSerialReopen = (Get-Date).AddSeconds(2) }
        } else {
            Start-Sleep -Milliseconds 500
            try { $sp = Open-Serial; Write-Host "串口已重连" -ForegroundColor Green } catch {}
        }
    }
    catch {
        # 其他异常：记录并继续，绝不无声退出
        Log-Err "循环异常: $($_.Exception.GetType().FullName): $($_.Exception.Message)"
        Start-Sleep -Milliseconds 200
    }
}

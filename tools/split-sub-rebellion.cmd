@echo off
rem Copies the Sub Rebellion disc image into two pieces under 400 MB each, in
rem Downloads\sub-rebellion-parts, so Claude can transfer them. The original
rem file in PCSX2's roms folder is only read, never changed.
powershell -NoProfile -ExecutionPolicy Bypass -Command "$in = Join-Path $env:USERPROFILE 'Documents\PCSX2\roms\Sub Rebellion (USA).bin'; $out = Join-Path $env:USERPROFILE 'Downloads\sub-rebellion-parts'; New-Item -ItemType Directory -Force -Path $out | Out-Null; $fs = [IO.File]::OpenRead($in); $buf = New-Object byte[] (300MB); $i = 0; while (($n = $fs.Read($buf, 0, $buf.Length)) -gt 0) { $o = [IO.File]::Create((Join-Path $out ('part' + $i))); $o.Write($buf, 0, $n); $o.Close(); $i++ }; $fs.Close(); Write-Host ('Done: ' + $i + ' pieces in ' + $out + '. You can close this window.')"
pause

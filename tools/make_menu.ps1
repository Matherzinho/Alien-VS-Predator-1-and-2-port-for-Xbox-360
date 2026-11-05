# Gera avp_menu.bin (1280x720 RGB bruto, 2.764.800 bytes) a partir de uma imagem, no Windows, sem Pillow.
#   powershell -ExecutionPolicy Bypass -File tools\make_menu.ps1 assets\menu_art.jpg build\avp_menu.bin
param([Parameter(Mandatory)][string]$In, [Parameter(Mandatory)][string]$Out)
Add-Type -AssemblyName System.Drawing
$W = 1280; $H = 720
$src = [System.Drawing.Image]::FromFile((Resolve-Path $In))
$bmp = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = 'HighQualityBicubic'
$g.DrawImage($src, 0, 0, $W, $H)
$g.Dispose(); $src.Dispose()
$rect = New-Object System.Drawing.Rectangle 0, 0, $W, $H
$data = $bmp.LockBits($rect, 'ReadOnly', $bmp.PixelFormat)
$row = New-Object byte[] ($data.Stride)
$buf = New-Object byte[] ($W * $H * 3)
for ($y = 0; $y -lt $H; $y++) {
	[System.Runtime.InteropServices.Marshal]::Copy([IntPtr]($data.Scan0.ToInt64() + $y * $data.Stride), $row, 0, $data.Stride)
	$o = $y * $W * 3
	for ($x = 0; $x -lt $W; $x++) {
		# GDI guarda BGR; o .bin e RGB.
		$buf[$o + $x * 3] = $row[$x * 3 + 2]
		$buf[$o + $x * 3 + 1] = $row[$x * 3 + 1]
		$buf[$o + $x * 3 + 2] = $row[$x * 3]
	}
}
$bmp.UnlockBits($data); $bmp.Dispose()
$dir = Split-Path -Parent ([System.IO.Path]::GetFullPath($Out))
New-Item -ItemType Directory -Force $dir | Out-Null
[System.IO.File]::WriteAllBytes([System.IO.Path]::GetFullPath($Out), $buf)
"$Out : $($buf.Length) bytes"

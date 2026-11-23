# アイコン画像を 240×240・4階調(Floyd-Steinberg)にし、PNGとスケッチ埋め込み用ヘッダを作る
# Pillow 不要版(Windows の System.Drawing を使う)
#
#   pwsh tools/make_icon.ps1 assets/icon_src.jpg
#
# 出力: papermono-proto/data/icon.png と papermono-proto/icon_png.h
param([Parameter(Mandatory = $true)][string]$Source)

Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent $PSScriptRoot
$sketch = Join-Path $root 'papermono-proto'
$size = 240

$src = [System.Drawing.Image]::FromFile((Resolve-Path $Source).ProviderPath)
try {
  # 中央を正方形に切り抜いて縮小
  $s = [Math]::Min($src.Width, $src.Height)
  $sx = [int](($src.Width - $s) / 2); $sy = [int](($src.Height - $s) / 2)
  $bmp = New-Object System.Drawing.Bitmap $size, $size
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $size, $size),
    (New-Object System.Drawing.Rectangle $sx, $sy, $s, $s), [System.Drawing.GraphicsUnit]::Pixel)
  $g.Dispose()
} finally { $src.Dispose() }

# グレースケール化
$lum = New-Object 'double[]' ($size * $size)
for ($y = 0; $y -lt $size; $y++) {
  for ($x = 0; $x -lt $size; $x++) {
    $c = $bmp.GetPixel($x, $y)
    $lum[$y * $size + $x] = 0.299 * $c.R + 0.587 * $c.G + 0.114 * $c.B
  }
}

# 4階調 (0, 85, 170, 255) へ Floyd-Steinberg ディザ
for ($y = 0; $y -lt $size; $y++) {
  for ($x = 0; $x -lt $size; $x++) {
    $i = $y * $size + $x
    $old = [Math]::Min(255.0, [Math]::Max(0.0, $lum[$i]))
    $new = [Math]::Round($old / 85.0) * 85
    $err = $old - $new
    $lum[$i] = $new
    if ($x + 1 -lt $size) { $lum[$i + 1] += $err * 7 / 16 }
    if ($y + 1 -lt $size) {
      if ($x -gt 0) { $lum[$i + $size - 1] += $err * 3 / 16 }
      $lum[$i + $size] += $err * 5 / 16
      if ($x + 1 -lt $size) { $lum[$i + $size + 1] += $err * 1 / 16 }
    }
    $v = [int]$new
    $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($v, $v, $v))
  }
}

$pngPath = Join-Path $sketch 'data/icon.png'
New-Item -ItemType Directory -Force (Split-Path $pngPath) | Out-Null
$bmp.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

$bytes = [System.IO.File]::ReadAllBytes($pngPath)
$sb = New-Object System.Text.StringBuilder
[void]$sb.Append("// tools/make_icon.ps1 で生成。手で編集しない`n#pragma once`n#include <cstdint>`n`n")
[void]$sb.Append("static const uint8_t kIconPng[$($bytes.Length)] = {`n")
for ($i = 0; $i -lt $bytes.Length; $i += 16) {
  $chunk = $bytes[$i..([Math]::Min($i + 15, $bytes.Length - 1))] | ForEach-Object { '0x{0:x2}' -f $_ }
  [void]$sb.Append('  ' + ($chunk -join ', ') + ",`n")
}
[void]$sb.Append("};`n")
$hPath = Join-Path $sketch 'icon_png.h'
[System.IO.File]::WriteAllText($hPath, $sb.ToString(), (New-Object System.Text.UTF8Encoding $false))
"$pngPath ($($bytes.Length) bytes) と icon_png.h を作成"

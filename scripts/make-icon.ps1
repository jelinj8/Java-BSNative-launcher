# Regenerates src/windows/launcher.ico: a white play symbol on a slate circle, deliberately not
# belonging to any app. Windows PowerShell only, as it uses System.Drawing.
Add-Type -AssemblyName System.Drawing
$sizes = 256, 64, 48, 32, 24, 16
$images = foreach ($size in $sizes) {
	$bitmap = New-Object Drawing.Bitmap $size, $size, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
	$graphics = [Drawing.Graphics]::FromImage($bitmap)
	$graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
	$graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
	$graphics.Clear([Drawing.Color]::Transparent)
	$inset = [Math]::Max(0.5, $size * 0.02)
	$circle = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(255, 71, 85, 105))
	$graphics.FillEllipse($circle, [single]$inset, [single]$inset, [single]($size - 2 * $inset), [single]($size - 2 * $inset))
	$points = [Drawing.PointF[]]@(
		(New-Object Drawing.PointF ([single]($size * 0.38)), ([single]($size * 0.27))),
		(New-Object Drawing.PointF ([single]($size * 0.38)), ([single]($size * 0.73))),
		(New-Object Drawing.PointF ([single]($size * 0.76)), ([single]($size * 0.50)))
	)
	$graphics.FillPolygon([Drawing.Brushes]::White, $points)
	$graphics.Dispose()
	$stream = New-Object IO.MemoryStream
	$bitmap.Save($stream, [Drawing.Imaging.ImageFormat]::Png)
	$bitmap.Dispose()
	, $stream.ToArray()
}

$path = Join-Path $PSScriptRoot '..\src\windows\launcher.ico'
$out = New-Object IO.BinaryWriter ([IO.File]::Create((Join-Path (Resolve-Path (Split-Path $path)) 'launcher.ico')))
$out.Write([uint16]0)
$out.Write([uint16]1)
$out.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
	$size = $sizes[$i]
	$out.Write([byte]($size % 256))
	$out.Write([byte]($size % 256))
	$out.Write([byte]0)
	$out.Write([byte]0)
	$out.Write([uint16]1)
	$out.Write([uint16]32)
	$out.Write([uint32]$images[$i].Length)
	$out.Write([uint32]$offset)
	$offset += $images[$i].Length
}
foreach ($image in $images) { $out.Write([byte[]]$image) }
$out.Close()

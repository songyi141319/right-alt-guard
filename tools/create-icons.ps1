$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$assets = Join-Path (Split-Path -Parent $PSScriptRoot) 'assets'
foreach ($name in @('guard','paused')) {
    $images = @()
    foreach ($size in @(16,32,48,256)) {
        $bitmap = [Drawing.Bitmap]::new($size,$size)
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.TextRenderingHint = [Drawing.Text.TextRenderingHint]::AntiAliasGridFit
        $color = if ($name -eq 'guard') { [Drawing.Color]::FromArgb(27,133,100) } else { [Drawing.Color]::FromArgb(111,118,125) }
        $brush = [Drawing.SolidBrush]::new($color)
        $graphics.Clear([Drawing.Color]::Transparent)
        $graphics.FillEllipse($brush,0,0,$size,$size)
        $font = [Drawing.Font]::new('Segoe UI',($size * 0.62),[Drawing.FontStyle]::Bold,[Drawing.GraphicsUnit]::Pixel)
        $format = [Drawing.StringFormat]::new()
        $format.Alignment = [Drawing.StringAlignment]::Center
        $format.LineAlignment = [Drawing.StringAlignment]::Center
        $graphics.DrawString('A',$font,[Drawing.Brushes]::White,[Drawing.RectangleF]::new(0,-$size*0.025,$size,$size),$format)
        $stream = [IO.MemoryStream]::new()
        $bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Png)
        $images += ,($stream.ToArray())
        $stream.Dispose(); $format.Dispose(); $font.Dispose(); $brush.Dispose(); $graphics.Dispose(); $bitmap.Dispose()
    }
    $file = [IO.File]::Create((Join-Path $assets ($name + '.ico')))
    $writer = [IO.BinaryWriter]::new($file)
    try {
        $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$images.Count)
        $offset = 6 + 16 * $images.Count
        $sizes = @(16,32,48,256)
        for ($i=0; $i -lt $images.Count; $i++) {
            $dimension = if ($sizes[$i] -eq 256) { 0 } else { $sizes[$i] }
            $writer.Write([byte]$dimension); $writer.Write([byte]$dimension)
            $writer.Write([byte]0); $writer.Write([byte]0); $writer.Write([uint16]1); $writer.Write([uint16]32)
            $writer.Write([uint32]$images[$i].Length); $writer.Write([uint32]$offset)
            $offset += $images[$i].Length
        }
        foreach ($bytes in $images) { $writer.Write([byte[]]$bytes) }
    } finally { $writer.Dispose(); $file.Dispose() }
}

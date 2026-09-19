
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
Add-Type -AssemblyName System.Drawing

$rawText = [System.IO.File]::ReadAllText("chars.txt", [System.Text.Encoding]::UTF8)
$chars = $rawText.ToCharArray() | Select-Object -Unique

$bmp = New-Object System.Drawing.Bitmap 16, 16
$g = [System.Drawing.Graphics]::FromImage($bmp)
$font = New-Object System.Drawing.Font("Microsoft JhengHei", 12, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$brush = [System.Drawing.Brushes]::White
$stringFormat = New-Object System.Drawing.StringFormat
$stringFormat.Alignment = [System.Drawing.StringAlignment]::Center
$stringFormat.LineAlignment = [System.Drawing.StringAlignment]::Center
$rect = New-Object System.Drawing.RectangleF 0, 0, 16, 16

$results = @()

foreach ($ch in $chars) {
    $g.Clear([System.Drawing.Color]::Black)
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit
    $g.DrawString($ch.ToString(), $font, $brush, $rect, $stringFormat)
    
    $bytes = @()
    for ($y = 0; $y -lt 16; $y++) {
        $row1 = 0
        $row2 = 0
        for ($x = 0; $x -lt 8; $x++) {
            if ($bmp.GetPixel($x, $y).R -gt 80) {
                $row1 = $row1 -bor (1 -shl (7 - $x))
            }
        }
        for ($x = 8; $x -lt 16; $x++) {
            if ($bmp.GetPixel($x, $y).R -gt 80) {
                $row2 = $row2 -bor (1 -shl (15 - $x))
            }
        }
        $bytes += ('0x{0:X2}, 0x{1:X2}' -f $row1, $row2)
    }
    $code = [int][char]$ch
    $hexCode = ('0x{0:X4}' -f $code)
    $byteStr = $bytes -join ', '
    
    $results += "  // '$ch' ($hexCode)`n  { $hexCode, {`n    $byteStr`n  }},"
}

$bmp.Dispose()
$g.Dispose()
$font.Dispose()

$finalOutput = $results -join "`n"
[System.IO.File]::WriteAllText("chinese_font_table.h", $finalOutput, [System.Text.Encoding]::UTF8)
Write-Output "Successfully generated Chinese font table with $($results.Count) glyphs!"

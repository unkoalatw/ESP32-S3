$htmlBytes = [System.IO.File]::ReadAllBytes("$PSScriptRoot\index.html")
$ms = New-Object System.IO.MemoryStream
$gzip = New-Object System.IO.Compression.GZipStream($ms, [System.IO.Compression.CompressionLevel]::Optimal)
$gzip.Write($htmlBytes, 0, $htmlBytes.Length)
$gzip.Close()
$gzBytes = $ms.ToArray()
$ms.Close()

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("// index_html.h - Auto-generated from project index.html, do not edit manually")
[void]$sb.AppendLine("#ifndef INDEX_HTML_H")
[void]$sb.AppendLine("#define INDEX_HTML_H")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("#include <pgmspace.h>")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("const uint32_t INDEX_HTML_GZ_LEN = $($gzBytes.Length);")
[void]$sb.AppendLine("const uint8_t INDEX_HTML_GZ[] PROGMEM = {")

for ($i = 0; $i -lt $gzBytes.Length; $i += 32) {
    $chunk = @()
    $end = [Math]::Min($i + 32, $gzBytes.Length)
    for ($j = $i; $j -lt $end; $j++) {
        $chunk += ("0x{0:x2}" -f $gzBytes[$j])
    }
    $line = "  " + ($chunk -join ",")
    if ($end -lt $gzBytes.Length) { $line += "," }
    [void]$sb.AppendLine($line)
}

[void]$sb.AppendLine("};")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("#endif // INDEX_HTML_H")

$utf8NoBom = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText("$PSScriptRoot\index_html.h", $sb.ToString(), $utf8NoBom)
Write-Output "Done! Generated index_html.h ($($gzBytes.Length) bytes gz, original $($htmlBytes.Length) bytes)"

$ErrorActionPreference = "Stop"

$baseDir = "C:\Users\USERNAME\mameDats"
$outputFile = "$baseDir\merged_mame.xml"

function Extract-GameData {
    param([string]$content, [string]$pattern)
    
    $games = @{}
    
    $regex = [regex]::new($pattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)
    $matches = $regex.Matches($content)
    
    foreach ($match in $matches) {
        $fullMatch = $match.Groups[0].Value
        
        $nameMatch = [regex]::Match($fullMatch, 'name="([^"]+)"')
        if (-not $nameMatch.Success) { continue }
        $name = $nameMatch.Groups[1].Value
        
        $isdeviceMatch = [regex]::Match($fullMatch, 'isdevice="yes"')
        if ($isdeviceMatch.Success) { continue }
        
        $cloneof = ""
        $cloneofMatch = [regex]::Match($fullMatch, 'cloneof="([^"]+)"')
        if ($cloneofMatch.Success) { $cloneof = $cloneofMatch.Groups[1].Value }
        
        $romof = ""
        $romofMatch = [regex]::Match($fullMatch, 'romof="([^"]+)"')
        if ($romofMatch.Success) { $romof = $romofMatch.Groups[1].Value }
        
        $description = ""
        $descMatch = [regex]::Match($fullMatch, '<description>([^<]+)</description>')
        if ($descMatch.Success) { $description = $descMatch.Groups[1].Value }
        
        $year = ""
        $yearMatch = [regex]::Match($fullMatch, '<year>([^<]+)</year>')
        if ($yearMatch.Success) { $year = $yearMatch.Groups[1].Value }
        
        $manufacturer = ""
        $manMatch = [regex]::Match($fullMatch, '<manufacturer>([^<]+)</manufacturer>')
        if ($manMatch.Success) { $manufacturer = $manMatch.Groups[1].Value }
        
        $games[$name] = @{
            name = $name
            cloneof = $cloneof
            romof = $romof
            description = $description
            year = $year
            manufacturer = $manufacturer
            fullXml = $fullMatch
        }
    }
    
    return $games
}

Write-Host "Reading files..."

$xmlContent = Get-Content "$baseDir\mame2003-plus.xml" -Raw -Encoding UTF8
$fbneoContent = Get-Content "$baseDir\fbneo.dat" -Raw -Encoding UTF8
$mameContent = Get-Content "$baseDir\MAME 0.284.dat" -Raw -Encoding UTF8

Write-Host "Processing mame2003-plus.xml..."
$pattern1 = '<game\s+name="[^"]+"[^>]*>.*?</game>'
$games1 = Extract-GameData -content $xmlContent -pattern $pattern1
Write-Host "  Found $($games1.Count) games"

Write-Host "Processing fbneo.dat..."
$pattern2 = '<game\s+name="[^"]+"[^>]*>.*?</game>'
$games2 = Extract-GameData -content $fbneoContent -pattern $pattern2
Write-Host "  Found $($games2.Count) games"

Write-Host "Processing MAME 0.284.dat..."
$pattern3 = '<machine\s+name="[^"]+"[^>]*>.*?</machine>'
$games3 = Extract-GameData -content $mameContent -pattern $pattern3
Write-Host "  Found $($games3.Count) machines"

Write-Host "Merging games..."
$mergedGames = @{}

foreach ($g in $games1.Values) {
    $mergedGames[$g.name] = $g
}

foreach ($g in $games2.Values) {
    if (-not $mergedGames.ContainsKey($g.name)) {
        $mergedGames[$g.name] = $g
    }
}

foreach ($g in $games3.Values) {
    if (-not $mergedGames.ContainsKey($g.name)) {
        $mergedGames[$g.name] = $g
    }
}

Write-Host "Total unique games: $($mergedGames.Count)"

Write-Host "Building XML output..."

$sb = New-Object System.Text.StringBuilder
$null = $sb.AppendLine('<?xml version="1.0"?>')
$null = $sb.AppendLine('<!DOCTYPE mame [')
$null = $sb.AppendLine('	<!ELEMENT mame (game+)>')
$null = $sb.AppendLine('		<!ELEMENT game (description, year?, manufacturer?)>')
$null = $sb.AppendLine('			<!ATTLIST game name CDATA #REQUIRED>')
$null = $sb.AppendLine('			<!ATTLIST game cloneof CDATA #IMPLIED>')
$null = $sb.AppendLine('			<!ATTLIST game romof CDATA #IMPLIED>')
$null = $sb.AppendLine('		<!ELEMENT description (#PCDATA)>')
$null = $sb.AppendLine('		<!ELEMENT year (#PCDATA)>')
$null = $sb.AppendLine('		<!ELEMENT manufacturer (#PCDATA)>')
$null = $sb.AppendLine(']>')
$null = $sb.AppendLine('<mame>')

$gameNames = $mergedGames.Keys | Sort-Object
foreach ($gameName in $gameNames) {
    $g = $mergedGames[$gameName]
    
    $attrs = "name=`"$($g.name)`""
    if ($g.cloneof) { $attrs += " cloneof=`"$($g.cloneof)`"" }
    if ($g.romof) { $attrs += " romof=`"$($g.romof)`"" }
    
    $null = $sb.AppendLine("	<game $attrs>")
    $null = $sb.AppendLine("		<description>$($g.description)</description>")
    if ($g.year) { $null = $sb.AppendLine("		<year>$($g.year)</year>") }
    if ($g.manufacturer) { $null = $sb.AppendLine("		<manufacturer>$($g.manufacturer)</manufacturer>") }
    $null = $sb.AppendLine("	</game>")
}

$null = $sb.AppendLine('</mame>')

$sb.ToString() | Out-File -FilePath $outputFile -Encoding UTF8 -NoNewline

Write-Host "Done! Output saved to: $outputFile"
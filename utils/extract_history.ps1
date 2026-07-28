$ErrorActionPreference = "Stop"

$baseDir = "C:\Users\USERNAME\mameDats"
$outputDir = "$baseDir\history_txt"

Write-Host "Creating output directory..."
if (Test-Path $outputDir) {
    Remove-Item -Path $outputDir -Recurse -Force
}
New-Item -ItemType Directory -Path $outputDir | Out-Null

Write-Host "Reading history.xml..."
$content = Get-Content "$baseDir\history.xml" -Raw -Encoding UTF8

Write-Host "Extracting entries..."
$entryPattern = '<entry>(.*?)</entry>'
$regex = [regex]::new($entryPattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)
$entries = $regex.Matches($content)

$fileCount = 0
$dupCount = 0

foreach ($entry in $entries) {
    $entryXml = $entry.Groups[1].Value
    
    $hasSystems = [regex]::Match($entryXml, '<systems>').Success
    if (-not $hasSystems) { continue }
    
    $itemNames = @()
    
    $systemRegex = [regex]::new('<system\s+([^>]+)\/?>', [System.Text.RegularExpressions.RegexOptions]::Singleline)
    $systemMatches = $systemRegex.Matches($entryXml)
    foreach ($m in $systemMatches) {
        $attrs = $m.Groups[1].Value
        
        $nameMatch = [regex]::Match($attrs, 'name="([^"]+)"')
        $gameMatch = [regex]::Match($attrs, 'game="yes"')
        
        if ($nameMatch.Success -and $gameMatch.Success) {
            $itemNames += $nameMatch.Groups[1].Value
        }
    }
    
    if ($itemNames.Count -eq 0) { continue }
    
    $textMatch = [regex]::Match($entryXml, '<text>(.*?)</text>', [System.Text.RegularExpressions.RegexOptions]::Singleline)
    if (-not $textMatch.Success) { continue }
    $text = $textMatch.Groups[1].Value
    
    foreach ($name in $itemNames) {
        $filename = "$name.txt"
        $filepath = Join-Path $outputDir $filename
        
        if (Test-Path $filepath) {
            $dupCount++
        }
        
        $text | Out-File -FilePath $filepath -Encoding UTF8 -NoNewline
        $fileCount++
    }
}

Write-Host "Created $fileCount files ($dupCount duplicates skipped)"
Write-Host "Output directory: $outputDir"
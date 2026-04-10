param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDir,

    [Parameter(Mandatory = $true)]
    [string]$OutputFile
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$resolvedSourceDir = (Resolve-Path -LiteralPath $SourceDir).Path
$resolvedOutputFile = [System.IO.Path]::GetFullPath($OutputFile)
$outputDirectory = Split-Path -Parent $resolvedOutputFile

if (-not (Test-Path -LiteralPath $outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

function Escape-Xml {
    param([string]$Value)

    return [System.Security.SecurityElement]::Escape($Value)
}

function Get-RelativePath {
    param(
        [string]$BasePath,
        [string]$TargetPath
    )

    $baseUri = [System.Uri]::new(([System.IO.Path]::GetFullPath($BasePath.TrimEnd("\", "/")) + [System.IO.Path]::DirectorySeparatorChar))
    $targetUri = [System.Uri]::new([System.IO.Path]::GetFullPath($TargetPath))
    $relativeUri = $baseUri.MakeRelativeUri($targetUri)

    return [System.Uri]::UnescapeDataString($relativeUri.ToString()).Replace("\", "/")
}

function New-StableId {
    param(
        [string]$Prefix,
        [string]$RelativePath
    )

    $normalized = $RelativePath.Replace("\", "/").ToLowerInvariant()
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hashBytes = $sha256.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($normalized))
    }
    finally {
        $sha256.Dispose()
    }
    $hash = ([System.BitConverter]::ToString($hashBytes)).Replace("-", "").Substring(0, 12)

    $sanitized = ($RelativePath -replace "[^A-Za-z0-9]", "_").Trim("_")
    if ([string]::IsNullOrWhiteSpace($sanitized)) {
        $sanitized = "root"
    }
    if ($sanitized[0] -match "[0-9]") {
        $sanitized = "_$sanitized"
    }

    $maxSanitizedLength = [Math]::Max(1, 70 - $Prefix.Length - $hash.Length)
    if ($sanitized.Length -gt $maxSanitizedLength) {
        $sanitized = $sanitized.Substring(0, $maxSanitizedLength)
    }

    return "${Prefix}_${sanitized}_${hash}"
}

$files = Get-ChildItem -LiteralPath $resolvedSourceDir -File -Recurse | Sort-Object FullName
if (-not $files) {
    throw "No files found in '$resolvedSourceDir'."
}

$directories = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
$directories.Add(".") | Out-Null
$filesByDirectory = @{}
$componentIds = [System.Collections.Generic.List[string]]::new()

foreach ($file in $files) {
    $relativePath = Get-RelativePath -BasePath $resolvedSourceDir -TargetPath $file.FullName
    $relativeDirectory = [System.IO.Path]::GetDirectoryName($relativePath)
    if ([string]::IsNullOrEmpty($relativeDirectory)) {
        $relativeDirectory = "."
    } else {
        $relativeDirectory = $relativeDirectory.Replace("\", "/")
    }

    if (-not $filesByDirectory.ContainsKey($relativeDirectory)) {
        $filesByDirectory[$relativeDirectory] = [System.Collections.Generic.List[object]]::new()
    }

    $filesByDirectory[$relativeDirectory].Add([pscustomobject]@{
            Name = $file.Name
            FullPath = $file.FullName
            RelativePath = $relativePath
        })

    if ($relativeDirectory -ne ".") {
        $segments = $relativeDirectory.Split("/")
        for ($index = 0; $index -lt $segments.Length; $index++) {
            $pathSegment = ($segments[0..$index] -join "/")
            $directories.Add($pathSegment) | Out-Null
        }
    }
}

$childDirectories = @{}
foreach ($directory in $directories) {
    if (-not $childDirectories.ContainsKey($directory)) {
        $childDirectories[$directory] = [System.Collections.Generic.List[string]]::new()
    }
}

foreach ($directory in $directories) {
    if ($directory -eq ".") {
        continue
    }

    $parent = [System.IO.Path]::GetDirectoryName($directory.Replace("/", [System.IO.Path]::DirectorySeparatorChar))
    if ([string]::IsNullOrEmpty($parent)) {
        $parent = "."
    } else {
        $parent = $parent.Replace("\", "/")
    }

    $childDirectories[$parent].Add($directory)
}

$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$writer = [System.IO.StreamWriter]::new($resolvedOutputFile, $false, $utf8NoBom)

function Write-IndentedLine {
    param(
        [System.IO.StreamWriter]$StreamWriter,
        [int]$IndentLevel,
        [string]$Text
    )

    $StreamWriter.WriteLine(("{0}{1}" -f (" " * $IndentLevel), $Text))
}

function Write-DirectoryContent {
    param(
        [System.IO.StreamWriter]$StreamWriter,
        [string]$DirectoryKey,
        [int]$IndentLevel
    )

    if ($filesByDirectory.ContainsKey($DirectoryKey)) {
        foreach ($entry in ($filesByDirectory[$DirectoryKey] | Sort-Object RelativePath)) {
            $componentId = New-StableId -Prefix "cmp" -RelativePath $entry.RelativePath
            $fileId = New-StableId -Prefix "fil" -RelativePath $entry.RelativePath
            $componentIds.Add($componentId) | Out-Null

            Write-IndentedLine -StreamWriter $StreamWriter -IndentLevel $IndentLevel -Text "<Component Id=""$componentId"" Guid=""*"">"
            Write-IndentedLine -StreamWriter $StreamWriter -IndentLevel ($IndentLevel + 2) -Text "<File Id=""$fileId"" Source=""$(Escape-Xml $entry.FullPath)"" KeyPath=""yes"" />"
            Write-IndentedLine -StreamWriter $StreamWriter -IndentLevel $IndentLevel -Text "</Component>"
        }
    }

    foreach ($childDirectory in ($childDirectories[$DirectoryKey] | Sort-Object)) {
        $directoryId = New-StableId -Prefix "dir" -RelativePath $childDirectory
        $directoryName = Split-Path -Path $childDirectory -Leaf

        Write-IndentedLine -StreamWriter $StreamWriter -IndentLevel $IndentLevel -Text "<Directory Id=""$directoryId"" Name=""$(Escape-Xml $directoryName)"">"
        Write-DirectoryContent -StreamWriter $StreamWriter -DirectoryKey $childDirectory -IndentLevel ($IndentLevel + 2)
        Write-IndentedLine -StreamWriter $StreamWriter -IndentLevel $IndentLevel -Text "</Directory>"
    }
}

try {
    Write-IndentedLine -StreamWriter $writer -IndentLevel 0 -Text "<Wix xmlns=""http://wixtoolset.org/schemas/v4/wxs"">"
    Write-IndentedLine -StreamWriter $writer -IndentLevel 2 -Text "<Fragment>"
    Write-IndentedLine -StreamWriter $writer -IndentLevel 4 -Text "<DirectoryRef Id=""INSTALLFOLDER"">"
    Write-DirectoryContent -StreamWriter $writer -DirectoryKey "." -IndentLevel 6
    Write-IndentedLine -StreamWriter $writer -IndentLevel 4 -Text "</DirectoryRef>"
    Write-IndentedLine -StreamWriter $writer -IndentLevel 2 -Text "</Fragment>"
    Write-IndentedLine -StreamWriter $writer -IndentLevel 2 -Text "<Fragment>"
    Write-IndentedLine -StreamWriter $writer -IndentLevel 4 -Text "<ComponentGroup Id=""ApplicationFiles"">"

    foreach ($componentId in ($componentIds | Sort-Object)) {
        Write-IndentedLine -StreamWriter $writer -IndentLevel 6 -Text "<ComponentRef Id=""$componentId"" />"
    }

    Write-IndentedLine -StreamWriter $writer -IndentLevel 4 -Text "</ComponentGroup>"
    Write-IndentedLine -StreamWriter $writer -IndentLevel 2 -Text "</Fragment>"
    Write-IndentedLine -StreamWriter $writer -IndentLevel 0 -Text "</Wix>"
}
finally {
    $writer.Dispose()
}

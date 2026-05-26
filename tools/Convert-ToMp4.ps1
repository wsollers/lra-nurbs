[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateNotNullOrEmpty()]
    [string] $InputPath,

    [Parameter(Position = 1)]
    [ValidateNotNullOrEmpty()]
    [string] $OutputPath,

    [ValidateRange(1, 240)]
    [int] $FrameRate = 60,

    [ValidateRange(1, 86400)]
    [int] $StillDurationSeconds = 5,

    [string] $FfmpegPath = "ffmpeg",

    [switch] $Overwrite,

    [switch] $OpenWhenDone
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-InputPath {
    param([string] $Path)

    $resolved = Resolve-Path -LiteralPath $Path -ErrorAction Stop
    return $resolved.ProviderPath
}

function Get-DefaultOutputPath {
    param([string] $ResolvedInputPath)

    $item = Get-Item -LiteralPath $ResolvedInputPath
    if ($item.PSIsContainer) {
        return Join-Path $item.FullName ($item.Name + ".mp4")
    }

    return [System.IO.Path]::ChangeExtension($item.FullName, ".mp4")
}

function Get-ImageSequencePattern {
    param([string] $Directory)

    $preferredPatterns = @(
        "frame_%06d.png",
        "frame_%05d.png",
        "frame_%04d.png",
        "*.png",
        "*.jpg",
        "*.jpeg"
    )

    foreach ($pattern in $preferredPatterns) {
        if ($pattern.Contains("%")) {
            $prefix = $pattern.Substring(0, $pattern.IndexOf("%"))
            $suffix = $pattern.Substring($pattern.LastIndexOf("d") + 1)
            $candidate = Get-ChildItem -LiteralPath $Directory -File |
                Where-Object { $_.Name.StartsWith($prefix) -and $_.Name.EndsWith($suffix) } |
                Select-Object -First 1
            if ($null -ne $candidate) {
                return $pattern
            }
            continue
        }

        $candidate = Get-ChildItem -LiteralPath $Directory -File -Filter $pattern |
            Select-Object -First 1
        if ($null -ne $candidate) {
            return $pattern
        }
    }

    throw "No image sequence was found in '$Directory'. Expected frame_000001.png or PNG/JPG files."
}

function Invoke-Ffmpeg {
    param(
        [string] $Exe,
        [string[]] $Arguments
    )

    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "ffmpeg failed with exit code $LASTEXITCODE."
    }
}

$resolvedInput = Resolve-InputPath $InputPath
$inputItem = Get-Item -LiteralPath $resolvedInput

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Get-DefaultOutputPath $resolvedInput
}

$resolvedOutput = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath)
$outputDirectory = Split-Path -Parent $resolvedOutput
if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

if ((Test-Path -LiteralPath $resolvedOutput) -and -not $Overwrite) {
    throw "Output file already exists: '$resolvedOutput'. Use -Overwrite to replace it."
}

$overwriteArg = if ($Overwrite) { "-y" } else { "-n" }
$codecArgs = @(
    "-c:v", "libx264",
    "-pix_fmt", "yuv420p",
    "-movflags", "+faststart"
)

if ($inputItem.PSIsContainer) {
    $pattern = Get-ImageSequencePattern $inputItem.FullName

    if ($pattern.Contains("%")) {
        $sequencePath = Join-Path $inputItem.FullName $pattern
        $args = @(
            $overwriteArg,
            "-framerate", "$FrameRate",
            "-i", $sequencePath
        ) + $codecArgs + @($resolvedOutput)
    } else {
        $globPath = Join-Path $inputItem.FullName $pattern
        $args = @(
            $overwriteArg,
            "-framerate", "$FrameRate",
            "-pattern_type", "glob",
            "-i", $globPath
        ) + $codecArgs + @($resolvedOutput)
    }

    Invoke-Ffmpeg -Exe $FfmpegPath -Arguments $args
} else {
    $extension = [System.IO.Path]::GetExtension($inputItem.FullName).ToLowerInvariant()
    $imageExtensions = @(".png", ".jpg", ".jpeg", ".bmp", ".tga", ".tif", ".tiff")

    if ($imageExtensions -contains $extension) {
        $args = @(
            $overwriteArg,
            "-loop", "1",
            "-t", "$StillDurationSeconds",
            "-i", $inputItem.FullName,
            "-vf", "fps=$FrameRate"
        ) + $codecArgs + @($resolvedOutput)
    } else {
        $args = @(
            $overwriteArg,
            "-i", $inputItem.FullName
        ) + $codecArgs + @($resolvedOutput)
    }

    Invoke-Ffmpeg -Exe $FfmpegPath -Arguments $args
}

Write-Host "Wrote MP4: $resolvedOutput"

if ($OpenWhenDone) {
    Invoke-Item -LiteralPath $resolvedOutput
}

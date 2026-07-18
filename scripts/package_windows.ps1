param(
    [string]$QtDir = "",
    [string]$BuildDir = "build",
    [string]$DistDir = "dist/LogLens",
    [switch]$SkipTests,
    [switch]$Zip
)

$ErrorActionPreference = "Stop"

function Resolve-QtDir {
    param([string]$RequestedQtDir, [string]$BuildDir)

    if ($RequestedQtDir) {
        $candidate = (Resolve-Path -LiteralPath $RequestedQtDir).Path
        if (Test-Path -LiteralPath (Join-Path $candidate "bin/windeployqt.exe")) {
            return $candidate
        }
        throw "QtDir does not contain bin/windeployqt.exe: $candidate"
    }

    $cache = Join-Path $BuildDir "CMakeCache.txt"
    if (Test-Path -LiteralPath $cache) {
        $prefixLine = Get-Content -LiteralPath $cache |
            Where-Object { $_ -match "^CMAKE_PREFIX_PATH(:[^=]+)?=" } |
            Select-Object -First 1
        if ($prefixLine) {
            $prefix = ($prefixLine -split "=", 2)[1].Trim()
            if ($prefix -and
                (Test-Path -LiteralPath (Join-Path $prefix "bin/windeployqt.exe"))) {
                return (Resolve-Path -LiteralPath $prefix).Path
            }
        }
    }

    $cmd = Get-Command "windeployqt.exe" -ErrorAction SilentlyContinue
    if ($cmd) {
        return (Resolve-Path -LiteralPath (Join-Path $cmd.Source "../..")).Path
    }

    throw "QtDir was not provided and windeployqt.exe was not found. Pass -QtDir C:/Qt/6.x/msvc2022_64."
}

function Copy-IfExists {
    param([string]$Source, [string]$Destination)

    if (Test-Path -LiteralPath $Source) {
        Copy-Item -LiteralPath $Source -Destination $Destination -Force
    }
}

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
Set-Location -LiteralPath $root

$qt = Resolve-QtDir -RequestedQtDir $QtDir -BuildDir $BuildDir
$windeployqt = Join-Path $qt "bin/windeployqt.exe"

Write-Host "Using Qt: $qt"
Write-Host "Build dir: $BuildDir"
Write-Host "Dist dir: $DistDir"

if (!(Test-Path -LiteralPath (Join-Path $BuildDir "CMakeCache.txt"))) {
    cmake -S . -B $BuildDir -G "Visual Studio 17 2022" -A x64 `
        -DCMAKE_PREFIX_PATH="$qt"
}

cmake --build $BuildDir --config Release

if (!$SkipTests) {
    cmake --build $BuildDir --config Debug
    ctest --test-dir $BuildDir -C Debug --output-on-failure
}

$exe = Join-Path $BuildDir "Release/LogLens.exe"
if (!(Test-Path -LiteralPath $exe)) {
    throw "Release executable was not found: $exe"
}

if (Test-Path -LiteralPath $DistDir) {
    Remove-Item -LiteralPath $DistDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null

Copy-Item -LiteralPath $exe -Destination $DistDir -Force
Copy-IfExists -Source "README.md" -Destination $DistDir
Copy-IfExists -Source "LICENSE" -Destination $DistDir

& $windeployqt --release --compiler-runtime --no-translations `
    (Join-Path $DistDir "LogLens.exe")

if ($Zip) {
    $zipPath = "$DistDir.zip"
    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }
    Compress-Archive -Path (Join-Path $DistDir "*") -DestinationPath $zipPath
    Write-Host "Created $zipPath"
}

Write-Host "Packaged LogLens at $DistDir"

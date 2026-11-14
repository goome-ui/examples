param([string]$What = "all")
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$failed = 0
$csharpDir = if ($env:GoomeSharpDir) { $env:GoomeSharpDir } else { Join-Path $root "..\csharp-goome" }

function Want($name) { return ($What -eq "all") -or ($What -eq $name) }

if (Want "c") {
    $cc = if ($env:CC) { $env:CC.Trim() } else { $null }
    if (-not $cc) {
        foreach ($name in "gcc", "clang", "cl") {
            if (Get-Command $name -ErrorAction SilentlyContinue) { $cc = $name; break }
        }
    }
    if (-not $cc) { throw "no C compiler here, install mingw-w64 or Visual Studio" }
    New-Item -ItemType Directory -Force build\c | Out-Null
    Write-Host "goome.h $(Get-Content vendor\VERSION)"
    if ($cc -eq "cl") {
        & cl /nologo /O2 /W3 /Ivendor /c vendor\goome.c /Fobuild\c\goome.obj
        if ($LASTEXITCODE -ne 0) { throw "goome.c didnt build" }
    } else {
        & $cc -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Ivendor -c vendor\goome.c -o build\c\goome.o
        if ($LASTEXITCODE -ne 0) { throw "goome.c didnt build" }
    }
    foreach ($d in Get-ChildItem c -Directory) {
        $name = $d.Name
        if ($cc -eq "cl") {
            & cl /nologo /O2 /W3 /Ivendor "c\$name\main.c" build\c\goome.obj "/Fobuild\c\$name.obj" "/Febuild\c\$name.exe" | Out-Null
        } else {
            & $cc -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Ivendor -o "build\c\$name.exe" "c\$name\main.c" build\c\goome.o
        }
        if ($LASTEXITCODE -eq 0) { Write-Host "built c/$name" } else { Write-Host "FAILED c/$name"; $failed++ }
    }
}

if ((Want "csharp") -or (Want "unity")) {
    if (-not (Get-Command dotnet -ErrorAction SilentlyContinue)) {
        Write-Host "no dotnet here, skipping c# and unity"
    } elseif (-not (Test-Path (Join-Path $csharpDir "src\GoomeSharp\GoomeSharp.csproj"))) {
        Write-Host "cant find csharp-goome at $csharpDir, clone it next to this repo or set GoomeSharpDir"
        $failed++
    } else {
        $env:GoomeSharpDir = (Resolve-Path $csharpDir).Path
        if (-not (Test-Path (Join-Path $csharpDir "runtimes\win-x64\native\goome.dll"))) {
            Push-Location $csharpDir
            & bash ./build.sh
            Pop-Location
        }
        $projects = @()
        if (Want "csharp") { $projects += Get-ChildItem csharp\*\*.csproj }
        if (Want "unity") { $projects += Get-ChildItem unity\preview\*.csproj, unity\check\*.csproj }
        foreach ($p in $projects) {
            $dir = Resolve-Path -Relative $p.DirectoryName
            $dir = $dir.TrimStart(".", "\").Replace("\", "/")
            & dotnet build $p.FullName -c Release -o "build\$dir" --nologo -v q --disable-build-servers
            if ($LASTEXITCODE -eq 0) { Write-Host "built $dir" } else { Write-Host "FAILED $dir"; $failed++ }
        }
    }
}

if (Want "python") {
    $py = if ($env:PYTHON) { $env:PYTHON } else { "python" }
    if (-not (Get-Command $py -ErrorAction SilentlyContinue)) {
        Write-Host "no python here, skipping it"
    } else {
        & $py -c "import goome" 2>$null
        if ($LASTEXITCODE -ne 0) {
            Write-Host "python cant import goome, run: pip install git+https://github.com/goome-ui/python-goome@dev"
            $failed++
        } else {
            foreach ($f in Get-ChildItem python\*\*.py) {
                & $py -m py_compile $f.FullName
                if ($LASTEXITCODE -eq 0) { Write-Host "checked $($f.Name)" } else { Write-Host "FAILED $($f.Name)"; $failed++ }
            }
        }
    }
}

if ($failed -gt 0) { Write-Host "$failed failed"; exit 1 }
Write-Host "all built"

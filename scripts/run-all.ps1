param([string]$What = "all")
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$py = if ($env:PYTHON) { $env:PYTHON } else { "python" }
$passed = 0
$failed = @()

function Want($name) { return ($What -eq "all") -or ($What -eq $name) }

function Run($id, $dir, $exe, [string[]]$argv) {
    $out = Join-Path $root "build\run\$id"
    if (Test-Path $out) { Remove-Item -Recurse -Force $out }
    New-Item -ItemType Directory -Force $out | Out-Null
    $test = Join-Path $root "$dir\test.txt"
    Push-Location $out
    $backend = $env:GOOME_BACKEND
    if (Test-Path $test) {
        $env:GOOME_TEST_SCRIPT = $test
        $env:GOOME_TEST_SHOTS = "."
    } else {
        Remove-Item Env:GOOME_BACKEND -ErrorAction SilentlyContinue
    }
    & $exe @argv *> log.txt
    $rc = $LASTEXITCODE
    Remove-Item Env:GOOME_TEST_SCRIPT, Env:GOOME_TEST_SHOTS -ErrorAction SilentlyContinue
    if ($backend) { $env:GOOME_BACKEND = $backend }
    Pop-Location
    $pics = @(Get-ChildItem $out -File | Where-Object { $_.Extension -in ".ppm", ".png" }).Count
    if ($rc -eq 0 -and $pics -gt 0) {
        Write-Host "ok     $id ($pics pictures)"
        $script:passed++
    } else {
        Write-Host "FAILED $id (exit $rc, $pics pictures)"
        Get-Content (Join-Path $out "log.txt") -Tail 20 | ForEach-Object { Write-Host "       $_" }
        $script:failed += $id
    }
}

if (Want "c") {
    foreach ($d in Get-ChildItem c -Directory) {
        $exe = Join-Path $root "build\c\$($d.Name).exe"
        if (Test-Path $exe) { Run "c/$($d.Name)" "c\$($d.Name)" $exe @() }
    }
}
if (Want "csharp") {
    foreach ($d in Get-ChildItem csharp -Directory) {
        $dll = Join-Path $root "build\csharp\$($d.Name)\$($d.Name).dll"
        if (Test-Path $dll) { Run "csharp/$($d.Name)" "csharp\$($d.Name)" "dotnet" @($dll) }
    }
}
if ((Want "unity") -and (Test-Path build\unity\preview\Preview.dll)) {
    Run "unity" "unity\preview" "dotnet" @((Join-Path $root "build\unity\preview\Preview.dll"))
}
if (Want "python") {
    & $py -c "import goome" 2>$null
    if ($LASTEXITCODE -eq 0) {
        foreach ($d in Get-ChildItem python -Directory) {
            Run "python/$($d.Name)" "python\$($d.Name)" $py @((Join-Path $root "python\$($d.Name)\$($d.Name).py"))
        }
    }
}

Write-Host "$passed passed, $($failed.Count) failed"
if ($failed.Count -gt 0) { exit 1 }

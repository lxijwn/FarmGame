param([ValidateSet('Release','Debug')][string]$Configuration='Release',[switch]$SkipBuild,[string]$Python='')
$ErrorActionPreference='Stop'
$ProjectRoot=Split-Path -Parent $PSScriptRoot
if(-not $Python) {
    $BundledPython=Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    if(Test-Path -LiteralPath $BundledPython) { $Python=$BundledPython }
    else { $Python=(Get-Command python -ErrorAction Stop).Source }
}
$RunId=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$RunRoot=Join-Path $ProjectRoot "Validation\runs\$RunId"
New-Item -ItemType Directory -Force -Path $RunRoot | Out-Null
$Summary=[ordered]@{schemaVersion=1;status='running';runId=$RunId;configuration=$Configuration;startedAt=(Get-Date).ToString('o');checks=@();runDirectory=$RunRoot;error=''}
function Save-Summary {
    $Json=$Summary | ConvertTo-Json -Depth 12
    [IO.File]::WriteAllText((Join-Path $RunRoot 'verification-report.json'),$Json,[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $ProjectRoot 'Validation\latest-verification.json'),$Json,[Text.UTF8Encoding]::new($false))
}
function Invoke-Python([string]$Script,[string[]]$Arguments=@()) {
    $ErrorActionPreference='Continue'
    & $Python (Join-Path $ProjectRoot "Tools\$Script") @Arguments 2>&1 |
        Tee-Object -FilePath (Join-Path $RunRoot "$Script.log")
    $Code=$LASTEXITCODE
    if($Code -ne 0) { throw "Python check failed: $Script (exit $Code)" }
}
function Invoke-Runtime([string]$ResourceRoot,[string]$ReportRoot) {
    New-Item -ItemType Directory -Force -Path $ReportRoot | Out-Null
    $Arguments=@('--smoke','--resources',('"'+$ResourceRoot+'"'),'--report-dir',('"'+$ReportRoot+'"'))
    $Process=New-Object System.Diagnostics.Process
    $Process.StartInfo.FileName=$Executable
    $Process.StartInfo.Arguments=($Arguments -join ' ')
    $Process.StartInfo.WorkingDirectory=$RunRoot
    $Process.StartInfo.UseShellExecute=$false
    $Process.StartInfo.WindowStyle=[Diagnostics.ProcessWindowStyle]::Hidden
    if(-not $Process.Start()) { throw 'Could not start runtime process' }
    if(-not $Process.WaitForExit(45000)) { $Process.Kill(); throw 'Runtime process exceeded 45 seconds' }
    $Process.Refresh()
    $Code=$Process.ExitCode
    if($null -eq $Code) { throw 'Runtime exit code unavailable' }
    $Report=Join-Path $ReportRoot 'runtime-report.json'
    if(-not (Test-Path -LiteralPath $Report)) { throw "Runtime did not produce a report (exit $Code)" }
    return [pscustomobject]@{exitCode=$Code;report=(Get-Content -Raw -Encoding UTF8 -LiteralPath $Report | ConvertFrom-Json)}
}
try {
    Save-Summary
    Invoke-Python 'validate_levels.py'
    Invoke-Python 'validate_assets.py'
    Copy-Item -LiteralPath (Join-Path $ProjectRoot 'Validation\levels_report.json') -Destination $RunRoot
    Copy-Item -LiteralPath (Join-Path $ProjectRoot 'Validation\assets_report.json') -Destination $RunRoot
    $Summary.checks+=@{name='data_and_assets';status='passed'}
    if(-not $SkipBuild) {
        & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration
        $Summary.checks+=@{name='cmake_build';status='passed'}
    } else { $Summary.checks+=@{name='cmake_build';status='skipped_existing_executable'} }
    $Executable=Join-Path $ProjectRoot "build-$Configuration\bin\FarmGame.exe"
    if(-not (Test-Path -LiteralPath $Executable)) { throw 'Executable missing; build first' }
    $SourceResources=Join-Path $ProjectRoot 'Resources'
    $BuiltResources=Join-Path (Split-Path -Parent $Executable) 'Resources'
    $FileCount=0
    foreach($File in Get-ChildItem -LiteralPath $SourceResources -Recurse -File) {
        $Relative=$File.FullName.Substring($SourceResources.Length+1)
        $Built=Join-Path $BuiltResources $Relative
        if(-not (Test-Path -LiteralPath $Built)) { throw "Packaged resource missing: $Relative" }
        if((Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $Built -Algorithm SHA256).Hash) {
            throw "Packaged resource stale: $Relative. Rebuild to synchronize resources."
        }
        ++$FileCount
    }
    $Summary.checks+=@{name='packaged_resources_match_source';status='passed';files=$FileCount}
    $Summary.executableSha256=(Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash
    $RuntimeRoot=Join-Path $RunRoot 'runtime'
    $Positive=Invoke-Runtime $BuiltResources $RuntimeRoot
    if($Positive.exitCode -ne 0 -or $Positive.report.status -ne 'passed') { throw "Runtime smoke failed: $($Positive.report.error) (exit $($Positive.exitCode))" }
    Invoke-Python 'validate_runtime.py' @($RuntimeRoot)
    $Summary.checks+=@{name='runtime_scene_and_atlas_smoke';status='passed'}
    $FixtureRoot=Join-Path $RunRoot 'fixtures'
    Invoke-Python 'make_negative_fixtures.py' @($BuiltResources,$FixtureRoot)
    $Errors=@{missing_resource='Missing resource:';malformed_json='Invalid JSON:';missing_frame='Atlas frame missing:'}
    foreach($Case in @('missing_resource','malformed_json','missing_frame')) {
        $Negative=Invoke-Runtime (Join-Path $FixtureRoot $Case) (Join-Path $RunRoot "negative-$Case")
        if($Negative.exitCode -eq 0 -or $Negative.report.status -ne 'failed' -or -not $Negative.report.error.Contains($Errors[$Case])) {
            throw "Negative case did not fail as expected: $Case (exit $($Negative.exitCode), $($Negative.report.error))"
        }
        $Summary.checks+=@{name="reject_$Case";status='passed';runtimeExitCode=$Negative.exitCode;error=$Negative.report.error}
    }
    $Summary.status='passed'
    $Summary.finishedAt=(Get-Date).ToString('o')
    Save-Summary
    Write-Host "Verification PASSED. Evidence: $RunRoot"
} catch {
    $Summary.status='failed'; $Summary.error=$_.Exception.Message
    Save-Summary
    throw
}

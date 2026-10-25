param([ValidateSet('Release','Debug')][string]$Configuration='Release',[string]$Level='')
$ErrorActionPreference='Stop'
$ProjectRoot=Split-Path -Parent $PSScriptRoot
$Executable=Join-Path $ProjectRoot "build-$Configuration\bin\FarmGame.exe"
if(-not (Test-Path -LiteralPath $Executable)) { throw 'Executable missing. Run build.ps1 first.' }
$ReportRoot=Join-Path $ProjectRoot ('Validation\manual\'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Force -Path $ReportRoot | Out-Null
$Arguments=@('--report-dir',('"'+$ReportRoot+'"'))
if($Level) { $Arguments+=@('--level',$Level) }
Start-Process -FilePath $Executable -ArgumentList $Arguments -WorkingDirectory (Split-Path -Parent $Executable)
Write-Host "Runtime reports: $ReportRoot"

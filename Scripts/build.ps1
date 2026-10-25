param([ValidateSet('Release','Debug')][string]$Configuration='Release',[int]$Jobs=6)
$ErrorActionPreference='Stop'
$ProjectRoot=Split-Path -Parent $PSScriptRoot
$TaskRoot=Split-Path -Parent $ProjectRoot
$ToolsRoot=Join-Path $TaskRoot '工具链'
$EngineRoot=Join-Path (Split-Path -Parent $TaskRoot) 'cocos2d-x-3.17.2'
$LogRoot=Join-Path $ProjectRoot 'logs'
New-Item -ItemType Directory -Force -Path $LogRoot | Out-Null
$Stamp=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$Mappings=[ordered]@{'S:'=$ProjectRoot;'T:'=$ToolsRoot;'U:'=$EngineRoot}
$Created=@()
function Invoke-Logged([string[]]$Arguments,[string]$Log) {
    $ErrorActionPreference='Continue'
    & $Cmake @Arguments 2>&1 | Tee-Object -FilePath $Log
    $Code=$LASTEXITCODE
    if($Code -ne 0) { throw "CMake failed with exit code $Code. Log: $Log" }
}
try {
    foreach($Drive in $Mappings.Keys) {
        if(Test-Path -LiteralPath "$Drive\") { throw "Drive already in use: $Drive" }
        & subst.exe $Drive $Mappings[$Drive]
        if($LASTEXITCODE -ne 0) { throw "Could not map $Drive" }
        $Created+=$Drive
    }
    . (Join-Path $TaskRoot '编译环境验证\toolchain.ps1') -ToolchainAlias 'T:\'
    $env:CL='/utf-8'
    $BuildRoot="S:/build-$Configuration"
    $Arguments=@('-S','S:/','-B',$BuildRoot,'-G','Ninja','-Wno-deprecated',
        "-DCMAKE_BUILD_TYPE=$Configuration",'-DCOCOS2DX_ROOT_PATH=U:/',
        '-DCMAKE_C_COMPILER:FILEPATH=T:/MSVC2019/VC/Tools/MSVC/14.29.30133/bin/Hostx64/x86/cl.exe',
        '-DCMAKE_CXX_COMPILER:FILEPATH=T:/MSVC2019/VC/Tools/MSVC/14.29.30133/bin/Hostx64/x86/cl.exe',
        '-DCMAKE_MAKE_PROGRAM:FILEPATH=T:/ninja/ninja.exe',
        '-DMSVC_RUNTIME_DIR:PATH=T:/MSVC2019/VC/Redist/MSVC/14.29.30133/x86/Microsoft.VC142.CRT',
        "-DCMAKE_RC_COMPILER:FILEPATH=$((Join-Path $SdkBin 'rc.exe').Replace('\','/'))",
        "-DCMAKE_MT:FILEPATH=$((Join-Path $SdkBin 'mt.exe').Replace('\','/'))")
    Invoke-Logged $Arguments (Join-Path $LogRoot "configure-$Configuration-$Stamp.log")
    Invoke-Logged @('--build',$BuildRoot,'--target','FarmGame','--parallel',"$Jobs") (Join-Path $LogRoot "build-$Configuration-$Stamp.log")
    Write-Host "Built: $(Join-Path $ProjectRoot "build-$Configuration\bin\FarmGame.exe")"
} finally {
    foreach($Drive in $Created) { & subst.exe $Drive /D }
}

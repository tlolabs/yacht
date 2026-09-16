param([ValidateSet('x64','arm64')][string]$Architecture='x64')
$ErrorActionPreference='Stop'
Set-Location (Join-Path $PSScriptRoot '..')
$version=python script/sync_version.py
$triple=if($Architecture -eq 'x64'){'x86_64-pc-windows-msvc'}else{'aarch64-pc-windows-msvc'}
$platform=if($Architecture -eq 'x64'){'x64'}else{'ARM64'}
cargo build --workspace --locked --release --target $triple
if($LASTEXITCODE){throw 'Rust build failed'}
$publish=Join-Path $PWD "target/windows-$Architecture"
dotnet restore platform/windows/YACHT/YACHT.csproj -p:Platform=$platform -p:RustTarget=$triple -p:RestoreLockedMode=true
if($LASTEXITCODE){throw 'Locked NuGet restore failed'}
dotnet publish platform/windows/YACHT/YACHT.csproj --no-restore -c Release -r "win-$Architecture" --self-contained true -p:Platform=$platform -p:RustTarget=$triple -o $publish
if($LASTEXITCODE){throw 'WinUI publish failed'}
Copy-Item "target/$triple/release/yacht.exe",LICENSE,README.md,DEPENDENCIES.md $publish
# Signing is optional. Certificate is selected from an ephemeral CI/user store;
# certificate material/passwords never appear in the repository or command line.
function Sign([string]$path){
  if($env:YACHT_SIGNING_THUMBPRINT){
    signtool sign /sha1 $env:YACHT_SIGNING_THUMBPRINT /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 $path
    if($LASTEXITCODE){throw "Signing failed: $path"}
    signtool verify /pa $path
    if($LASTEXITCODE){throw "Signature verification failed: $path"}
  }
}
Sign (Join-Path $publish 'YachtApp.exe');Sign (Join-Path $publish 'yacht.exe');Sign (Join-Path $publish 'yacht_ffi.dll')
New-Item -ItemType Directory -Force release | Out-Null
$installer=if($Architecture -eq 'x64'){'x64compatible'}else{'arm64'}
$iscc=(Get-Command ISCC.exe -ErrorAction SilentlyContinue).Source
if(!$iscc){$iscc="${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"}
& $iscc "/DVersion=$version" "/DArchitecture=$installer" "/DLabel=$Architecture" "/DPublishDirectory=$publish" "/DOutputDirectory=$PWD/release" platform/windows/installer/YACHT.iss
if($LASTEXITCODE){throw 'Installer build failed'}
Sign "$PWD/release/YACHT-$version-windows-$Architecture-setup.exe"
Compress-Archive -Path "$publish/*" -DestinationPath "release/YACHT-$version-windows-$Architecture.zip" -Force
Get-ChildItem "release/YACHT-$version-windows-$Architecture*" | ForEach-Object { $hash=Get-FileHash $_.FullName -Algorithm SHA256; "$($hash.Hash.ToLower())  $($_.Name)" } | Set-Content "release/SHA256SUMS-windows-$Architecture"
& "$publish/yacht.exe" --help
if($LASTEXITCODE){throw 'Packaged CLI failed'}

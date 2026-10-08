param([ValidateSet('x64','arm64')][string]$Architecture='x64', [switch]$BuildOnly)
$ErrorActionPreference='Stop'
Set-Location (Join-Path $PSScriptRoot '..')
$version=python script/sync_version.py
$triple=if($Architecture -eq 'x64'){'x86_64-pc-windows-msvc'}else{'aarch64-pc-windows-msvc'}
cargo build --workspace --locked --release --target $triple
if($LASTEXITCODE){throw 'Rust build failed'}
$publish=Join-Path $PWD "target/windows-$Architecture"
if(Test-Path $publish){Remove-Item $publish -Recurse -Force}
New-Item -ItemType Directory -Force $publish | Out-Null
cmake -S platform/qt -B "build/qt-$Architecture" -DCMAKE_BUILD_TYPE=Release
if($LASTEXITCODE){throw 'CMake configure failed'}
cmake --build "build/qt-$Architecture" --config Release --target YachtApp
if($LASTEXITCODE){throw 'Qt build failed'}
$qtExe="build/qt-$Architecture/Release/YachtApp.exe"
if(!(Test-Path $qtExe)){$qtExe="build/qt-$Architecture/YachtApp.exe"}
Copy-Item $qtExe (Join-Path $publish 'YachtApp.exe')
windeployqt --release --no-translations --no-compiler-runtime (Join-Path $publish 'YachtApp.exe')
if($LASTEXITCODE){throw 'Qt runtime deployment failed'}
# windeployqt cannot locate the compiler runtime on hosted runners when
# VCINSTALLDIR is unset. Copy the redistributable for this package's CPU.
$redistRoot=$env:VCToolsRedistDir
if(!$redistRoot -or !(Test-Path $redistRoot)){
  $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  if(!(Test-Path $vswhere)){throw 'Visual Studio redistributable locator missing'}
  $vsInstall=& $vswhere -latest -products * -property installationPath | Select-Object -First 1
  if(!$vsInstall){throw 'Visual Studio installation not found'}
  $redistRoot=Join-Path $vsInstall 'VC\Redist\MSVC'
}
$crtDir=Get-ChildItem $redistRoot -Recurse -Directory -Filter 'Microsoft.VC*.CRT' |
  Where-Object { $_.Parent.Name -ieq $Architecture } |
  Sort-Object FullName -Descending | Select-Object -First 1
if(!$crtDir){throw "MSVC $Architecture redistributable directory not found under $redistRoot"}
Copy-Item (Join-Path $crtDir.FullName '*.dll') $publish -Force
Copy-Item "target/$triple/release/yacht_ffi.dll" $publish
Copy-Item "target/$triple/release/yacht-update.exe","target/$triple/release/yacht.exe",LICENSE,README.md,THIRD_PARTY_NOTICES.md,PRIVACY.md,'docs/DEPENDENCIES.md' $publish
Copy-Item LICENSE-NOTICE.md $publish
python script/collect_qt_notices.py $publish --rid "win-$Architecture"
if($LASTEXITCODE){throw 'Windows third-party notice collection failed'}
python script/check_windows_runtime.py $publish --arch $Architecture
if($LASTEXITCODE){throw 'Windows native runtime dependency audit failed'}
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
Sign (Join-Path $publish 'yacht-update.exe');Sign (Join-Path $publish 'YachtApp.exe');Sign (Join-Path $publish 'yacht.exe');Sign (Join-Path $publish 'yacht_ffi.dll')
if($BuildOnly){
  & "$publish/yacht.exe" --help
  if($LASTEXITCODE){throw 'Packaged CLI failed'}
  exit 0
}
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

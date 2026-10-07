param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference='Stop'
$testDirectory=Join-Path $env:TEMP ('yacht-qt-'+[guid]::NewGuid())
New-Item -ItemType Directory -Path $testDirectory | Out-Null
$env:YACHT_TEST_DATA=Join-Path $testDirectory 'preferences'
$started=Get-Date
$failure=$null
$cleanupFailure=$null
$process=Start-Process -FilePath $Executable -ArgumentList @('--ui-smoke-test',('"'+$testDirectory+'"')) -PassThru
try {
  if (!$process.WaitForExit(90000)) { $process.Kill(); throw 'Qt smoke test timed out' }
  if (Test-Path (Join-Path $testDirectory 'failed.txt')) { throw (Get-Content (Join-Path $testDirectory 'failed.txt') -Raw) }
  if ($process.ExitCode -ne 0 -or !(Test-Path (Join-Path $testDirectory 'passed.txt'))) {
    $startup=Join-Path $env:YACHT_TEST_DATA 'startup-failed.txt'
    if (Test-Path $startup) { Write-Output (Get-Content $startup -Raw) }
    Get-WinEvent -FilterHashtable @{LogName='Application'; StartTime=$started; Level=2} -ErrorAction SilentlyContinue | Select-Object -First 8 -ExpandProperty Message | Write-Output
    throw "Qt smoke test failed with process exit code $($process.ExitCode)"
  }
  Get-Content (Join-Path $testDirectory 'passed.txt')
} catch { $failure=$_ } finally {
  # WebView2 closes asynchronously after its owning window exits. Allow its own
  # processes to release this isolated profile; never kill unrelated browsers.
  for ($attempt=0; $attempt -lt 40; $attempt++) {
    try { Remove-Item $testDirectory -Recurse -Force; $cleanupFailure=$null; break }
    catch { $cleanupFailure=$_; Start-Sleep -Milliseconds 250 }
  }
  Remove-Item Env:YACHT_TEST_DATA
  $process.Dispose()
}
if ($failure) {
  if ($cleanupFailure) { Write-Output "Additional profile cleanup failure: $cleanupFailure" }
  throw $failure
}
if ($cleanupFailure) { throw $cleanupFailure }

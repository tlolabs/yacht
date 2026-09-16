param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference='Stop'
$testDirectory=Join-Path $env:TEMP ('yacht-winui-'+[guid]::NewGuid())
New-Item -ItemType Directory -Path $testDirectory | Out-Null
$env:YACHT_TEST_DATA=Join-Path $testDirectory 'preferences'
$process=Start-Process -FilePath $Executable -ArgumentList @('--ui-smoke-test',('"'+$testDirectory+'"')) -PassThru
try {
  if (!$process.WaitForExit(90000)) { $process.Kill(); throw 'WinUI smoke test timed out' }
  if (Test-Path (Join-Path $testDirectory 'failed.txt')) { throw (Get-Content (Join-Path $testDirectory 'failed.txt') -Raw) }
  if ($process.ExitCode -ne 0 -or !(Test-Path (Join-Path $testDirectory 'passed.txt'))) { throw 'WinUI smoke test failed' }
  Get-Content (Join-Path $testDirectory 'passed.txt')
} finally { Remove-Item $testDirectory -Recurse -Force; Remove-Item Env:YACHT_TEST_DATA }

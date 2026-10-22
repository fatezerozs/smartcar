param([string]$UV4='C:\Keil_v5\UV4\UV4.exe',[switch]$StoppedAndReady,[switch]$C12Open)
$ErrorActionPreference='Stop'
if(-not $StoppedAndReady){throw 'Flashing requires explicit confirmation that outputs are stopped and vehicle is secured.'}
if(-not $C12Open){throw 'Open the C12 master switch and pass -C12Open before flashing: a restored profile may launch after reset.'}
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskProject=Join-Path $taskRoot 'demo\mdk\base\rt1064.uvprojx'
$taskAxf=Join-Path $taskRoot '.build\keil\Objects\rt1064.axf'
$taskBuildLog=Join-Path $taskRoot '.build\keil\build.log'
if(-not (Test-Path -LiteralPath $UV4) -or -not (Test-Path -LiteralPath $taskAxf)){throw 'Keil executable or firmware is absent.'}
if(-not (Test-Path -LiteralPath $taskBuildLog) -or
   (Get-Content -LiteralPath $taskBuildLog -Raw) -notmatch '0 Error\(s\), 0 Warning\(s\)'){throw 'A verified complete build is required first.'}
$taskInputs=@(Get-ChildItem (Join-Path $taskRoot 'demo\code'),(Join-Path $taskRoot 'demo\user') -Recurse -File | Where-Object Extension -in @('.c','.h'))
$taskInputs+=@(Get-Item -LiteralPath $taskProject,(Join-Path $taskRoot 'demo\mdk\scf\MIMXRT1064xxxxx_flexspi_nor.scf'))
$taskNewest=$taskInputs | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if($taskNewest.LastWriteTimeUtc -gt (Get-Item -LiteralPath $taskAxf).LastWriteTimeUtc){throw 'Sources are newer than AXF; rebuild before flashing.'}
$taskLog=Join-Path $taskRoot ('.build\keil\flash-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'.log')
$taskStart=[Diagnostics.ProcessStartInfo]::new()
$taskStart.FileName=$UV4
$taskStart.Arguments='-f "'+$taskProject+'" -t "nor_sdram_zf_dtcm" -j0 -o "'+$taskLog+'"'
$taskStart.WorkingDirectory=Split-Path $taskProject -Parent
$taskStart.UseShellExecute=$false;$taskStart.CreateNoWindow=$true
$taskStart.WindowStyle=[Diagnostics.ProcessWindowStyle]::Hidden
$taskProcess=[Diagnostics.Process]::Start($taskStart)
if(-not $taskProcess.WaitForExit(45000)){
    # Never kill a helper that may still be erasing/programming flash. Report
    # its exact PID and log; caller must inspect completion before any retry.
    Write-Output ('FLASH_PENDING_PID='+$taskProcess.Id)
    throw ('Flash still pending; do NOT start another download. Log: '+$taskLog)
}
$taskExit=$taskProcess.ExitCode
$taskText=if(Test-Path -LiteralPath $taskLog){Get-Content -LiteralPath $taskLog -Raw}else{''}
Write-Output $taskText
Write-Output ('FLASH_LOG='+$taskLog)
if($taskExit -ne 0 -or $taskText -notmatch 'Programming Done' -or $taskText -notmatch 'Verify OK' -or
   $taskText -match '(?im)^Error:|Flash Download failed|SWD/JTAG Communication Failure|RDDI-DAP Error'){
    throw ('Download NOT confirmed; Keil exit='+$taskExit)
}
Write-Output 'FLASH VERIFIED. Runtime identity must still be checked over serial; this does not arm or calibrate.'

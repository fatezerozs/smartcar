param([string]$UV4='E:\ElecTronic\Keil\UV4\UV4.exe',
      [ValidateSet('train','auto','route','switch')][string]$Variant='train')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskMdk=Join-Path $taskRoot 'demo\mdk\base'
$taskBuildName='midterm-'+$Variant
$taskProjectName='midterm_'+$Variant+'.uvprojx'
$taskExtraDefine='VISION_MIDTERM_TRAINING=1,VISION_ENCODER_ENABLE=1,VISION_MIDTERM_AUTONOMOUS='+$(if($Variant -eq 'auto'){'1'}else{'0'})+','
if($Variant -in @('route','switch')){$taskExtraDefine+='VISION_MIDTERM_ROUTE_TRIAL=1,'}
if($Variant -eq 'switch'){$taskExtraDefine+='VISION_ROUTE_SWITCH=1,'}
$taskOutput=Join-Path $taskRoot ('.build\'+$taskBuildName)
New-Item -ItemType Directory -Force -Path (Join-Path $taskOutput 'Objects'),(Join-Path $taskOutput 'Listings'),(Join-Path $taskOutput 'tmp') | Out-Null
if(-not(Test-Path -LiteralPath $UV4)){throw 'UV4 executable not found; supply -UV4.'}
$taskSource=Get-Content -LiteralPath (Join-Path $taskMdk 'rt1064.uvprojx') -Raw
[xml]$taskXml=$taskSource
$taskTarget=@($taskXml.Project.Targets.Target | Where-Object TargetName -eq 'nor_sdram_zf_dtcm')
if($taskTarget.Count -ne 1){throw 'Expected one target.'}
$taskDefines=[string]$taskTarget[0].TargetOption.TargetArmAds.Cads.VariousControls.Define
if($taskDefines -match 'VISION_MOTOR_THRESHOLD_DIAG|VISION_MOTION_DIAG|VISION_ENCODER_ENABLE|VISION_LINK_MVP'){throw 'Source target must be the normal firmware, without diagnostic/MVP overrides.'}
# Preserve Keil project serialization and relative source paths.
$taskSource=$taskSource.Replace('<Define>'+$taskDefines+'</Define>','<Define>'+$taskExtraDefine+$taskDefines+'</Define>')
$taskSource=$taskSource.Replace('<OutputDirectory>'+[string]$taskTarget[0].TargetOption.TargetCommonOption.OutputDirectory+'</OutputDirectory>','<OutputDirectory>..\..\..\.build\'+$taskBuildName+'\Objects\</OutputDirectory>')
$taskSource=$taskSource.Replace('<ListingPath>'+[string]$taskTarget[0].TargetOption.TargetCommonOption.ListingPath+'</ListingPath>','<ListingPath>..\..\..\.build\'+$taskBuildName+'\Listings\</ListingPath>')
$taskProjectDir=Join-Path (Split-Path $taskMdk -Parent) $(if($Variant -eq 'switch'){'competition'}else{'training'})
New-Item -ItemType Directory -Force -Path $taskProjectDir | Out-Null
$taskProject=Join-Path $taskProjectDir $taskProjectName
# Windows PowerShell's Set-Content -Encoding utf8 adds a BOM rejected by UV4.
[IO.File]::WriteAllText($taskProject,$taskSource,[Text.UTF8Encoding]::new($false))
$taskLog=Join-Path $taskOutput ('build-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'.log')
$taskStart=[Diagnostics.ProcessStartInfo]::new()
$taskStart.FileName=$UV4
$taskStart.Arguments='-r "'+$taskProject+'" -t "nor_sdram_zf_dtcm" -j0 -o "'+$taskLog+'"'
$taskStart.WorkingDirectory=$taskProjectDir
$taskStart.UseShellExecute=$false
$taskStart.CreateNoWindow=$true
$taskStart.WindowStyle=[Diagnostics.ProcessWindowStyle]::Hidden
$taskStart.EnvironmentVariables['TEMP']=Join-Path $taskOutput 'tmp'
$taskStart.EnvironmentVariables['TMP']=Join-Path $taskOutput 'tmp'
$taskBuildStarted=Get-Date
$taskProcess=[Diagnostics.Process]::Start($taskStart)
$taskProcess.WaitForExit()
if(-not(Test-Path -LiteralPath $taskLog)){throw ('Keil did not produce a build log; exit='+$taskProcess.ExitCode)}
$taskLogText=Get-Content -LiteralPath $taskLog -Raw
Get-Content -LiteralPath $taskLog -Tail 10
$taskArtifact=Join-Path $taskOutput 'Objects\rt1064.axf'
if($taskProcess.ExitCode -ne 0 -or $taskLogText -notmatch '0 Error\(s\), 0 Warning\(s\)' -or -not(Test-Path -LiteralPath $taskArtifact)){
    throw ('Midterm build failed: '+$taskLog)
}
if((Get-Item -LiteralPath $taskArtifact).LastWriteTime -lt $taskBuildStarted){throw 'Midterm artifact is stale.'}
Write-Output ('BUILT ONLY; NOT FLASHED: '+$taskArtifact)
Write-Output ('Midterm project: '+$taskProject)

param([string]$UV4='E:\ElecTronic\Keil\UV4\UV4.exe',
      [ValidateSet('train','auto','route','switch','auto-track','auto_track')][string]$Variant='train')
$ErrorActionPreference='Stop'
if($Variant -in @('auto','auto_track')) {
    if($Variant -eq 'auto'){Write-Warning "Variant 'auto' is deprecated; using track-gated auto-track instead."}
    $Variant='auto-track'
}
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskMdk=Join-Path $taskRoot 'demo\mdk\base'
$taskBuildName='midterm-'+$Variant
$taskProjectName='midterm_'+$Variant+'.uvprojx'
$taskExtraDefine='VISION_MIDTERM_TRAINING=1,VISION_ENCODER_ENABLE=1,VISION_MIDTERM_AUTONOMOUS='+$(if($Variant -in @('auto','auto-track')){'1'}else{'0'})+','
if($Variant -in @('route','switch','auto-track')){$taskExtraDefine+='VISION_MIDTERM_ROUTE_TRIAL=1,'}
if($Variant -in @('switch','auto-track')){$taskExtraDefine+='VISION_ROUTE_SWITCH=1,'}
if($Variant -eq 'auto-track'){
    # Software-only safety configuration: no physical selector authority and
    # no automatic claim that camera/raised-wheel review has been completed.
    $taskExtraDefine+='VISION_MIDTERM_AUTO_TRACK=1,VISION_START_SWITCH_ENABLE=0,MIDTERM_VISUAL_REVIEWED=0,'
}
$taskOutput=Join-Path $taskRoot ('.build\'+$taskBuildName)
New-Item -ItemType Directory -Force -Path (Join-Path $taskOutput 'Objects'),(Join-Path $taskOutput 'Listings'),(Join-Path $taskOutput 'tmp') | Out-Null
if(-not(Test-Path -LiteralPath $UV4)){throw 'UV4 executable not found; supply -UV4.'}
$taskSource=Get-Content -LiteralPath (Join-Path $taskMdk 'rt1064.uvprojx') -Raw
[xml]$taskXml=$taskSource
$taskTarget=@($taskXml.Project.Targets.Target | Where-Object TargetName -eq 'nor_sdram_zf_dtcm')
if($taskTarget.Count -ne 1){throw 'Expected one target.'}
$taskDefines=[string]$taskTarget[0].TargetOption.TargetArmAds.Cads.VariousControls.Define
# Reject inherited build-policy overrides instead of silently introducing
# conflicting definitions after the variant-specific prefix.
$taskReserved='VISION_MIDTERM_TRAINING|VISION_MOTOR_THRESHOLD_DIAG|VISION_MOTION_DIAG|VISION_ENCODER_ENABLE|VISION_LINK_MVP|VISION_MIDTERM_AUTONOMOUS|VISION_MIDTERM_AUTO_TRACK|VISION_MIDTERM_ROUTE_TRIAL|VISION_ROUTE_SWITCH|VISION_START_SWITCH_ENABLE|MIDTERM_VISUAL_REVIEWED'
if($taskDefines -match ('\b(?:'+$taskReserved+')\b')){
    throw 'Source target contains build-policy or safety-review overrides; use the base firmware target.'
}
# Preserve Keil project serialization and relative source paths.
$taskOldDefine='<Define>'+$taskDefines+'</Define>'
$taskNewDefine='<Define>'+$taskExtraDefine+$taskDefines+'</Define>'
if(-not $taskSource.Contains($taskOldDefine)){
    throw 'Base Keil project compile definitions could not be located; refusing to build without variant macros.'
}
$taskSource=$taskSource.Replace($taskOldDefine,$taskNewDefine)
if(-not $taskSource.Contains($taskNewDefine)){
    throw 'Generated Keil project is missing variant compile definitions.'
}
$taskOldOutput='<OutputDirectory>'+[string]$taskTarget[0].TargetOption.TargetCommonOption.OutputDirectory+'</OutputDirectory>'
$taskOldListing='<ListingPath>'+[string]$taskTarget[0].TargetOption.TargetCommonOption.ListingPath+'</ListingPath>'
# Fail closed if the base project serialization changes; do not accidentally
# leave an output path pointing into releases or an unspecified directory.
if(-not $taskSource.Contains($taskOldOutput) -or -not $taskSource.Contains($taskOldListing)){
    throw 'Base project output/listing paths could not be safely rewritten.'
}
$taskNewOutput='<OutputDirectory>..\..\..\.build\'+$taskBuildName+'\Objects\</OutputDirectory>'
$taskNewListing='<ListingPath>..\..\..\.build\'+$taskBuildName+'\Listings\</ListingPath>'
$taskSource=$taskSource.Replace($taskOldOutput,$taskNewOutput)
$taskSource=$taskSource.Replace($taskOldListing,$taskNewListing)
if(-not $taskSource.Contains($taskNewOutput) -or -not $taskSource.Contains($taskNewListing)){
    throw 'Generated Keil project output/listing paths are not confined to .build.'
}
$taskProjectDir=Join-Path (Split-Path $taskMdk -Parent) $(if($Variant -in @('switch','auto-track')){'competition'}else{'training'})
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

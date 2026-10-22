param([string]$UV4='E:\ElecTronic\Keil\UV4\UV4.exe')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskMdk=Join-Path $taskRoot 'demo\mdk\base'
$taskOutput=Join-Path $taskRoot '.build\link-mvp'
New-Item -ItemType Directory -Force -Path (Join-Path $taskOutput 'Objects'),(Join-Path $taskOutput 'Listings'),(Join-Path $taskOutput 'tmp') | Out-Null
if(-not(Test-Path -LiteralPath $UV4)){throw 'UV4 executable not found; supply -UV4.'}
$taskSource=Get-Content -LiteralPath (Join-Path $taskMdk 'rt1064.uvprojx') -Raw
[xml]$taskXml=$taskSource
$taskTarget=@($taskXml.Project.Targets.Target | Where-Object TargetName -eq 'nor_sdram_zf_dtcm')
if($taskTarget.Count -ne 1){throw 'Expected one target.'}
$taskDefines=[string]$taskTarget[0].TargetOption.TargetArmAds.Cads.VariousControls.Define
$taskSource=$taskSource.Replace('<Define>'+$taskDefines+'</Define>','<Define>VISION_LINK_MVP=1,VISION_DUAL_UART_ENABLE=1,'+$taskDefines+'</Define>')
$taskSource=$taskSource.Replace('<OutputDirectory>'+[string]$taskTarget[0].TargetOption.TargetCommonOption.OutputDirectory+'</OutputDirectory>','<OutputDirectory>..\..\..\.build\link-mvp\Objects\</OutputDirectory>')
$taskSource=$taskSource.Replace('<ListingPath>'+[string]$taskTarget[0].TargetOption.TargetCommonOption.ListingPath+'</ListingPath>','<ListingPath>..\..\..\.build\link-mvp\Listings\</ListingPath>')
$taskProjectDir=Join-Path (Split-Path $taskMdk -Parent) 'diagnostics'
New-Item -ItemType Directory -Force -Path $taskProjectDir | Out-Null
$taskProject=Join-Path $taskProjectDir 'link_mvp.uvprojx'
# Avoid Windows PowerShell UTF-8 BOM, which this UV4 rejects with exit 15.
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
$taskProcess=[Diagnostics.Process]::Start($taskStart)
$taskProcess.WaitForExit()
if(Test-Path -LiteralPath $taskLog){Get-Content -LiteralPath $taskLog -Tail 8}
if($taskProcess.ExitCode -ne 0){throw ('MVP build failed: '+$taskLog)}
Write-Output ('LINK MVP BUILT. No flashing. Generated project: '+$taskProject)

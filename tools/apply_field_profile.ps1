param([Parameter(Mandatory=$true)][string]$Port,
      [string]$Profile=(Join-Path (Split-Path $PSScriptRoot -Parent) 'profiles\field-20261019.json'))
$ErrorActionPreference='Stop'
$taskProfile=Get-Content -LiteralPath $Profile -Raw | ConvertFrom-Json
if($taskProfile.schema_version -ne 1){throw 'Unsupported field profile schema.'}
$taskServo=$taskProfile.servo
foreach($taskPulse in @($taskServo.center_us,$taskServo.min_us,$taskServo.max_us)){
    if($null -eq $taskPulse -or $taskPulse -lt 500 -or $taskPulse -gt 2500 -or $taskPulse -ne [int]$taskPulse){throw 'Invalid servo pulse.'}
}
if($taskServo.min_us -ge $taskServo.center_us -or $taskServo.center_us -ge $taskServo.max_us -or
   $taskServo.right_sign -notin @(-1,1)){throw 'Invalid servo bounds or direction.'}
$taskCamera=$taskProfile.camera
foreach($taskCoordinate in @($taskCamera.near_y,$taskCamera.far_y,$taskCamera.half_height)){
    if($null -eq $taskCoordinate -or [double]::IsNaN([double]$taskCoordinate) -or
       [double]::IsInfinity([double]$taskCoordinate) -or $taskCoordinate -le 0 -or $taskCoordinate -ge 1){throw 'Invalid camera sampling band.'}
}
if($taskCamera.near_y-$taskCamera.far_y -le 2*$taskCamera.half_height){throw 'Overlapping camera bands.'}
$taskCulture=[Globalization.CultureInfo]::InvariantCulture
# Restore observed servo/sampling parameters only. No motor polarity, camera
# axis or positive duty can be restored by this script, even if JSON changes.
# First command enforces zero motor profile; FIELD_2 keeps motor gate separate.
$taskCommands=@('@motor 0 0 0.1',
    ('@bands '+([double]$taskCamera.near_y).ToString('G9',$taskCulture)+' '+
        ([double]$taskCamera.far_y).ToString('G9',$taskCulture)+' '+([double]$taskCamera.half_height).ToString('G9',$taskCulture)),
    ('@servo '+$taskServo.center_us+' '+$taskServo.min_us+' '+$taskServo.max_us+' '+$taskServo.right_sign))
& (Join-Path $PSScriptRoot 'field_serial.ps1') -Port $Port -Command $taskCommands -RequireMotorGate -Seconds 1

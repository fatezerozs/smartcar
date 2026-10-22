$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../../tools/field_protocol.ps1')
$fieldChecks=0
function Assert-Protocol([bool]$Result) { $script:fieldChecks++;if(-not $Result){throw "Protocol check $script:fieldChecks failed"} }
foreach($fieldValue in @('@encoder_status','@encoder_hw','@speed_status','@encoder_sign -1 1','@speed_cfg 40 1 0.05','@speed_mode 1','@camera 0.5 0.5','@servo 1550 1500 1600 -1')) { Assert-Protocol (Test-FieldCommand $fieldValue) }
foreach($fieldValue in @('a','h','@speed_cfg 40 1 nan',"@status`na",'@speed_mode 1 a','@speed_cfg 40 1','@speed_mode 1.0 extra')) { Assert-Protocol (-not (Test-FieldCommand $fieldValue)) }
Assert-Protocol (Test-FieldReply '@status' '[FIELD] version=MIDTERM_TRAIN_1 ready=0')
Assert-Protocol (-not (Test-FieldReply '@status' '[FIELD] version=MIDTERM_AUTO_1 ready=1'))
Assert-Protocol (Test-FieldReply '@speed_cfg 40 1 0.05' '[SPEED] OK; stopped; RAM only')
Assert-Protocol (-not (Test-FieldReply '@speed_cfg 40 1 0.05' '[SPEED] REJECT; stopped'))
Assert-Protocol (-not (Test-FieldReply '@speed_mode 1' '[FIELD] OK; unrelated reply'))
Assert-Protocol (Test-FieldReply '@encoder_status' "[ENCODER] mode=PULSE_DIR`r`n[SPEED] enabled=0")
Assert-Protocol (-not (Test-FieldReply '@encoder_status' '[ENCODER] incomplete'))
Assert-Protocol (Test-FieldReply '@encoder_hw' '[ENC_HW] CM3=5 CM4=5')

Assert-Protocol (Test-FieldReply "@status" "[FIELD] version=MIDTERM_ROUTE_TRAIN_1 ready=1")
Assert-Protocol (Test-FieldReply '@status' '[FIELD] version=MIDTERM_C26_10 ready=1')
Assert-Protocol (Test-FieldReply '@status' '[FIELD] version=MIDTERM_C26_11 ready=1')
Assert-Protocol (Test-FieldReply '@status' '[FIELD] version=MIDTERM_C26_12 ready=1')
Assert-Protocol (Test-FieldReply '@status' '[FIELD] version=MIDTERM_C26_13 ready=1')
Assert-Protocol (Test-FieldReply '@status' '[FIELD] version=MIDTERM_C26_14 ready=1')
Assert-Protocol (Test-FieldReply '@status' '[FIELD] version=MIDTERM_C26_15 ready=1')
Assert-Protocol (Test-FieldReply '@status' '[FIELD] version=MIDTERM_C26_16 ready=1')
Assert-Protocol (Test-FieldReply '@status' '[FIELD] version=MIDTERM_C26_17 ready=1')
Assert-Protocol (Test-FieldReply '@status' '[FIELD] version=MIDTERM_C26_18 ready=1')
Assert-Protocol (Test-FieldReply '@status' '[FIELD] version=MIDTERM_C26_19 ready=1')
foreach($fieldVersion in 20..28) {
    Assert-Protocol (Test-FieldReply '@status' "[FIELD] version=MIDTERM_C26_$fieldVersion ready=1")
}
foreach($fieldUnknown in @('29','280','28_extra','0')) {
    Assert-Protocol (-not (Test-FieldReply '@status' "[FIELD] version=MIDTERM_C26_$fieldUnknown ready=1"))
}
Write-Output "FIELD PROTOCOL PASS: $fieldChecks checks; no COM opened."

# Protocol helpers only: no port access or vehicle commands on import.
$fieldVersionPattern='(?:MIDTERM_FIELD_[1234]|MIDTERM_TRAIN_1)'
function Test-FieldCommand([string]$Value) {
    if($Value -cin @('@status','@save','@load','@encoder_status','@speed_status','@encoder_hw')){return $true}
    $fieldNumber='[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?'
    foreach($fieldSpec in @(@('startup',2),@('gain',3),@('motor',3),@('balance',2),@('camera',2),@('bands',3),@('servo',4),@('duration',1),@('probe',2),@('wheel',4),@('motor_dir',2),@('encoder_sign',2),@('speed_cfg',3),@('speed_mode',1))) {
        if($Value -cmatch ('\A@'+$fieldSpec[0]+'(?: '+$fieldNumber+'){'+$fieldSpec[1]+'}\z')){return $true}
    }
    return $false
}
function Test-FieldReply([string]$Command,[string]$Reply) {
    if($Reply -match '\[(?:FIELD|SPEED)\] REJECT'){return $false}
    if($Command -ceq '@status'){return $Reply -match ('\[FIELD\] version='+$fieldVersionPattern+' ')}
    if($Command -cin @('@encoder_status','@speed_status')){return $Reply -match '\[ENCODER\]' -and $Reply -match '\[SPEED\] enabled='}
    if($Command -ceq '@encoder_hw'){return $Reply -match '\[ENC_HW\]'}
    if($Command -cmatch '\A@(?:encoder_sign|speed_cfg|speed_mode) '){return $Reply -match '\[SPEED\] OK;'}
    return $Reply -match '\[FIELD\] OK;'
}

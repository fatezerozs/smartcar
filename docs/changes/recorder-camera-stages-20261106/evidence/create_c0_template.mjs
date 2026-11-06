import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { Workbook } from '@oai/artifact-tool';

const here = path.dirname(fileURLToPath(import.meta.url));
const columns = ['record_id','measurement_type','wheel','repeat_index','measurement_date','operator','status','axle_distance_mm','track_distance_mm','roll_distance_mm','wheel_turns','encoder_before_counts','encoder_after_counts','counter_bits','command_us','angle_deg','command_position','approach_direction','power_state','load_condition','surface','battery_V','tire_pressure_kPa','instrument','instrument_resolution_mm','instrument_resolution_deg','uncertainty_standard_mm','uncertainty_standard_deg','temperature_C','photo_ref','notes'];
const rows = [columns];
let id = 0;
function observation(type, wheel = '', direction = '', index = 1, position = '') {
  const row = Object.fromEntries(columns.map(c => [c, null]));
  Object.assign(row, {record_id:`C0-${String(++id).padStart(3,'0')}`,measurement_type:type,wheel,repeat_index:index,status:'unknown',approach_direction:direction,command_position:position});
  if (type === 'encoder_scale') row.counter_bits = 16;
  if (type === 'servo_angle') row.notes = 'Requires separate actuator approval and mechanical limit review';
  rows.push(columns.map(c => row[c]));
}
for (let repeat = 1; repeat <= 3; repeat++) {
  observation('wheelbase','','',repeat);
  observation('rear_track','','',repeat);
  for (const wheel of ['left','right']) {
    observation('rolling_circumference',wheel,'forward',repeat);
    for (const direction of ['forward','reverse']) observation('encoder_scale',wheel,direction,repeat);
  }
  for (const direction of ['from_left','from_right']) {
    for (const point of ['left','left_mid','center','right_mid','right']) {
      for (const wheel of ['left_front','right_front']) observation('servo_angle',wheel,direction,repeat,point);
    }
  }
}
const wb = Workbook.create();
const sheet = wb.worksheets.add('Measurements');
sheet.getRange('A1:AE'+rows.length).values = rows;
wb.recalculate();
const values = sheet.getRange('A1:AE'+rows.length).values;
if (values.length !== 85 || values[0].length !== columns.length) throw new Error('Measurement grid dimensions');
for (const row of values.slice(1)) {
  if (row[6] !== 'unknown' || row[7] != null || row[8] != null || row[9] != null || row[14] != null) throw new Error('Unmeasured numeric values must remain blank');
}
const quote = value => value == null ? '' : /[",\r\n]/.test(String(value)) ? '"'+String(value).replaceAll('"','""')+'"' : String(value);
const csv = values.map(row => row.map(quote).join(',')).join('\r\n')+'\r\n';
await fs.writeFile(path.join(here,'../C0_MEASUREMENT_TEMPLATE.csv'),'\uFEFF'+csv,{flag:'wx'});
await fs.writeFile(path.join(here,'c0-template-check.json'),JSON.stringify({rows:values.length-1,columns:columns.length,numeric_observations_blank:true,format:'CSV as specified; no formulas or native workbook controls'},null,2));
console.log('Created CSV with '+(values.length-1)+' unknown observations; no measured constants invented.');

#include "vision_storage.h"
#include <string.h>

#define VS_MAGIC 0x56465334U
#define VS_SCHEMA 1U
/* Changing the algorithm/geometry contract requires a schema migration, not
 * blindly reusing calibration from an incompatible firmware. */
#define VS_FLOATS(X) \
 X(control.motor.lateral_weight) X(control.motor.heading_weight) \
 X(control.motor.steering_weight) X(control.motor.missing_far_scale) \
 X(control.near_y) X(control.far_y) X(control.band_half_height) \
 X(control.axis_near_x) X(control.axis_far_x) \
 X(control.kp) X(control.kd) X(control.heading_gain) X(control.derivative_filter_s) \
 X(control.steer_rate) X(control.duty_rise_per_s) X(control.duty_cap) \
 X(control.cruise_duty) X(control.max_lateral) X(motor1_scale) X(motor2_scale)
static vs_io_t storage;
static vs_info_t info;
typedef char vs_float_must_be_32_bits[sizeof(float)==sizeof(uint32_t)?1:-1];
void vs_get_info(vs_info_t *out) { *out=info; }

static uint32_t crc(const uint32_t *w)
{
    uint32_t value=0xFFFFFFFFU; unsigned i,b,k;
    /* Explicit little-endian word bytes: independent of compiler layout. */
    for(i=0;i<VS_RECORD_WORDS-1;++i) for(b=0;b<4;++b) {
        value^=(w[i]>>(8*b))&255U;
        for(k=0;k<8;++k) value=(value>>1)^((value&1U)?0xEDB88320U:0U);
    }
    return value^0xFFFFFFFFU;
}

static void encode(const vd_profile_t *p, uint32_t sequence, uint32_t *w)
{
    unsigned n=4;
    memset(w,0,VS_RECORD_WORDS*sizeof(*w));
    w[0]=VS_MAGIC;w[1]=VS_SCHEMA;w[2]=sequence;w[3]=VS_RECORD_WORDS;
#define PUT_FLOAT(field) memcpy(&w[n++],&p->field,sizeof(float));
    VS_FLOATS(PUT_FLOAT)
#undef PUT_FLOAT
    w[n++]=p->control.frame_timeout_ms;w[n++]=p->control.command_timeout_ms;
    w[n++]=p->control.max_run_ms;w[n++]=p->control.arm_frames;
    w[n++]=p->control.min_coverage;w[n++]=p->control.min_band_rows;
    w[n++]=p->control.calibration_confirmed;
    w[n++]=p->servo_center_us;w[n++]=p->servo_min_us;w[n++]=p->servo_max_us;
    w[n++]=p->right_sign==-1?0xFFFFFFFFU:(uint32_t)p->right_sign;
    w[n++]=p->hardware_confirmed;w[n++]=p->motor_direction_confirmed;
    w[n++]=p->motor1_forward_level;w[n++]=p->motor2_forward_level;
    w[n++]=p->autostart_enabled;w[n++]=p->startup_delay_ms;
    w[VS_RECORD_WORDS-1]=crc(w);
}

static int decode(const uint32_t *w, vd_profile_t *p)
{
    unsigned n=4;
    if(w[0]!=VS_MAGIC || w[1]!=VS_SCHEMA || w[3]!=VS_RECORD_WORDS ||
       w[VS_RECORD_WORDS-1]!=crc(w)) return 0;
    memset(p,0,sizeof(*p));
#define GET_FLOAT(field) memcpy(&p->field,&w[n++],sizeof(float));
    VS_FLOATS(GET_FLOAT)
#undef GET_FLOAT
    p->control.frame_timeout_ms=w[n++];p->control.command_timeout_ms=w[n++];
    p->control.max_run_ms=w[n++];
#define GET_U8(field) if(w[n]>255U)return 0; p->field=(uint8_t)w[n++];
#define GET_U16(field) if(w[n]>65535U)return 0; p->field=(uint16_t)w[n++];
    GET_U8(control.arm_frames) GET_U8(control.min_coverage) GET_U8(control.min_band_rows)
    GET_U8(control.calibration_confirmed)
    GET_U16(servo_center_us) GET_U16(servo_min_us) GET_U16(servo_max_us)
    if(w[n]!=1 && w[n]!=0xFFFFFFFFU) return 0;
    p->right_sign=w[n++]==1?1:-1;
    GET_U8(hardware_confirmed) GET_U8(motor_direction_confirmed)
    GET_U8(motor1_forward_level) GET_U8(motor2_forward_level) GET_U8(autostart_enabled)
#undef GET_U8
#undef GET_U16
    p->startup_delay_ms=w[n++];
    /* Reserved words are zero in schema 1, not unchecked future parameters. */
    while(n<VS_RECORD_WORDS-1) if(w[n++]!=0) return 0;
    return vision_drive_profile_ready(p);
}

void vs_init(const vs_io_t *io)
{
    memset(&storage,0,sizeof(storage));memset(&info,0,sizeof(info));
    info.error=VS_EMPTY;
    if(io)storage=*io;
}

int vs_load(vd_profile_t *out)
{
    uint32_t record[VS_RECORD_WORDS],chosen_generation=0;
    vd_profile_t candidate, chosen={0};unsigned slot,i;int found=0,dirty=0;
    if(!out || !storage.read){info.valid=0;info.error=VS_IO_ERROR;return 0;}
    for(slot=0;slot<2;++slot) {
        if(!storage.read(slot,record,VS_RECORD_WORDS)){info.valid=0;info.error=VS_IO_ERROR;return 0;}
        for(i=0;i<VS_RECORD_WORDS;++i)if(record[i]!=0xFFFFFFFFU){dirty=1;break;}
        if(!decode(record,&candidate))continue;
        if(!found || (record[2]!=chosen_generation &&
            (uint32_t)(record[2]-chosen_generation)<0x80000000U)) {
            chosen=candidate;chosen_generation=record[2];info.slot=(uint8_t)slot;found=1;
        }
    }
    info.valid=(uint8_t)found;info.generation=chosen_generation;
    info.error=found?VS_OK:(dirty?VS_CORRUPT:VS_EMPTY);
    if(found)*out=chosen;
    return found;
}

int vs_save(const vd_profile_t *p)
{
    uint32_t record[VS_RECORD_WORDS],readback[VS_RECORD_WORDS],generation;
    vd_profile_t old,verified;unsigned slot;int have;
    if(!vision_drive_profile_ready(p)){info.error=VS_INVALID;return 0;}
    if(!storage.read || !storage.replace){info.error=VS_IO_ERROR;return 0;}
    have=vs_load(&old);
    if(info.error==VS_IO_ERROR)return 0;
    generation=have?info.generation+1U:1U;slot=have?1U-info.slot:0U;
    if(have) {
        /* Identical saves do not spend erase cycles. Never compare padding. */
        encode(p,info.generation,record);encode(&old,info.generation,readback);
        if(!memcmp(record,readback,sizeof(record)))return 1;
    }
    encode(p,generation,record);
    if(!storage.replace(slot,record,VS_RECORD_WORDS)){info.error=VS_IO_ERROR;return 0;}
    if(!storage.read(slot,readback,VS_RECORD_WORDS) ||
       memcmp(record,readback,sizeof(record)) || !decode(readback,&verified)) {
        info.error=VS_VERIFY_ERROR;return 0;
    }
    info.valid=1;info.slot=(uint8_t)slot;info.generation=generation;info.error=VS_OK;
    return 1;
}

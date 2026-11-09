/* Diagnostic only: classify the ACTUAL adapter's rejection without a second
 * detector, changed thresholds, guessed motion or relaxed ground domain. */
#include "vision_planner_runtime.h"
#include "phase3_shadow_profile.h"
#include <stdio.h>
#include <string.h>
static uint8_t gray[VISION_WIDTH*VISION_HEIGHT],binary[VISION_WIDTH*VISION_HEIGHT];
enum { ACCEPT=0,SOURCE,PAIRED_ROWS,ADAPTER,DOMAIN }; /* Historical reason5 was near gating. */
typedef struct {unsigned reason,row;float x,y;unsigned flags;} detail_t;
static void resample_rows(uint8_t *rows,unsigned count,const vision_result_t *road)
{
    unsigned first=rows[0],last=rows[count-1],size=0;uint8_t pool[VISION_HEIGHT];
    if(first>=VISION_HEIGHT||last>=first||road->row_flags[first]!=3||road->row_flags[last]!=3)return;
    for(unsigned y=first;;--y){if(road->row_flags[y]==3)pool[size++]=(uint8_t)y;if(y==last)break;}
    if(size<count)return;
    for(unsigned j=0;j<count;++j)rows[j]=pool[j*(size-1U)/(count-1U)];
}
static detail_t explain(const vm_vision_t *state,const vision_result_t *road,
                        const vm_vision_config_t *c,unsigned full,unsigned sequence)
{
    detail_t d={ACCEPT,0,0,0,0};sp_geometry_history_t h=full?state->full_history:state->near_history;
    sp_preview_t preview;const uint8_t *rows=full?c->full_rows:c->near_rows;unsigned n=full?c->full_count:c->near_count;
    if(!road->valid||road->status!=VISION_TRACK||road->confidence<c->min_confidence||road->confidence>100){d.reason=SOURCE;return d;}
    /* Describe a visible failed prerequisite. Actual acceptance below still
     * comes from sp_vision_preview + the unmodified vm_vision_frame. */
    for(unsigned j=0;j<n;++j)if(road->row_flags[rows[j]]!=3){d.reason=PAIRED_ROWS;d.row=rows[j];d.flags=road->row_flags[rows[j]];return d;}
    if(!sp_vision_preview(&h,road,rows,n,c->min_confidence,c->homography,sequence,sequence*20U,sequence*20U+4U,
                         c->max_age_ms,c->max_step_m,&preview)){d.reason=ADAPTER;return d;}
    for(unsigned j=0;j<h.count;++j){
        if(h.points[j].x<c->min_x_m||h.points[j].x>c->max_x_m||h.points[j].y<c->min_y_m||h.points[j].y>c->max_y_m){
            d.reason=DOMAIN;d.row=rows[j];d.x=h.points[j].x;d.y=h.points[j].y;return d;
        }
    }
    return d;
}
int main(int argc,char **argv)
{
    if(argc!=3&&argc!=4)return 2;FILE *input=fopen(argv[1],"rb"),*output=fopen(argv[2],"w");if(!input||!output)return 3;
    vm_vision_t state;vm_vision_init(&state);vision_midterm_reset();unsigned index=0,armed=0;
    vps_state_t planner;if(!vps_init(&planner,VP_SHADOW,&phase3_shadow_configuration.planning))return 8;
    const vm_vision_config_t *c=&phase3_shadow_configuration.vision;
    fprintf(output,"index,raw_valid,confidence,near_reason,near_row,near_x_m,near_y_m,near_flags,full_reason,full_row,full_x_m,full_y_m,full_flags,near_accepted,full_accepted,arm_event,planner_status,supervisor_mode,target_right,target_left,supervisor_faults,candidate_status\n");
    while(fread(gray,1,sizeof(gray),input)==sizeof(gray)){
        vision_result_t road;vm_frame_t frame;vision_midterm_process(gray,binary,&road);
        vm_vision_config_t selected=*c;
        if(argc==4){resample_rows(selected.near_rows,selected.near_count,&road);resample_rows(selected.full_rows,selected.full_count,&road);}
        detail_t near=explain(&state,&road,&selected,0,index+1),full=explain(&state,&road,&selected,1,index+1);
        if(!vm_vision_frame(&state,&selected,&road,index+1,(index+1)*20U,(index+1)*20U+4U,&frame))return 4;
        if((near.reason==ACCEPT)!=frame.near_valid||(full.reason==ACCEPT)!=frame.far_valid)return 5;
        /* Real image geometry, explicit stationary synthetic encoder. This
         * is software composition only, never an MCU physical output lease. */
        vps_input_t in;vps_result_t planned;memset(&in,0,sizeof(in));memset(&planned,0,sizeof(planned));
        uint32_t cap=(index+1)*20U,now=cap+4U;unsigned arm_event=0;vps_status_t status=VPS_INACTIVE;
        in.identity=(vp_stamp_t){.output_epoch=1,.config_revision=c->revision,.frame_seq=index+1,
            .encoder_seq=index+1,.capture_ms=cap,.encoder_ms=now};
        in.observation=(sp_input_t){now,index+1,cap,index+1,now,0,0,0,1,frame.near_valid,frame.far_valid,1,0,0,1};
        in.near_preview=frame.near_preview;in.full_preview=frame.full_preview;
        in.valid=VP_VALID_ENCODER|(frame.near_valid?(VP_VALID_NEAR|VP_VALID_METRIC_GEOMETRY|VP_VALID_GROUND_DOMAIN):0U);
        in.external_admitted=1;in.requested_speed_m_s=phase3_shadow_configuration.requested_speed_m_s;
        if(!armed){if(vps_arm(&planner,&in)){armed=1;arm_event=1;}}
        else status=vps_tick(&planner,&in,&planned);
        fprintf(output,"%u,%u,%u,%u,%u,%.8g,%.8g,%u,%u,%u,%.8g,%.8g,%u,%u,%u,%u,%u,%u,%u,%u,%lu,%u\n",index,
            road.valid,road.confidence,near.reason,near.row,(double)near.x,(double)near.y,near.flags,
            full.reason,full.row,(double)full.x,(double)full.y,full.flags,frame.near_valid,frame.far_valid,
            arm_event,(unsigned)status,(unsigned)planner.supervisor.mode,planned.candidate.right_ticks_10ms,planned.candidate.left_ticks_10ms,
            (unsigned long)planner.supervisor.faults,(unsigned)planned.candidate.status);++index;
    }
    if(ferror(input)||ferror(output))return 6;fclose(input);if(fclose(output))return 6;
    printf("METRIC INPUT AUDIT: %u original frames; classifications equal actual native acceptance.\n",index);
    return index==750?0:7;
}

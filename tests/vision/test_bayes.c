#include "track_bayes.h"
#include "midterm_open_loop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static tb_filter_t filter;
static tb_model_t model;
static uint8_t binary[VISION_WIDTH * VISION_HEIGHT];
static vision_result_t lane;

static void probability(void)
{
    float sum = 0; int j;
    for (j=0;j<TB_STATES;++j) {
        CHECK(filter.result.posterior[j] >= 0 && filter.result.posterior[j] <= 1);
        sum += filter.result.posterior[j];
    }
    CHECK(fabsf(sum-1) < .00001f);
    CHECK(filter.result.entropy >= 0 && filter.result.entropy <= 1.00001f);
}

static void model_tests(void)
{
    tb_observation_t o = {{0},1,(1U<<TB_FEATURES)-1};
    float left, right; unsigned i, j;
    tb_default_model(&model); CHECK(tb_init(&filter,&model)); CHECK(!filter.result.trained);
    for (i=1;i<=100;++i) { CHECK(tb_update(&filter,&o,i,i*20)); probability(); }
    CHECK(filter.result.candidate == TB_STRAIGHT);
    CHECK(filter.result.state == TB_UNKNOWN && !filter.result.accepted);
    /* 已拟合参数路径用测试专用标志覆盖；不冒称默认模型实地训练过。 */
    model.trained=1; CHECK(tb_init(&filter,&model));
    for(i=1;i<=100;++i)CHECK(tb_update(&filter,&o,i,i*20));
    CHECK(filter.result.state == TB_STRAIGHT);
    left = filter.result.probability;
    CHECK(!tb_update(&filter,&o,100,2000)); CHECK(filter.result.probability == left);
    tb_poll(&filter,2100); CHECK(filter.result.state == TB_UNKNOWN);
    CHECK(tb_update(&filter,&o,101,2120));
    CHECK(!tb_update(&filter,&o,99,2100)); CHECK(filter.result.state == TB_UNKNOWN);
    tb_reset(&filter); /* 新录制序列必须显式复位，倒序输入不能偷换时间基准。 */
    for (i=1;i<=100;++i) {
        memcpy(o.x,model.mean[TB_LEFT],sizeof(o.x));
        CHECK(tb_update(&filter,&o,i,3000+i*20)); probability();
    }
    CHECK(filter.result.state == TB_LEFT); left = filter.result.probability;
    tb_reset(&filter);
    for (i=1;i<=100;++i) {
        memcpy(o.x,model.mean[TB_RIGHT],sizeof(o.x));
        CHECK(tb_update(&filter,&o,i,3000+i*20));
    }
    CHECK(filter.result.state == TB_RIGHT); right = filter.result.probability;
    CHECK(fabsf(left-right)<.0001f);
    /* 所有语义类都必须有可区分的合成似然；这不是实地精度测试。 */
    for (j=TB_STRAIGHT;j<TB_STATES;++j) {
        tb_reset(&filter); memcpy(o.x,model.mean[j],sizeof(o.x));
        for (i=1;i<=80;++i) { CHECK(tb_update(&filter,&o,i,i*20)); probability(); }
        CHECK(filter.result.candidate == (tb_state_t)j);
        if(j==TB_RING_L_ENTRY||j==TB_RING_R_ENTRY)CHECK(filter.result.accepted);
        if(filter.result.accepted){
            int hint=j==TB_RING_L_ENTRY?-1:j==TB_RING_R_ENTRY?1:0;
            CHECK(tb_route_hint(&filter,1600)==hint);
            CHECK(tb_route_hint(&filter,1600+model.stale_ms)==0);
            filter.result.trained=0;CHECK(tb_route_hint(&filter,1600)==0);
            filter.result.trained=1;filter.result.accepted=0;CHECK(tb_route_hint(&filter,1600)==0);
        }
    }
    CHECK(tb_route_hint(NULL,0)==0);
    /* 复现现场：无几何 mask=60，单侧全开、多个亮区。未训练即使 p 很高
     * 也不能在室内场景上确认十字。这个回归与逐飞抓取的现场特征一致。 */
    {
        tb_model_t untrained=model; tb_filter_t observer;
        tb_observation_t indoor={{0,0,.414f,1,.241f,.055f},.706f,60U};
        untrained.trained=0;CHECK(tb_init(&observer,&untrained));
        for(i=1;i<=100;++i)CHECK(tb_update(&observer,&indoor,i,i*20));
        CHECK(observer.result.state==TB_UNKNOWN && !observer.result.accepted);
        CHECK(observer.result.candidate==TB_CROSS);
    }
    o.quality=0; CHECK(tb_update(&filter,&o,81,1620)); CHECK(!filter.result.accepted); probability();
    CHECK(!tb_update(&filter,&o,81,1620));
    tb_poll(&filter,1800); CHECK(!tb_update(&filter,&o,81,1620));
    o.quality=1; o.mask=0; CHECK(tb_update(&filter,&o,82,1640)); CHECK(!filter.result.accepted);
    o.mask=(1U<<TB_FEATURES)-1; o.x[0]=NAN;
    CHECK(tb_update(&filter,&o,83,1660)); CHECK(!filter.result.accepted);
    for (j=0;j<TB_FEATURES;++j) o.x[j]=7;
    CHECK(tb_update(&filter,&o,84,1680)); CHECK(!filter.result.accepted); probability();
    model.variance[1][0]=0; CHECK(!tb_init(&filter,&model));
    tb_default_model(&model); model.transition[1][0]=-1; CHECK(!tb_init(&filter,&model));
    tb_default_model(&model); model.near_y=NAN; CHECK(!tb_init(&filter,&model));
    tb_default_model(&model); CHECK(tb_init(&filter,&model));
    memset(o.x,0,sizeof(o.x));
    CHECK(tb_update(&filter,&o,UINT32_MAX,UINT32_MAX-10));
    CHECK(tb_update(&filter,&o,0,9)); probability();
    /* 随机观测与缺失特征：始终保持有限的归一化概率。 */
    for(i=1;i<=3000;++i) {
        for(j=0;j<TB_FEATURES;++j) o.x[j]=(float)(rand()%2001-1000)/1000;
        o.mask=(uint32_t)(rand()%64); o.quality=(float)(rand()%101)/100;
        (void)tb_update(&filter,&o,i,9+i*20); probability();
    }
}

static void extraction_tests(void)
{
    tb_observation_t a,b; int y,x;
    tb_default_model(&model); memset(&lane,0,sizeof(lane));
    lane.contrast=200; lane.status=VISION_TRACK;
    for(y=0;y<VISION_HEIGHT;++y) {
        lane.left[y]=50; lane.right[y]=138; lane.row_flags[y]=3;
        for(x=0;x<VISION_WIDTH;++x) binary[y*VISION_WIDTH+x]=(uint8_t)((x>=50&&x<=138)?255:0);
    }
    tb_extract(&model,binary,&lane,&a); CHECK(a.mask==63); CHECK(fabsf(a.x[TB_HEADING])<.00001f);
    CHECK(a.x[TB_SPLIT]==0 && a.x[TB_STRIPES]==0);
    /* 相机与参考轴一起平移，归一化 heading/curvature 不应受安装横移影响。 */
    for(y=0;y<VISION_HEIGHT;++y) { lane.left[y]+=10; lane.right[y]+=10; }
    model.axis_near_x+=10.0f/(VISION_WIDTH-1); model.axis_far_x+=10.0f/(VISION_WIDTH-1);
    tb_extract(&model,binary,&lane,&b);
    CHECK(fabsf(a.x[TB_HEADING]-b.x[TB_HEADING])<.00001f);
    CHECK(fabsf(a.x[TB_CURVATURE]-b.x[TB_CURVATURE])<.00001f);
    memset(lane.row_flags,0,sizeof(lane.row_flags)); tb_extract(&model,binary,&lane,&b);
    CHECK(!(b.mask&((1U<<TB_HEADING)|(1U<<TB_CURVATURE))));
    lane.status=VISION_LOW_CONTRAST; tb_extract(&model,binary,&lane,&b); CHECK(b.quality==0);
}

static void motor_tests(void)
{
    mol_config_t c; float duty, a,b; int i;
    mol_default_config(&c); CHECK(mol_config_valid(&c));
    CHECK(mol_step(&c,0,0,0,1,1,.2f,.3f,.1f,0,.02f,&duty)); CHECK(fabsf(duty-.002f)<.000001f);
    CHECK(mol_step(&c,0,0,0,0,1,.2f,.3f,.1f,.1f,.02f,&duty)); CHECK(duty==0);
    CHECK(!mol_step(&c,NAN,0,0,1,1,.2f,.3f,.1f,0,.02f,&duty)); CHECK(duty==0);
    CHECK(!mol_step(&c,0,0,0,1,1,.4f,.3f,.1f,0,.02f,&duty)); CHECK(duty==0);
    CHECK(!mol_step(&c,0,0,0,1,1,.2f,.3f,.1f,0,0,&duty));
    CHECK(mol_step(&c,1,.2f,.1f,1,1,.2f,.3f,.1f,.2f,.02f,&a));
    CHECK(mol_step(&c,-1,-.2f,-.1f,1,1,.2f,.3f,.1f,.2f,.02f,&b)); CHECK(a==b);
    /* Independent open-loop attenuation cannot increase the requested duty,
     * bypass its cap, produce motion from zero, or accept nonfinite inputs. */
    CHECK(mol_split(.2f,.3f,1,.8f,&a,&b));
    CHECK(a==.2f && fabsf(b-.16f)<.000001f);
    CHECK(mol_split(0,0,1,1,&a,&b)); CHECK(a==0 && b==0);
    CHECK(mol_split(.2f,.3f,0,1,&a,&b)); CHECK(a==0 && b==.2f);
    CHECK(!mol_split(.4f,.3f,1,1,&a,&b)); CHECK(a==0 && b==0);
    CHECK(!mol_split(NAN,.3f,1,1,&a,&b)); CHECK(a==0 && b==0);
    CHECK(!mol_split(.2f,INFINITY,1,1,&a,&b)); CHECK(a==0 && b==0);
    CHECK(!mol_split(.2f,.3f,1.01f,1,&a,&b)); CHECK(a==0 && b==0);
    CHECK(!mol_split(.2f,.3f,1,-.01f,&a,&b)); CHECK(a==0 && b==0);
    CHECK(!mol_split(.2f,.3f,NAN,1,&a,&b)); CHECK(a==0 && b==0);
    CHECK(!mol_split(.2f,.3f,1,1,NULL,&b)); CHECK(b==0);
    CHECK(!mol_split(.2f,.3f,1,1,&a,NULL)); CHECK(a==0);
    CHECK(!mol_split(.2f,.3f,1,1,&a,&a)); CHECK(a==0);
    for(i=0;i<3000;++i) {
        float q=(float)(rand()%101)/100;
        CHECK(mol_step(&c,(float)(rand()%100)/30,0,0,q,i%2,.2f,.3f,.1f,.1f,.02f,&duty));
        CHECK(duty>=0 && duty<=.102001f);
        CHECK(mol_split(duty,.3f,(float)(rand()%101)/100,(float)(rand()%101)/100,&a,&b));
        CHECK(a>=0 && a<=duty && b>=0 && b<=duty);
    }
}
int main(void)
{
    model_tests(); extraction_tests(); motor_tests();
    printf("BAYES + OPEN LOOP PASS: %u checks (synthetic, not track validation)\n",checks); return 0;
}

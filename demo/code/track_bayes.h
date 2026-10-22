#ifndef TRACK_BAYES_H
#define TRACK_BAYES_H
#include "vision_midterm.h"

/* 完整赛道的隐状态，不再用启动时间或固定像素坐标推进环岛步骤。
 * UNKNOWN 是拒识结果；环岛三阶段是语义阶段，不是旧八步补线的别名。
 * 本模块只估计赛道，不修改图像、不补造边界、不直接操作执行器。 */
typedef enum {
    TB_UNKNOWN, TB_STRAIGHT, TB_LEFT, TB_RIGHT, TB_CROSS, TB_ZEBRA,
    TB_RING_L_ENTRY, TB_RING_L_IN, TB_RING_L_EXIT,
    TB_RING_R_ENTRY, TB_RING_R_IN, TB_RING_R_EXIT, TB_STATES
} tb_state_t;
typedef enum {
    TB_HEADING, TB_CURVATURE, TB_OPEN_LEFT, TB_OPEN_RIGHT,
    TB_SPLIT, TB_STRIPES, TB_FEATURES
} tb_feature_t;

/* 几何项以实测道路半宽归一化，拓扑项为 ROI 内比例。mask 明确缺失项，
 * 缺失不等于零。quality 是观测可用性而非分类正确概率。 */
typedef struct { float x[TB_FEATURES], quality; uint32_t mask; } tb_observation_t;
typedef struct {
    float prior[TB_STATES];
    float transition[TB_STATES][TB_STATES]; /* 行归一化；离开当前状态后的分布。 */
    float mean[TB_STATES][TB_FEATURES], variance[TB_STATES][TB_FEATURES];
    float dwell_s, evidence_s; /* 时间尺度，不是赛道行驶定时器。 */
    float min_quality, min_posterior, min_margin, max_entropy, max_distance;
    float axis_near_x, axis_far_x, near_y, far_y, band_half_height;
    float min_run_fraction, stripe_scale;
    uint32_t stale_ms;
    uint8_t min_geometry_rows, min_features, trained;
} tb_model_t;
typedef struct {
    float posterior[TB_STATES], probability, margin, entropy;
    tb_state_t candidate, state; /* candidate 是最大项；state 通过拒识门限后才有效。 */
    uint8_t accepted, trained; /* 未训练只保留候选，accepted 必为 0。 */
} tb_result_t;
typedef struct {
    tb_model_t model; /* 按值持有配置，调用者可释放传入配置。 */
    tb_result_t result;
    uint32_t last_ms, last_sequence;
    uint8_t configured, have_frame;
} tb_filter_t;

/* 默认模型是可替换的专家初值，trained=0，绝非场地训练/校准后的概率。 */
void tb_default_model(tb_model_t *model);
int tb_init(tb_filter_t *filter, const tb_model_t *model);
void tb_reset(tb_filter_t *filter);
/* 输入 binary 为 188x120 的 0/255 图；result 与之必须来自同一帧。
 * model 必须已通过 tb_init；两点安装轴只纠正偏移/偏航近似，不能替代
 * 镜头畸变和俯仰标定。 */
void tb_extract(const tb_model_t *model, const uint8_t *binary,
                const vision_result_t *lane, tb_observation_t *observation);
/* 返回 1 表示消费了新观测；重复/倒序返回 0。毫秒时间允许 uint32 回绕。
 * log-domain 贝叶斯递推；帧相关性用 evidence_s 降温，异常输入退回 UNKNOWN。 */
int tb_update(tb_filter_t *filter, const tb_observation_t *observation,
              uint32_t sequence, uint32_t frame_ms);
void tb_poll(tb_filter_t *filter, uint32_t now_ms);
/* Navigation adapter: only a trained, accepted, fresh ENTRY hypothesis can
 * reinforce same-side pixel geometry. Other states/unknown return zero. */
int tb_route_hint(const tb_filter_t *filter, uint32_t now_ms);
const char *tb_state_name(tb_state_t state);
#endif

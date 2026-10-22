#include "track_bayes.h"
#include <math.h>
#include <float.h>
#include <string.h>
#if VISION_BAYES_FITTED_MODEL
#include "track_model_fitted.h"
#endif

static int finite_number(float x) { return x == x && x >= -FLT_MAX && x <= FLT_MAX; }
static float limit(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static int bounded(float x, float lo, float hi) { return finite_number(x) && x >= lo && x <= hi; }

int tb_route_hint(const tb_filter_t *f, uint32_t now)
{
    if(!f||!f->configured||!f->have_frame||!f->model.trained||
       !f->result.trained||!f->result.accepted||
       (uint32_t)(now-f->last_ms)>=f->model.stale_ms)return 0;
    return f->result.state==TB_RING_L_ENTRY?-1:f->result.state==TB_RING_R_ENTRY?1:0;
}

/* 以下矩阵是观测模型的可替换初值，集中在一处；无单张照片坐标。
 * 对角高斯的独立性只是近似。真实部署应使用标注序列估计均值、方差、
 * 转移分布和拒识门限，并在不同光照/安装位置的留出集上验证。
 * 左右镜像共享先验，heading/curvature 变号且左右开口交换。
 * 环岛入口/出口可能在单帧中不可辨，必须保留后验分布而非强选一个状态。 */
void tb_default_model(tb_model_t *m)
{
    static const float means[TB_STATES][TB_FEATURES] = {
        {0,0,.5f,.5f,.5f,.5f}, {0,0,0,0,0,0},
        {-.45f,-.20f,0,0,0,0}, {.45f,.20f,0,0,0,0},
        {0,0,.65f,.65f,.25f,.05f}, {0,0,0,0,.65f,.70f},
        {-.20f,-.15f,.55f,.05f,.40f,.05f},
        {-.70f,-.40f,.15f,.05f,.30f,.05f},
        {-.15f,.20f,.10f,.50f,.35f,.05f},
        {.20f,.15f,.05f,.55f,.40f,.05f},
        {.70f,.40f,.05f,.15f,.30f,.05f},
        {.15f,-.20f,.50f,.10f,.35f,.05f}
    };
    int i, j, k;
    memset(m, 0, sizeof(*m));
    memcpy(m->mean, means, sizeof(means));
    for (i = 0; i < TB_STATES; ++i) {
        m->prior[i] = 1.0f / TB_STATES;
        for (k = 0; k < TB_FEATURES; ++k)
            m->variance[i][k] = i == TB_UNKNOWN ? 1.0f : (k <= TB_CURVATURE ? .09f : .035f);
        /* 普通路段可以遇到任意元素；未知状态允许从赛道任意位置恢复。 */
        for (j = 0; j < TB_STATES; ++j) m->transition[i][j] = 1.0f / TB_STATES;
    }
    /* 环岛语义阶段的拓扑先验：保持/前进/拒识，不以固定行驶时间跳阶段。
     * 少量恢复质量分配给所有状态，使误判可恢复，不形成不可退出的吸收态。 */
    for (i = TB_RING_L_ENTRY; i < TB_STATES; ++i) {
        int next = (i == TB_RING_L_EXIT || i == TB_RING_R_EXIT) ? TB_STRAIGHT : i + 1;
        for (j = 0; j < TB_STATES; ++j) m->transition[i][j] = .10f / TB_STATES;
        m->transition[i][next] += .70f;
        m->transition[i][TB_UNKNOWN] += .20f;
    }
    m->dwell_s = .35f; m->evidence_s = .10f;
    m->min_quality = .20f; m->min_posterior = .65f; m->min_margin = .20f;
    m->max_entropy = .65f; m->max_distance = 16.0f;
    m->axis_near_x = VISION_AXIS_NEAR_X; m->axis_far_x = VISION_AXIS_FAR_X;
    m->near_y = VISION_AXIS_NEAR_Y; m->far_y = VISION_AXIS_FAR_Y;
    m->band_half_height = VISION_CONTROL_BAND_HALF_HEIGHT;
    m->min_run_fraction = (float)VISION_MIN_WIDTH_PERMILLE / 1000;
    m->stripe_scale = .15f; m->stale_ms = VISION_CONTROL_FRAME_TIMEOUT_MS;
    m->min_geometry_rows = VISION_CONTROL_MIN_BAND_ROWS; m->min_features = 4;
    m->trained = 0;
#if VISION_BAYES_FITTED_MODEL
    tb_apply_fitted_model(m); /* 标注数据替换先验/似然/转移；仍需 tb_init 校验。 */
#endif
}

/* 概率向量允许未归一化的正权重，加载时归一化；零总和/负数立即拒绝。 */
static int normalize(float *p, int n)
{
    float sum = 0; int i;
    for (i = 0; i < n; ++i) { if (!bounded(p[i], 0, 1.0e6f)) return 0; sum += p[i]; }
    if (!(sum > FLT_MIN) || !finite_number(sum)) return 0;
    for (i = 0; i < n; ++i) p[i] /= sum;
    return 1;
}

/* 拒识清空证据但保留已消费帧的序号/时间，坏帧不能反复被当作新帧。 */
static void reject_observation(tb_filter_t *f)
{
    memset(&f->result, 0, sizeof(f->result));
    f->result.posterior[TB_UNKNOWN] = 1;
    f->result.trained = f->model.trained;
}

void tb_reset(tb_filter_t *f)
{
    reject_observation(f);
    f->have_frame = 0;
}

int tb_init(tb_filter_t *f, const tb_model_t *model)
{
    tb_model_t saved = *model; int i, k, ok = 1;
    memset(f, 0, sizeof(*f)); f->model = saved;
    ok &= normalize(f->model.prior, TB_STATES);
    for (i = 0; i < TB_STATES; ++i) {
        ok &= normalize(f->model.transition[i], TB_STATES);
        for (k = 0; k < TB_FEATURES; ++k)
            ok &= bounded(saved.mean[i][k], -8, 8) && bounded(saved.variance[i][k], .0001f, 100);
    }
    ok &= bounded(saved.dwell_s, .001f, 60) && bounded(saved.evidence_s, .001f, 60);
    ok &= bounded(saved.min_quality, 0, 1) && bounded(saved.min_posterior, 0, 1);
    ok &= bounded(saved.min_margin, 0, 1) && bounded(saved.max_entropy, 0, 1);
    ok &= bounded(saved.max_distance, .01f, 1000) && saved.stale_ms >= 1 && saved.stale_ms < 0x80000000U;
    ok &= saved.min_features > 0 && saved.min_features <= TB_FEATURES && saved.trained <= 1;
    ok &= bounded(saved.axis_near_x, 0, 1) && bounded(saved.axis_far_x, 0, 1);
    ok &= bounded(saved.near_y, 0, 1) && bounded(saved.far_y, 0, 1) && saved.near_y > saved.far_y;
    ok &= bounded(saved.band_half_height, .001f, .2f) && saved.near_y - saved.far_y > 2 * saved.band_half_height;
    ok &= saved.far_y - saved.band_half_height >= (float)VISION_ROI_TOP / (VISION_HEIGHT - 1);
    ok &= saved.near_y + saved.band_half_height <= (float)VISION_ROI_BOTTOM / (VISION_HEIGHT - 1);
    ok &= bounded(saved.min_run_fraction, .001f, .5f) && bounded(saved.stripe_scale, .001f, 1);
    ok &= saved.min_geometry_rows > 0 && saved.min_geometry_rows <= VISION_HEIGHT;
    f->configured = (uint8_t)(ok != 0); tb_reset(f); return f->configured;
}

static float axis_at(const tb_model_t *m, float y)
{
    return m->axis_far_x + (m->axis_near_x - m->axis_far_x) * (y - m->far_y) / (m->near_y - m->far_y);
}

/* 几何只用左右都实测的行，避免补线结果反过来成为识别自己的证据。 */
static int band(const tb_model_t *m, const vision_result_t *r, float y0, float *value)
{
    int y, n = 0; float sum = 0;
    for (y = VISION_ROI_TOP; y <= VISION_ROI_BOTTOM; ++y) {
        float yf = (float)y / (VISION_HEIGHT - 1);
        if (fabsf(yf - y0) > m->band_half_height || r->row_flags[y] != 3 ||
            r->left[y] >= r->right[y] || r->right[y] >= VISION_WIDTH) continue;
        sum += ((r->left[y] + r->right[y]) * .5f - axis_at(m, yf) * (VISION_WIDTH - 1)) /
               ((r->right[y] - r->left[y] + 1) * .5f);
        ++n;
    }
    if (n < m->min_geometry_rows) return 0;
    *value = sum / n; return 1;
}

void tb_extract(const tb_model_t *m, const uint8_t *binary, const vision_result_t *r, tb_observation_t *o)
{
    int y, x, rows = VISION_ROI_BOTTOM - VISION_ROI_TOP + 1;
    int min_run = (int)ceilf(m->min_run_fraction * VISION_WIDTH);
    float near_value, middle_value, far_value;
    memset(o, 0, sizeof(*o));
    if (!binary || !r) return;
    for (y = VISION_ROI_TOP; y <= VISION_ROI_BOTTOM; ++y) {
        int runs = 0, length = 0, transitions = 0;
        for (x = 0; x < VISION_WIDTH; ++x) {
            int white = binary[y * VISION_WIDTH + x] != 0;
            if (x && white != (binary[y * VISION_WIDTH + x - 1] != 0)) ++transitions;
            if (white) ++length;
            if ((!white || x == VISION_WIDTH - 1) && length) {
                if (length >= min_run) ++runs;
                length = 0;
            }
        }
        o->x[TB_OPEN_LEFT] += binary[y * VISION_WIDTH] != 0;
        o->x[TB_OPEN_RIGHT] += binary[y * VISION_WIDTH + VISION_WIDTH - 1] != 0;
        o->x[TB_SPLIT] += runs > 1;
        /* 两个正常路缘不算斑马线；计数按图宽归一化而不是固定跳变次数。 */
        o->x[TB_STRIPES] += limit((float)(transitions - 2) / (VISION_WIDTH * m->stripe_scale), 0, 1);
    }
    for (x = TB_OPEN_LEFT; x < TB_FEATURES; ++x) {
        o->x[x] /= rows; o->mask |= 1U << x;
    }
    if (band(m, r, m->near_y, &near_value) && band(m, r, m->far_y, &far_value)) {
        o->x[TB_HEADING] = far_value - near_value; o->mask |= 1U << TB_HEADING;
        if (band(m, r, (m->near_y + m->far_y) * .5f, &middle_value)) {
            o->x[TB_CURVATURE] = far_value - 2 * middle_value + near_value;
            o->mask |= 1U << TB_CURVATURE;
        }
    }
    /* 拓扑可在车道跟踪失败时仍可见；但低对比图不能提供可靠证据。 */
    o->quality = limit((float)r->contrast / 255, 0, 1);
    if (r->status == VISION_LOW_CONTRAST) o->quality = 0;
}

int tb_update(tb_filter_t *f, const tb_observation_t *o, uint32_t sequence, uint32_t ms)
{
    float prediction[TB_STATES] = {0}, logp[TB_STATES], distance[TB_STATES];
    float dt, mix, power, maxlog = -FLT_MAX, sum = 0, second = 0;
    int i, j, k, n = 0, best = TB_UNKNOWN;
    if (!f->configured) { tb_reset(f); return 0; }
    if (f->have_frame) {
        uint32_t ds = sequence - f->last_sequence, elapsed = ms - f->last_ms;
        if (ds == 0) return 0;
        if (ds >= 0x80000000U || elapsed == 0 || elapsed >= 0x80000000U) { reject_observation(f); return 0; }
        if (elapsed >= f->model.stale_ms) tb_reset(f);
    }
    dt = f->have_frame ? (ms - f->last_ms) * .001f : f->model.evidence_s;
    mix = 1 - expf(-dt / f->model.dwell_s);
    power = limit(dt / f->model.evidence_s, 0, 1);
    if (!f->have_frame) memcpy(prediction, f->model.prior, sizeof(prediction));
    else for (j = 0; j < TB_STATES; ++j) {
        prediction[j] += (1 - mix) * f->result.posterior[j];
        for (i = 0; i < TB_STATES; ++i)
            prediction[j] += mix * f->result.posterior[i] * f->model.transition[i][j];
    }
    f->last_ms = ms; f->last_sequence = sequence; f->have_frame = 1;
    if (!o || !bounded(o->quality, f->model.min_quality, 1) || o->mask >> TB_FEATURES) {
        reject_observation(f); return 1;
    }
    for (k = 0; k < TB_FEATURES; ++k) if (o->mask & (1U << k)) {
        if (!bounded(o->x[k], k < TB_OPEN_LEFT ? -8.0f : 0.0f,
                     k < TB_OPEN_LEFT ? 8.0f : 1.0f)) { reject_observation(f); return 1; } ++n;
    }
    if (n < f->model.min_features) { reject_observation(f); return 1; }
    /* alpha_t(j) ∝ likelihood(x_t|j)^power * Σ_i T_ij(dt) alpha_(t-1)(i).
     * 在 log 域计算；高斯常数对所有状态相同，省去公共的 log(2*pi)。 */
    for (j = 0; j < TB_STATES; ++j) {
        float ll = 0; distance[j] = 0;
        for (k = 0; k < TB_FEATURES; ++k) if (o->mask & (1U << k)) {
            float d = o->x[k] - f->model.mean[j][k], v = f->model.variance[j][k];
            distance[j] += d * d / v;
            ll -= .5f * (d * d / v + logf(v));
        }
        distance[j] /= n;
        logp[j] = prediction[j] > 0 ? logf(prediction[j]) + power * o->quality * ll : -FLT_MAX;
        if (logp[j] > maxlog) maxlog = logp[j];
    }
    for (j = 0; j < TB_STATES; ++j) { f->result.posterior[j] = expf(logp[j] - maxlog); sum += f->result.posterior[j]; }
    if (!(sum > 0) || !finite_number(sum)) { reject_observation(f); return 1; }
    f->result.entropy = 0;
    for (j = 0; j < TB_STATES; ++j) {
        float p = (f->result.posterior[j] /= sum);
        if (p > 0) f->result.entropy -= p * logf(p) / logf((float)TB_STATES);
    }
    /* 在归一化全部完成后重新找最大项（不能比较新旧尺度的概率）。 */
    best = TB_UNKNOWN;
    for (j = 0; j < TB_STATES; ++j) if (f->result.posterior[j] > f->result.posterior[best]) best = j;
    for (j = 0; j < TB_STATES; ++j) if (j != best && f->result.posterior[j] > second) second = f->result.posterior[j];
    f->result.candidate = (tb_state_t)best;
    f->result.probability = f->result.posterior[best];
    f->result.margin = f->result.probability - second;
    /* 专家初值在普通室内物体上也可能得到很尖锐的后验。未训练模型只
     * 显示 candidate/p，不对外确认元素；不能把数值尖锐误当场景已验证。
     * 中期转向由独立实测车道控制，不依赖该未训练语义标签。 */
    f->result.accepted = (uint8_t)(f->model.trained && best != TB_UNKNOWN && f->result.probability >= f->model.min_posterior &&
        f->result.margin >= f->model.min_margin && f->result.entropy <= f->model.max_entropy &&
        distance[best] <= f->model.max_distance);
    f->result.state = f->result.accepted ? (tb_state_t)best : TB_UNKNOWN;
    return 1;
}

void tb_poll(tb_filter_t *f, uint32_t now)
{
    if (f->have_frame && (uint32_t)(now - f->last_ms) >= f->model.stale_ms) reject_observation(f);
}

const char *tb_state_name(tb_state_t s)
{
    static const char *const names[TB_STATES] = {"UNKNOWN","STRAIGHT","LEFT","RIGHT","CROSS","ZEBRA",
        "RING_L_ENTRY","RING_L_IN","RING_L_EXIT","RING_R_ENTRY","RING_R_IN","RING_R_EXIT"};
    return (unsigned)s < TB_STATES ? names[s] : "UNKNOWN";
}

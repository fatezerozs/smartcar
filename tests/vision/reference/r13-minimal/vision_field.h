#ifndef VISION_FIELD_H
#define VISION_FIELD_H
#include "vision_drive.h"
#define VF_RESPONSE_CAPACITY 768

/* 现场参数命令以 @ 起始、换行结束；每帧可分段接收，不使用阻塞 scanf。
 * @ 内字符绝不落入旧单字符 a/h/s 控制；超长或超时丢弃至换行。
 * ! 或 Ctrl-C 在任何解析状态立即停车。所有参数仅保存在 RAM。 */
typedef struct {
    char line[VISION_FIELD_LINE_CAPACITY];
    uint32_t started_ms;
    uint8_t active, discarded, length, changed;
    /* Caller enables only in midterm text mode with no STOP/mode switch in
     * this RX batch. Permission is latched per line across fragmented input. */
    uint8_t probe_permitted, probe_line_permitted;
} vf_parser_t;
void vf_init(vf_parser_t *parser);
/* 返回 0=非本协议字符，1=已消费，2=产生 response；response 至少 VF_RESPONSE_CAPACITY 字节。
 * 一旦收到 @ 就停车；返回后上层仅在文字模式发送响应。 */
int vf_feed(vf_parser_t *parser, uint8_t byte, uint32_t now_ms, char *response);
#endif

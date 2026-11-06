# B2 metrics=2 实板短窗：PASS

用户确认手动换烧后，于2026-11-06约04:28:52（北京时间）发送一次B2 START。R0000003记录101帧，正常关闭/同步卸载、停止CSI，完整B2METRIC END及统计哨兵/栈检查通过。没有STOP重试或第二次START。

随后在closed模式直接下载R0000003和保留的R0000002，不复位、不新建写入Session、不删除覆盖任何SD文件。两次下载都恢复115200并关闭COM14。R3的101帧与R2的100帧均完整PC校验通过；旧R2测量仍FAIL。

metrics-review.json仅判定测量完整性，不独自作硬件验收。最终配对结论见上级evidence/b2-final-acceptance.json，完整报告见../../B2_CAMERA_SESSION_VALIDATION.md。本次PASS范围为用户批准的独立B2静止短窗，不包括长期稳定、生产控制并行或车辆运动。

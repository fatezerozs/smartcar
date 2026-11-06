# 本车几何参考 v0.1

用户于2026-11-06补充截图作为参考。已直接核对原始PNG并保留原图SHA256；下面是截图转录，不是新的实际测量。截图中的“C1暂定基线”是参考资料命名，不表示任务书C1动态标定已开始或通过。

| 参数 | 参考值 | 截图标注 / 本轮证据等级 |
| --- | ---: | --- |
| 整车长度 | 280mm | 截图标为厂家标称，原厂规格尚未独立核验 |
| 整车宽度 | 180mm | 同上 |
| 轮胎直径 | 64mm | 同上 |
| 轮胎宽度 | 27mm | 同上 |
| 理论滚动周长 | 201.06mm | 由pi×64重新计算为201.0619298297mm，computed |
| 轴距L | 205mm | 截图标为图像估算，非本车实测 |
| 后轮轮距B | 152mm | 同上，测量定义和误差待复核 |
| 前轮轮距 | 150mm | 同上 |

用于Host几何模型、尺寸检查与测量方案，状态provisional。实际负载滚动周长、左右差、Encoder counts/轮与倍频/减速关系、转向命令—实际轮角、安全PWM限幅、误差和电池/地面条件均UNKNOWN。

未把理论周长201.06当成负载实测，也没有按“1024”推导Encoder比例。物理速度换算需每轮实际counts和滚动尺度：v=(delta_counts/counts_per_turn)×circumference/delta_time；目前比例未知，因此不生成数值m/s限速或写生产参数。

原图：evidence/user-reference-geometry-v0.1.png；结构化参考：REFERENCE_GEOMETRY_V0_1.json。读取层为原图视觉核对和结构化转录，圆周长为computed；没有md_extracted或pdf_verified材料。

## 已有规范化参考与历史端点

发现并读取现有docs/reference/C1_VEHICLE_MODEL.md及profiles/c1-host-geometry-v0.1.json，与截图主要几何值一致；这些并行新增材料保持原状，本包仅交叉引用。历史2026-10-22卸载记录确认右/中/左命令1430/1550/1670us，当前route profile一致。该归档明确不是当前运行验收说明，端点指令值不等于实际轮角或负载性能；轮端CPR依然null，不把1024标签作为CPR。

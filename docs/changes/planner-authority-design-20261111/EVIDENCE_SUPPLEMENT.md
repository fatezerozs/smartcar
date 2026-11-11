# 可信延续与受控停车：补证据建议

2026-11-11。针对[设计共识](README.md)第35—37行；基线为 `METRIC_EXPERIMENT_20261111_4`。本文仅作研究建议，无生产阈值、固件修改或硬件操作。

## 必要性与最小方案

需要补的是“旧证据还允许怎样运动”的边界，**完整SLAM、EKF、IMU不是统一前置条件**。最小版可限定低速、路形和场地，用实测保守包络约束旧转向延续；包络覆盖横向偏离、车身占用及误差，并包含减速至停稳全过程。该分期是本项目建议，外部模型不替本车作保证。

已有 `sp_preview_advance` 按标量行程裁剪距离、插值曲率；但没有平移/旋转及横向误差传播。`odometer_m` 是双轮各自绝对增量换算后的平均累计量，不能直接解释为沿旧路径可信前进距离。`heading` 是 `far_error-near_error`，不是真实航向。[r4规划源码](../../../.build/metric-experiment-20261111-r4/workspace/tools/speed_planning/speed_planner.c)、[里程源码](../../../.build/metric-experiment-20261111-r4/workspace/demo/code/vision_metric_io.c)、[控制源码](../../../.build/metric-experiment-20261111-r4/workspace/demo/code/vision_control.c)

车式运动学依赖轮角、轴距和无滑移假设；轮速差可作理想条件下的估计或一致性检查，不能直接充当阿克曼车真实航向。[ROS2官方运动学](https://control.ros.org/rolling/doc/ros2_controllers/doc/mobile_robot_kinematics.html)

## 最小新增三类证据

1. **真实PI减速及停稳。** 复用已有响应与尾段，补拟采用的“参考下降→原PI→目标零→实际停稳”，覆盖初始速度/加速度、积分状态、延迟、jerk和跟踪误差。历史PWM清零停车仅作对照。r4零目标会重置PI，代码无显式反向制动命令；零PWM的H桥状态和机械停距须实测。[本车PI](../../../.build/metric-experiment-20261111-r4/workspace/demo/code/wheel_speed.c)、[官方含初始加速度及jerk的停车距离源码](https://github.com/autowarefoundation/autoware_core/blob/main/planning/autoware_velocity_smoother/src/smoother/analytical_jerk_constrained_smoother/velocity_planning_utils.cpp#L26-L132)
2. **横向包络。** 核对直道、入弯、S弯及恢复切换中，旧转向延续和减速全程的道路余量；用独立场次验证，未覆盖工况不取得延续能力。舵机命令不能当实际轮角，响应滞后应计入。[Autoware官方转向模型](https://github.com/autowarefoundation/autoware_universe/blob/main/control/autoware_mpc_lateral_controller/README.md#design)
3. **运行时序与动态失效恢复。** 补实际负载采集至物理输出的对齐与延迟范围，检查旧帧不续期；已有静止处理时间不能替代完整链路。[已有成果与缺口](../phase2-calibration-20261108/DELIVERY_STATUS.md)

判定须同时满足：停车最坏消耗不超剩余覆盖，车身及误差全程不越可信道路。后续需要扩大适用域时，再用局部平面运动变换重投影历史点，并传播误差；方向观测与融合按缺口增加。[Nav2官方路径坐标变换实例](https://github.com/ros-navigation/navigation2/blob/main/nav2_regulated_pure_pursuit_controller/README.md)

## 来源与自查

本地C/JSON为 `structured_parsed`，Markdown直接读取；网页说明为 `md_extracted`，运动学另核[官方原生RST](https://raw.githubusercontent.com/ros-controls/ros2_controllers/master/doc/mobile_robot_kinematics.rst)，停车依赖另核原生C++。无PDF、无新增 `pdf_verified` 或 `computed` 实车结论。包络、受控停车上界和动态延迟仍未验证；外部main/rolling链接会变化。仅新增本文件，不新增已取消的采集方案；未修改全局配置、技能、持久Agent行为或生产参数。

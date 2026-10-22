> 历史归档：本文只描述原版本，不作为当前启动、参数或验收说明。当前现状见 [项目设计与演进档案](../../PROJECT_EVOLUTION.md)。

# 中期第一阶段 训练准备与静态视觉

## 2026-10-20 静态参考轴现场记录

用户重新居中摆正后授权助手通过COM10静态调参。已发送停车/接管/文字模式，未发送a、h、运动或保存命令。
MIDTERM_TRAIN_1，近/远/半带宽0.78/0.62/0.035，连续文字样本TRACK、近远均有效，初始覆盖评分约81～91。
原始记录.build/field/serial-20261020-234139-595.log，叠线camera-o-20261020-234229-141.preview.png。
远端更高行仍有背景边界误选，不以这些行证明全图识别正确；当前参考轴只来自选定的近远有效采样带。
已在RAM写入@camera 0.50802 0.50535，收到OK和cam=1读回。后续日志中近点偏差约-0.003、航向差约0.003～0.004，
保持state=0、duty=0、speed enabled=0、pwm=0、faults=0；采样行可用不等于动态稳定性已验收。
复核日志.build/field/serial-20261020-234423-990.log。没有@save，复位会丢失RAM标定。
下一步需用户人工左右平移/小角度偏转，分别读回偏差方向；之后架空验证实际舵机纠偏，视觉确认位仍0。

平移验证：左移后near约+0.199、TRACK、近远有效，日志serial-20261020-234726-737.log。
首次右移近边界出画，CAL无效，未将清零误判为居中；缩小右移幅度后near约-0.259、heading约-0.010，
CAL valid=1/far_ok=1，row90约7/77/146，日志serial-20261020-235234-134.log。
确认在有效视野内横向误差左移为正、右移为负；尚待姿态偏转及实际舵机纠偏检查。

## 2026-10-21 提交总结

静态平移与航向方向检查已完成，尚未进行实际舵机视觉纠偏测试。以下数值为原始串口日志读回，
是归一化控制误差，不是厘米或弧度；手动偏转角仅操作近似值，未测量角度增益。

| 实车姿态 | near | heading | 近远状态 | 结论 |
| --- | --- | --- | --- | --- |
| 居中、标定后 | 约-0.003 | 约+0.003～+0.004 | 有效 | 参考轴残差接近零 |
| 左平移 | 约+0.199 | 约-0.017～-0.019 | 有效 | 横向为正 |
| 右平移，缩小幅度后 | 约-0.259 | 约-0.010 | 有效 | 横向为负 |
| 车头左偏 | 约+0.192 | 约+0.071～+0.076 | 有效 | 航向为正 |
| 车头右偏，缩小幅度后 | 约-0.219 | 约-0.068～-0.076 | 有效 | 航向为负 |

左偏记录serial-20261020-235556-724.log，右偏记录serial-20261020-235951-636.log，均在.build/field。
过大右移/右偏时近采样带失效，日志清零不代表居中；缩小幅度才恢复有效。不放宽覆盖/双边门槛来制造通过。
本轮助手经授权打开COM10进行静态STOP/接管/配置/采图，未发送a/h、未驱动车辆、未@save。
相机坐标仍仅RAM：近0.50802/远0.50535；采样带近0.78/远0.62/半宽0.035。
运行源速度配置已记录40/1/0.05与符号-1/+1；当前车上静态阶段速度环关闭、cap/cruise为0、无舵机硬件配置。
MIDTERM_VISUAL_REVIEWED仍0，不能把偏差符号检查当作实体纠偏、动态循迹或自主路线通过。
下一步回中并可靠架空驱动轮，配置实测舵机/电机参数，短时验证“车偏左则前轮向右、反之亦然”，
实际方向确认后再做低速直道及弯道训练。重复采图流程见下文，已完成项目无需原样重做。

## 验收依据与读取证据

以MIDTERM_IMPLEMENTATION.md为实施主线，参考用户指定C:/download_temp/中期检查要求 (1).docx。
本环境无MarkItDown MCP，改用DOCX原生document.xml逐段读取（structured_parsed），并直接查看两张原始内嵌PNG。
摄像头组文字要求：A/B任一点出发，到另一条红线即成功，不经过环岛或十字，不放红砖，终点可人工截停。
原始赛道图红线核对为沿左侧弯道和底部直道连接A/B。没有按图片比例推算距离或设定运行秒数。
三秒一次启动来自实施文档，不是这份检查要求新增的规则；惯导组的3米/约1米半径不套用于摄像头组。
本次没有整页渲染DOCX，不宣称核对完整页面排版；上述文字为结构解析，路径为原始内嵌图核对，没有仅依赖md_extracted的结论。

## 已完成

- midterm_profile.h固化已验证符号-1/+1、max_ticks40、Kp1、Ki0.05（Q8实际0.046875），速度确认位1；视觉确认位保持0。
- 训练版仍上电停止；这些源参数供自主入口使用，训练版仍显式配置RAM，不擅自加载相机参考轴或开始运动。
- field_serial.ps1支持MIDTERM_TRAIN_1、编码器/速度命令及对应回复；发送配置前先停车、x接管并核对当前端口拥有控制权。
- capture_vision.ps1同样先停车接管，再进入图像模式，退出恢复文字。均使用现有115200/8N1。
- field_protocol.ps1为无串口的协议校验模块；23项测试覆盖训练版本识别、速度回复、错误回复、残缺命令及换行注入拒绝。

## 下一次现场操作

1. 下载demo/mdk/midterm_train.uvprojx生成的.build/midterm-train/Objects/rt1064.axf，复位确认MIDTERM_TRAIN_1。
2. 车辆停在真实直道，车身居中且朝向与赛道平行；保持DRIVE档，仅采集图像，不发a/h，不保存配置。
3. 关闭串口助手，在仓库根目录逐条执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/field_serial.ps1 -Port COM10 -TextMode -Command '@status' -Seconds 2
powershell -NoProfile -ExecutionPolicy Bypass -File tools/capture_vision.ps1 -Port COM10 -Mode g
powershell -NoProfile -ExecutionPolicy Bypass -File tools/capture_vision.ps1 -Port COM10 -Mode o
powershell -NoProfile -ExecutionPolicy Bypass -File tools/field_serial.ps1 -Port COM10 -TextMode -Command '@status' -Seconds 2
```

工具输出PNG/GRAY/PREVIEW路径，日志在.build/field。PNG和gray仅94x60预览，不是188x120算法原帧。
把灰度、叠线图和最后日志交给项目核对，重点检查真实左右边界、近远行带和CAL near_x/far_x。
此时不要照抄历史@camera坐标，不先改舵机中位，不把NO_LANE归咎于速度环。

4. 图像确认有效后，停车用本次近远坐标标定相机；接下来架空验证实际视觉纠偏方向，再做低速直道和左右弯道。
5. 两项视觉检查都通过后才能修改MIDTERM_VISUAL_REVIEWED。当前不下载自主版进行落地自动发车。

## 离线检查

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/vision/test_field_protocol.ps1
cmd /c tests/vision/run_midterm_tests.cmd
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_midterm.ps1 -Variant train
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_midterm.ps1 -Variant auto
```

主机启动/完整应用回归已通过。没有由助手打开COM、烧录或驱动车辆；静态图像和真实纠偏方向待现场完成。

# 构建输入与 XML 哈希差异

原始字节哈希与语义XML分别比较。格式/空白变化可改变哈希，不能仅靠哈希猜测具体原因。

## 已有可比快照

比较 sd_performance/evidence/export-preflight-20261105T164657Z/before/mdk/sd_performance.uvprojx 与当前工程，原生XML展开得到6个叶/结构差异：OutputDirectory、ListingPath改到新版run；增加export.c对应File节点、FileName、FileType和FilePath。该区间没有Define/MiscControls/ScatterFile/SD驱动文件/调试配置语义变化。逐项before/after见evidence/sd-performance-xml-diff.json。

## 无法恢复的区间

最早SD Diagnostic和性能v1的交付记录只冻结了当时uvprojx SHA256，没有完整XML前像。因此首次人工打开/烧录前后的全部XML差异为UNKNOWN，不能断言“只是输出路径/只是Keil格式化”。已保留recorded/current哈希链及当前XML。SDK/源码哈希和旧产物证明各自范围，不能替代缺失的XML前像。完整因果审计仍欠原始XML或当时备份。
后续已记录的uvoptx缓存ListingPath问题确实导致一次MAP写旧路径；此前已用原对象恢复旧MAP且哈希精确匹配，见原export preflight/map-recovery/result.json。当前B2和C26隔离构建不复制用户uvoptx，并使用新命名输出。

## 本轮冻结

C26源工程SHA256保持2d990ceca40a70ca4736f389620dff24fb34648069f4ef2711109c43b7c99810，宏仍AUTO=0/ROUTE_SWITCH=1。本轮helper仅重定位私有副本路径、禁用副本调试自动下载，所有原源码/主线工程未改。
B2候选先bootstrap获得编译依赖，再冻结文件/配置，最后编译验证fresh .d闭包一致；含190个源、头、现有库、工具与配置输入，排除自引用identity头以避免循环，其生成内容单独保留。输入文件清单、adapter起源/哈希、manifest digest在最终构建inputs.json，AXF/BIN/MAP/日志哈希在build-evidence。
旧已审计SDK adapters按精确哈希复用，没有运行会遍历全库的旧prepare_recorder.py，也没有改SDK驱动。Camera SCCB已有二进制库作为不透明依赖冻结。
本轮Recorder存储模块仅新增可配置volume/identity/stop_reason，默认保留原训练行为；候选选择2:且motion_reviewed=false。原训练项目和主线宏未改。所有本轮增量见evidence/final-review.json，原有dirty diff不归为本轮改动。

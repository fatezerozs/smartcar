> **维护口径（2026-11-09）：** Phase2已有车辆诊断/标定原始记录，但不等于performance-host所需的版本化输入、独立标签、测量adapter及批准基线齐备。接口延期约定保持，不因本轮有测量数据而宣称性能基准通过。 统一状态与历史适用范围见[文档维护总览](../../docs/DOCUMENTATION_STATUS.md)。

# Performance and quality integration boundary

No labelled, approved performance dataset is shipped with this refactor.
The existing 94×60 tangent-preview fixtures are unlabelled, subsampled static
scenes. They continue to serve functional/contract regression, and do not become
full-resolution ground truth or captured control timing.

`manifests/vision-host.json` is a real, schema-validated pending contract. During
Host/PreFlash it produces `SKIPPED`, is not required and is not a prerequisite of
any functional, contract or build suite. No synthetic accuracy/performance pass is
substituted for missing data.

Future integration:

1. Add immutable dataset and ground-truth manifests with stable IDs, versions and
   SHA-256 hashes under `datasets/` and `ground_truth/`, or use another explicitly
   reviewed repository-relative location. Keep earlier versions.
   The dataset content manifest contains `id`, `version` and `samples`; each sample
   has `id`, repository-relative `path`, `sha256`, `source` and `scenario`. Both the
   content manifest and every sample file are hash-checked. Pair `groundTruthPath`
   with `groundTruthSha256`; labels are hash-checked too.
2. Implement an offline measurement adapter under `tools/` or `tests/`. Change
   `adapter` and `status` only after the dataset and labels are ready. The framework
   never guesses or silently runs an adapter.
3. Record `gitSha` (full commit ID), `dirty` (boolean), `inputTreeSha256` (source/input
   snapshot), `compilerId`, `environmentId`, `mode`, `dataset`
   (exact manifest object), `parameters`, `warmup`, `repetitions`, and `samples`.
   Each sample has `sampleId`, zero-based measured `repetition`, explicit boolean
   `valid`, and `metrics` containing exactly the declared metric IDs. Exclude warmup
   samples. Every measured repetition must contain valid samples. Record dirty
   state and source hashes with the measurement adapter; do not label it as a clean
   historical commit when local changes were measured.
4. Define metric units, direction, validity rule, statistic and threshold source.
   Without reliable ground truth, quality metrics can only be observations.
5. A baseline is a prior measurement record plus
   `approval: {"reviewer":"...", "approvedAt":"ISO-8601", "gitSha":"40-char SHA"}`.
   Set `approvedBaseline` to the reviewed file under `baselines/`. There is no
   automatic baseline promotion and no assumption that latest main is approved.
6. Run `tools/testing/Invoke-Benchmark.ps1 -Measurements <repository-relative JSON>`.
   Results include raw JSONL, archived inputs, independent metrics and Markdown.
7. After dataset/adapter validation, a future change can make `performance-host`
   required and connect its recorded measurements to the automatic profile. Until
   then its pending status remains independent of other tests.

Comparison rejects different datasets, labels, parameters, compiler, host identity,
mode, warmup and repetition count. Mean/median/min/max and P95/P99 are supported;
quantiles linearly interpolate sorted samples at `(n - 1) * p`. Absolute, relative
regression, no-regression and observation gates are independent. A noisy difference
or a missing approved comparison baseline is `INCONCLUSIVE` and exits nonzero for
the explicit benchmark command. A quality hard-gate failure cannot be offset by a
speed improvement. `noiseFraction` defines a declared uncertainty band; it is not
an automatically computed confidence interval or proof of statistical significance.

Host timing applies only to that host and configuration. MCU cycles, stack, WCET,
DMA/interrupt behavior and closed-loop physical responses require separate Target/
Hardware evidence. Measurement adapters must not open hardware as part of Host CI.

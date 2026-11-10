# Vision image performance and regression

Status: **PASS**

| Metric | Value | Unit | Decision |
| --- | ---: | --- | --- |
| processing-time | 0.279055 | ms/frame | OBSERVATION |
| metric-adapter-time | 0.0009 | ms/frame | OBSERVATION |
| vision-plus-metric-time | 0.2796 | ms/frame | OBSERVATION |
| boundary-mae | 0.428571429 | px | OBSERVATION |
| boundary-max-error | 1 | px | PASS |
| missing-pair | 0 | fraction | PASS |
| within-reference-band | 1 | fraction | OBSERVATION |

Original frames: 750; local labelled pairs: 28 per replay.
Accuracy and speed gate independently; missing labelled pairs are counted as failures.
All-frame acceptance is availability, not accuracy. Timings are host observations.

## Near/full rejection audit

near: {"accepted": 470, "source_invalid_or_quality": 6, "requested_row_not_two_measured_edges": 256, "temporal_history_rejection_confirmed_by_native_cold_retry": 8, "non_forward_ground_projection": 3, "outside_configured_ground_domain": 7}

full: {"accepted": 376, "source_invalid_or_quality": 6, "requested_row_not_two_measured_edges": 310, "temporal_history_rejection_confirmed_by_native_cold_retry": 8, "non_forward_ground_projection": 47, "outside_configured_ground_domain": 3}

## Targeted clear-scene review

Frames 32, 80, 99: visually visible road at row54. Current outputs below are diagnostic observations; local labelled-band PASS does not certify these cases.
[{"index": 32, "review": "Visible white road and side transitions at row54; targeted diagnostic review, not independent labels", "rawValid": 1, "row54Flags": 4, "row54Threshold": 127, "junctionRows": 22, "junctionAnchors": 19, "metricNearAccepted": 0, "metricFullAccepted": 0}, {"index": 80, "review": "Visible white road and side transitions at row54; targeted diagnostic review, not independent labels", "rawValid": 1, "row54Flags": 0, "row54Threshold": 139, "junctionRows": 0, "junctionAnchors": 0, "metricNearAccepted": 0, "metricFullAccepted": 0}, {"index": 99, "review": "Visible white road and side transitions at row54; targeted diagnostic review, not independent labels", "rawValid": 0, "row54Flags": 4, "row54Threshold": 130, "junctionRows": 33, "junctionAnchors": 4, "metricNearAccepted": 0, "metricFullAccepted": 0}]

28 local post-selection boundary bands, no independent holdout or all-frame accuracy claim
Timing observation only: no approved host regression baseline or target WCET claim
Uniform diagnostic time and stationary encoder; recorded historical board timings are not current implementation timings

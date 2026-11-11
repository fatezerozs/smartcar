#ifndef SMARTCAR_VISION_PLANNER_BUILD_H
#define SMARTCAR_VISION_PLANNER_BUILD_H
/* Lightweight switches also used by the legacy driver and private builds. */
#ifndef VISION_PLANNER_ACTIVE
#define VISION_PLANNER_ACTIVE 0
#endif
#ifndef VISION_PLANNER_RUNTIME
#define VISION_PLANNER_RUNTIME 0
#endif
#ifndef VISION_PLANNER_PROFILE_REVIEWED
#define VISION_PLANNER_PROFILE_REVIEWED 0
#endif
#if (VISION_PLANNER_ACTIVE != 0 && VISION_PLANNER_ACTIVE != 1) || \
    (VISION_PLANNER_RUNTIME != 0 && VISION_PLANNER_RUNTIME != 1) || \
    (VISION_PLANNER_PROFILE_REVIEWED != 0 && VISION_PLANNER_PROFILE_REVIEWED != 1)
#error "Planner modes must be zero or one."
#endif
#if VISION_PLANNER_ACTIVE && (!VISION_PLANNER_RUNTIME || !VISION_PLANNER_PROFILE_REVIEWED)
#error "Active metric planner requires reviewed PI/supervisor integration; shadow only."
#endif
#endif

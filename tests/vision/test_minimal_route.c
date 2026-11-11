/* Current grayscale -> public vision result and route progress. No PWM owner. */
#include "vision_midterm.h"
#include <stdio.h>
#include <string.h>

static uint8_t gray[VISION_WIDTH * VISION_HEIGHT], binary[VISION_WIDTH * VISION_HEIGHT];
static vision_result_t lane;
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "minimal-route:%d %s state=%u side=%d valid=%u\n", __LINE__, #x, lane.roundabout_state, lane.roundabout_side, lane.valid); return 1; } } while (0)

static void process(unsigned frames)
{ while (frames--) vision_midterm_process(gray, binary, &lane); }
static void road(int side)
{
    for (int y = 0; y < VISION_HEIGHT; ++y) {
        int center = 94 + side * (110 - y) / 2;
        for (int x = 0; x < VISION_WIDTH; ++x)
            gray[y * VISION_WIDTH + x] = (uint8_t)(x >= center - 40 && x <= center + 40 ? 210 : 30);
    }
}
static void mirror(void)
{
    for (int y = 0; y < VISION_HEIGHT; ++y) for (int x = 0; x < VISION_WIDTH / 2; ++x) {
        uint8_t a = gray[y * VISION_WIDTH + x];
        gray[y * VISION_WIDTH + x] = gray[y * VISION_WIDTH + VISION_WIDTH - 1 - x];
        gray[y * VISION_WIDTH + VISION_WIDTH - 1 - x] = a;
    }
}
static void island(int side)
{
    road(0);
    for (int y = 0; y < VISION_HEIGHT; ++y) for (int x = 0; x < VISION_WIDTH; ++x) {
        int dx = x - 55, dy = y - 65;
        if (dx*dx*52*52 + dy*dy*64*64 <= 64*64*52*52) gray[y*VISION_WIDTH+x] = 210;
        if (dx*dx*20*20 + dy*dy*18*18 <= 18*18*20*20) gray[y*VISION_WIDTH+x] = 30;
    }
    if (side > 0) mirror();
}
static void opening(int side)
{
    road(0);
    for (int y = 84; y <= 110; ++y)
        for (int x = side < 0 ? 94 : 0; x <= (side < 0 ? VISION_WIDTH - 1 : 94); ++x)
            gray[y * VISION_WIDTH + x] = 210;
}
static unsigned completed(void)
{
    vision_route_progress_t progress;
    vision_midterm_route_progress(&progress);
    return progress.rings_completed;
}
static void boot(void)
{
    vision_midterm_route_enable(0); vision_midterm_reset(); road(0); process(10);
    vision_midterm_route_enable(1);
}
int main(void)
{
    for (int side = -1; side <= 1; side += 2) {
        boot(); CHECK(lane.valid && completed() == 0);
        /* A turn or a crossing without a closed island cannot consume order. */
        road(side); process(15); CHECK(completed() == 0 && lane.roundabout_state == 0);
        road(0); process(10);
        for (unsigned ring = 0; ring < 2; ++ring) {
            island(side); process(4);
            CHECK(lane.roundabout_state == 1 && completed() == ring);
            /* The entry opening is not an exit or a completed circle. */
            opening(side); process(4);
            CHECK(lane.roundabout_state == 1 && !lane.roundabout_committed && completed() == ring);
            road(side); process(8);
            CHECK(lane.roundabout_state == 2 && lane.roundabout_committed);
            road(0); process(12);
            CHECK(lane.roundabout_state == 2 && completed() == ring);
            opening(side); process(2); CHECK(lane.roundabout_state == 2);
            road(side); process(1); /* One rejected opening does not consume the ring. */
            opening(side); process(4); CHECK(lane.roundabout_state == 2);
            process(2); CHECK(lane.roundabout_state == 3 && lane.valid && completed() == ring);
            road(0);
            for (int y = 56; y < VISION_HEIGHT; ++y)
                memset(gray + y * VISION_WIDTH, 30, VISION_WIDTH);
            process(4); /* Distant road alone is not a usable ordinary-road exit. */
            CHECK(!lane.valid && completed() == ring);
            road(0); process(4);
            CHECK(lane.roundabout_state == 4 && lane.valid && completed() == ring + 1);
            process(12); CHECK(lane.roundabout_state == 0 && completed() == ring + 1);
        }
        /* Camera history expires within the same C26 run; task order survives. */
        vision_midterm_reset(); road(0); process(10);
        CHECK(lane.roundabout_state == 0 && completed() == 2);
        /* After both planned rings, another island cannot start a third one. */
        island(side); process(12); CHECK(lane.roundabout_state == 0 && completed() == 2);
        road(-side); process(8); CHECK(lane.valid && completed() == 2);
        /* Darkness cannot fake an exit; C26 OFF starts a genuinely new task. */
        memset(gray, 30, sizeof(gray)); process(8); CHECK(!lane.valid && completed() == 2);
        vision_midterm_route_enable(0); process(1); CHECK(lane.roundabout_state == 0 && completed() == 0);
        road(0); process(10); vision_midterm_route_enable(1);
        island(side); process(4); CHECK(lane.roundabout_state == 1 && completed() == 0);
    }
    printf("PASS minimal route: %u checks; two grayscale ring episodes, current exit evidence, road reacquisition, no third ring, OFF reset; not physical lap proof\n", checks);
    return 0;
}

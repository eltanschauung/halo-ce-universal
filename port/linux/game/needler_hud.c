/* Thirty-shot HUD for the optional Needler rules. Each count is a cached
bitmap, drawn through the ordinary meter pipeline in one quad. Binary fill
values stay constant around each needle's antialiased edge; interpolation
cannot leave a partially lit needle at an odd ammunition count.

The silhouette is the existing hand-drawn HUD's #slash path in
port/assets/hud/svg/hud_xbox_order__bitmaps__combined__hud_ammo_meters.svg.
Its thirty needles fit the stock 111 by 17 logical-pixel rectangle. */
#include "cseries.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_group.h"
#include "interface/hud_draw.h"
#include "interface/unit_hud_interface_definition.h"
#include "math/real_math.h"
#include "needler_balance.h"
#include "needler_hud.h"
#include "tag_files/tag_groups.h"

#define NEEDLER_HUD_ROUNDS 30
#define NEEDLER_HUD_WIDTH 512
#define NEEDLER_HUD_HEIGHT 128
#define NEEDLER_HUD_PIXELS (NEEDLER_HUD_WIDTH * NEEDLER_HUD_HEIGHT)

static struct bitmap_data *needler_meters[NEEDLER_HUD_ROUNDS + 1];
static byte needle_coverage[NEEDLER_HUD_PIXELS];
static byte needle_owner[NEEDLER_HUD_PIXELS];

/* Horizontal intersections of the needle polygon at a row of its redraw.
Coordinates are relative to the original needle's upper-left corner. */
static boolean needle_span(real y, real *left, real *right)
{
    static real const points[6][2] = {
        {40.07f, 0.f}, {27.70f, 0.f}, {27.34f, .88f},
        {0.f, 59.57f}, {17.46f, 48.60f}, {38.87f, 2.65f}
    };
    short i, previous = 5;
    boolean found = FALSE;
    *left = REAL_MAX;
    *right = -REAL_MAX;
    for (i = 0; i < 6; previous = i++)
    {
        real ay = points[previous][1], by = points[i][1];
        if ((ay <= y && y < by) || (by <= y && y < ay))
        {
            real x = points[previous][0] + (y - ay) *
                (points[i][0] - points[previous][0]) / (by - ay);
            *left = MIN(*left, x);
            *right = MAX(*right, x);
            found = TRUE;
        }
    }
    return found;
}

static void needle_mask_build(void)
{
    /* Preserve the old 20-needle row's occupied width, with thinner needles. */
    real const squeeze = (40.07f + 19.f * 20.705f) / (40.07f + 29.f * 20.705f);
    real const pitch = 20.705f * squeeze;
    long x, y;
    short sample;
    for (y = 0; y < NEEDLER_HUD_HEIGHT; y++)
    {
        real left[4], right[4], center_left, center_right;
        boolean visible[4];
        needle_span((real)y + .5f - 4.f, &center_left, &center_right);
        for (sample = 0; sample < 4; sample++)
        {
            visible[sample] = needle_span((real)y + ((real)sample + .5f) / 4.f - 4.f,
                &left[sample], &right[sample]);
            left[sample] = 4.25f + left[sample] * squeeze;
            right[sample] = 4.25f + right[sample] * squeeze;
        }
        for (x = 0; x < NEEDLER_HUD_WIDTH; x++)
        {
            real center = 4.25f + (center_left + center_right) * .5f * squeeze;
            short owner = (short)PIN((long)floor(((real)x + .5f - center) / pitch + .5f), 0, 29);
            short hits = 0, sx;
            long offset = y * NEEDLER_HUD_WIDTH + x;
            for (sample = 0; sample < 4; sample++) if (visible[sample])
                for (sx = 0; sx < 4; sx++)
                {
                    real xx = (real)x + ((real)sx + .5f) / 4.f - owner * pitch;
                    if (xx >= left[sample] && xx < right[sample]) hits++;
                }
            needle_owner[offset] = (byte)owner;
            needle_coverage[offset] = (byte)((hits * 255 + 8) / 16);
        }
    }
}

void needler_hud_unloaded(void)
{
    short count;
    for (count = 0; count <= NEEDLER_HUD_ROUNDS; count++)
    {
        if (needler_meters[count]) bitmap_delete(needler_meters[count]);
        needler_meters[count] = NULL;
    }
}

void needler_hud_loaded(void)
{
    short count, x, y;
    needler_hud_unloaded();
    needle_mask_build();
    for (count = 0; count <= NEEDLER_HUD_ROUNDS; count++)
    {
        struct bitmap_data *bitmap = bitmap_2d_new(NEEDLER_HUD_WIDTH, NEEDLER_HUD_HEIGHT, 0, 3);
        needler_meters[count] = bitmap;
        if (!bitmap || !bitmap->base_address)
        {
            needler_hud_unloaded();
            return;
        }
        for (y = 0; y < NEEDLER_HUD_HEIGHT; y++) for (x = 0; x < NEEDLER_HUD_WIDTH; x++)
        {
            long offset = y * NEEDLER_HUD_WIDTH + x;
            word fill = needle_owner[offset] < count ? 0 : 255;
            *(word *)bitmap_2d_address(bitmap, x, y, 0) =
                (word)((needle_coverage[offset] << 8) | fill);
        }
        bitmap_rebuild(bitmap);
    }
}

boolean needler_hud_draw(struct weapon_definition const *weapon, short state, short local_player,
    struct hud_absolute_placement_definition const *absolute,
    struct meter_hud_element_definition const *meter, short rounds, short flags, real flash_time)
{
    struct meter_hud_element_definition replacement;
    struct bitmap_data const *original = NULL;
    real_rectangle2d const *original_clip = NULL;
    real_rectangle2d clip;
    if (state != 1 || !needler_balance_hud_active(weapon) || !needler_meters[0] ||
        meter->meter_bitmap.index == NONE ||
        csstrcmp(tag_get_name(meter->meter_bitmap.index), "ui\\hud\\bitmaps\\combined\\hud_ammo_meters"))
        return FALSE;
    hud_retrieve_bitmap_and_bounding_rect(meter->meter_bitmap.index, meter->sequence_index, 0,
        &original, &original_clip);
    /* Other/custom layouts keep their authored meter, rather than replacing
       a battery or a script's different graphic with stock Needler art. */
    if (!original || !original_clip ||
        fabs((original_clip->x1 - original_clip->x0) * original->width - 111.f) > .01f ||
        fabs((original_clip->y1 - original_clip->y0) * original->height - 17.f) > .01f)
        return FALSE;
    replacement = *meter;
    replacement.alpha_multiplier = 1;
    replacement.alpha_bias = replacement.minimum_value = 0;
    replacement.placement.scale.i *= .25f;
    replacement.placement.scale.j *= .25f;
    clip.x0 = clip.y0 = 0.f;
    clip.x1 = 444.f / NEEDLER_HUD_WIDTH;
    clip.y1 = 68.f / NEEDLER_HUD_HEIGHT;
    hud_draw_meter_bitmap(local_player, absolute, &replacement, 128, 128, flags, flash_time, 0.f,
        needler_meters[PIN(rounds, 0, NEEDLER_HUD_ROUNDS)], &clip, FALSE);
    return TRUE;
}

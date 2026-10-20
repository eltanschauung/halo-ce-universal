#ifndef NEEDLER_HUD_H
#define NEEDLER_HUD_H
struct weapon_definition;
struct hud_absolute_placement_definition;
struct meter_hud_element_definition;
void needler_hud_loaded(void);
void needler_hud_unloaded(void);
boolean needler_hud_draw(struct weapon_definition const *weapon, short state, short local_player,
    struct hud_absolute_placement_definition const *absolute,
    struct meter_hud_element_definition const *meter, short rounds, short flags, real flash_time);
#endif

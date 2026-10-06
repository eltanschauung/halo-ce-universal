/*
RASTERIZER_TRANSPARENT_GEOMETRY.H

Narrow cross-translation-unit interface owned by RASTERIZER_TRANSPARENT_GEOMETRY.C.
*/

#ifndef __RASTERIZER_TRANSPARENT_GEOMETRY_H
#define __RASTERIZER_TRANSPARENT_GEOMETRY_H
#pragma once

#include "cseries.h"

struct transparent_geometry_group;

/* Renderer-only flags; no tag or persistent-state layout changes. */
enum
{
	_rasterizer_geometry_enclosed_energy_bit = 28,
	_rasterizer_geometry_glass_front_bit = 29,
	_rasterizer_geometry_glass_back_bit = 30,
};

long rasterizer_transparent_geometry_model_begin(void);
void rasterizer_transparent_geometry_model_end(long first_group);
void rasterizer_transparent_geometry_order_enclosures(short *order, long count);

void rasterizer_transparent_geometry_groups_begin(
	void);
void rasterizer_transparent_geometry_groups_end(
	void);
void rasterizer_transparent_geometry_group_draw(
	struct transparent_geometry_group *group,
	boolean dirty);
void rasterizer_transparent_geometry_group_draw__internal(
	struct transparent_geometry_group const *group,
	boolean has_lightmap);

#endif /* __RASTERIZER_TRANSPARENT_GEOMETRY_H */

/* Shared per-player growth for campaign squads and pickups. A step counts
complete groups of additional players; the first player never adds growth. */
#ifndef __COOP_SCALING_H
#define __COOP_SCALING_H

enum { COOP_SCALING_MAXIMUM_GROWTH = 8 };

static inline long coop_scaling_growth(short players, short percent, short player_step)
{
	if (players <= 1 || percent <= 0)
		return 0;
	return MIN((long)MIN(percent, 200) *
		((MIN(players, HALO_PORT_MAXIMUM_NETWORK_PLAYERS) - 1) / PIN(player_step, 1, 8)),
		(COOP_SCALING_MAXIMUM_GROWTH - 1) * 100);
}

#endif

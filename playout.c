#define DEBUG
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "debug.h"
#include "engine.h"
#include "move.h"
#include "ownermap.h"
#include "playout.h"


int
play_random_game(struct playout_setup *setup,
                 struct board *b, enum stone starting_color,
		 struct playout_amafmap *amafmap,
		 struct board_ownermap *ownermap,
		 struct playout_policy *policy)
{
	assert(setup && policy);

	int gamelen = setup->gamelen - b->moves;
	if (gamelen < 10)
		gamelen = 10;

	if (policy->setboard)
		policy->setboard(policy, b);

	enum stone color = starting_color;

	int passes = is_pass(b->last_move.coord) && b->moves > 0;

	while (gamelen-- && passes < 2) {
		coord_t coord;
		coord = policy->choose(policy, b, color);

		if (is_pass(coord)) {
play_random:
			/* Defer to uniformly random move choice. */
			/* This must never happen if the policy is tracking
			 * internal board state, obviously. */
			assert(!policy->setboard);
			board_play_random(b, color, &coord, (ppr_permit) policy->permit, policy);

		} else {
			struct move m;
			m.coord = coord; m.color = color;
			if (board_play(b, &m) < 0) {
				if (DEBUGL(8)) {
					fprintf(stderr, "Pre-picked move %d,%d is ILLEGAL:\n",
						coord_x(coord, b), coord_y(coord, b));
					board_print(b, stderr);
				}
				goto play_random;
			}
		}

#if 0
		/* For UCT, superko test here is downright harmful since
		 * in superko-likely situation we throw away literally
		 * 95% of our playouts; UCT will deal with this fine by
		 * itself. */
		if (unlikely(b->superko_violation)) {
			/* We ignore superko violations that are suicides. These
			 * are common only at the end of the game and are
			 * rather harmless. (They will not go through as a root
			 * move anyway.) */
			if (group_at(b, coord)) {
				if (DEBUGL(3)) {
					fprintf(stderr, "Superko fun at %d,%d in\n", coord_x(coord, b), coord_y(coord, b));
					if (DEBUGL(4))
						board_print(b, stderr);
				}
				return 0;
			} else {
				if (DEBUGL(6)) {
					fprintf(stderr, "Ignoring superko at %d,%d in\n", coord_x(coord, b), coord_y(coord, b));
					board_print(b, stderr);
				}
				b->superko_violation = false;
			}
		}
#endif

		if (DEBUGL(7)) {
			fprintf(stderr, "%s %s\n", stone2str(color), coord2sstr(coord, b));
			if (DEBUGL(8))
				board_print(b, stderr);
		}

		if (unlikely(is_pass(coord))) {
			passes++;
		} else {
			/* We don't care about nakade counters, since we want
			 * to avoid taking pre-nakade moves into account only
			 * if they happenned in the tree before nakade nodes;
			 * but this is always out of the tree. */
			if (amafmap) {
				if (amafmap->map[coord] == S_NONE || amafmap->map[coord] == color)
					amafmap->map[coord] = color;
				else if (amafmap->record_nakade)
					amaf_op(amafmap->map[coord], +);
				amafmap->game[amafmap->gamelen].coord = coord;
				amafmap->game[amafmap->gamelen].color = color;
				amafmap->gamelen++;
				assert(amafmap->gamelen < sizeof(amafmap->game) / sizeof(amafmap->game[0]));
			}

			passes = 0;
		}

		color = stone_other(color);
	}

	float score = board_fast_score(b);
	int result = (starting_color == S_WHITE ? score * 2 : - (score * 2));

	if (DEBUGL(6)) {
		fprintf(stderr, "Random playout result: %d (W %f)\n", result, score);
		if (DEBUGL(7))
			board_print(b, stderr);
	}

	if (ownermap)
		board_ownermap_fill(ownermap, b);

	if (b->ps)
		free(b->ps);

	return result;
}

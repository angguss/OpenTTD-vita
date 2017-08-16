/* $Id: base.cpp 26482 2014-04-23 20:13:33Z rubidium $ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file base.cpp Implementation of the base for all blitters. */

#include "../stdafx.h"
#include "base.hpp"
#include "../core/math_func.hpp"

#include "../safeguards.h"

void Blitter::DrawLine(void *video, int x, int y, int x2, int y2, int screen_width, int screen_height, uint8 colour, int width, int dash)
{
	int dy;
	int dx;
	int stepx;
	int stepy;

	dy = (y2 - y) * 2;
	if (dy < 0) {
		dy = -dy;
		stepy = -1;
	} else {
		stepy = 1;
	}

	dx = (x2 - x) * 2;
	if (dx < 0) {
		dx = -dx;
		stepx = -1;
	} else {
		stepx = 1;
	}

	if (dx == 0 && dy == 0) {
		/* The algorithm below cannot handle this special case; make it work at least for line width 1 */
		if (x >= 0 && x < screen_width && y >= 0 && y < screen_height) this->SetPixel(video, x, y, colour);
		return;
	}

	int frac_diff = width * max(dx, dy);
	if (width > 1) {
		/* compute frac_diff = width * sqrt(dx*dx + dy*dy)
		 * Start interval:
		 *    max(dx, dy) <= sqrt(dx*dx + dy*dy) <= sqrt(2) * max(dx, dy) <= 3/2 * max(dx, dy) */
		int frac_sq = width * width * (dx * dx + dy * dy);
		int frac_max = 3 * frac_diff / 2;
		while (frac_diff < frac_max) {
			int frac_test = (frac_diff + frac_max) / 2;
			if (frac_test * frac_test < frac_sq) {
				frac_diff = frac_test + 1;
			} else {
				frac_max = frac_test - 1;
			}
		}
	}

	int gap = dash;
	if (dash == 0) dash = 1;
	int dash_count = 0;
	if (dx > dy) {
		int y_low     = y;
		int y_high    = y;
		int frac_low  = dy - frac_diff / 2;
		int frac_high = dy + frac_diff / 2;

		while (frac_low + dx / 2 < 0) {
			frac_low += dx;
			y_low -= stepy;
		}
		while (frac_high - dx / 2 >= 0) {
			frac_high -= dx;
			y_high += stepy;
		}
		x2 += stepx;

		while (x != x2) {
			if (dash_count < dash && x >= 0 && x < screen_width) {
				for (int y = y_low; y != y_high; y += stepy) {
					if (y >= 0 && y < screen_height) this->SetPixel(video, x, y, colour);
				}
			}
			if (frac_low >= 0) {
				y_low += stepy;
				frac_low -= dx;
			}
			if (frac_high >= 0) {
				y_high += stepy;
				frac_high -= dx;
			}
			x += stepx;
			frac_low += dy;
			frac_high += dy;
			if (++dash_count >= dash + gap) dash_count = 0;
		}
	} else {
		int x_low     = x;
		int x_high    = x;
		int frac_low  = dx - frac_diff / 2;
		int frac_high = dx + frac_diff / 2;

		while (frac_low + dy / 2 < 0) {
			frac_low += dy;
			x_low -= stepx;
		}
		while (frac_high - dy / 2 >= 0) {
			frac_high -= dy;
			x_high += stepx;
		}
		y2 += stepy;

		while (y != y2) {
			if (dash_count < dash && y >= 0 && y < screen_height) {
				for (int x = x_low; x != x_high; x += stepx) {
					if (x >= 0 && x < screen_width) this->SetPixel(video, x, y, colour);
				}
			}
			if (frac_low >= 0) {
				x_low += stepx;
				frac_low -= dy;
			}
			if (frac_high >= 0) {
				x_high += stepx;
				frac_high -= dy;
			}
			y += stepy;
			frac_low += dx;
			frac_high += dx;
			if (++dash_count >= dash + gap) dash_count = 0;
		}
	}
}

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lib/SMSlib.h"
#include "lib/PSGlib.h"
#include "actor.h"
#include "data.h"

#define PLAYER_SPEED (2)
#define PLAYER_SHOT_SPEED (4)
#define PLAYER_TOP (16)
#define PLAYER_LEFT (8)
#define PLAYER_RIGHT (256 - 16)
#define PLAYER_BOTTOM (SCREEN_H - 24)

#define MAX_ENEMIES_X (3)
#define MAX_ENEMIES_Y (3)
#define MAX_ENEMY_SHOTS (2)
#define ENEMY_SHOT_SPEED (3)

#define MAX_LEVELS (5)
#define MAX_ENEMY_TILES (15)
#define LV_ODD_SPACING (0x01)
#define LV_ODD_X_SPEED (0x02)
#define LV_FULL_HEIGHT (0x04)

typedef struct level_info {
	int spd_x, spd_y;
	int stop_x_timer, invert_x_timer;
	int stop_y_timer, advance_y_timer, invert_y_timer;
	char flags;
} level_info;

typedef struct numeric_label {
	char x, y;
	unsigned int value;
	char dirty;
} numeric_label;

actor player;
actor shot;

actor enemies[MAX_ENEMIES_Y][MAX_ENEMIES_X];
actor enemy_shots[MAX_ENEMY_SHOTS];

struct level {
	char number;
	
	char enemy_count;
	char enemy_tile;
	int enemy_score;
	
	char horizontal_spacing, horizontal_odd_spacing;
	char vertical_spacing;	
	char odd_x_speed;
	int starting_y;
	
	fixed incr_x, incr_y;
	fixed spd_x, spd_y;

	int stop_x_timer, invert_x_timer;
	int stop_x_timer_max, invert_x_timer_max;

	int stop_y_timer, advance_y_timer, invert_y_timer;
	int stop_y_timer_max, advance_y_timer_max, invert_y_timer_max;
	
	char cheat_skip;
} level;

numeric_label score;

const level_info level_infos[MAX_LEVELS] = {
	{192, 0, 0, 0, 0, 0, 0, LV_ODD_SPACING},
	{128, 128, 0, 120, 0, 0, 0, LV_ODD_SPACING | LV_FULL_HEIGHT},
	{160, 160, 0, 120, 0, 0, 0, LV_ODD_X_SPEED | LV_FULL_HEIGHT},
	{256, 160, 0, 0, 0, 0, 60, LV_ODD_SPACING},
	{160, 256, 60, 60, 37, 93, 0, LV_FULL_HEIGHT}
};

void add_score(int delta);

void load_standard_palettes() {
	SMS_loadBGPalette(sprites_palette_bin);
	SMS_loadSpritePalette(sprites_palette_bin);
	SMS_setSpritePaletteColor(0, 0);
}

void configure_text() {
	SMS_load1bppTiles(font_1bpp, 352, font_1bpp_size, 0, 1);
	SMS_configureTextRenderer((352 - 32) | TILE_PRIORITY);
}

void handle_player_input() {
	unsigned char joy = SMS_getKeysStatus();

	if (joy & PORT_A_KEY_LEFT) {
		if (player.x > PLAYER_LEFT) player.x -= PLAYER_SPEED;
	} else if (joy & PORT_A_KEY_RIGHT) {
		if (player.x < PLAYER_RIGHT) player.x += PLAYER_SPEED;
	}
	
	if (joy & (PORT_A_KEY_1 | PORT_A_KEY_2)) {
		if (!shot.active) {
			shot.x = player.x + 4;
			shot.y = player.y;
			shot.active = 1;

			PSGPlayNoRepeat(player_shot_psg);
		}
	}
	
	if ((joy & PORT_A_KEY_UP) && (joy & PORT_A_KEY_1)&& (joy & PORT_A_KEY_2)) {
		level.cheat_skip = 1;
	}
}

void wait_button_release() {
	unsigned char joy;
	
	do {
		SMS_waitForVBlank();
		joy = SMS_getKeysStatus();		
	} while (joy & (PORT_A_KEY_1 | PORT_A_KEY_2));
}

void handle_shot_movement() {
	if (!shot.active) return;
	
	shot.y -= PLAYER_SHOT_SPEED;	
	if (shot.y < -8) shot.active = 0;
}

char is_colliding_with_shot(actor *act) {
	static int delta;
	
	if (!shot.active) return 0;
	if (!act->active) return 0;

	delta = act->y - shot.y;
	if (delta < -6 || delta > act->pixel_h + 14) return 0;	

	delta = act->x - shot.x;
	if (delta < -6 || delta > act->pixel_w + 14) return 0;
	
	return 1;
}

void init_enemies() {
	static char i, j;
	static int x, y;
	static actor *enemy;

	for (i = 0, y = level.starting_y; i != MAX_ENEMIES_Y; i++, y += level.vertical_spacing) {
		enemy = enemies[i];
		for (j = 0, x = i & 1 ? level.horizontal_odd_spacing : 0; j != MAX_ENEMIES_X; j++, x += level.horizontal_spacing) {
			init_actor(enemy, x, y, 2, 1, level.enemy_tile, 1);
			enemy++;
		}
	}
}

void draw_enemies() {
	static char i, j;
	static actor *enemy;

	for (i = 0; i != MAX_ENEMIES_Y; i++) {
		enemy = enemies[i];
		for (j = 0; j != MAX_ENEMIES_X; j++) {
			draw_actor(enemy);
			enemy++;
		}
	}
}

void handle_enemies_movement() {
	static char i, j;
	static actor *enemy;
	static int incr_x, incr_y;
	
	level.incr_x.w += level.spd_x.w;
	level.incr_y.w += level.spd_y.w;
	
	incr_x = level.incr_x.b.h;
	if (level.stop_x_timer) incr_x = 0;
	
	incr_y = level.incr_y.b.h;
	if (level.stop_y_timer) incr_y = 0;

	for (i = 0; i != MAX_ENEMIES_Y; i++) {
		enemy = enemies[i];
		for (j = 0; j != MAX_ENEMIES_X; j++) {
			if (enemy->active) {
				enemy->x += incr_x;
				enemy->y += incr_y;
				
				if (enemy->x > 255) {
					enemy->x -= 255;
				} else if (enemy->x < 0) {
					enemy->x += 255;
				}
					
				if (enemy->y > SCREEN_H) enemy->y -= SCREEN_H;
				
				if (is_colliding_with_shot(enemy)) {
					enemy->active = 0;
					shot.active = 0;

					add_score(level.enemy_score);
					PSGSFXPlay(enemy_death_psg, SFX_CHANNELS2AND3);
				}
			}

			enemy++;
		}
		
		if (level.odd_x_speed) incr_x = -incr_x;
	}

	level.incr_x.b.h = 0;
	level.incr_y.b.h = 0;
	
	if (level.stop_x_timer) {
		level.stop_x_timer--;
	} else if (level.invert_x_timer) {
		level.invert_x_timer--;
		if (!level.invert_x_timer) level.spd_x.w = -level.spd_x.w;
	} else {
		// Completed both timers: reset them.
		level.stop_x_timer = level.stop_x_timer_max;
		level.invert_x_timer = level.invert_x_timer_max;
	}
	
	if (level.stop_y_timer) {
		level.stop_y_timer--;
	} else if (level.advance_y_timer) {
		level.advance_y_timer--;
	} else if (level.invert_y_timer) {
		level.invert_y_timer--;
		if (!level.invert_y_timer) level.spd_y.w = -level.spd_y.w;
	} else {
		// Completed both timers: reset them.
		level.stop_y_timer = level.stop_y_timer_max;
		level.advance_y_timer = level.advance_y_timer_max;
		level.invert_y_timer = level.invert_y_timer_max;
	}
}

char count_enemies() {
	static char i, j, count;
	static actor *enemy;

	count = 0;
	for (i = 0; i != MAX_ENEMIES_Y; i++) {
		enemy = enemies[i];
		for (j = 0; j != MAX_ENEMIES_X; j++) {
			if (enemy->active) count++;
			enemy++;
		}
	}
	
	return count;
}

void fire_as_enemy_shot(actor *enm_shot) {
	static char i, j, count, target;
	static actor *enemy;
	
	if (!level.enemy_count) return;

	count = 0;
	target = rand() % level.enemy_count;
	for (i = 0; i != MAX_ENEMIES_Y; i++) {
		enemy = enemies[i];
		for (j = 0; j != MAX_ENEMIES_X; j++) {
			if (enemy->active) {
				if (count == target) {
					enm_shot->x = enemy->x + 4;
					enm_shot->y = enemy->y + 12;
					enm_shot->active = 1;
					
					return;
				}
				count++;
			}
			enemy++;
		}
	}
}

void init_enemy_shots() {
	static char i;
	static actor *enm_shot;

	for (i = 0, enm_shot = enemy_shots; i != MAX_ENEMY_SHOTS; i++, enm_shot++) {
		init_actor(enm_shot, (i << 4) + 8, i << 4, 1, 1, 8, 1);
		enm_shot->active = 0;
	}
}

void draw_enemy_shots() {
	static char i;
	static actor *enm_shot;

	for (i = 0, enm_shot = enemy_shots; i != MAX_ENEMY_SHOTS; i++, enm_shot++) {
		draw_actor(enm_shot);
	}
}

void handle_enemy_shots_movement() {
	static char i;
	static actor *enm_shot;

	for (i = 0, enm_shot = enemy_shots; i != MAX_ENEMY_SHOTS; i++, enm_shot++) {
		if (enm_shot->active) {
			enm_shot->y += ENEMY_SHOT_SPEED;
			if (enm_shot->y > SCREEN_H) enm_shot->active = 0;
		} else {
			if (rand() & 0x1F) fire_as_enemy_shot(enm_shot);
		}
	}
}

void interrupt_handler() {
	PSGFrame();
	PSGSFXFrame();
}

void set_numeric_label(numeric_label *lbl, unsigned int value) {
	lbl->value = value;
	lbl->dirty = 1;
}

void init_numeric_label(numeric_label *lbl, char x, char y) {
	set_numeric_label(lbl, 0);
	lbl->x = x;
	lbl->y = y;
}

void add_numeric_label(numeric_label *lbl, int delta) {
	lbl->value += delta;
	lbl->dirty = 1;	
}

void draw_numeric_label(numeric_label *lbl) {
	if (!lbl->dirty) return;
	
	SMS_setNextTileatXY(lbl->x, lbl->y);
	printf("%d", lbl->value);
	
	lbl->dirty = 0;
}

void init_score() {
	init_numeric_label(&score, 3, 0);
}

void add_score(int delta) {
	add_numeric_label(&score, delta);
}

void draw_score() {
	draw_numeric_label(&score);
}

void init_level() {
	level_info *info = level_infos + ((level.number - 1) % MAX_LEVELS);
	
	level.enemy_tile = 64 + (((level.number - 1) % MAX_ENEMY_TILES) << 2);
	level.enemy_score = level.number / 3 * 5 + 5;
	
	level.incr_x.w = 0;
	level.incr_y.w = 0;
	level.spd_x.w = info->spd_x;
	level.spd_y.w = info->spd_y;
	
	level.horizontal_spacing = 256 / 3;
	level.horizontal_odd_spacing = (info->flags & LV_ODD_SPACING) ? 256 / 6 : 0;
	level.vertical_spacing = (info->flags & LV_FULL_HEIGHT) ? 64 : 24;
	level.odd_x_speed = info->flags & LV_ODD_X_SPEED;
	level.starting_y = (info->flags & LV_FULL_HEIGHT) ? -128 : 0;
	
	level.stop_x_timer_max = info->stop_x_timer;
	level.invert_x_timer_max = info->invert_x_timer;
	level.stop_x_timer = level.stop_x_timer_max;
	level.invert_x_timer = level.invert_x_timer_max;
	
	level.stop_y_timer_max = info->stop_y_timer;
	level.advance_y_timer_max = info->advance_y_timer;
	level.invert_y_timer_max = info->invert_y_timer;
	level.stop_y_timer = level.stop_y_timer_max;
	level.advance_y_timer = level.advance_y_timer_max;
	level.invert_y_timer = level.invert_y_timer_max;

	
	level.cheat_skip = 0;
	
	init_enemies();
	init_enemy_shots();		
	shot.active = 0;
}

void main() {
	SMS_useFirstHalfTilesforSprites(1);
	SMS_setSpriteMode(SPRITEMODE_TALL);
	SMS_VDPturnOnFeature(VDPFEATURE_HIDEFIRSTCOL);

	SMS_displayOff();
	SMS_loadPSGaidencompressedTiles(sprites_tiles_psgcompr, 0);
	configure_text();
	load_standard_palettes();

	SMS_setNextTileatXY(1, 1);
	puts("Test!");

	SMS_setLineInterruptHandler(&interrupt_handler);
	SMS_setLineCounter(180);
	SMS_enableLineInterrupt();

	SMS_displayOn();
	
	init_actor(&player, 120, PLAYER_BOTTOM, 2, 1, 2, 1);
	init_actor(&shot, 120, PLAYER_BOTTOM - 8, 1, 1, 6, 1);
	
	level.number = 1;
	init_score();
	init_level();

	while (1) {
		level.enemy_count = count_enemies();
		if (!level.enemy_count || level.cheat_skip) {
			if (level.cheat_skip) {
				wait_button_release();
			}
			
			level.number++;
			init_level();
		}
		
		handle_player_input();
		handle_shot_movement();
		handle_enemies_movement();
		handle_enemy_shots_movement();
		
		SMS_initSprites();

		draw_actor(&player);
		draw_actor(&shot);
		draw_enemies();
		draw_enemy_shots();
		
		SMS_finalizeSprites();
		SMS_waitForVBlank();
		SMS_copySpritestoSAT();

		draw_score();
	}
}

SMS_EMBED_SEGA_ROM_HEADER(9999,0); // code 9999 hopefully free, here this means 'homebrew'
SMS_EMBED_SDSC_HEADER(0,1, 2022,03,13, "Haroldo-OK\\2022", "Food Fighter",
  "A food-based SHMUP.\n"
  "Made for the SMS Power! Coding Competition 2022 - https://www.smspower.org/forums/18879-Competitions2022DeadlineIs27thMarch\n"
  "Built using devkitSMS & SMSlib - https://github.com/sverx/devkitSMS");

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

actor player;
actor shot;

actor enemies[MAX_ENEMIES_Y][MAX_ENEMIES_X];
actor enemy_shots[MAX_ENEMY_SHOTS];

struct level {
	char number;
	
	char enemy_count;
	char horizontal_spacing, horizontal_odd_spacing;
	
	fixed incr_x, incr_y;
	fixed spd_x, spd_y;
} level;

void load_standard_palettes() {
	SMS_loadBGPalette(sprites_palette_bin);
	SMS_loadSpritePalette(sprites_palette_bin);
	SMS_setSpritePaletteColor(0, 0);
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

	for (i = 0, y = 0; i != MAX_ENEMIES_Y; i++, y += 32) {
		enemy = enemies[i];
		for (j = 0, x = i & 1 ? level.horizontal_odd_spacing : 0; j != MAX_ENEMIES_X; j++, x += level.horizontal_spacing) {
			init_actor(enemy, x, i << 5, 2, 1, 64, 6);
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
	
	level.incr_x.w += level.spd_x.w;
	level.incr_y.w += level.spd_y.w;

	for (i = 0; i != MAX_ENEMIES_Y; i++) {
		enemy = enemies[i];
		for (j = 0; j != MAX_ENEMIES_X; j++) {
			if (enemy->active) {
				enemy->x += level.incr_x.b.h;
				enemy->y += level.incr_y.b.h;
				
				if (enemy->x > 255) enemy->x -= 255;
				if (enemy->y > SCREEN_H) enemy->y -= SCREEN_H;
				
				if (is_colliding_with_shot(enemy)) {
					enemy->active = 0;
					shot.active = 0;

					PSGSFXPlay(enemy_death_psg, SFX_CHANNELS2AND3);
				}
			}

			enemy++;
		}
	}

	level.incr_x.b.h = 0;
	level.incr_y.b.h = 0;
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

void init_level() {
	level.incr_x.w = 0;
	level.incr_y.w = 0;
	level.spd_x.w = 192;
	level.spd_y.w = 0;
	
	level.horizontal_spacing = 256 / 3;
	level.horizontal_odd_spacing = 256 / 6;

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
	load_standard_palettes();

	SMS_setLineInterruptHandler(&interrupt_handler);
	SMS_setLineCounter(180);
	SMS_enableLineInterrupt();

	SMS_displayOn();
	
	init_actor(&player, 120, PLAYER_BOTTOM, 2, 1, 2, 1);
	init_actor(&shot, 120, PLAYER_BOTTOM - 8, 1, 1, 6, 1);

	level.number = 1;
	init_level();

	while (1) {
		level.enemy_count = count_enemies();
		if (!level.enemy_count) {
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
	}
}

SMS_EMBED_SEGA_ROM_HEADER(9999,0); // code 9999 hopefully free, here this means 'homebrew'
SMS_EMBED_SDSC_HEADER(0,2, 2021,10,02, "Haroldo-OK\\2021", "Geometry Blaster",
  "A geometric shoot-em-up.\n"
  "Made for the Minimalist Game Jam - https://itch.io/jam/minimalist-game-jam\n"
  "Built using devkitSMS & SMSlib - https://github.com/sverx/devkitSMS");

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lib/SMSlib.h"
#include "lib/PSGlib.h"
#include "actor.h"
#include "data.h"

#define PLAYER_SPEED 2
#define PLAYER_SHOT_SPEED 4
#define PLAYER_TOP (16)
#define PLAYER_LEFT (8)
#define PLAYER_RIGHT (256 - 16)
#define PLAYER_BOTTOM (SCREEN_H - 24)

actor player;
actor shot;
actor enemy;

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
		}
	}
}

void handle_shot_movement() {
	if (!shot.active) return;
	
	shot.y -= PLAYER_SHOT_SPEED;	
	if (shot.y < -8) shot.active = 0;
}

void main() {
	SMS_useFirstHalfTilesforSprites(1);
	SMS_setSpriteMode(SPRITEMODE_TALL);
	SMS_VDPturnOnFeature(VDPFEATURE_HIDEFIRSTCOL);

	SMS_displayOff();
	SMS_loadPSGaidencompressedTiles(sprites_tiles_psgcompr, 0);
	load_standard_palettes();

	SMS_displayOn();
	
	init_actor(&player, 120, PLAYER_BOTTOM, 2, 1, 2, 1);
	init_actor(&shot, 120, PLAYER_BOTTOM - 8, 2, 1, 6, 1);
	init_actor(&enemy, 120, 32, 2, 1, 64, 6);
	
	shot.active = 0;

	while (1) {
		handle_player_input();
		handle_shot_movement();
		
		SMS_initSprites();

		draw_actor(&player);
		draw_actor(&shot);
		draw_actor(&enemy);
		
		SMS_finalizeSprites();
		SMS_waitForVBlank();
		SMS_copySpritestoSAT();
	}
}

SMS_EMBED_SEGA_ROM_HEADER(9999,0); // code 9999 hopefully free, here this means 'homebrew'
SMS_EMBED_SDSC_HEADER(0,1, 2021,10,01, "Haroldo-OK\\2021", "Geometry Blaster",
  "A geometric shoot-em-up.\n"
  "Made for the Minimalist Game Jam - https://itch.io/jam/minimalist-game-jam\n"
  "Built using devkitSMS & SMSlib - https://github.com/sverx/devkitSMS");

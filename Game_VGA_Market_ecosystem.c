#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#define VGA_PIXEL_BUFFER_BASE 0x08000000 // 0xC8000000 - For ARMv7-DE1-SoC
#define CHAR_BUFFER_BASE      0x09000000 // 0xC9000000 - For ARMv7-DE1-SoC
#define KEY_BASE              0xFF200050
#define SW_BASE               0xFF200040
#define HEX3_HEX0_BASE 		  0xFF200020
#define HEX5_HEX4_BASE        0xFF200030

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

#define HEADER_HEIGHT 20
#define FOOTER_HEIGHT 28
#define PLAY_HEIGHT   (SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT)

#define CELL_SIZE 4
#define LOGICAL_WIDTH  (SCREEN_WIDTH / CELL_SIZE)
#define LOGICAL_HEIGHT (PLAY_HEIGHT / CELL_SIZE)

#define COLOR_HEADER  0x2965 
#define COLOR_FOOTER  0x2965 
#define COLOR_GRID    0x2104 

#define COLOR_ALIVE   0x07E0 
#define COLOR_DECAY   0xFD30 
#define COLOR_DEAD    0x0000 

#define THRESHOLD_HEALTHY 100
#define GROWTH_RATE       20
#define DECAY_RATE        75
#define BAILOUT_CAPITAL   150
#define LEVERAGE_CRITICAL 6

typedef struct {
    uint8_t capital;
    uint8_t leverage;
} entity_t;

entity_t grid_a[LOGICAL_HEIGHT][LOGICAL_WIDTH];
entity_t grid_b[LOGICAL_HEIGHT][LOGICAL_WIDTH];

entity_t (*current_grid)[LOGICAL_WIDTH] = grid_a;
entity_t (*next_grid)[LOGICAL_WIDTH] = grid_b;

volatile int *key_data = (volatile int *)KEY_BASE;
volatile int *key_edge = (volatile int *)(KEY_BASE + 0xC);
volatile int *sw_data  = (volatile int *)SW_BASE;
volatile int *hex3_hex0 = (volatile int *)HEX3_HEX0_BASE;
volatile int *hex5_hex4 = (volatile int *)HEX5_HEX4_BASE;

int old_x = -1;
int old_y = -1;

int curr_alive = 17; 
int curr_decay = 0;

const uint8_t hex_digits[10] = {
    0x3F, 
    0x06, 
    0x5B, 
    0x4F, 
    0x66, 
    0x6D, 
    0x7D, 
    0x07, 
    0x7F, 
    0x67  
};

void write_pixel(int x, int y, short colour) {
    volatile short *vga_addr = (volatile short*)(VGA_PIXEL_BUFFER_BASE + (y << 10) + (x << 1));
    *vga_addr = colour;
}

void write_char(int x, int y, char c) {
    volatile char *char_addr = (volatile char *)(CHAR_BUFFER_BASE + (y << 7) + x);
    *char_addr = c;
}

void write_string(int x, int y, const char *str) {
    while (*str) {
        write_char(x, y, *str);
        x++;
        str++;
    }
}

void draw_rect(int start_x, int start_y, int width, int height, short colour) {
    for (int y = start_y; y < start_y + height; y++) {
        for (int x = start_x; x < start_x + width; x++) {
            write_pixel(x, y, colour);
        }
    }
}

void write_cell(int logical_x, int logical_y, short colour) {
    int start_x = logical_x * CELL_SIZE;
    int start_y = HEADER_HEIGHT + (logical_y * CELL_SIZE);
  
    for (int dy = 0; dy < CELL_SIZE - 1; dy++) {
        for (int dx = 0; dx < CELL_SIZE - 1; dx++) {
            write_pixel(start_x + dx, start_y + dy, colour);
        }
    }
}

void clear_char_buffer() {
    for (int y = 0; y < 60; y++) {
        for (int x = 0; x < 80; x++) {
            write_char(x, y, ' ');
        }
    }
}

void draw_ui_framework() {
    draw_rect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COLOR_HEADER);
    draw_rect(0, SCREEN_HEIGHT - FOOTER_HEIGHT, SCREEN_WIDTH, FOOTER_HEIGHT, COLOR_FOOTER);
    draw_rect(0, HEADER_HEIGHT, SCREEN_WIDTH, PLAY_HEIGHT, COLOR_GRID);

    write_string(24, 2, "Sarthak's Game of Market Ecosystems");
}

void update_footer_ui(bool is_playing, bool draw_brush, bool erase_brush, bool slow_speed) {
    write_string(1, 54, "                                                               ");
    write_string(1, 56, "                                                               ");
	write_string(1, 58, "                                                               ");

    if (is_playing) {
		if (slow_speed) 
						  write_string(2, 54, "[ STATUS: PLAYING ( SLOW ) ] | SW9: PAUSE ( DOWN )");
        else               
						  write_string(2, 54, "[ STATUS: PLAYING ( FAST ) ] | SW9: PAUSE ( DOWN )");
	}
    else                  write_string(2, 54, "[ STATUS: PAUSED ]           | SW9: PLAY ( UP )   ");

    if (draw_brush)       write_string(2, 56, "[ BRUSH: DRAW ( SW0 ) ]      | SW2: INFO          | SW8: RESET SCREEN");
    else if (erase_brush) write_string(2, 56, "[ BRUSH: DEL ( SW1 )  ]      | SW2: INFO          | SW8: RESET SCREEN");
    else                  write_string(2, 56, "[ BRUSH: OFF          ]      | SW2: INFO          | SW8: RESET SCREEN");
	
	write_string(2, 58, "KEYS: BO -> RIGHT  B1 -> LEFT  B2 -> DOWN  B3 -> UP");
}

void show_info_screen() {
    draw_rect(0, HEADER_HEIGHT, SCREEN_WIDTH, PLAY_HEIGHT, COLOR_GRID);
	write_string(5, 8,  "ECOSYSTEM RULES:");
    write_string(5, 11, "1. SURVIVAL: 2 or 3 healthy neighbors. Capital grows steadily.");
    write_string(5, 13, "2. RECESSION: < 2 healthy neighbors. Market decays (Vermillion).");
    write_string(5, 15, "3. COMPETITION: > 3 healthy neighbors. Market dies from overcrowding.");
    write_string(5, 17, "4. BAILOUT: Exactly 3 healthy neighbors inject capital to revive.");
    write_string(5, 19, "5. LEVERAGE RISK: Surviving consecutive turns increases leverage.");
    write_string(5, 21, "   If leverage hits CRITICAL limit, the market instantly crashes.");
    write_string(5, 24, "HOW THIS SIMULATES THE MARKET:");
    write_string(5, 27, "- Each cell is a regional market. Green = Thriving, Red = Failing.");
    write_string(5, 29, "- Markets rely on local trade (neighbors) to sustain themselves.");
    write_string(5, 31, "- Too little trade causes decay (neighbors < 2).");
	write_string(5, 33, "- Too much trade/competition causes systemic collapse. (neighbors > 3)");
    write_string(5, 35, "- Sustained prosperity breeds dangerous risk (Leverage). If a market");
    write_string(5, 37, "  stays healthy too long without correcting, a crash is inevitable.");
    write_string(5, 39, "- A perfect balance of neighbors ( =3 ) can pool resources for a Bailout.");
    write_string(5, 43, "Turn SW2 DOWN to return to the simulation.");
}

void redraw_grid_pixels() {
    draw_rect(0, HEADER_HEIGHT, SCREEN_WIDTH, PLAY_HEIGHT, COLOR_GRID);
    for (int y = 0; y < LOGICAL_HEIGHT; y++) {
        for (int x = 0; x < LOGICAL_WIDTH; x++) {
            short color = COLOR_DEAD;
            if (current_grid[y][x].capital >= THRESHOLD_HEALTHY) color = COLOR_ALIVE;
            else if (current_grid[y][x].capital > 0) color = COLOR_DECAY;
            write_cell(x, y, color);
        }
    }
}

void hide_info_screen() {
    for (int y = 6; y < 54; y++) {
        for (int x = 0; x < 80; x++) {
            write_char(x, y, ' ');
        }
    }
    redraw_grid_pixels();
}

void draw_cursor(int char_x, int char_y, bool is_playing, bool info_mode) {
    if (old_x != -1 && old_y != -1 && (old_x != char_x || old_y != char_y || is_playing || info_mode)) {
        write_char(old_x, old_y, ' ');
        if (is_playing || info_mode) {
            old_x = -1;
            old_y = -1;
        }
    }

    if (!is_playing && !info_mode) {
        write_char(char_x, char_y, '+');
        old_x = char_x;
        old_y = char_y;
    }
}

void init_grid() {
    for (int y = 0; y < LOGICAL_HEIGHT; y++) {
        for (int x = 0; x < LOGICAL_WIDTH; x++) {
            grid_a[y][x].capital = 0;
            grid_a[y][x].leverage = 0;
            
            grid_b[y][x].capital = 0;
            grid_b[y][x].leverage = 0;
        }
    }

    int cx = LOGICAL_WIDTH / 2;
    int cy = LOGICAL_HEIGHT / 2;
    
    current_grid[cy - 1][cx].capital = 255; current_grid[cy - 1][cx].leverage = 1; 
	current_grid[cy - 2][cx].capital = 255; current_grid[cy - 2][cx].leverage = 1; 
    current_grid[cy - 2][cx + 1].capital = 255; current_grid[cy - 2][cx + 1].leverage = 1; 
	current_grid[cy - 2][cx + 2].capital = 255; current_grid[cy - 2][cx + 2].leverage = 1; 
    current_grid[cy][cx - 1].capital = 255; current_grid[cy][cx - 1].leverage = 1; 
	current_grid[cy][cx - 2].capital = 255; current_grid[cy][cx - 2].leverage = 1;
    current_grid[cy - 1][cx - 2].capital = 255; current_grid[cy - 1][cx - 2].leverage = 1; 
	current_grid[cy - 2][cx - 2].capital = 255; current_grid[cy - 2][cx - 2].leverage = 1;
 	current_grid[cy][cx + 1].capital = 255; current_grid[cy][cx + 1].leverage = 1; 
	current_grid[cy][cx + 2].capital = 255; current_grid[cy][cx + 2].leverage = 1;
    current_grid[cy + 1][cx + 2].capital = 255; current_grid[cy + 1][cx + 2].leverage = 1; 
	current_grid[cy + 2][cx + 2].capital = 255; current_grid[cy + 2][cx + 2].leverage = 1; 
    current_grid[cy][cx].capital = 255; current_grid[cy][cx].leverage = 1; 
    current_grid[cy + 1][cx].capital = 255; current_grid[cy + 1][cx].leverage = 1; 
	current_grid[cy + 2][cx].capital = 255; current_grid[cy + 2][cx].leverage = 1; 
	current_grid[cy + 2][cx - 1].capital = 255; current_grid[cy + 2][cx - 1].leverage = 1; 
	current_grid[cy + 2][cx - 2].capital = 255; current_grid[cy + 2][cx - 2].leverage = 1; 
}

void calculate_next_generation() {
	curr_alive = 0;
	curr_decay = 0;
	
    for (int y = 1; y < LOGICAL_HEIGHT-1; y++) {
        for (int x = 1; x < LOGICAL_WIDTH-1; x++) {
            int healthy_neighbors = 0;

            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    
                    int nx = x + dx;
                    int ny = y + dy;
                    
                    if (current_grid[ny][nx].capital >= THRESHOLD_HEALTHY) {
                        healthy_neighbors++;
                    }
                }
            }

            entity_t current = current_grid[y][x];
            entity_t next_state = current;

            if (current.capital >= THRESHOLD_HEALTHY) {
                if (healthy_neighbors > 3) {
                    next_state.capital = 0; 
					next_state.leverage = 0;
                } else if (healthy_neighbors < 2) {
                    if (next_state.capital > DECAY_RATE) next_state.capital -= DECAY_RATE;
                    else {
						next_state.capital = 0;
						next_state.leverage = 0;
					}
                } else {
                    if (next_state.capital < 255 - GROWTH_RATE) next_state.capital += GROWTH_RATE;
                	next_state.leverage += 1;
				}
            } 
            else if (current.capital > 0) {
                if (healthy_neighbors > 3) {
                    next_state.capital = 0; 
                } else if (healthy_neighbors == 3) {
                    next_state.capital = BAILOUT_CAPITAL; 
                } else {
                    if (next_state.capital > DECAY_RATE) next_state.capital -= DECAY_RATE;
                    else next_state.capital = 0;
                }
            } 
            else {
                if (healthy_neighbors == 3) {
                    next_state.capital = BAILOUT_CAPITAL;
                }
            }
			
			if (next_state.leverage > LEVERAGE_CRITICAL){
				next_state.capital = 0; 
				next_state.leverage = 0;
			}
			
			if (next_state.capital >= THRESHOLD_HEALTHY) {
                curr_alive++;
            } else if (next_state.capital > 0) {
                curr_decay++;
            }

            next_grid[y][x] = next_state;
			
            uint16_t color = COLOR_DEAD;
            if (next_state.capital >= THRESHOLD_HEALTHY) color = COLOR_ALIVE;
            else if (next_state.capital > 0) color = COLOR_DECAY;
            
            write_cell(x, y, color);
        }
    }

    entity_t (*temp)[LOGICAL_WIDTH] = current_grid;
    current_grid = next_grid;
    next_grid = temp;
}

void update_hex_displays(int alive, int decay) {
    if (alive > 999) alive = 999;
    if (decay > 999) decay = 999;

    int d0 = hex_digits[decay % 10];
    int d1 = hex_digits[(decay / 10) % 10];
    int d2 = hex_digits[(decay / 100) % 10];

    int a0 = hex_digits[alive % 10];
    int a1 = hex_digits[(alive / 10) % 10];
    int a2 = hex_digits[(alive / 100) % 10];

    *hex3_hex0 = (a0 << 24) | (d2 << 16) | (d1 << 8) | d0;

    *hex5_hex4 = (a2 << 8) | a1;
}

int main() {
    clear_char_buffer();
    draw_ui_framework();
    init_grid();
    redraw_grid_pixels();

    int cursor_x = LOGICAL_WIDTH / 2;
    int cursor_y = LOGICAL_HEIGHT / 2;
    bool showing_info = false;

    *key_edge = 0xF; 
	
	bool prev_clear_screen = false;

    while (1) {
        int sw = *sw_data;
        bool is_playing = (sw & (1 << 9)) != 0;
        bool clear_screen = (sw & (1 << 8)) != 0;
		bool slow_speed = (sw & (1 << 7)) != 0;
        bool draw_brush = (sw & (1 << 0)) != 0;
        bool erase_brush = (sw & (1 << 1)) != 0;
        bool want_info  = (sw & (1 << 2)) != 0;

        if(clear_screen && !is_playing && !prev_clear_screen) {
			curr_alive = 17;
			curr_decay = 0;
            cursor_x = LOGICAL_WIDTH / 2;
            cursor_y = LOGICAL_HEIGHT / 2;
            showing_info = false;
            clear_char_buffer();
            draw_ui_framework();
            init_grid();
            redraw_grid_pixels();
        }
		
		prev_clear_screen = clear_screen;

        if (want_info && !showing_info) {
            show_info_screen();
            showing_info = true;
        } else if (!want_info && showing_info) {
            hide_info_screen();
            showing_info = false;
        }


        update_footer_ui(is_playing, draw_brush, erase_brush, slow_speed);

        update_hex_displays(curr_alive, curr_decay);

        int char_x = cursor_x;
        int char_y = (HEADER_HEIGHT / 4) + cursor_y; 
        
        draw_cursor(char_x, char_y, is_playing, showing_info);

        int edge = *key_edge;
        if (edge != 0) {
            if (!is_playing && !showing_info) {
                if (edge & 0x8) { 
                    cursor_y--;
                    if (cursor_y < 0) cursor_y = LOGICAL_HEIGHT - 1;
                }
                if (edge & 0x4) { 
                    cursor_y++;
                    if (cursor_y >= LOGICAL_HEIGHT) cursor_y = 0;
                }
                if (edge & 0x2) { 
                    cursor_x--;
                    if (cursor_x < 0) cursor_x = LOGICAL_WIDTH - 1;
                }
                if (edge & 0x1) { 
                    cursor_x++;
                    if (cursor_x >= LOGICAL_WIDTH) cursor_x = 0;
                }
            }
            *key_edge = edge; 
        }

        if (!is_playing && !showing_info) {
            if (draw_brush) {
                uint8_t old_cap = current_grid[cursor_y][cursor_x].capital;

                if (old_cap < THRESHOLD_HEALTHY) {
                    curr_alive++; 
                    if (old_cap > 0) {
                        curr_decay--;
                    }
                }
                current_grid[cursor_y][cursor_x].capital = 255;
                current_grid[cursor_y][cursor_x].leverage = 1;
                write_cell(cursor_x, cursor_y, COLOR_ALIVE);
            } else if (erase_brush) {
                uint8_t old_cap = current_grid[cursor_y][cursor_x].capital;

                if (old_cap >= THRESHOLD_HEALTHY) {
                    curr_alive--;
                } else if (old_cap > 0) {
                    curr_decay--;
                }
                current_grid[cursor_y][cursor_x].capital = 0;
                current_grid[cursor_y][cursor_x].leverage = 0;
                write_cell(cursor_x, cursor_y, COLOR_DEAD);
            }
        } else if (is_playing && !showing_info) {
            calculate_next_generation();
        }
		
		int speed = slow_speed ? 4000000 : 40000;

        for (int i = 0; i < speed; i++) {
            asm volatile("nop");
        }
    }

    return 0;
}
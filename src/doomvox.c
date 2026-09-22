//
// Copyright(C) 2022 Wojciech Graj
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#include "doomkeys.h"
#include "m_controls.h"
#include "i_system.h"
#include "doomgeneric.h"

#include "roto_matrix.h"
#include "vx_main.h"
#include "m_argv.h"
#include "doomstat.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <time.h>


static uint8_t button_map[] = {
	KEY_ENTER, // BUTTON_A
	KEY_USE, // BUTTON_B
	KEY_FIRE, // BUTTON_X
	KEY_TAB, // BUTTON_Y
	KEY_STRAFE_L, // BUTTON_LB
	KEY_STRAFE_R, // BUTTON_RB
	0,//KEY_F11, // BUTTON_VIEW
	KEY_ESCAPE, // BUTTON_MENU
	KEY_LEFTARROW, // BUTTON_LEFT
	KEY_RIGHTARROW, // BUTTON_RIGHT
	KEY_UPARROW, // BUTTON_UP
	KEY_DOWNARROW // BUTTON_DOWN
};

typedef enum {
    BUTTON_A,
    BUTTON_B,
    BUTTON_X,
    BUTTON_Y,
    BUTTON_LB,
    BUTTON_RB,
    BUTTON_VIEW,
    BUTTON_MENU,
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_UP,
    BUTTON_DOWN
} button_t;


#define CLK CLOCK_REALTIME

struct timespec ts_init;
int pauseattic = -1;

extern bool sendpause;

void DG_Init() {
	int t;

	pauseattic = -1;
    if ((t = M_CheckParmWithArgs("-pauseattic", 1)) > 0) {
		int tic = atoi(myargv[t + 1]);
		if (tic > 1) {
			pauseattic = tic;
		}
	}

	clock_gettime(CLK, &ts_init);

	key_menu_confirm = KEY_USE;
}

void DG_DrawFrame() {
}

void DG_SleepMs(uint32_t ms)
{
	struct timespec ts = (struct timespec) {
		.tv_sec = ms / 1000,
		.tv_nsec = (ms % 1000ul) * 1000000,
	};
	nanosleep(&ts, NULL);
}

uint32_t DG_GetTicksMs() {
	struct timespec ts;
	clock_gettime(CLK, &ts);

	return (ts.tv_sec - ts_init.tv_sec) * 1000 + (ts.tv_nsec - ts_init.tv_nsec) / 1000000;
}

static bool dpad_state[4] = {0,0,0,0};

button_t dpad_event(uint8_t axis, int16_t value) {
	button_t pressed = -1;

    if (axis == 6) {
        if (value < -24000) {
            if (!dpad_state[0]) {
                dpad_state[0] = true;
				return BUTTON_LEFT;
            }
        } else if (value > 24000) {
            if (!dpad_state[1]) {
                dpad_state[1] = true;
                return BUTTON_RIGHT;
            }
        } else if (value > -8192 && value < 8192) {
			if (dpad_state[0]) {
				pressed = BUTTON_LEFT;
			}
			if (dpad_state[1]) {
				pressed = BUTTON_RIGHT;
			}
            dpad_state[0] = false;
            dpad_state[1] = false;
        }
    } else if (axis == 7) {
        if (value < -24000) {
            if (!dpad_state[2]) {
                dpad_state[2] = true;
                return BUTTON_UP;
            }
        } else if (value > 24000) {
            if (!dpad_state[3]) {
                dpad_state[3] = true;
                return BUTTON_DOWN;
            }
        } else if (value > -8192 && value < 8192) {
			if (dpad_state[2]) {
				pressed = BUTTON_UP;
			}
			if (dpad_state[3]) {
				pressed = BUTTON_DOWN;
			}
            dpad_state[2] = false;
            dpad_state[3] = false;
        }
    }
	return pressed;
}

static int get_doom_key(char* console_input, unsigned char* doomKey) {
	switch (console_input[0]) {
		case 10:
			*doomKey = KEY_ENTER;
			return 1;

		case 27:
			switch (console_input[1]) {
				case '[':
					switch (console_input[2]) {
						case 'A': *doomKey = KEY_UPARROW; 		return 3;
						case 'B': *doomKey = KEY_DOWNARROW; 	return 3;
						case 'C': *doomKey = KEY_RIGHTARROW; 	return 3;
						case 'D': *doomKey = KEY_LEFTARROW; 	return 3;
					}
				default:
					*doomKey = KEY_ESCAPE;
					return 1;
			}

		case ' ':
			*doomKey = KEY_FIRE;
			return 1;

		default:
			*doomKey = tolower(console_input[0]);
			return 1;
	}
}
extern void bayer_tweak(char ch);

static int get_console_key(int* pressed, unsigned char* doomKey) {
	static char input_buffer[16];
	static int input_size = 0;
	static int input_current = 0;
	static bool input_pressed = 0;

	if (input_current >= input_size) {
		struct termios original, noncanon;

		tcgetattr(STDIN_FILENO, &original);
		noncanon = original;
		noncanon.c_lflag &= ~(ICANON);
		noncanon.c_cc[VMIN] = 0;
		noncanon.c_cc[VTIME] = 0;
		tcsetattr(STDIN_FILENO, TCSANOW, &noncanon);

		input_size = read(2, input_buffer, countof(input_buffer) - 1u);
		input_current = 0;
		input_pressed = 0;
		if (input_size > 0) {
			input_buffer[input_size] = '\0';
		}

		tcsetattr(STDIN_FILENO, TCSANOW, &original);
		tcflush(STDIN_FILENO, TCIFLUSH);
	}

	if (input_current >= input_size) {
		return 0;
	}

	int advance = get_doom_key(&input_buffer[input_current], doomKey);
	//printf("c 0x%x 0x%x\n", input_buffer[input_current], *doomKey);

	input_pressed = !input_pressed;
	*pressed = input_pressed;

	if (!input_pressed)	{
		switch (input_buffer[input_current]) {
			case ']': {
				++pauseattic;
				printf("pause attic %d\n", pauseattic);
			} break;

			case 'p': {
				if (pauseattic == leveltime) {
					pauseattic = -1;
				} else {
					pauseattic = leveltime;
					printf("pause attic %d\n", pauseattic);
				}
			} break;

			case 'q':
			case 'a':
			case 'w':
			case 's': {
				bayer_tweak(input_buffer[input_current]);
			} break;
		}
		

		input_current += advance;
	}
	return 1;
}

#include <linux/joystick.h> // stomps doomkeys key defines!

static event_t joystick = {ev_joystick};
float js_axis[8] = {0};

int DG_GetKey(int* pressed, unsigned char* doomKey) {

	if (pauseattic >= 0 && leveltime >= pauseattic) {
		sendpause = true;
	}

	if (get_console_key(pressed, doomKey)) {
		return 1;
	}

	static int js = -1;
    struct js_event event;

	if (js == -1) {
		js = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
	} else {
		ssize_t events;
		while ((events = read(js, &event, sizeof(event))) > 0) {
			if (event.type == JS_EVENT_BUTTON) {
				*pressed = event.value;
				*doomKey = button_map[event.number % sizeof(button_map)];
				//printf("j %x 0x%x\n", *pressed, *doomKey);
				return 1;
			} else if (event.type == JS_EVENT_AXIS) {
				if (event.number == 6 || event.number == 7) {
					button_t dpad = dpad_event(event.number, event.value);
					if (dpad >= 0) {
						*pressed = dpad_state[dpad - BUTTON_LEFT];
						*doomKey = button_map[dpad];
						return 1;
					}
				}
				switch (event.number) {
					case 0: {
						joystick.data4 = event.value / 128; //strafe
					} break;

					case 1: {
						joystick.data3 = event.value / 128; // y
					} break;

					case 3: {
						joystick.data2 = event.value / 128; // x
					} break;
				}
				if (event.number < countof(js_axis)) {
					js_axis[event.number] = (float)event.value / (float)32767.0f;
					if (event.number == 2 || event.number == 5) {
						js_axis[event.number] = (js_axis[event.number] + 1.0f) * 0.5f;
					}
				}
			}

		}
	}

	return 0;
}

void DG_PostJoystick() {
	D_PostEvent(&joystick);

	float zoom = js_axis[2] - js_axis[5];
	vx_scale = (vx_scale * (1.0f + zoom * 0.0625f));
	//printf("vx_scale: %g\n", vx_scale);
}

void DG_SetWindowTitle(const char *title)
{
	(void)title;
}

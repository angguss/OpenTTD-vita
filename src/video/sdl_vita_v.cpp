/* $Id: sdl_v.cpp 27775 2017-03-11 13:05:54Z frosch $ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sdl_v.cpp Implementation of the SDL video driver. */

#if defined(WITH_SDL) && defined(__vita__)

#include "../stdafx.h"
#include "../openttd.h"
#include "../gfx_func.h"
#include "../sdl.h"
#include "../rev.h"
#include "../blitter/factory.hpp"
#include "../network/network.h"
#include "../thread/thread.h"
#include "../progress.h"
#include "../core/random_func.hpp"
#include "../core/math_func.hpp"
#include "../fileio_func.h"
#include "sdl_v.h"
#include <SDL.h>

#include "../safeguards.h"

static FVideoDriver_SDL iFVideoDriver_SDL;

//static uint32 *_screenbuf = NULL;
static SDL_Texture *_sdl_screentext;
// Operations:
// We create a surface to draw to, handle palettes etc
// We draw these to the texture
// Once done, we sync the texture to the screen
static SDL_Surface *_sdl_realsurface;
//static SDL_Surface *_sdl_surface;
static SDL_Window *_sdl_window;
static SDL_Renderer *_sdl_renderer;
// The renderer dest format for the conversion
static SDL_PixelFormat *_dst_format;
static SDL_Joystick *_sdl_joystick;
static bool _all_modes;

/** Whether the drawing is/may be done in a separate thread. */
static bool _draw_threaded;
/** Thread used to 'draw' to the screen, i.e. push data to the screen. */
static ThreadObject *_draw_thread = NULL;
/** Mutex to keep the access to the shared memory controlled. */
static ThreadMutex *_draw_mutex = NULL;
/** Should we keep continue drawing? */
static volatile bool _draw_continue;
static Palette _local_palette;

#define MAX_DIRTY_RECTS 100
static SDL_Rect _dirty_rects[MAX_DIRTY_RECTS];
static int _num_dirty_rects;
static int _use_hwpalette;
static int _requested_hwpalette; /* Did we request a HWPALETTE for the current video mode? */

// PS Vita max touch coordinates
#define MAX_TOUCH_X (960 * 2)
#define MAX_TOUCH_Y (544 * 2)
static float _touch_scale_x = 0.f;
static float _touch_scale_y = 0.f;

void VideoDriver_SDL::MakeDirty(int left, int top, int width, int height)
{
	if (_num_dirty_rects < MAX_DIRTY_RECTS) {
		_dirty_rects[_num_dirty_rects].x = left;
		_dirty_rects[_num_dirty_rects].y = top;
		_dirty_rects[_num_dirty_rects].w = width;
		_dirty_rects[_num_dirty_rects].h = height;
	}
	_num_dirty_rects++;
}

static void UpdatePalette(bool init = false)
{
	SDL_Color pal[256];

	for (int i = 0; i != _local_palette.count_dirty; i++) {
		pal[i].r = _local_palette.palette[_local_palette.first_dirty + i].r;
		pal[i].g = _local_palette.palette[_local_palette.first_dirty + i].g;
		pal[i].b = _local_palette.palette[_local_palette.first_dirty + i].b;
		pal[i].a = 0;
	}

	SDL_SetPaletteColors(_sdl_realsurface->format->palette, pal, _local_palette.first_dirty, _local_palette.count_dirty);

	//SDL_BlitSurface(_sdl_realsurface, NULL, _sdl_surface, NULL);
	//SDL_UpdateWindowSurface(_sdl_window);

	//SDL_BlitSurface(_sdl_surface, NULL, _sdl_realsurface, NULL);


	//SDL_SetColors(_sdl_screen, pal, _local_palette.first_dirty, _local_palette.count_dirty);
	//SDL_SetPaletteColors(_sdl_screen->format->palette, pal, _local_palette.first_dirty, _local_palette.count_dirty);
	//if (_sdl_screen != _sdl_realscreen && init) {
		/* When using a shadow surface, also set our palette on the real screen. This lets SDL
		 * allocate as much colors (or approximations) as
		 * possible, instead of using only the default SDL
		 * palette. This allows us to get more colors exactly
		 * right and might allow using better approximations for
		 * other colors.
		 *
		 * Note that colors allocations are tried in-order, so
		 * this favors colors further up into the palette. Also
		 * note that if two colors from the same animation
		 * sequence are approximated using the same color, that
		 * animation will stop working.
		 *
		 * Since changing the system palette causes the colours
		 * to change right away, and allocations might
		 * drastically change, we can't use this for animation,
		 * since that could cause weird coloring between the
		 * palette change and the blitting below, so we only set
		 * the real palette during initialisation.
		 */
	//	SDL_CALL SDL_SetColors(_sdl_realscreen, pal, _local_palette.first_dirty, _local_palette.count_dirty);
	//}

	//if (_sdl_screen != _sdl_realscreen && !init) {
		/* We're not using real hardware palette, but are letting SDL
		 * approximate the palette during shadow -> screen copy. To
		 * change the palette, we need to recopy the entire screen.
		 *
		 * Note that this operation can slow down the rendering
		 * considerably, especially since changing the shadow
		 * palette will need the next blit to re-detect the
		 * best mapping of shadow palette colors to real palette
		 * colors from scratch.
		 */
	//	SDL_CALL SDL_BlitSurface(_sdl_screen, NULL, _sdl_realscreen, NULL);
	//	SDL_CALL SDL_UpdateRect(_sdl_realscreen, 0, 0, 0, 0);
	//}

	//SDL_BlitSurface(_sdl_screen, NULL, SDL_GetWindowSurface(_sdl_window), NULL);
}

static void DrawSurfaceToScreen();

static void InitPalette()
{
	_local_palette = _cur_palette;
	_local_palette.first_dirty = 0;
	_local_palette.count_dirty = 256;
	UpdatePalette(true);
	DrawSurfaceToScreen();
}

static void CheckPaletteAnim()
{
	if (_cur_palette.count_dirty != 0) {
		Blitter *blitter = BlitterFactory::GetCurrentBlitter();

		switch (blitter->UsePaletteAnimation()) {
			case Blitter::PALETTE_ANIMATION_VIDEO_BACKEND:
				UpdatePalette();
				break;

			case Blitter::PALETTE_ANIMATION_BLITTER:
				blitter->PaletteAnimate(_local_palette);
				break;

			case Blitter::PALETTE_ANIMATION_NONE:
				break;

			default:
				NOT_REACHED();
		}
		_cur_palette.count_dirty = 0;
	}
}

// Scale whatever we have to the vita screen resolution
SDL_Rect _sdldest {0, 0, 960, 544};

static void DrawSurfaceToScreen()
{
	int n = _num_dirty_rects;
	if (n == 0) return;

	_num_dirty_rects = 0;
	// SDL_BlitSurface(_sdl_realsurface, NULL, _sdl_surface, NULL);
	// SDL_UpdateWindowSurface(_sdl_window);

	// //SDL_UpdateTexture(_sdl_screentext, NULL, _screenbuf, _screen.pitch);
	SDL_Surface *temp = SDL_ConvertSurface(_sdl_realsurface, _dst_format, 0);
//	SDL_UpdateTexture(_sdl_screentext, NULL, _sdl_realsurface->pixels, _screen.pitch);
	SDL_UpdateTexture(_sdl_screentext, NULL, temp->pixels, temp->pitch);
	// We need to scale whatever resolution we've got to the vita resolution
	SDL_RenderCopy(_sdl_renderer, _sdl_screentext, NULL, &_sdldest);
	SDL_RenderPresent(_sdl_renderer);

	SDL_FreeSurface(temp);

	//SDL_Texture *temptex = SDL_CreateTextureFromSurface(_sdl_renderer, _sdl_surface);
	//SDL_RenderCopy(_sdl_renderer, _sdl_screentext, NULL, NULL);
	//SDL_RenderCopy(_sdl_renderer, temptex, NULL, &_sdldest);
	//SDL_RenderPresent(_sdl_renderer);

	//SDL_SetPaletteColors

	//SDL_DestroyTexture(temptex);

	//SDL_DestroyTexture(temptex);

	//if (n > MAX_DIRTY_RECTS) {
		//if (_sdl_screen != _sdl_realscreen) {
	//SDL_BlitSurface(_sdl_screen, NULL, SDL_GetWindowSurface(_sdl_window), NULL);

		//}
	//SDL_UpdateRect(_sdl_window, 0, 0, 0, 0);
	//SDL_RenderPresent(_sdl_renderer);
	//} else {
	//	if (_sdl_screen != _sdl_realscreen) {
	//		for (int i = 0; i < n; i++) {
	//			SDL_BlitSurface(_sdl_screen, &_dirty_rects[i], _sdl_realscreen, &_dirty_rects[i]);
	//		}
	//	}
	//	SDL_UpdateRects(_sdl_realscreen, n, _dirty_rects);
	//}
}

static void DrawSurfaceToScreenThread(void *)
{
	debugNetPrintf(1, "DrawSurfaceToScreenThread.\n");
	/* First tell the main thread we're started */
	_draw_mutex->BeginCritical();
	_draw_mutex->SendSignal();

	/* Now wait for the first thing to draw! */
	_draw_mutex->WaitForSignal();

	while (_draw_continue) {
		CheckPaletteAnim();
		/* Then just draw and wait till we stop */
		DrawSurfaceToScreen();
		_draw_mutex->WaitForSignal();
	}

	_draw_mutex->EndCritical();
	_draw_thread->Exit();
}

// Vita only has two resolutions that really work. Can look at scaling this later maybe
static const Dimension _default_resolutions[] = {
	{ 480,  272},
	{ 960,	544}
};

static void GetVideoModes()
{
	// int sdl_display_ct = SDL_GetNumVideoDisplays();
	// debugNetPrintf(DBGN_INFO, "SDL2: Got %d displays\n", sdl_display_ct);

	// int sdl_mode_ct = SDL_GetNumDisplayModes(0);
	// debugNetPrintf(DBGN_INFO, "SDL2: Got %d modes\n", sdl_mode_ct);

	// // Error here, what do we dooo??
	// if (sdl_mode_ct < 0)
	// 	usererror("sdl2: no displays available");

	// if (sdl_mode_ct == 0)
	// 	usererror("sdl2: no modes available");

	int n = 0;
	memcpy(_resolutions, _default_resolutions, sizeof(_default_resolutions));
	_num_resolutions = 1;
}

static void GetAvailableVideoMode(uint *w, uint *h)
{
	/* All modes available? */
	if (_all_modes || _num_resolutions == 0) return;

	/* Is the wanted mode among the available modes? */
	for (int i = 0; i != _num_resolutions; i++) {
		if (*w == _resolutions[i].width && *h == _resolutions[i].height) return;
	}

	/* Use the closest possible resolution */
	int best = 0;
	uint delta = Delta(_resolutions[0].width, *w) * Delta(_resolutions[0].height, *h);
	for (int i = 1; i != _num_resolutions; ++i) {
		uint newdelta = Delta(_resolutions[i].width, *w) * Delta(_resolutions[i].height, *h);
		if (newdelta < delta) {
			best = i;
			delta = newdelta;
		}
	}
	*w = _resolutions[best].width;
	*h = _resolutions[best].height;
}

bool VideoDriver_SDL::CreateMainSurface(uint w, uint h)
{

	if (_sdl_window != NULL)
		return true;
	// On the vita, w and h are always 960x544
	char caption[32];
	seprintf(caption, lastof(caption), "OpenTTD %s", _openttd_revision);

	//SDL_CreateWindowAndRenderer(w, h, SDL_WINDOW_FULLSCREEN_DESKTOP, &_sdl_window, &_sdl_renderer);
	_sdl_window = SDL_CreateWindow(caption, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_SHOWN);
	_sdl_renderer = SDL_CreateRenderer(_sdl_window, -1, 0);	
	_sdl_screentext = SDL_CreateTexture(_sdl_renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, w, h);
	//_sdl_surface =  SDL_GetWindowSurface(_sdl_window);
	_sdl_realsurface = SDL_CreateRGBSurface(0, w, h, 8, 0, 0, 0, 0);

	_touch_scale_x = MAX_TOUCH_X / w;
	_touch_scale_y = MAX_TOUCH_Y / h;

	debugNetPrintf(1, "w = %d, MAX_TOUCH_X = %d, _touch_scale_x = %f\n", w, MAX_TOUCH_X, _touch_scale_x);
	debugNetPrintf(1, "h = %d, MAX_TOUCH_Y = %d, _touch_scale_y = %f\n", h, MAX_TOUCH_Y, _touch_scale_y);

	debugNetPrintf(1, "CreateMainSurface [%d,%d]\n", w, h);
	debugNetPrintf(1, "GetScreenDepth() [%d]\n", BlitterFactory::GetCurrentBlitter()->GetScreenDepth());

	/*SDL_Surface *newscreen = SDL_CreateRGBSurface(0, w, h, 32,
												0x00FF0000,
												0x0000FF00,
												0x000000FF,
												0xFF000000);*/

	//if (_screenbuf != NULL)
	//{
	//	delete[] _screenbuf;
	//}
	//_screenbuf = new uint32[w * h];

	// Assign _screen values so the rest of openttd can draw to this surface
	_screen.width = w;//newscreen->w;
	_screen.height = h;//newscreen->h;
	// assume bpp is 32
	_screen.pitch = _cur_resolution.width;//newscreen->pitch / (32 / 8);
	// 960 results in 4
	// 960 / 2 results in 8
	// so 960 * 2 == the right?
	//_screen.pitch = _sdl_surface->pitch / (32 / 8);
	_screen.dst_ptr = _sdl_realsurface->pixels;
	//_sdl_screen = newscreen;

	//Uint32 format = _sdl_renderer->info.texture_formats[0];
	Uint32 format = SDL_PIXELFORMAT_RGB888;
	_dst_format = SDL_AllocFormat(format);
	

	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	if (SDL_NumJoysticks() > 0) {
		_sdl_joystick = SDL_JoystickOpen(0);
		if (_sdl_joystick) {
	        debugNetPrintf(1, "Opened Joystick 0\n");
	        debugNetPrintf(1, "Name: %s\n", SDL_JoystickNameForIndex(0));
	        debugNetPrintf(1, "Number of Axes: %d\n", SDL_JoystickNumAxes(_sdl_joystick));
	        debugNetPrintf(1, "Number of Buttons: %d\n", SDL_JoystickNumButtons(_sdl_joystick));
	        debugNetPrintf(1, "Number of Balls: %d\n", SDL_JoystickNumBalls(_sdl_joystick));
	    } else {
	        debugNetPrintf(1, "Couldn't open Joystick 0\n");
	    }
	}

	/* When in full screen, we will always have the mouse cursor
	 * within the window, even though SDL does not give us the
	 * appropriate event to know this. */
	if (_fullscreen) 
		_cursor.in_window = true;

	Blitter *blitter = BlitterFactory::GetCurrentBlitter();
	blitter->PostResize();

	InitPalette();

	GameSizeChanged();

	return true;
}

bool VideoDriver_SDL::ClaimMousePointer()
{
	SDL_CALL SDL_ShowCursor(0);
	return true;
}

struct VkMapping {
#if SDL_VERSION_ATLEAST(1, 3, 0)
	SDL_Keycode vk_from;
#else
	uint16 vk_from;
#endif
	byte vk_count;
	byte map_to;
};

#define AS(x, z) {x, 0, z}
#define AM(x, y, z, w) {x, (byte)(y - x), z}

static const VkMapping _vk_mapping[] = {
	/* Pageup stuff + up/down */
	AM(SDLK_PAGEUP, SDLK_PAGEDOWN, WKC_PAGEUP, WKC_PAGEDOWN),
	AS(SDLK_UP,     WKC_UP),
	AS(SDLK_DOWN,   WKC_DOWN),
	AS(SDLK_LEFT,   WKC_LEFT),
	AS(SDLK_RIGHT,  WKC_RIGHT),

	AS(SDLK_HOME,   WKC_HOME),
	AS(SDLK_END,    WKC_END),

	AS(SDLK_INSERT, WKC_INSERT),
	AS(SDLK_DELETE, WKC_DELETE),

	/* Map letters & digits */
	AM(SDLK_a, SDLK_z, 'A', 'Z'),
	AM(SDLK_0, SDLK_9, '0', '9'),

	AS(SDLK_ESCAPE,    WKC_ESC),
	AS(SDLK_PAUSE,     WKC_PAUSE),
	AS(SDLK_BACKSPACE, WKC_BACKSPACE),

	AS(SDLK_SPACE,     WKC_SPACE),
	AS(SDLK_RETURN,    WKC_RETURN),
	AS(SDLK_TAB,       WKC_TAB),

	/* Function keys */
	AM(SDLK_F1, SDLK_F12, WKC_F1, WKC_F12),

	/* Numeric part. */
	AM(SDLK_KP_0, SDLK_KP_9, '0', '9'),
	AS(SDLK_KP_DIVIDE,   WKC_NUM_DIV),
	AS(SDLK_KP_MULTIPLY, WKC_NUM_MUL),
	AS(SDLK_KP_MINUS,    WKC_NUM_MINUS),
	AS(SDLK_KP_PLUS,     WKC_NUM_PLUS),
	AS(SDLK_KP_ENTER,    WKC_NUM_ENTER),
	AS(SDLK_KP_PERIOD,   WKC_NUM_DECIMAL),

	/* Other non-letter keys */
	AS(SDLK_SLASH,        WKC_SLASH),
	AS(SDLK_SEMICOLON,    WKC_SEMICOLON),
	AS(SDLK_EQUALS,       WKC_EQUALS),
	AS(SDLK_LEFTBRACKET,  WKC_L_BRACKET),
	AS(SDLK_BACKSLASH,    WKC_BACKSLASH),
	AS(SDLK_RIGHTBRACKET, WKC_R_BRACKET),

	AS(SDLK_QUOTE,   WKC_SINGLEQUOTE),
	AS(SDLK_COMMA,   WKC_COMMA),
	AS(SDLK_MINUS,   WKC_MINUS),
	AS(SDLK_PERIOD,  WKC_PERIOD)
};

static uint ConvertSdlKeyIntoMy(SDL_Keysym *sym, WChar *character)
{
	const VkMapping *map;
	uint key = 0;

	for (map = _vk_mapping; map != endof(_vk_mapping); ++map) {
		if ((uint)(sym->sym - map->vk_from) <= map->vk_count) {
			key = sym->sym - map->vk_from + map->map_to;
			break;
		}
	}

	/* check scancode for BACKQUOTE key, because we want the key left of "1", not anything else (on non-US keyboards) */
#if defined(WIN32) || defined(__OS2__)
	if (sym->scancode == 41) key = WKC_BACKQUOTE;
#elif defined(__APPLE__)
	if (sym->scancode == 10) key = WKC_BACKQUOTE;
#elif defined(__MORPHOS__)
	if (sym->scancode == 0)  key = WKC_BACKQUOTE;  // yes, that key is code '0' under MorphOS :)
#elif defined(__BEOS__)
	if (sym->scancode == 17) key = WKC_BACKQUOTE;
#elif defined(__SVR4) && defined(__sun)
	if (sym->scancode == 60) key = WKC_BACKQUOTE;
	if (sym->scancode == 49) key = WKC_BACKSPACE;
#elif defined(__sgi__)
	if (sym->scancode == 22) key = WKC_BACKQUOTE;
#else
	if (sym->scancode == 49) key = WKC_BACKQUOTE;
#endif

	if (sym->mod & KMOD_SHIFT) key |= WKC_SHIFT;
	if (sym->mod & KMOD_CTRL)  key |= WKC_CTRL;
	if (sym->mod & KMOD_ALT)   key |= WKC_ALT;

	//*character = sym->unicode;
	// SDL2 removed unicode so we'll just hack this in for ascii support for now
	*character = (char)sym->scancode;
	return key;
}


int VideoDriver_SDL::PollEvent()
{
	SDL_Event ev;

	if (!SDL_CALL SDL_PollEvent(&ev)) 
	{
		//debugNetPrintf(1, "SDL_PollEvent -2\n");
		return -2;
	}

	debugNetPrintf(1, "SDL_PollEvent: %d\n", ev.type);
	bool cursor_updated = false;
	switch (ev.type) {
		case SDL_FINGERDOWN:
			// possible to touch by just hovering without this
			//if (ev.tfinger.pressure > 40)
			if (_cursor.UpdateCursorPosition(ev.tfinger.x / _touch_scale_x, ev.tfinger.y / _touch_scale_y, true))
			{
				SDL_WarpMouseInWindow(_sdl_window, _cursor.pos.x, _cursor.pos.y);
			}
			_left_button_down = true;
			//_left_button_clicked = true;
			HandleMouseEvents();
			break;
		case SDL_FINGERUP:
			if (_cursor.UpdateCursorPosition(ev.tfinger.x / _touch_scale_x, ev.tfinger.y / _touch_scale_y, true))
			{
				SDL_WarpMouseInWindow(_sdl_window, _cursor.pos.x, _cursor.pos.y);
			}
			_left_button_down = false;
			_left_button_clicked = false;
			HandleMouseEvents();
			break;
		case SDL_FINGERMOTION:
			if (_cursor.UpdateCursorPosition(ev.tfinger.x / _touch_scale_x, ev.tfinger.y / _touch_scale_y, true))
			{
				SDL_WarpMouseInWindow(_sdl_window, _cursor.pos.x, _cursor.pos.y);
			}
			HandleMouseEvents();
			break;
		case SDL_JOYAXISMOTION:
			// X axis
			if (ev.jaxis.axis == 0 || ev.jaxis.axis == 2)
				cursor_updated = _cursor.UpdateCursorPosition(_cursor.pos.x + ev.jaxis.value / 100, _cursor.pos.y, true);
			// Y axis
			else if (ev.jaxis.axis == 1 || ev.jaxis.axis == 3)
				cursor_updated = _cursor.UpdateCursorPosition(_cursor.pos.x, _cursor.pos.y + ev.jaxis.value / 100, true);
			
			if (cursor_updated)
			{
				SDL_WarpMouseInWindow(_sdl_window, _cursor.pos.x, _cursor.pos.y);
			}
			//debugNetPrintf(1, "Axis: %d, Value: %ld\n", ev.jaxis.axis, ev.jaxis.value);
			HandleMouseEvents();	
			
			break;
		case SDL_MOUSEMOTION:
			if (_cursor.UpdateCursorPosition(ev.motion.x, ev.motion.y, true)) {
				SDL_CALL SDL_WarpMouseInWindow(_sdl_window, _cursor.pos.x, _cursor.pos.y);
			}
			HandleMouseEvents();
			break;
		case SDL_MOUSEWHEEL:
			//SDL_MouseWheelEvent *mwev = &ev;
			_cursor.wheel += ev.wheel.y;
			break;
		case SDL_MOUSEBUTTONDOWN:
			if (_rightclick_emulate && SDL_CALL SDL_GetModState() & KMOD_CTRL) {
				ev.button.button = SDL_BUTTON_RIGHT;
			}

			switch (ev.button.button) {
				case SDL_BUTTON_LEFT:
					_left_button_down = true;
					break;

				case SDL_BUTTON_RIGHT:
					_right_button_down = true;
					_right_button_clicked = true;
					break;
				default: break;
			}
			HandleMouseEvents();
			break;

		case SDL_MOUSEBUTTONUP:
			if (_rightclick_emulate) {
				_right_button_down = false;
				_left_button_down = false;
				_left_button_clicked = false;
			} else if (ev.button.button == SDL_BUTTON_LEFT) {
				_left_button_down = false;
				_left_button_clicked = false;
			} else if (ev.button.button == SDL_BUTTON_RIGHT) {
				_right_button_down = false;
			}
			HandleMouseEvents();
			break;

		case SDL_WINDOWEVENT:
			//if (!(ev.active.state & SDL_APPMOUSEFOCUS)) break;

			if (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
			//if (ev.active.gain) { // mouse entered the window, enable cursor
				_cursor.in_window = true;
			} else {
				UndrawMouseCursor(); // mouse left the window, undraw cursor
				_cursor.in_window = false;
			}
			break;

		// case SDL_QUIT:
		// 	HandleExitGameRequest();
		// 	break;
		case SDL_JOYBUTTONDOWN:
			if (ev.jbutton.button == 1)
			{
				_right_button_down = true;
				_right_button_clicked = true;
				HandleMouseEvents();
			}
			else if (ev.jbutton.button == 2)
			{
				_left_button_down = true;
				HandleMouseEvents();
			}
			else if (ev.jbutton.button == 6)
			{
				WChar val = 1073741906;
				HandleKeypress(SDLK_UP, val);
			}
			else if (ev.jbutton.button == 7)
			{
				HandleKeypress(WKC_LEFT, NULL);
			}
			else if (ev.jbutton.button == 8)
			{
				HandleKeypress(WKC_UP, NULL);
			}
			else if (ev.jbutton.button == 9)
			{
				HandleKeypress(WKC_RIGHT, NULL);
			}
			debugNetPrintf(1, "button: %d\n", ev.jbutton.button);
			// down = 6, left = 7, up = 8, right = 9
			
			break;
		case SDL_JOYBUTTONUP:
			if(ev.jbutton.button == 1)
			{
				_right_button_down = false;
				_right_button_clicked = false;
			}
			else if (ev.jbutton.button == 2)
			{
				_left_button_down = false;
				_left_button_clicked = false;
			}
			HandleMouseEvents();
			break;
		case SDL_KEYDOWN: // Toggle full-screen on ALT + ENTER/F
			if ((ev.key.keysym.mod & KMOD_ALT) &&
					(ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_f)) {
				ToggleFullScreen(!_fullscreen);
			} else {
				WChar character;
				uint keycode = ConvertSdlKeyIntoMy(&ev.key.keysym, &character);
				HandleKeypress(keycode, character);
			}

			debugNetPrintf(1, "ev.key.keysym.sym: %d\n", ev.key.keysym.sym);

			break;
		// Can't resize on the vita
		// case SDL_VIDEORESIZE: {
		// 	int w = max(ev.resize.w, 64);
		// 	int h = max(ev.resize.h, 64);
		// 	CreateMainSurface(w, h);
		// 	break;
		// }
		// case SDL_VIDEOEXPOSE: 
			 // Force a redraw of the entire screen. Note
			 // * that SDL 1.2 seems to do this automatically
			 // * in most cases, but 1.3 / 2.0 does not. 
		//         _num_dirty_rects = MAX_DIRTY_RECTS + 1;
		// 	break;
		
	}
	return -1;
}

const char *VideoDriver_SDL::Start(const char * const *parm)
{
	// Temporary
	//_cur_resolution.width = 960;
	//_cur_resolution.height = 544;

	debugNetPrintf(DBGN_INFO, "VideoDriver_SDL::Start enter\n");
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		return "SDL_INIT_VIDEO failed";

	GetVideoModes();

	if (!CreateMainSurface(_cur_resolution.width, _cur_resolution.height)) {
		debugNetPrintf(DBGN_INFO, "CreateMainSurface: %d, %d\n", _cur_resolution.width, _cur_resolution.height);
		return "Failed to make surface";
	}

	int renderCt = SDL_GetNumRenderDrivers();

	if (renderCt < 0) {
		return "renderCt is < 0";
	}

	SDL_RendererInfo info;
	if (SDL_GetRenderDriverInfo(0, &info))
	{
		DEBUG(driver, 1, "SDL: using driver '%s'", info.name);
	}

	MarkWholeScreenDirty();
	SetupKeyboard();

	_draw_threaded = GetDriverParam(parm, "no_threads") == NULL && GetDriverParam(parm, "no_thread") == NULL;

	debugNetPrintf(DBGN_INFO, "VideoDriver_SDL::Start exit\n");
	return NULL;
}

void VideoDriver_SDL::SetupKeyboard()
{

}

void VideoDriver_SDL::Stop()
{
	SDL_DestroyTexture(_sdl_screentext);
	SDL_DestroyRenderer(_sdl_renderer);
	SDL_DestroyWindow(_sdl_window);
}

void VideoDriver_SDL::MainLoop()
{
	debugNetPrintf(1, "MainLoop.\n");
	uint32 cur_ticks = SDL_CALL SDL_GetTicks();
	uint32 last_cur_ticks = cur_ticks;
	uint32 next_tick = cur_ticks + MILLISECONDS_PER_TICK;
	uint32 mod;
	int numkeys;
	

	CheckPaletteAnim();

	if (_draw_threaded) {
		/* Initialise the mutex first, because that's the thing we *need*
		 * directly in the newly created thread. */
		_draw_mutex = ThreadMutex::New();
		if (_draw_mutex == NULL) {
			_draw_threaded = false;
		} else {
			_draw_mutex->BeginCritical();
			_draw_continue = true;

			_draw_threaded = ThreadObject::New(&DrawSurfaceToScreenThread, NULL, &_draw_thread, "ottd:draw-sdl");

			/* Free the mutex if we won't be able to use it. */
			if (!_draw_threaded) {
				_draw_mutex->EndCritical();
				delete _draw_mutex;
				_draw_mutex = NULL;
			} else {
				/* Wait till the draw mutex has started itself. */
				_draw_mutex->WaitForSignal();
			}
		}
	}

	DEBUG(driver, 1, "SDL: using %sthreads", _draw_threaded ? "" : "no ");

	for (;;) {
		uint32 prev_cur_ticks = cur_ticks; // to check for wrapping
		InteractiveRandom(); // randomness

		while (PollEvent() == -1) {}
		if (_exit_game) break;

		mod = SDL_CALL SDL_GetModState();
#if SDL_VERSION_ATLEAST(1, 3, 0)
		const Uint8 *keys = SDL_CALL SDL_GetKeyboardState(&numkeys);
#else
		keys = SDL_CALL SDL_GetKeyState(&numkeys);
#endif
#if defined(_DEBUG)
		if (_shift_pressed)
#else
		/* Speedup when pressing tab, except when using ALT+TAB
		 * to switch to another application */
#if SDL_VERSION_ATLEAST(1, 3, 0)
		if (keys[SDL_SCANCODE_TAB] && (mod & KMOD_ALT) == 0)
#else
		if (keys[SDLK_TAB] && (mod & KMOD_ALT) == 0)
#endif /* SDL_VERSION_ATLEAST(1, 3, 0) */
#endif /* defined(_DEBUG) */
		{
			if (!_networking && _game_mode != GM_MENU) _fast_forward |= 2;
		} else if (_fast_forward & 2) {
			_fast_forward = 0;
		}

		cur_ticks = SDL_CALL SDL_GetTicks();
		if (cur_ticks >= next_tick || (_fast_forward && !_pause_mode) || cur_ticks < prev_cur_ticks) {
			_realtime_tick += cur_ticks - last_cur_ticks;
			last_cur_ticks = cur_ticks;
			next_tick = cur_ticks + MILLISECONDS_PER_TICK;

			bool old_ctrl_pressed = _ctrl_pressed;

			_ctrl_pressed  = !!(mod & KMOD_CTRL);
			_shift_pressed = !!(mod & KMOD_SHIFT);

			/* determine which directional keys are down */
			_dirkeys =
#if SDL_VERSION_ATLEAST(1, 3, 0)
				(keys[SDL_SCANCODE_LEFT]  ? 1 : 0) |
				(keys[SDL_SCANCODE_UP]    ? 2 : 0) |
				(keys[SDL_SCANCODE_RIGHT] ? 4 : 0) |
				(keys[SDL_SCANCODE_DOWN]  ? 8 : 0);
#else
				(keys[SDLK_LEFT]  ? 1 : 0) |
				(keys[SDLK_UP]    ? 2 : 0) |
				(keys[SDLK_RIGHT] ? 4 : 0) |
				(keys[SDLK_DOWN]  ? 8 : 0);
#endif
			if (old_ctrl_pressed != _ctrl_pressed) HandleCtrlChanged();

			/* The gameloop is the part that can run asynchronously. The rest
			 * except sleeping can't. */
			if (_draw_mutex != NULL) _draw_mutex->EndCritical();

			GameLoop();

			if (_draw_mutex != NULL) _draw_mutex->BeginCritical();

			UpdateWindows();
			_local_palette = _cur_palette;
		} else {
			/* Release the thread while sleeping */
			if (_draw_mutex != NULL) _draw_mutex->EndCritical();
			CSleep(1);
			if (_draw_mutex != NULL) _draw_mutex->BeginCritical();

			NetworkDrawChatMessage();
			DrawMouseCursor();
		}

		/* End of the critical part. */
		if (_draw_mutex != NULL && !HasModalProgress()) {
			_draw_mutex->SendSignal();
		} else {
			/* Oh, we didn't have threads, then just draw unthreaded */
			CheckPaletteAnim();
			DrawSurfaceToScreen();
		}
	}

	if (_draw_mutex != NULL) {
		_draw_continue = false;
		/* Sending signal if there is no thread blocked
		 * is very valid and results in noop */
		_draw_mutex->SendSignal();
		_draw_mutex->EndCritical();
		_draw_thread->Join();

		delete _draw_mutex;
		delete _draw_thread;

		_draw_mutex = NULL;
		_draw_thread = NULL;
	}
}

bool VideoDriver_SDL::ChangeResolution(int w, int h)
{
	if (_draw_mutex != NULL) _draw_mutex->BeginCritical(true);
	bool ret = CreateMainSurface(w, h);
	if (_draw_mutex != NULL) _draw_mutex->EndCritical(true);
	return ret;
}

bool VideoDriver_SDL::ToggleFullscreen(bool fullscreen)
{
	if (_draw_mutex != NULL) _draw_mutex->BeginCritical(true);
	_fullscreen = fullscreen;
	GetVideoModes(); // get the list of available video modes
	bool ret = _num_resolutions != 0 && CreateMainSurface(_cur_resolution.width, _cur_resolution.height);

	if (!ret) {
		/* switching resolution failed, put back full_screen to original status */
		_fullscreen ^= true;
	}

	if (_draw_mutex != NULL) _draw_mutex->EndCritical(true);
	return ret;
}

bool VideoDriver_SDL::AfterBlitterChange()
{
	return CreateMainSurface(_screen.width, _screen.height);
}

void VideoDriver_SDL::AcquireBlitterLock()
{
	if (_draw_mutex != NULL) _draw_mutex->BeginCritical(true);
}

void VideoDriver_SDL::ReleaseBlitterLock()
{
	if (_draw_mutex != NULL) _draw_mutex->EndCritical(true);
}

#endif /* WITH_SDL */

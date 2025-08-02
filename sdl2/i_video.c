#include <SDL.h>

#include "../command.h"
#include "../doomdef.h"
#include "../i_system.h"
#include "../i_video.h"
#include "../screen.h"
#include "../z_zone.h"

#include "i_video.h"

rendermode_t rendermode = render_soft;

boolean highcolor = false;

boolean allow_fullscreen = false;

// Stores the current mode
int currentmode;

// Various unction protos
void I_ToggleFullscreen(void);
boolean VID_InitConsole(void);
void VID_Command_Vidmode(void);
void VID_Command_Listmodes(void);

consvar_t cv_fullscreen = {"fullscreen", "Off", CV_SAVE | CV_CALL, CV_OnOff, I_ToggleFullscreen};

SDL_Window* SDL_window = NULL;

SDL_Renderer* SDL_renderer;
SDL_Surface* surface;
SDL_Surface* window_surface;
SDL_Surface* icon_surface;

// todo: un-hardcode this
#define NUM_SDLMODES 11

vmode_t window_modes[NUM_SDLMODES] = {
		// Fallback mode, 320x200 is gross
		{
			NULL,
			"320x200W", //faB: W to make sure it's the windowed mode
			320, 200,   //(200.0/320.0)*(320.0/240.0),
			320, 1,     // rowbytes, bytes per pixel
			1, 2,       // windowed (TRUE), numpages
			NULL,
			NULL,
			0          // misc
		},
		// Non-fallback copy of 320x200W, if you WANT to use 320x200W for some reason
		{
			NULL,
			"320x200W", //faB: W to make sure it's the windowed mode
			320, 200,   //(200.0/320.0)*(320.0/240.0),
			320, 1,     // rowbytes, bytes per pixel
			1, 2,       // windowed (TRUE), numpages
			NULL,
			NULL,
			0          // misc
		},
		{
			NULL,
			"320x240W", //faB: W to make sure it's the windowed mode
			320, 240,   //(200.0/320.0)*(320.0/240.0),
			320, 1,     // rowbytes, bytes per pixel
			1, 2,       // windowed (TRUE), numpages
			NULL,
			NULL,
			0          // misc
		},
		{
			NULL,
			"640x400W", //faB: W to make sure it's the windowed mode
			640, 400,   //(200.0/320.0)*(320.0/240.0),
			640, 1,     // rowbytes, bytes per pixel
			1, 2,       // windowed (TRUE), numpages
			NULL,
			NULL,
			0          // misc
		},
		{
			NULL,
			"640x480W", //faB: W to make sure it's the windowed mode
			640, 480,   //(200.0/320.0)*(320.0/240.0),
			640, 1,     // rowbytes, bytes per pixel
			1, 2,       // windowed (TRUE), numpages
			NULL,
			NULL,
			0          // misc
		},
		{
			NULL,
			"800x600W", //faB: W to make sure it's the windowed mode
			800, 600,   //(200.0/320.0)*(320.0/240.0),
			800, 1,     // rowbytes, bytes per pixel
			1, 2,       // windowed (TRUE), numpages
			NULL,
			NULL,
			0          // misc
		},
		{
			NULL,
			"1024x768W", //faB: W to make sure it's the windowed mode
			1024, 768,   //(200.0/320.0)*(320.0/240.0),
			1024, 1,     // rowbytes, bytes per pixel
			1, 2,       // windowed (TRUE), numpages
			NULL,
			NULL,
			0          // misc
		},
		{
			NULL,
			"1280x720W", //faB: W to make sure it's the windowed mode
			1280, 720,   //(200.0/320.0)*(320.0/240.0),
			1280, 1,     // rowbytes, bytes per pixel
			1, 2,       // windowed (TRUE), numpages
			NULL,
			NULL,
			0          // misc
		},
		{
			NULL,
			"1280x800W", //faB: W to make sure it's the windowed mode
			1280, 800,   //(200.0/320.0)*(320.0/240.0),
			1280, 1,     // rowbytes, bytes per pixel
			1, 2,       // windowed (TRUE), numpages
			NULL,
			NULL,
			0          // misc
		},
		{
			NULL,
			"1920x1080W", //faB: W to make sure it's the windowed mode
			1920, 1080,   //(200.0/320.0)*(320.0/240.0),
			1920, 1,     // rowbytes, bytes per pixel
			1, 2,       // windowed (TRUE), numpages
			NULL,
			NULL,
			0          // misc
		},
		{
			NULL,
			"1920x1200W", //faB: W to make sure it's the windowed mode
			1920, 1200,   //(200.0/320.0)*(320.0/240.0),
			1920, 1,     // rowbytes, bytes per pixel
			1, 2,       // windowed (TRUE), numpages
			NULL,
			NULL,
			0          // misc
		},
};

static void SetSDLIcon(SDL_Window* window)
{

	// Somebody please find a better way to do this later, I hate #including .c files
#include "Srb2win.c"

	Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	int shift = (my_icon.bytes_per_pixel == 3) ? 8 : 0;
	rmask = 0xff000000 >> shift;
	gmask = 0x00ff0000 >> shift;
	bmask = 0x0000ff00 >> shift;
	amask = 0x000000ff >> shift;
#else // little endian, like x86
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = (gimp_image.bytes_per_pixel == 3) ? 0 : 0xff000000;
#endif

	icon_surface = SDL_CreateRGBSurfaceFrom((void*)gimp_image.pixel_data,
		gimp_image.width, gimp_image.height, gimp_image.bytes_per_pixel * 8,
		gimp_image.bytes_per_pixel * gimp_image.width, rmask, gmask, bmask, amask);

	SDL_SetWindowIcon(SDL_window, icon_surface);

	SDL_FreeSurface(icon_surface);

}

void I_ToggleFullscreen(void) {
	VID_SetMode(currentmode);
}

void I_ShutdownGraphics(void){
	CONS_Printf("I_ShutdownGraphics...\n");
	SDL_DestroyRenderer(SDL_renderer);
	SDL_DestroyWindow(SDL_window);
	SDL_FreeSurface(surface);
	free(vid.buffer);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	graphics_started = false;
}

SDL_Color palettebuf[256];

// Translate Doom palette into SDL palette
void I_SetPalette(byte *palette)
{
	RGB_t* rgbpalette;
	rgbpalette = palette;

	// 256 colors * 3 color channels
	for (int i = 0; i < 256; i++) {

		palettebuf[i].r = rgbpalette[i].r;
		palettebuf[i].g = rgbpalette[i].g;
		palettebuf[i].b = rgbpalette[i].b;
		palettebuf[i].a = 255;

		SDL_SetPaletteColors(surface->format->palette, palettebuf, 0, 256);
	}
}

int VID_NumModes(void)
{
	// TODO: Find a way to get length of windowed_modes and return that
	return NUM_SDLMODES;
}

int VID_GetModeForSize(int w, int h)
{
	int diffx, diffy;
	int bestdiff, bestmode;

	bestdiff = 9999;

	// Iterate through (accessible) modes
	for (int i = 1; i < NUM_SDLMODES; i++) {

		// Get difference between current window mode's width and our requested width
		diffx = window_modes[i].width - w;
		if (diffx < 0) {
			diffx *= -1;
		}

		// Get difference between current window mode's height and our requested height
		diffy = window_modes[i].height - h;
		if (diffy < 0) {
			diffy *= -1;
		}

		// If the mode has the exact coords of our dimensions, use that!
		if (diffx == 0 && diffy == 0)
			return i;

		// If our current mode is the closest found so far to requested dimensions, record it as our best candidate so far
		if (bestdiff > diffx + diffy) {
			bestdiff = diffx + diffy;
			bestmode = i;
		}
	}

	// Well, we didn't find any exact matches. Darn.
	// Oh well, that's what that whole closest match system was for!
	return bestmode;
}

// TODO: Some of the stuff done when we change modes might not be needed. See how much we can keep between mode switches (For example, we might just be able to change the window's dimensions instead of destroying and remaking it)
int VID_SetMode(int modenum)
{
	int flags;

	if (SDL_window != NULL)
		SDL_DestroyWindow(SDL_window);

	if (SDL_renderer != NULL)
		SDL_DestroyRenderer(SDL_renderer);

	if (surface != NULL)
		SDL_FreeSurface(surface);

	vid.modenum = modenum;
	vid.width = window_modes[modenum].width;
	vid.height = window_modes[modenum].height;
	vid.bpp = window_modes[modenum].bytesperpixel;
	vid.rowbytes = window_modes[modenum].rowbytes;
	vid.dupx = vid.width / 320;
	vid.dupy = vid.height / 200;
	vid.recalc = 1;

	// Handle 
	if (cv_fullscreen.value)
		flags = SDL_WINDOW_FULLSCREEN;
	else
		flags = NULL;

	SDL_window = SDL_CreateWindow("Doom Legacy 1.28", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, vid.width, vid.height, flags);
	SetSDLIcon(SDL_window);

	if (!SDL_window)
		I_Error("VID_SetMode(): Could not create window!");

	SDL_renderer = SDL_CreateRenderer(SDL_window, -1, SDL_RENDERER_ACCELERATED);
	if (!SDL_renderer)
		I_Error("VID_SetMode(): Could not create renderer!");

	surface = SDL_CreateRGBSurfaceWithFormat(0, vid.width, vid.height, 8, SDL_PIXELFORMAT_INDEX8);

	window_surface = SDL_GetWindowSurface(SDL_window);

	// allocate buffer
	if (vid.buffer)
		free(vid.buffer);

	vid.buffer = malloc((vid.width * vid.height * vid.bpp * NUMSCREENS) + (vid.width * ST_HEIGHT * vid.bpp));

	// Set mode number accordingly
	currentmode = modenum;

	return 0;
}

void I_StartupGraphics(void) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		I_Error("I_StartupGraphics(): Could not initialize SDL2: %s\n", SDL_GetError());
	
	if (VID_InitConsole())
		I_Error("I_StartupGraphics(): Could not initialize commands / console variables!\n");

	VID_SetMode(3);

	graphics_started = true;
}

const char *VID_GetModeName(int modenum)
{
	return window_modes[modenum].name;
}

void I_UpdateNoBlit(void){}

void I_FinishUpdate(void)
{
	byte* pixels = surface->pixels;
	// Copy vid.buffer to our surface
	for (int i = 0; i < vid.width * vid.height; i++) {
		pixels[i] = vid.buffer[i];
	}

	SDL_BlitSurface(surface, NULL, window_surface, NULL);
	SDL_UpdateWindowSurface(SDL_window);
}

void I_WaitVBL(int count)
{
	count = 0;
}

void I_ReadScreen(byte *scr)
{
	SDL_memcpy(scr, vid.buffer, vid.width * vid.height * vid.bpp);
}

// For some reason, I get a message about this not existing when building on Linux
#if _WIN32
void VID_BlitLinearScreen(byte* srcptr, byte* destptr, int width, int height, int srcrowbytes, int destrowbytes){
	if (srcrowbytes == destrowbytes)
	{
		SDL_memcpy(destptr, srcptr, srcrowbytes * height);
	}
	else
	{
		while (height--)
		{
			memcpy(destptr, srcptr, width);
			destptr += destrowbytes;
			srcptr += srcrowbytes;
		}
	}
}
#endif

void I_BeginRead(void){}

void I_EndRead(void){}


// COMMAND STUFF

boolean VID_InitConsole(void) {
	// Register cvars
	CV_RegisterVar(&cv_fullscreen);

	// Register commands
	COM_AddCommand("videomode", VID_Command_Vidmode);
	COM_AddCommand("listmodes", VID_Command_Listmodes);

	return 0;
}

void VID_Command_Vidmode(void) {

	int modenum;

	if (COM_Argc() != 2) {
		CONS_Printf("videomode <mode number>: Changes the video mode to the specified one. Number must be between 1 and %d.\n", NUM_SDLMODES - 1);
		return 0;
	}

	modenum = atoi(COM_Argv(1));

	if (modenum < 1 || modenum > NUM_SDLMODES - 1) {
		CONS_Printf("Invalid video mode \"%d\"! Must be between 1 and %d.\n", modenum, NUM_SDLMODES - 1);
		return;
	} else {
		VID_SetMode(modenum);
	}

	CONS_Printf("Changed to video mode %s (index %d).\n", VID_GetModeName(modenum), modenum);
}

void VID_Command_Listmodes(void) {
	for (int i = 1; i < NUM_SDLMODES; i++) {
		CONS_Printf("Video mode %d: %s.\n", i, VID_GetModeName(i));
	}
}

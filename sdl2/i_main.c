#include "../doomdef.h"
#include "../d_main.h"
#include "../m_argv.h"
#include <SDL.h>
#include <SDL_rwops.h>

int mb_used = 32;

SDL_RWops* logstream;

int main(int argc, char** argv)
{

	myargc = argc;
	myargv = argv; /// \todo pull out path to exe from this string


	// init logstream
	logstream = SDL_RWFromFile("sdllog.txt", "a");
	if (!logstream)
		I_Error("Unable to get write access for log file. \n Are you running multiple instances of NewMillenium?");

	// startup SRB2
	CONS_Printf ("Setting up SRB2...\n");
	D_DoomMain();
	CONS_Printf ("Entering main game loop...\n");
	// never return
	D_DoomLoop();

	// return to OS
#ifndef __GNUC__
	return 0;
#endif
}

void I_FPrintf(char* lpFmt, ...)
{
	char    str[1999];
	va_list arglist;

	va_start(arglist, lpFmt);
	vsprintf(str, lpFmt, arglist);
	va_end(arglist);

	SDL_RWwrite(logstream, &str, sizeof(char), strlen(str));
}
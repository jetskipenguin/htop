#ifndef HEADER_HelpScreen
#define HEADER_HelpScreen
/*
htop - HelpScreen.h
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "Panel.h"
#include "Settings.h"


typedef struct HelpScreen_ {
   Panel* display;
} HelpScreen;

HelpScreen* HelpScreen_init(HelpScreen* self, const Settings* settings);

HelpScreen* HelpScreen_done(HelpScreen* self);

void HelpScreen_run(HelpScreen* self);

#endif

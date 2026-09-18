#ifndef HEADER_HelpScreen
#define HEADER_HelpScreen
/*
htop - HelpScreen.h
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "FunctionBar.h"
#include "IncSet.h"
#include "Object.h"
#include "Panel.h"
#include "Vector.h"


typedef struct HelpScreen_ {
   Object super;
   Panel* display;
   Vector* lines;
} HelpScreen;

typedef struct HelpScreenClass_ {
   const ObjectClass super;
} HelpScreenClass;

extern const HelpScreenClass HelpScreen_class;

HelpScreen* HelpScreen_init(HelpScreen* self);

HelpScreen* HelpScreen_done(HelpScreen* self);

void HelpScreen_run(HelpScreen* self);

#endif
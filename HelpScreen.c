/*
htop - HelpScreen.c
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "HelpScreen.h"

#include <stddef.h>
#include <stdlib.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "Macros.h"
#include "Object.h"
#include "Panel.h"
#include "Platform.h"
#include "ProvideCurses.h"
#include "RichString.h"
#include "XUtils.h"


typedef struct HelpLineSegment_ {
   int attr;
   const char* text;
} HelpLineSegment;

typedef struct HelpLine_ {
   Object super;
   struct {
      int attr;
      char* text;
   }* segments;
   size_t count;
} HelpLine;

static void HelpLine_display(const Object* cast, RichString* out) {
   const HelpLine* this = (const HelpLine*)cast;

   for (size_t i = 0; i < this->count; i++) {
      RichString_appendWide(out, this->segments[i].attr, this->segments[i].text);
   }
}

static void HelpLine_delete(Object* cast) {
   HelpLine* this = (HelpLine*)cast;

   for (size_t i = 0; i < this->count; i++) {
      free(this->segments[i].text);
   }
   free(this->segments);
   free(this);
}

static const ObjectClass HelpLine_class = {
   .extends = Class(Object),
   .display = HelpLine_display,
   .delete = HelpLine_delete,
};

static HelpLine* HelpLine_new(const HelpLineSegment* segments, size_t count) {
   HelpLine* this = xMalloc(sizeof(HelpLine));
   Object_setClass(this, Class(HelpLine));
   this->count = count;
   this->segments = count ? xReallocArray(NULL, count, sizeof(*this->segments)) : NULL;

   for (size_t i = 0; i < count; i++) {
      this->segments[i].attr = segments[i].attr;
      this->segments[i].text = xStrdup(segments[i].text);
   }

   return this;
}

static const struct {
   const char* key;
   bool roInactive;
   const char* info;
} helpLeft[] = {
   { .key = "      #: ",  .roInactive = false, .info = "hide/show header meters" },
   { .key = "    Tab: ",  .roInactive = false, .info = "switch to next screen tab" },
   { .key = " Arrows: ",  .roInactive = false, .info = "scroll process list" },
   { .key = " Digits: ",  .roInactive = false, .info = "incremental PID search" },
   { .key = "   F3 /: ",  .roInactive = false, .info = "incremental name search" },
   { .key = "   F4 \\: ", .roInactive = false, .info = "incremental name filtering" },
   { .key = "   F5 t: ",  .roInactive = false, .info = "tree view" },
   { .key = "      p: ",  .roInactive = false, .info = "toggle program path" },
   { .key = "      m: ",  .roInactive = false, .info = "toggle merged command" },
   { .key = "      Z: ",  .roInactive = false, .info = "pause/resume process updates" },
   { .key = "      u: ",  .roInactive = false, .info = "show processes of a single user" },
   { .key = "      H: ",  .roInactive = false, .info = "hide/show user process threads" },
   { .key = "      K: ",  .roInactive = false, .info = "hide/show kernel threads" },
   { .key = "      O: ",  .roInactive = false, .info = "hide/show processes in containers" },
   { .key = "      F: ",  .roInactive = false, .info = "cursor follows process" },
   { .key = "  + - *: ",  .roInactive = false, .info = "expand/collapse tree/toggle all" },
   { .key = "N P M T: ",  .roInactive = false, .info = "sort by PID, CPU%, MEM% or TIME" },
   { .key = "      I: ",  .roInactive = false, .info = "invert sort order" },
   { .key = " F6 > .: ",  .roInactive = false, .info = "select sort column" },
   { .key = NULL, .info = NULL }
};

static const struct {
   const char* key;
   bool roInactive;
   const char* info;
} helpRight[] = {
   { .key = "  S-Tab: ", .roInactive = false, .info = "switch to previous screen tab" },
   { .key = "  Space: ", .roInactive = false, .info = "tag process" },
   { .key = "      c: ", .roInactive = false, .info = "tag process and its children" },
   { .key = "      U: ", .roInactive = false, .info = "untag all processes" },
   { .key = "   F9 k: ", .roInactive = true,  .info = "kill process/tagged processes" },
   { .key = "   F7 ]: ", .roInactive = true,  .info = "higher priority (root only)" },
   { .key = "   F8 [: ", .roInactive = true,  .info = "lower priority (+ nice)" },
#if (defined(HAVE_LIBHWLOC) || defined(HAVE_AFFINITY))
   { .key = "      a: ", .roInactive = true, .info = "set CPU affinity" },
#endif
#if defined(HAVE_BACKTRACE_SCREEN)
   { .key = "      b: ", .roInactive = false, .info = "show process backtrace" },
#endif
   { .key = "      e: ", .roInactive = false, .info = "show process environment" },
   { .key = "      i: ", .roInactive = true,  .info = "set IO priority" },
   { .key = "      l: ", .roInactive = true,  .info = "list open files with lsof" },
   { .key = "      x: ", .roInactive = false, .info = "list file locks of process" },
   { .key = "      s: ", .roInactive = true,  .info = "trace syscalls with strace" },
   { .key = "      w: ", .roInactive = false, .info = "wrap process command in multiple lines" },
#ifdef SCHEDULER_SUPPORT
   { .key = "      Y: ", .roInactive = true,  .info = "set scheduling policy" },
#endif
   { .key = " F2 C S: ", .roInactive = false, .info = "setup" },
   { .key = " F1 h ?: ", .roInactive = false, .info = "show this help screen" },
   { .key = "  F10 q: ", .roInactive = false, .info = "quit" },
   { .key = NULL, .info = NULL }
};

HelpScreen* HelpScreen_init(HelpScreen* self) {
    // TODO: Add all functions to bar
   FunctionBar* bar = FunctionBar_newEnterEsc("Done   ", "Done   ");
   self->display = Panel_new(0, 0, COLS, MAXIMUM(LINES - 1, 1), Class(HelpLine), true, bar);
   self->lines = NULL;

   const HelpLineSegment first[] = {
      { .attr = CRT_colors[HELP_BOLD], .text = "Mock label: " },
      { .attr = CRT_colors[DEFAULT_COLOR], .text = "example text " },
      { .attr = CRT_colors[PROCESS_THREAD], .text = "with another color" },
   };
   const HelpLineSegment second[] = {
      { .attr = CRT_colors[HELP_BOLD], .text = "Another mock label: " },
      { .attr = CRT_colors[HELP_SHADOW], .text = "dimmed example text" },
   };


   // HelpLine_new copies these local segments; the owning panel frees each line.
   Panel_add(self->display, (Object*)HelpLine_new(first, ARRAYSIZE(first)));
   Panel_add(self->display, (Object*)HelpLine_new(NULL, 0));
   Panel_add(self->display, (Object*)HelpLine_new(second, ARRAYSIZE(second)));

   clear();
   return self;
}

HelpScreen* HelpScreen_done(HelpScreen* self) {
   Panel_delete((Object*)self->display);
   self->display = NULL;
   return self;
}

void HelpScreen_run(HelpScreen* self) {
   Panel* panel = self->display;

   while (true) {
      // A full redraw without selection highlighting preserves the segment colors.
      Panel_draw(panel, true, true, false, false);

      int ch = Panel_getCh(panel);
      switch (ch) {
         case ERR:
            continue;
         case 13:
         case KEY_ENTER:
         case 27:
         case 'q':
         case KEY_F(10):
            clear();
            return;
         case KEY_RESIZE:
            Panel_resize(panel, COLS, MAXIMUM(LINES - 1, 1));
            clear();
            break;
         case KEY_CTRL('L'):
            clear();
            break;
         default:
            Panel_onKey(panel, ch);
            break;
      }
   }
}

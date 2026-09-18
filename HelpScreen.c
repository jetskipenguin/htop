/*
htop - HelpScreen.c
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "HelpScreen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "FunctionBar.h"
#include "Macros.h"
#include "Object.h"
#include "Panel.h"
#include "ProvideCurses.h"
#include "RichString.h"
#include "Scheduling.h"
#include "Settings.h"
#include "XUtils.h"


enum {
   HELP_LEFT_COLUMN = 1,
   HELP_RIGHT_COLUMN = 43,
   HELP_ENTRY_SEGMENTS = 3,
};

typedef struct HelpEntry_ {
   const char* key;
   bool roInactive;
   const char* info;
   const char* suffix;
   ColorElements suffixColor;
} HelpEntry;

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
   this->segments = NULL;
   if (!count)
      return this;

   this->segments = xReallocArray(NULL, count, sizeof(*this->segments));
   for (size_t i = 0; i < count; i++) {
      this->segments[i].attr = segments[i].attr;
      this->segments[i].text = xStrdup(segments[i].text);
   }

   return this;
}

static const HelpEntry helpLeft[] = {
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
   { .key = "      H: ",  .roInactive = false, .info = "hide/show user process ",
      .suffix = "threads", .suffixColor = PROCESS_THREAD },
   { .key = "      K: ",  .roInactive = false, .info = "hide/show kernel ",
      .suffix = "threads", .suffixColor = PROCESS_THREAD },
   { .key = "      O: ",  .roInactive = false, .info = "hide/show processes in containers" },
   { .key = "      F: ",  .roInactive = false, .info = "cursor follows process" },
   { .key = "  + - *: ",  .roInactive = false, .info = "expand/collapse tree/toggle all" },
   { .key = "N P M T: ",  .roInactive = false, .info = "sort by PID, CPU%, MEM% or TIME" },
   { .key = "      I: ",  .roInactive = false, .info = "invert sort order" },
   { .key = " F6 > .: ",  .roInactive = false, .info = "select sort column" },
};

static const HelpEntry helpRight[] = {
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
};

static size_t HelpEntry_getWidth(const HelpEntry* entry) {
   if (!entry)
      return 0;

   // Shortcut entries are ASCII, so byte lengths also give their display widths.
   return strlen(entry->key) + strlen(entry->info) + (entry->suffix ? strlen(entry->suffix) : 0);
}

// The caller provides space for HELP_ENTRY_SEGMENTS segments.
static size_t HelpEntry_getSegments(const HelpEntry* entry, bool readonly, HelpLineSegment* segments) {
   if (!entry)
      return 0;

   bool inactive = readonly && entry->roInactive;
   segments[0] = (HelpLineSegment) {
      .attr = CRT_colors[inactive ? HELP_SHADOW : HELP_BOLD],
      .text = entry->key,
   };
   segments[1] = (HelpLineSegment) {
      .attr = CRT_colors[inactive ? HELP_SHADOW : DEFAULT_COLOR],
      .text = entry->info,
   };
   if (!entry->suffix)
      return 2;

   segments[2] = (HelpLineSegment) {
      .attr = CRT_colors[inactive ? HELP_SHADOW : entry->suffixColor],
      .text = entry->suffix,
   };
   return 3;
}

static HelpLine* HelpLine_newShortcutRow(const HelpEntry* left, const HelpEntry* right, bool readonly, size_t rightColumn) {
   if (!left && !right)
      return HelpLine_new(NULL, 0);

   HelpLineSegment segments[2 + 2 * HELP_ENTRY_SEGMENTS];
   char margin[HELP_LEFT_COLUMN + 1];
   memset(margin, ' ', HELP_LEFT_COLUMN);
   margin[HELP_LEFT_COLUMN] = '\0';
   segments[0] = (HelpLineSegment) { .attr = CRT_colors[DEFAULT_COLOR], .text = margin };
   size_t count = 1 + HelpEntry_getSegments(left, readonly, segments + 1);

   char* padding = NULL;
   if (right) {
      size_t width = HELP_LEFT_COLUMN + HelpEntry_getWidth(left);
      size_t gap = MAXIMUM(rightColumn, width + 1) - width;
      padding = xMalloc(gap + 1);
      memset(padding, ' ', gap);
      padding[gap] = '\0';
      segments[count++] = (HelpLineSegment) { .attr = CRT_colors[DEFAULT_COLOR], .text = padding };
      count += HelpEntry_getSegments(right, readonly, segments + count);
   }

   // HelpLine_new copies the temporary segments; the panel owns the resulting line.
   HelpLine* line = HelpLine_new(segments, count);
   free(padding);
   return line;
}

static void HelpScreen_addShortcuts(HelpScreen* self) {
   bool readonly = Settings_isReadonly();
   size_t rightColumn = HELP_RIGHT_COLUMN;
   for (size_t i = 0; i < ARRAYSIZE(helpLeft); i++) {
      size_t end = HELP_LEFT_COLUMN + HelpEntry_getWidth(&helpLeft[i]);
      rightColumn = MAXIMUM(rightColumn, end + 1);
   }

   for (size_t i = 0; i < MAXIMUM(ARRAYSIZE(helpLeft), ARRAYSIZE(helpRight)); i++) {
      const HelpEntry* left = i < ARRAYSIZE(helpLeft) ? &helpLeft[i] : NULL;
      const HelpEntry* right = i < ARRAYSIZE(helpRight) ? &helpRight[i] : NULL;
      Panel_add(self->display, (Object*)HelpLine_newShortcutRow(left, right, readonly, rightColumn));
   }

   // TODO: we may not need this because function bar will have keys for exiting
   HelpLineSegment paddingSegment = (HelpLineSegment) { .attr = CRT_colors[DEFAULT_COLOR], .text = " " };
   Panel_add(self->display, (Object*)HelpLine_new( &paddingSegment, 1));
   HelpLineSegment escHelpText = (HelpLineSegment) { .attr = CRT_colors[DEFAULT_COLOR], .text = "Press escape or enter to return." };
   Panel_add(self->display, (Object*)HelpLine_new( &escHelpText, 1));
}

HelpScreen* HelpScreen_init(HelpScreen* self) {
   // TODO: Add all functions to bar
   FunctionBar* bar = FunctionBar_newEnterEsc("Done   ", "Done   ");
   self->display = Panel_new(0, 0, COLS, MAXIMUM(LINES - 1, 1), Class(HelpLine), true, bar);
   HelpScreen_addShortcuts(self);

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

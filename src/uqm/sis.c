//Copyright Paul Reiche, Fred Ford. 1992-2002

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "sis.h"

#include "colors.h"
#include "races.h"
#include "starmap.h"
#include "units.h"
#include "menustat.h"
		// for DrawMenuStateStrings()
#include "gamestr.h"
#include "options.h"
#include "battle.h"
		// For BATTLE_FRAME_RATE
#include "element.h"
#include "setup.h"
#include "state.h"
#include "flash.h"
#include "libs/graphics/gfx_common.h"
#include "libs/tasklib.h"
#include "libs/alarm.h"
#include "libs/log.h"
#include "hyper.h"
#include "gameopt.h"

#include <stdio.h>

static StatMsgMode curMsgMode = SMM_DEFAULT;

static const UNICODE *describeWeapon (BYTE moduleType);

void
RepairSISBorder (void)
{
	RECT r;
	CONTEXT OldContext;

	OldContext = SetContext (ScreenContext);

	BatchGraphics ();

	// Left border
	r.corner.x = SIS_ORG_X - RES_SCALE (1);
	r.corner.y = SIS_ORG_Y - RES_SCALE (1);
	r.extent.width = RES_SCALE (1);
	r.extent.height = SIS_SCREEN_HEIGHT + RES_SCALE (2);
	SetContextForeGroundColor (SIS_LEFT_BORDER_COLOR);
	DrawFilledRectangle (&r);

	// Right border
	SetContextForeGroundColor (SIS_BOTTOM_RIGHT_BORDER_COLOR);
	r.corner.x += (SIS_SCREEN_WIDTH + RES_SCALE (2)) - RES_SCALE (1);
	DrawFilledRectangle (&r);

	// Bottom border
	r.corner.x = SIS_ORG_X - RES_SCALE (1);
	r.corner.y += (SIS_SCREEN_HEIGHT + RES_SCALE (2)) - RES_SCALE (1);
	r.extent.width = SIS_SCREEN_WIDTH + RES_SCALE (1);
	r.extent.height = RES_SCALE (1);
	DrawFilledRectangle (&r);

	DrawBorder (9, FALSE);

	UnbatchGraphics ();

	SetContext (OldContext);
}

void
ClearSISRect (BYTE ClearFlags)
{
	RECT r;
	Color OldColor;
	CONTEXT OldContext;

	OldContext = SetContext (StatusContext);
	OldColor = SetContextForeGroundColor (
			BUILD_COLOR (MAKE_RGB15 (0x0A, 0x0A, 0x0A), 0x08));

	r.corner.x = RES_SCALE (2);
	r.extent.width = STATUS_WIDTH - RES_SCALE (4);

	BatchGraphics ();
	if (ClearFlags & DRAW_SIS_DISPLAY)
	{
		DeltaSISGauges (UNDEFINED_DELTA, UNDEFINED_DELTA, UNDEFINED_DELTA);
	}

	if (ClearFlags & CLEAR_SIS_RADAR)
	{
		DrawMenuStateStrings ((BYTE)~0, 1);
#ifdef NEVER
		r.corner.x = RADAR_X - RES_SCALE (1);
		r.corner.y = RADAR_Y - RES_SCALE (1);
		r.extent.width = RADAR_WIDTH + RES_SCALE (2);
		r.extent.height = RADAR_HEIGHT + RES_SCALE (2);

		DrawStarConBox (&r, RES_SCALE (1);,
				BUILD_COLOR (MAKE_RGB15 (0x10, 0x10, 0x10), 0x19),
				BUILD_COLOR (MAKE_RGB15 (0x08, 0x08, 0x08), 0x1F),
				TRUE, BUILD_COLOR (MAKE_RGB15 (0x00, 0x0E, 0x00), 0x6C));
#endif /* NEVER */
	}
	UnbatchGraphics ();

	SetContextForeGroundColor (OldColor);
	SetContext (OldContext);
}

// Draw the SIS title. This is the field at the top of the screen, on the
// right hand side, containing the coordinates in HyperSpace, or the planet
// name in IP.
void
DrawSISTitle (UNICODE *pStr)
{
	TEXT t;
	CONTEXT OldContext;
	RECT r;

	t.baseline.x = RES_SCALE ((RES_DESCALE (SIS_TITLE_WIDTH) >> 1));
	t.baseline.y = SIS_TITLE_HEIGHT - RES_SCALE (2);
	t.align = ALIGN_CENTER;
	t.pStr = pStr;
	t.CharCount = (COUNT)~0;

	OldContext = SetContext (OffScreenContext);
	r.corner.x = SIS_ORG_X + SIS_SCREEN_WIDTH - SIS_TITLE_BOX_WIDTH
			+ RES_SCALE (1);
	r.corner.y = SIS_ORG_Y - SIS_TITLE_HEIGHT;
	r.extent.width = SIS_TITLE_WIDTH;
	r.extent.height = SIS_TITLE_HEIGHT - RES_SCALE (1);
	SetContextFGFrame (Screen);
	SetContextClipRect (&r);
	if (isPC (optWhichFonts) || SaveOrLoad)
		SetContextFont (TinyFont);
	else
	{
		SetContextFont (TinyFontBold);
		replaceChar (t.pStr, UNICHAR_SPACE, UNICHAR_TAB);
	}

	BatchGraphics ();

	// Background color
	SetContextBackGroundColor (SIS_TITLE_BACKGROUND_COLOR);
	ClearDrawable ();
	
	DrawBorder (3, FALSE);

	// Text color
	SetContextForeGroundColor (SIS_TITLE_TEXT_COLOR);
	font_DrawText (&t);

	UnbatchGraphics ();

	SetContextClipRect (NULL);

	SetContext (OldContext);
}

void
DrawHyperCoords (POINT universe)
{
	UNICODE buf[100];
	char *SpaceOrNull = (isPC (optWhichFonts) ? STR_SPACE : "");

	snprintf (buf, sizeof buf, "%03u.%01u%s:%s%03u.%01u",
			universe.x / 10, universe.x % 10,
			SpaceOrNull, SpaceOrNull,
			universe.y / 10, universe.y % 10);

	DrawSISTitle (buf);
}

void
DrawDiffSeed (SDWORD seed, BYTE difficulty, BOOLEAN extended, BOOLEAN nomad)
{
	if (seed)
	{
		UNICODE buf[100];
		char TempDiff[11];

		strncpy (
			TempDiff, GAME_STRING (MAINMENU_STRING_BASE + 56 + difficulty),
			ARRAY_SIZE (TempDiff)
		);

		snprintf (buf, sizeof buf, "%s %s%s%s",
				GAME_STRING (MAINMENU_STRING_BASE + 55),
				TempDiff,
				(extended ? GAME_STRING (MAINMENU_STRING_BASE + 59) : ""),
				(nomad ? GAME_STRING (MAINMENU_STRING_BASE + 60) : "")
			);
		DrawSISMessage (buf);

		snprintf (buf, sizeof buf, "%u", seed);
		DrawSISTitle (buf);
	}
	else
	{
		DrawSISMessage ("");
		DrawSISTitle ("");
	}
}

void
DrawSISMessage (const UNICODE *pStr)
{
	DrawSISMessageEx (pStr, -1, -1, DSME_NONE);
}

// See sis.h for the allowed flags. This is the field at the top of the screen, on the
// left hand side.
BOOLEAN
DrawSISMessageEx (const UNICODE *pStr, SIZE CurPos, SIZE ExPos, COUNT flags)
{
	UNICODE buf[256];
	CONTEXT OldContext;
	TEXT t;
	RECT r;

	OldContext = SetContext (OffScreenContext);
	// prepare the context
	r.corner.x = SIS_ORG_X + RES_SCALE (1);
	r.corner.y = SIS_ORG_Y - SIS_MESSAGE_HEIGHT;
	r.extent.width = SIS_MESSAGE_WIDTH;
	r.extent.height = SIS_MESSAGE_HEIGHT - RES_SCALE (1);
	SetContextFGFrame (Screen);
	SetContextClipRect (&r);
	
	BatchGraphics ();
	SetContextBackGroundColor (SIS_MESSAGE_BACKGROUND_COLOR);

	if (pStr == 0)
	{
		switch (LOBYTE (GLOBAL (CurrentActivity)))
		{
			default:
			case IN_ENCOUNTER:
				pStr = "";
				break;
			case IN_LAST_BATTLE:
			case IN_INTERPLANETARY:
				GetClusterName (CurStarDescPtr, buf);
				pStr = buf;
				break;
			case IN_HYPERSPACE:
				if (inHyperSpace ())
				{
					pStr = GAME_STRING (NAVIGATION_STRING_BASE);
							// "HyperSpace"
				}
				else
				{
					POINT Log = MAKE_POINT (
							LOGX_TO_UNIVERSE (GLOBAL_SIS (log_x)),
							LOGY_TO_UNIVERSE (GLOBAL_SIS (log_y)));

					pStr = GAME_STRING (NAVIGATION_STRING_BASE + 1);
							// "QuasiSpace"

					if (GET_GAME_STATE (ARILOU_HOME_VISITS)
						&& (Log.x == ARILOU_HOME_X && Log.y == ARILOU_HOME_Y))
					{
						utf8StringCopy (
								GLOBAL_SIS (PlanetName),
								sizeof GLOBAL_SIS (PlanetName),
								GAME_STRING (STAR_STRING_BASE + 148)
							);
							// "Falayalaralfali"
					}
				}
				break;
		}
	}

	if (!(flags & DSME_MYCOLOR))
		SetContextForeGroundColor (SIS_MESSAGE_TEXT_COLOR);

	t.baseline.y = SIS_MESSAGE_HEIGHT - RES_SCALE (2);
	t.baseline.x = RES_SCALE (RES_DESCALE (SIS_MESSAGE_WIDTH) >> 1);
	t.pStr = pStr;
	t.CharCount = (COUNT)~0;
	if (isPC (optWhichFonts) || SaveOrLoad)
		SetContextFont (TinyFont);
	else
	{
		SetContextFont (TinyFontBold);
		if (CurPos < 0 && ExPos < 0)
			replaceChar (t.pStr, UNICHAR_SPACE, UNICHAR_TAB);
	}

	if (flags & DSME_CLEARFR)
		SetFlashRect (NULL, FALSE);

	if (CurPos < 0 && ExPos < 0)
	{	// normal state
		ClearDrawable ();

		DrawBorder (2, FALSE);
		t.align = ALIGN_CENTER;
		font_DrawText (&t);
	}
	else
	{	// editing state
		int i;
		RECT text_r;
		// XXX: 128 is currently safe, but it would be better to specify
		//   the size to TextRect()
		BYTE char_deltas[128];
		BYTE *pchar_deltas;
		SIZE bl;

		t.align = ALIGN_CENTER;
		TextRect (&t, &text_r, char_deltas);
		if (text_r.extent.width + t.baseline.x + RES_SCALE (2) >= r.extent.width)
		{	// the text does not fit the input box size and so
			// will not fit when displayed later
			// disallow the change
			UnbatchGraphics ();
			SetContextClipRect (NULL);
			SetContext (OldContext);
			return (FALSE);
		}

		ClearDrawable ();
		DrawBorder (2, FALSE);
		SetCursorFlashBlock (FALSE);


		if (CurPos >= 0 && CurPos <= t.CharCount)
		{	// calc and draw the cursor
			RECT cur_r = text_r;
			TEXT h = t;
			RECT text_ex;

			if (CurPos > 0)
			{	// recalculate cursor rect because of central alignment
				UNICODE buffer[50];
				strncpy(buffer, t.pStr, CurPos);
				buffer[CurPos] = '\0';
				h.pStr = buffer;
				TextRect(&h, &text_ex, char_deltas);
				cur_r = text_ex;
			}

			cur_r.corner.y = 0;
			cur_r.extent.height = r.extent.height;

			for (i = CurPos, pchar_deltas = char_deltas; i > 0; --i)
				cur_r.corner.x += (SIZE)*pchar_deltas++;
			if (CurPos < h.CharCount) /* end of line */
				cur_r.corner.x -= RES_SCALE (1);
			
			if (flags & DSME_BLOCKCUR)
			{	// Use block cursor for keyboardless systems
				SetCursorFlashBlock (TRUE);
				if (CurPos == h.CharCount)
				{	// cursor at end-line -- use insertion point
					cur_r.extent.width = RES_SCALE (1);
					SetCursorFlashBlock (FALSE);
				}
				else if (CurPos + 1 == h.CharCount)
				{	// extra pixel for last char margin
					cur_r.extent.width = (SIZE)*pchar_deltas + RES_SCALE (2);
				}
				else
				{	// normal mid-line char
					cur_r.extent.width = (SIZE)*pchar_deltas + RES_SCALE (1);
				}

				if (cur_r.extent.width >= 200)
				{
					SetCursorFlashBlock (FALSE);
					cur_r.extent.width = RES_SCALE (1);
				}
				else
				{
					SetContextForeGroundColor (SIS_MESSAGE_CURSOR_COLOR);
					DrawFilledRectangle (&cur_r);
				}
			}
			else
			{	// Insertion point cursor
				cur_r.extent.width = RES_SCALE (1);
			}
			
			SetCursorRect (&cur_r, OffScreenContext);
			bl = cur_r.corner.x + RES_SCALE (1);
		}

		SetContextForeGroundColor (SIS_MESSAGE_TEXT_COLOR);

		if (ExPos >= 0 && ExPos < t.CharCount)
		{	// handle extra characters
			t.CharCount = ExPos;
			font_DrawText (&t);

			// print extra chars
			t.align = ALIGN_LEFT;
			t.baseline.x = bl; // no matter where the cursor is - print extra text right after it
			SetContextForeGroundColor (SIS_MESSAGE_EXTRA_TEXT_COLOR);
			t.pStr = skipUTF8Chars (t.pStr, ExPos);
			t.CharCount = (COUNT)~0;
			font_DrawText (&t);
		}
		else
		{	// just print the text
			font_DrawText (&t);
		}
	}

	if (flags & DSME_SETFR)
	{
		r.corner.x = 0;
		r.corner.y = 0;
	}

	UnbatchGraphics ();

	SetContextClipRect (NULL);
	SetContext (OldContext);

	return (TRUE);
}

void
DateToString (char *buf, size_t bufLen,
		BYTE month_index, BYTE day_index, COUNT year_index)
{
	switch (optDateFormat)
	{
		case 1: /* MM.DD.YYYY */
			snprintf (buf, bufLen, "%02d%s%02d%s%04d", month_index,
					STR_MIDDLE_DOT, day_index, STR_MIDDLE_DOT, year_index);
			break;
		case 2: /* DD MMM YYYY */
			snprintf (buf, bufLen, "%02d %s%s%04d", day_index,
					GAME_STRING (MONTHS_STRING_BASE + month_index - 1),
					STR_MIDDLE_DOT, year_index);
			break;
		case 3: /* DD.MM.YYYY */
			snprintf (buf, bufLen, "%02d%s%02d%s%04d", day_index,
					STR_MIDDLE_DOT, month_index, STR_MIDDLE_DOT, year_index);
			break;
		case 0:
		default: /* MMM DD.YYYY */
			snprintf (buf, bufLen, "%s %02d%s%04d",
					GAME_STRING (MONTHS_STRING_BASE + month_index - 1),
					day_index, STR_MIDDLE_DOT, year_index);
			break;
	}
}

void
GetStatusMessageRect (RECT *r)
{
	r->corner.x = RES_SCALE (2);
	r->corner.y = RES_SCALE (130);
	r->extent.width = STATUS_MESSAGE_WIDTH;
	r->extent.height = STATUS_MESSAGE_HEIGHT;
}

void
DrawStatusMessage (const UNICODE *pStr)
{
	RECT r;
	RECT ctxRect;
	TEXT t;
	UNICODE buf[128];
	CONTEXT OldContext;

	OldContext = SetContext (StatusContext);
	GetContextClipRect (&ctxRect);
	// XXX: Technically, this does not need OffScreenContext. The only reason
	//   it is used is to avoid preserving StatusContext settings.
	SetContext (OffScreenContext);
	SetContextFGFrame (Screen);
	GetStatusMessageRect (&r);
	r.corner.x += ctxRect.corner.x;
	r.corner.y += ctxRect.corner.y;
	SetContextClipRect (&r);

	BatchGraphics ();
	SetContextBackGroundColor (STATUS_MESSAGE_BACKGROUND_COLOR);
	ClearDrawable ();

	DrawBorder (7, FALSE);

	if (!pStr)
	{
		if (curMsgMode == SMM_CREDITS)
		{
			snprintf (buf, sizeof buf, "%u %s", MAKE_WORD (
					GET_GAME_STATE (MELNORME_CREDIT0),
					GET_GAME_STATE (MELNORME_CREDIT1)
					), GAME_STRING (STATUS_STRING_BASE + 0)); // "Cr"
		}
		else if (curMsgMode == SMM_RES_UNITS)
		{
			if (GET_GAME_STATE (CHMMR_BOMB_STATE) >= 2 || optInfiniteRU)
			{
				snprintf (buf, sizeof buf, "%s %s",
						(isPC (optWhichMenu) && isPC (optWhichFonts)) ?
							GAME_STRING (STATUS_STRING_BASE + 2)
							: STR_INFINITY_SIGN, // "UNLIMITED"
						GAME_STRING (STATUS_STRING_BASE + 1)); // "RU"
			}
			else
			{
				snprintf (buf, sizeof buf, "%u %s", GLOBAL_SIS (ResUnits),
						GAME_STRING (STATUS_STRING_BASE + 1)); // "RU"
			}
		}
		else
		{	// Just a date
			DateToString (buf, sizeof buf,
					GLOBAL (GameClock.month_index),
					GLOBAL (GameClock.day_index),
					GLOBAL (GameClock.year_index));
		}
		pStr = buf;
	}

	t.baseline.x = (STATUS_MESSAGE_WIDTH >> 1);
	t.baseline.y = STATUS_MESSAGE_HEIGHT - RES_SCALE (1);
	t.align = ALIGN_CENTER;
	t.pStr = pStr;
	t.CharCount = (COUNT)~0;

	if (curMsgMode == SMM_WARNING) {
		SetContextForeGroundColor (STATUS_MESSAGE_WARNING_TEXT_COLOR);
	} else if (curMsgMode == SMM_ALERT) {
		SetContextForeGroundColor (STATUS_MESSAGE_ALERT_TEXT_COLOR);
	} else {
		SetContextForeGroundColor (STATUS_MESSAGE_TEXT_COLOR);
	}

	if (isPC (optWhichFonts) || optCustomBorder)
		SetContextFont (TinyFont);
	else
	{
		SetContextFont (TinyFontBold);
		replaceChar (t.pStr, UNICHAR_SPACE, UNICHAR_TAB);
	}

	SetContextForeGroundColor (STATUS_MESSAGE_TEXT_COLOR);
	font_DrawText (&t);
	UnbatchGraphics ();

	SetContextClipRect (NULL);

	SetContext (OldContext);
}

StatMsgMode
SetStatusMessageMode (StatMsgMode newMode)
{
	StatMsgMode oldMode = curMsgMode;
	curMsgMode = newMode;
	return oldMode;
}

void
DrawCaptainsName (bool NewGame)
{
	RECT r;
	TEXT t;
	CONTEXT OldContext;
	FONT OldFont;
	Color OldColor;

	OldContext = SetContext (StatusContext);
	if (isPC (optWhichFonts))
		OldFont = SetContextFont (TinyFont);
	else
		OldFont = SetContextFont (TinyFontBold);

	OldColor = SetContextForeGroundColor (CAPTAIN_NAME_BACKGROUND_COLOR);

	r.corner.x = RES_SCALE (2 + 1);
	r.corner.y = RES_SCALE (10);
	r.extent.width = SHIP_NAME_WIDTH - RES_SCALE (2);
	r.extent.height = SHIP_NAME_HEIGHT;
	DrawFilledRectangle (&r);

	if(!NewGame)
		DrawBorder (6, FALSE);

	t.baseline.x = (STATUS_WIDTH >> 1) - RES_SCALE (1);
	t.baseline.y = r.corner.y + RES_SCALE (6);
	t.align = ALIGN_CENTER;
	t.pStr = GLOBAL_SIS (CommanderName);
	t.CharCount = (COUNT)~0;
	SetContextForeGroundColor (CAPTAIN_NAME_TEXT_COLOR);
	font_DrawText (&t);

	SetContextForeGroundColor (OldColor);
	SetContextFont (OldFont);
	SetContext (OldContext);
}

void
DrawFlagshipName (BOOLEAN InStatusArea, bool NewGame)
{
	RECT r, rHD;
	TEXT t;
	FONT OldFont;
	Color OldColor;
	CONTEXT OldContext;
	FRAME OldFontEffect;
	UNICODE buf[250];

	OldFontEffect = SetContextFontEffect (NULL);
	OldColor = SetContextForeGroundColor (FLAGSHIP_NAME_BACKGROUND_COLOR);

	if (InStatusArea)
	{
		OldContext = SetContext (StatusContext);
		OldFont = SetContextFont (StarConFont);

		r.corner.x = RES_SCALE (2);
		r.corner.y = RES_SCALE (20);
		r.extent.width = SHIP_NAME_WIDTH;
		r.extent.height = SHIP_NAME_HEIGHT;

		t.pStr = GLOBAL_SIS (ShipName);

		DrawFilledRectangle (&r);
	}
	else
	{
		OldContext = SetContext (SpaceContext);
		OldFont = SetContextFont (MicroFont);

		r.corner.x = 0;
		r.corner.y = RES_SCALE (1);
		r.extent.width = SIS_SCREEN_WIDTH;
		r.extent.height = SHIP_NAME_HEIGHT;

		t.pStr = buf;
		snprintf (buf, sizeof buf, "%s %s",
				GAME_STRING (NAMING_STRING_BASE + 1), GLOBAL_SIS (ShipName));
		// XXX: this will not work with UTF-8 strings
		strupr (buf);

		// JMS: Handling the a-umlaut and o-umlaut characters
		{
			unsigned char *ptr;
			ptr = (unsigned char*)buf;
			while (*ptr) {
				if (*ptr == 0xc3) {
					ptr++;
					if (*ptr == 0xb6 || *ptr == 0xa4) {
						*ptr += 'A' - 'a';
					}
				}
				ptr++;
			}
		}

		if (IS_HD)
		{
			rHD.extent.width = SIS_SCREEN_WIDTH * 0.75;
			rHD.corner.x = (SIS_SCREEN_WIDTH - r.extent.width) / 2;
		}

		DrawFilledRectangle (RES_BOOL (&r, &rHD));
	}

	if (!NewGame)
		DrawBorder (12, FALSE);

	t.baseline.x = r.corner.x + RES_SCALE (RES_DESCALE (r.extent.width) >> 1);
	t.baseline.y = r.corner.y + (SHIP_NAME_HEIGHT - RES_SCALE (InStatusArea));
	t.align = ALIGN_CENTER;
	t.CharCount = (COUNT)~0;
	if (isPC (optWhichFonts))
		SetContextFontEffect (SetAbsFrameIndex (FontGradFrame,
				InStatusArea ? 0 : 3));
	else
		SetContextForeGroundColor (THREEDO_FLAGSHIP_NAME_TEXT_COLOR);
	
	font_DrawText (&t);

	SetContextFontEffect (OldFontEffect);
	SetContextForeGroundColor (OldColor);
	SetContextFont (OldFont);
	SetContext (OldContext);
}

void
DrawFlagshipStats (void)
{
	RECT r;
	TEXT t;
	FONT OldFont;
	Color OldColor;
	FRAME OldFontEffect;
	CONTEXT OldContext;
	UNICODE buf[128];
	SIZE leading;
	BYTE i;
	BYTE energy_regeneration, energy_wait, turn_wait;
	COUNT max_thrust;
	DWORD fuel;

	/* collect stats */
#define ENERGY_REGENERATION 1
#define ENERGY_WAIT 10
#define MAX_THRUST 10
#define TURN_WAIT 17
	energy_regeneration = ENERGY_REGENERATION;
	energy_wait = ENERGY_WAIT;
	max_thrust = MAX_THRUST;
	turn_wait = TURN_WAIT;
	fuel = 10 * FUEL_TANK_SCALE;

	for (i = 0; i < NUM_MODULE_SLOTS; i++)
	{
		switch (GLOBAL_SIS (ModuleSlots[i])) {
			case FUEL_TANK:
				fuel += FUEL_TANK_CAPACITY;
				break;
			case HIGHEFF_FUELSYS:
				fuel += HEFUEL_TANK_CAPACITY;
				break;
			case DYNAMO_UNIT:
				energy_wait -= 2;
				if (energy_wait < 4)
					energy_wait = 4;
				break;
			case SHIVA_FURNACE:
				energy_regeneration++;
				break;
		}
	}

	for (i = 0; i < NUM_DRIVE_SLOTS; ++i)
		if (GLOBAL_SIS (DriveSlots[i]) == FUSION_THRUSTER)
			max_thrust += 2;

	for (i = 0; i < NUM_JET_SLOTS; ++i)
		if (GLOBAL_SIS (JetSlots[i]) == TURNING_JETS)
			turn_wait -= 2;
	/* END collect stats */

	OldContext = SetContext (SpaceContext);
	OldFont = SetContextFont (StarConFont);
	OldFontEffect = SetContextFontEffect (NULL);
	GetContextFontLeading (&leading);

	/* we need room to play.  full screen width, 4 lines tall */
	r.corner.x = 0;
	r.corner.y = SIS_SCREEN_HEIGHT - (4 * leading);
	r.extent.width = SIS_SCREEN_WIDTH;
	r.extent.height = (4 * leading);

	OldColor = SetContextForeGroundColor (BLACK_COLOR);
	DrawFilledRectangle (&r);

	/*
	   now that we've cleared out our playground, compensate for the
	   fact that the leading is way more than is generally needed.
	*/
	leading -= RES_SCALE (3);
	t.baseline.x = RES_SCALE (ORIG_SIS_SCREEN_WIDTH / 6); //wild-assed guess, but it worked
	t.baseline.y = r.corner.y + leading + RES_SCALE (3);
	t.align = ALIGN_RIGHT;
	t.CharCount = (COUNT)~0;

	SetContextFontEffect (SetAbsFrameIndex (FontGradFrame, 4));

	t.pStr = GAME_STRING (FLAGSHIP_STRING_BASE + 0); // "nose:"
	font_DrawText (&t);
	t.baseline.y += leading;
	t.pStr = GAME_STRING (FLAGSHIP_STRING_BASE + 1); // "spread:"
	font_DrawText (&t);
	t.baseline.y += leading;
	t.pStr = GAME_STRING (FLAGSHIP_STRING_BASE + 2); // "side:"
	font_DrawText (&t);
	t.baseline.y += leading;
	t.pStr = GAME_STRING (FLAGSHIP_STRING_BASE + 3); // "tail:"
	font_DrawText (&t);

	t.baseline.x += RES_SCALE (5);
	t.baseline.y = r.corner.y + leading + RES_SCALE (3);
	t.align = ALIGN_LEFT;
	t.pStr = buf;

	snprintf (buf, sizeof buf, "%-7.7s",
			describeWeapon (GLOBAL_SIS (ModuleSlots[15])));
	font_DrawText (&t);
	t.baseline.y += leading;
	snprintf (buf, sizeof buf, "%-7.7s",
			describeWeapon (GLOBAL_SIS (ModuleSlots[14])));
	font_DrawText (&t);
	t.baseline.y += leading;
	snprintf (buf, sizeof buf, "%-7.7s",
			describeWeapon (GLOBAL_SIS (ModuleSlots[13])));
	font_DrawText (&t);
	t.baseline.y += leading;
	snprintf (buf, sizeof buf, "%-7.7s",
			describeWeapon (GLOBAL_SIS (ModuleSlots[0])));
	font_DrawText (&t);

	t.baseline.x = r.extent.width - RES_SCALE (25);
	t.baseline.y = r.corner.y + leading + RES_SCALE (3);
	t.align = ALIGN_RIGHT;

	SetContextFontEffect (SetAbsFrameIndex (FontGradFrame, 5));

	t.pStr = GAME_STRING (FLAGSHIP_STRING_BASE + 4); // "maximum velocity:"
	font_DrawText (&t);
	t.baseline.y += leading;
	t.pStr = GAME_STRING (FLAGSHIP_STRING_BASE + 5); // "turning rate:"
	font_DrawText (&t);
	t.baseline.y += leading;
	t.pStr = GAME_STRING (FLAGSHIP_STRING_BASE + 6); // "combat energy:"
	font_DrawText (&t);
	t.baseline.y += leading;
	t.pStr = GAME_STRING (FLAGSHIP_STRING_BASE + 7); // "maximum fuel:"
	font_DrawText (&t);

	t.baseline.x = r.extent.width - RES_SCALE (2);
	t.baseline.y = r.corner.y + leading + RES_SCALE (3);
	t.pStr = buf;

	snprintf (buf, sizeof buf, "%4u", max_thrust * 4);
	font_DrawText (&t);
	t.baseline.y += leading;
	snprintf (buf, sizeof buf, "%4u", 1 + TURN_WAIT - turn_wait);
	font_DrawText (&t);
	t.baseline.y += leading;
	{
		unsigned int energy_per_10_sec =
				(((100 * ONE_SECOND * energy_regeneration) /
				((1 + energy_wait) * BATTLE_FRAME_RATE)) + 5) / 10;
		snprintf (buf, sizeof buf, "%2u.%1u",
				energy_per_10_sec / 10, energy_per_10_sec % 10);
	}
	font_DrawText (&t);
	t.baseline.y += leading;
	snprintf (buf, sizeof buf, "%4u", (fuel / FUEL_TANK_SCALE));
	font_DrawText (&t);

	SetContextFontEffect (OldFontEffect);
	SetContextForeGroundColor (OldColor);
	SetContextFont (OldFont);
	SetContext (OldContext);
}

static const UNICODE *
describeWeapon (BYTE moduleType)
{
	switch (moduleType)
	{
		case GUN_WEAPON:
			return GAME_STRING (FLAGSHIP_STRING_BASE + 8); // "gun"
		case BLASTER_WEAPON:
			return GAME_STRING (FLAGSHIP_STRING_BASE + 9); // "blaster"
		case CANNON_WEAPON:
			return GAME_STRING (FLAGSHIP_STRING_BASE + 10); // "cannon"
		case BOMB_MODULE_0:
		case BOMB_MODULE_1:
		case BOMB_MODULE_2:
		case BOMB_MODULE_3:
		case BOMB_MODULE_4:
		case BOMB_MODULE_5:
			return GAME_STRING (FLAGSHIP_STRING_BASE + 11); // "n/a"
		default:
			return GAME_STRING (FLAGSHIP_STRING_BASE + 12); // "none"
	}
}

void
DrawLanders (void)
{
	BYTE i;
	SIZE width;
	RECT r;
	STAMP s;
	CONTEXT OldContext;

	OldContext = SetContext (StatusContext);

	s.frame = IncFrameIndex (FlagStatFrame);
	GetFrameRect (s.frame, &r);

	i = GLOBAL_SIS (NumLanders);
	r.corner.x = (STATUS_WIDTH >> 1) - r.corner.x;
	s.origin.x = r.corner.x
			- RES_SCALE (
					RES_DESCALE ((r.extent.width * i) + (RES_SCALE (2)
						* (i - 1))) >> 1
				);
	s.origin.y = RES_SCALE (29);

	width = r.extent.width + RES_SCALE (2);
	r.extent.width = (r.extent.width * MAX_LANDERS)
			+ (RES_SCALE (2) * (MAX_LANDERS - 1)) + RES_SCALE (2);
	r.corner.x -= RES_SCALE (RES_DESCALE (r.extent.width) >> 1);
	r.corner.y += s.origin.y;
	SetContextForeGroundColor (BLACK_COLOR);
	DrawFilledRectangle (&r);
	while (i--)
	{
		DrawStamp (&s);
		s.origin.x += width;
	}

	SetContext (OldContext);
}

// Draw the storage bays, below the picture of the flagship.
void
DrawStorageBays (BOOLEAN Refresh)
{
	BYTE i;
	RECT r;
	CONTEXT OldContext;

	OldContext = SetContext (StatusContext);

	r.extent.width  = RES_SCALE (2);
	r.extent.height = RES_SCALE (4);
	r.corner.y		= RES_SCALE (123);
	if (Refresh)
	{
		r.extent.width = NUM_MODULE_SLOTS * (r.extent.width + 1);
		r.corner.x = (STATUS_WIDTH >> 1) - (r.extent.width >> 1);

		SetContextForeGroundColor (BLACK_COLOR);
		DrawFilledRectangle (&r);
		r.extent.width = RES_SCALE (2);
	}

	i = (BYTE)CountSISPieces (STORAGE_BAY);
	if (i)
	{
		COUNT j;

		r.corner.x = (STATUS_WIDTH >> 1)
				- RES_SCALE (
						RES_DESCALE (i * (r.extent.width + RES_SCALE (1)))
							>> 1
					);
		SetContextForeGroundColor (STORAGE_BAY_FULL_COLOR);
		for (j = GLOBAL_SIS (TotalElementMass);
				j >= STORAGE_BAY_CAPACITY; j -= STORAGE_BAY_CAPACITY)
		{
			DrawFilledRectangle (&r);
			r.corner.x += r.extent.width + RES_SCALE (1);;

			--i;
		}

		r.extent.height = (RES_SCALE (4) * j + (STORAGE_BAY_CAPACITY - 1))
				/ STORAGE_BAY_CAPACITY;
		if (r.extent.height)
		{
			r.corner.y += RES_SCALE (4) - r.extent.height;
			DrawFilledRectangle (&r);
			r.extent.height = RES_SCALE (4) - r.extent.height;
			if (r.extent.height)
			{
				r.corner.y = RES_SCALE (123);
				SetContextForeGroundColor (STORAGE_BAY_EMPTY_COLOR);
				DrawFilledRectangle (&r);
			}
			r.corner.x += r.extent.width + RES_SCALE (1);

			--i;
		}
		r.extent.height = RES_SCALE (4);

		SetContextForeGroundColor (STORAGE_BAY_EMPTY_COLOR);
		while (i--)
		{
			DrawFilledRectangle (&r);
			r.corner.x += r.extent.width + RES_SCALE (1);
		}
	}

	SetContext (OldContext);
}

void
GetGaugeRect (RECT *pRect, BOOLEAN IsCrewRect)
{
	pRect->extent.width = RES_SCALE (24);
	pRect->corner.x = (STATUS_WIDTH >> 1) - (pRect->extent.width >> 1);
	pRect->extent.height = RES_SCALE (5);
	pRect->corner.y = IsCrewRect ? RES_SCALE (117) : RES_SCALE (38);
}

static void
DrawPC_SIS (void)
{
	TEXT t;
	RECT r;

	GetGaugeRect (&r, FALSE);
	t.baseline.x = (STATUS_WIDTH >> 1);
	t.baseline.y = r.corner.y - RES_SCALE (1);
	t.align = ALIGN_CENTER;
	t.CharCount = (COUNT)~0;
	SetContextFont (TinyFont);
	SetContextForeGroundColor (BLACK_COLOR);

	// Black rectangle behind "FUEL" text and fuel amount.
	r.corner.y -= RES_SCALE (6);
	r.corner.x -= RES_SCALE (1);
	r.extent.width += RES_SCALE (2);
	DrawFilledRectangle (&r);

	SetContextFontEffect (SetAbsFrameIndex (FontGradFrame, 1));
	t.pStr = GAME_STRING (STATUS_STRING_BASE + 3); // "FUEL"
	font_DrawText (&t);

	// Black rectangle behind "CREW" text and crew amount.
	r.corner.y += RES_SCALE (79);
	t.baseline.y += RES_SCALE (79);
	DrawFilledRectangle (&r);

	SetContextFontEffect (SetAbsFrameIndex (FontGradFrame, 2));
	t.pStr = GAME_STRING (STATUS_STRING_BASE + 4); // "CREW"
	font_DrawText (&t);
	SetContextFontEffect (NULL);

	// Background of text "CAPTAIN".
	r.corner.x = RES_SCALE (2 + 1);
	r.corner.y = RES_SCALE (3);
	r.extent.width = RES_SCALE (58);
	r.extent.height = RES_SCALE (7);
	SetContextForeGroundColor (PC_CAPTAIN_STRING_BACKGROUND_COLOR);
	DrawFilledRectangle (&r);

	DrawBorder (4, FALSE);

	// Text "CAPTAIN".
	SetContextForeGroundColor (PC_CAPTAIN_STRING_TEXT_COLOR);
	t.baseline.y = r.corner.y + RES_SCALE (6);
	t.pStr = GAME_STRING (STATUS_STRING_BASE + 5); // "CAPTAIN"
	font_DrawText (&t);
}

static void
DrawThrusters (void)
{
	STAMP s;
	COUNT i;

	s.origin.x = RES_SCALE (1);
	s.origin.y = 0;
	for (i = 0; i < NUM_DRIVE_SLOTS; ++i)
	{
		BYTE which_piece = GLOBAL_SIS (DriveSlots[i]);
		if (which_piece < EMPTY_SLOT)
		{
			s.frame = SetAbsFrameIndex (FlagStatFrame, which_piece + 1 + 0);
			DrawStamp (&s);
			s.frame = IncFrameIndex (s.frame);
			DrawStamp (&s);
		}

		s.origin.y -= RES_SCALE (3);
	}
}

static void
DrawTurningJets (void)
{
	STAMP s;
	COUNT i;

	s.origin.x = RES_SCALE (1);
	s.origin.y = 0;
	for (i = 0; i < NUM_JET_SLOTS; ++i)
	{
		BYTE which_piece = GLOBAL_SIS (JetSlots[i]);
		if (which_piece < EMPTY_SLOT)
		{
			s.frame = SetAbsFrameIndex (FlagStatFrame, which_piece + 1 + 1);
			DrawStamp (&s);
			s.frame = IncFrameIndex (s.frame);
			DrawStamp (&s);
		}

		s.origin.y -= RES_SCALE (3);
	}
}

static void
DrawModules (void)
{
	STAMP s;
	COUNT i;

	// This properly centers the modules.
	s.origin.x = RES_SCALE (1);
	s.origin.y = RES_SCALE (1);
	for (i = 0; i < NUM_MODULE_SLOTS; ++i)
	{
		BYTE which_piece = GLOBAL_SIS (ModuleSlots[i]);
		if (which_piece < EMPTY_SLOT)
		{
			s.frame = SetAbsFrameIndex (FlagStatFrame, which_piece + 1 + 2);
			DrawStamp (&s);
		}

		s.origin.y -= RES_SCALE (3);
	}
}

static void
DrawSupportShips (void)
{
	HSHIPFRAG hStarShip;
	HSHIPFRAG hNextShip;
	const POINT *pship_pos;
	const POINT ship_pos[MAX_BUILT_SHIPS] =
	{
		SUPPORT_SHIP_PTS
	};

	for (hStarShip = GetHeadLink (&GLOBAL (built_ship_q)),
			pship_pos = ship_pos;
			hStarShip; hStarShip = hNextShip, ++pship_pos)
	{
		SHIP_FRAGMENT *StarShipPtr;
		STAMP s;

		StarShipPtr = LockShipFrag (&GLOBAL (built_ship_q), hStarShip);
		hNextShip = _GetSuccLink (StarShipPtr);

		s.origin = MAKE_POINT (
				RES_SCALE (pship_pos->x), RES_SCALE (pship_pos->y));
		s.frame = SetAbsFrameIndex (StarShipPtr->icons, 0);
		DrawStamp (&s);

		UnlockShipFrag (&GLOBAL (built_ship_q), hStarShip);
	}
}

static void
DeltaSISGauges_crewDelta (SIZE crew_delta)
{
	if (crew_delta == 0)
		return;

	if (crew_delta != UNDEFINED_DELTA)
	{
		COUNT CrewCapacity;

		if (crew_delta < 0
				&& GLOBAL_SIS (CrewEnlisted) <= (COUNT)-crew_delta)
			GLOBAL_SIS (CrewEnlisted) = 0;
		else
		{
			GLOBAL_SIS (CrewEnlisted) += crew_delta;
			CrewCapacity = GetCrewPodCapacity ();
			if (GLOBAL_SIS (CrewEnlisted) > CrewCapacity)
				GLOBAL_SIS (CrewEnlisted) = CrewCapacity;
		}
	}

	{
		TEXT t;
		UNICODE buf[60];
		RECT r;

		snprintf (buf, sizeof buf, "%u", GLOBAL_SIS (CrewEnlisted));

		GetGaugeRect (&r, TRUE);
		
		t.baseline.x = (STATUS_WIDTH >> 1);
		t.baseline.y = r.corner.y + r.extent.height;
		t.align = ALIGN_CENTER;
		t.pStr = buf;
		t.CharCount = (COUNT)~0;

		SetContextForeGroundColor (BLACK_COLOR);
		DrawFilledRectangle (&r);
		SetContextForeGroundColor (
				BUILD_COLOR (MAKE_RGB15 (0x00, 0x0E, 0x00), 0x6C));
		font_DrawText (&t);
	}
}

static void
DeltaSISGauges_fuelDelta (SDWORD fuel_delta)
{
	DWORD OldCoarseFuel;
	DWORD NewCoarseFuel;

	if (fuel_delta == 0)
		return;

	if (fuel_delta == UNDEFINED_DELTA)
		OldCoarseFuel = (DWORD)~0;
	else {

		OldCoarseFuel = (GLOBAL_SIS(FuelOnBoard) / (!optWholeFuel ? FUEL_TANK_SCALE : 1));
		if (fuel_delta < 0
				&& GLOBAL_SIS (FuelOnBoard) <= (DWORD)-fuel_delta) {
			GLOBAL_SIS (FuelOnBoard) = 0;
		} else {
			DWORD FuelCapacity = GetFuelTankCapacity ();
			GLOBAL_SIS (FuelOnBoard) += fuel_delta;
			if (GLOBAL_SIS (FuelOnBoard) > FuelCapacity)
				GLOBAL_SIS (FuelOnBoard) = FuelCapacity;
		}
	}

	NewCoarseFuel = (GLOBAL_SIS(FuelOnBoard) / (!optWholeFuel ? FUEL_TANK_SCALE : 1));
	if (NewCoarseFuel != OldCoarseFuel)
	{
		TEXT t;
		// buf from [60] to [7]: The max fuel anyone can ever get is 1610 (1610.00 in whole value)
		// I.E. only 4 (7) characters, we don't need that much extra padding.
		UNICODE buf[7];
		RECT r;
		// Serosis: Cast as a double and divided by FUEL_TANK_SCALE to get a decimal
		double dblFuelOnBoard = (double)NewCoarseFuel / FUEL_TANK_SCALE;

		if (!optInfiniteFuel) {
			if(!optWholeFuel)
				snprintf(buf, sizeof buf, "%u", NewCoarseFuel);
			else if (dblFuelOnBoard > 999.99)
				snprintf(buf, sizeof buf, "%.1f", dblFuelOnBoard);
			else
				snprintf(buf, sizeof buf, "%.2f", dblFuelOnBoard);
		}
		else
			snprintf (buf, sizeof buf, "%s", STR_INFINITY_SIGN);


		GetGaugeRect (&r, FALSE);

		if (optWhichFonts == OPT_3DO && !optWholeFuel)
			SetContextFont (TinyFontBold);
		else
			SetContextFont (TinyFont);
		
		t.baseline.x = (STATUS_WIDTH >> 1);
		t.baseline.y = r.corner.y + r.extent.height;
		t.align = ALIGN_CENTER;
		t.pStr = buf;
		t.CharCount = (COUNT)~0;

		SetContextForeGroundColor (BLACK_COLOR);
		if (optWholeFuel)
		{
			r.corner.x -= RES_SCALE (1);
			r.extent.width += RES_SCALE (2);
		}
		DrawFilledRectangle (&r);
		SetContextForeGroundColor (
				BUILD_COLOR (MAKE_RGB15 (0x13, 0x00, 0x00), 0x2C));
		font_DrawText (&t);
	}
}
	
static void
DeltaSISGauges_resunitDelta (SIZE resunit_delta)
{
	if (resunit_delta == 0)
		return;

	if (resunit_delta != UNDEFINED_DELTA)
	{
		if (resunit_delta < 0
				&& GLOBAL_SIS (ResUnits) <= (DWORD)-resunit_delta)
			GLOBAL_SIS (ResUnits) = 0;
		else
			GLOBAL_SIS (ResUnits) += resunit_delta;

		assert (curMsgMode == SMM_RES_UNITS);
	}
	else
	{
		RECT r;

		GetStatusMessageRect (&r);
		SetContextForeGroundColor (
				BUILD_COLOR (MAKE_RGB15 (0x00, 0x08, 0x00), 0x6E));
		DrawFilledRectangle (&r);
	}
		
	DrawStatusMessage (NULL);
}

void
DeltaSISGauges (SIZE crew_delta, SDWORD fuel_delta, int resunit_delta)
{
	CONTEXT OldContext;

	if (crew_delta == 0 && fuel_delta == 0 && resunit_delta == 0)
		return;

	OldContext = SetContext (StatusContext);

	BatchGraphics ();
	if (fuel_delta == UNDEFINED_DELTA)
	{
		STAMP s;
		s.origin.x = 0;
		s.origin.y = 0;

		s.frame = FlagStatFrame;

		if (optFlagshipColor == OPT_3DO)
			s.frame = SetAbsFrameIndex (s.frame, 23);
		else
			s.frame = SetAbsFrameIndex (s.frame, 0);

		DrawStamp (&s);

		DrawBorder (1, FALSE);

		if (optWhichFonts == OPT_PC)
			DrawPC_SIS();

		DrawThrusters ();
		DrawTurningJets ();
		DrawModules ();

		DrawSupportShips ();
	}

	if (isPC (optWhichFonts))
		SetContextFont (TinyFont);
	else
		SetContextFont (TinyFontBold);

	DeltaSISGauges_crewDelta (crew_delta);
	DeltaSISGauges_fuelDelta (fuel_delta);

	if (fuel_delta == UNDEFINED_DELTA)
	{
		if(optWhichFonts == OPT_3DO)
			DrawBorder (5, FALSE);
		DrawFlagshipName (TRUE, FALSE);
		DrawCaptainsName (FALSE);
		DrawLanders ();
		DrawStorageBays (FALSE);
	}

	DeltaSISGauges_resunitDelta (resunit_delta);

	UnbatchGraphics ();

	SetContext (OldContext);
}


////////////////////////////////////////////////////////////////////////////
// Crew
////////////////////////////////////////////////////////////////////////////

// Get the total amount of crew aboard the SIS.
COUNT
GetCrewCount (void)
{
	return GLOBAL_SIS (CrewEnlisted);
}

// Get the number of crew which fit in a module of a specified type.
COUNT
GetModuleCrewCapacity (BYTE moduleType)
{
	if (moduleType == CREW_POD)
		return CREW_POD_CAPACITY;

	return 0;
}

// Gets the amount of crew which currently fit in the ship's crew pods.
COUNT
GetCrewPodCapacity (void)
{
	COUNT capacity = 0;
	COUNT slotI;

	for (slotI = 0; slotI < NUM_MODULE_SLOTS; slotI++)
	{
		BYTE moduleType = GLOBAL_SIS (ModuleSlots[slotI]);
		capacity += GetModuleCrewCapacity (moduleType);
	}

	return capacity;
}

// Find the slot number of the crew pod and "seat" number in that crew pod,
// where the Nth crew member would be located.
// If the crew member does not fit, false is returned, and *slotNr and
// *seatNr are unchanged.
static bool
GetCrewPodForCrewMember (COUNT crewNr, COUNT *slotNr, COUNT *seatNr)
{
	COUNT slotI;
	COUNT capacity = 0;

	slotI = NUM_MODULE_SLOTS;
	while (slotI--) {
		BYTE moduleType = GLOBAL_SIS (ModuleSlots[slotI]);
		COUNT moduleCapacity = GetModuleCrewCapacity (moduleType);

		if (crewNr < capacity + moduleCapacity)
		{
			*slotNr = slotI;
			*seatNr = crewNr - capacity;
			return true;
		}
		capacity += moduleCapacity;
	}

	return false;
}

// Get the point where to draw the next crew member,
// set the foreground color to the color for that crew member,
// and return GetCrewPodCapacity ().
// TODO: Split of the parts of this function into separate functions.
COUNT
GetCPodCapacity (POINT *ppt)
{
	COUNT crewCount;
	COUNT slotNr;
	COUNT seatNr;

	COUNT rowNr;
	COUNT colNr;
				
	static const Color crewRows[] = PC_CREW_COLOR_TABLE;

	crewCount = GetCrewCount ();
	if (!GetCrewPodForCrewMember (crewCount, &slotNr, &seatNr))
	{
		// Crew does not fit. *ppt is unchanged.
		return GetCrewPodCapacity ();
	}

	rowNr = seatNr / CREW_PER_ROW;
	colNr = seatNr % CREW_PER_ROW;

	if (optWhichFonts == OPT_PC)
		SetContextForeGroundColor (crewRows[rowNr]);
	else
		SetContextForeGroundColor (THREEDO_CREW_COLOR);
		
	ppt->x = RES_SCALE (27) + (slotNr * SHIP_PIECE_OFFSET)
			- RES_SCALE (colNr * 2);
	ppt->y = RES_SCALE (34 - rowNr * 2) + IF_HD (2);

	return GetCrewPodCapacity ();
}


////////////////////////////////////////////////////////////////////////////
// Storage bays
////////////////////////////////////////////////////////////////////////////

// Get the total amount of minerals aboard the SIS.
static COUNT
GetElementMass (void)
{
	return GLOBAL_SIS (TotalElementMass);
}

// Get the number of crew which fit in a module of a specified type.
COUNT
GetModuleStorageCapacity (BYTE moduleType)
{
	if (moduleType == STORAGE_BAY)
		return STORAGE_BAY_CAPACITY;

	return 0;
}

// Gets the amount of minerals which currently fit in the ship's storage.
COUNT
GetStorageBayCapacity (void)
{
	COUNT capacity = 0;
	COUNT slotI;

	for (slotI = 0; slotI < NUM_MODULE_SLOTS; slotI++)
	{
		BYTE moduleType = GLOBAL_SIS (ModuleSlots[slotI]);
		capacity += GetModuleStorageCapacity (moduleType);
	}

	return capacity;
}

// Find the slot number of the storage bay and "storage cell" number in that
// storage bay, where the N-1th mineral unit would be located.
// If the mineral unit does not fit, false is returned, and *slotNr and
// *cellNr are unchanged.
static bool
GetStorageCellForMineralUnit (COUNT unitNr, COUNT *slotNr, COUNT *cellNr)
{
	COUNT slotI;
	COUNT capacity = 0;

	slotI = NUM_MODULE_SLOTS;
	while (slotI--) {
		BYTE moduleType = GLOBAL_SIS (ModuleSlots[slotI]);
		COUNT moduleCapacity = GetModuleStorageCapacity (moduleType);

		if (unitNr <= capacity + moduleCapacity)
		{
			*slotNr = slotI;
			*cellNr = unitNr - capacity;
			return true;
		}
		capacity += moduleCapacity;
	}

	return false;
}

// Get the point where to draw the next mineral unit,
// set the foreground color to the color for that mineral unit,
// and return GetStorageBayCapacity ().
// TODO: Split of the parts of this function into separate functions.
COUNT
GetSBayCapacity (POINT *ppt)
{
	COUNT massCount;
	COUNT slotNr;
	COUNT cellNr;

	COUNT rowNr;
	COUNT colNr;

	static const Color colorBars[] = STORAGE_BAY_COLOR_TABLE;

	massCount = GetElementMass ();
	if (!GetStorageCellForMineralUnit (massCount, &slotNr, &cellNr))
	{
		// Mineral does not fit. *ppt is unchanged.
		return GetStorageBayCapacity ();
	}

	rowNr = cellNr / SBAY_MASS_PER_ROW;
	colNr = cellNr % SBAY_MASS_PER_ROW;

	if (rowNr == 0)
		SetContextForeGroundColor (BLACK_COLOR);
	else
	{
		rowNr--;
		SetContextForeGroundColor (colorBars[rowNr]);
	}
		
	ppt->x = RES_SCALE (19) + (slotNr * SHIP_PIECE_OFFSET);
	ppt->y = RES_SCALE (34 - (rowNr * 2)) + IF_HD (2);

	return GetStorageBayCapacity ();
}


////////////////////////////////////////////////////////////////////////////
// Fuel tanks
////////////////////////////////////////////////////////////////////////////

// Get the total amount of fuel aboard the SIS.
static DWORD
GetFuelTotal (void)
{
	return GLOBAL_SIS (FuelOnBoard);
}

// Get the amount of fuel which fits in a module of a specified type.
DWORD
GetModuleFuelCapacity (BYTE moduleType)
{
	if (moduleType == FUEL_TANK)
		return FUEL_TANK_CAPACITY;

	if (moduleType == HIGHEFF_FUELSYS)
		return HEFUEL_TANK_CAPACITY;

	return 0;
}

// Gets the amount of fuel which currently fits in the ship's fuel tanks.
DWORD
GetFuelTankCapacity (void)
{
	DWORD capacity = FUEL_RESERVE;
	COUNT slotI;

	for (slotI = 0; slotI < NUM_MODULE_SLOTS; slotI++)
	{
		BYTE moduleType = GLOBAL_SIS (ModuleSlots[slotI]);
		capacity += GetModuleFuelCapacity (moduleType);
	}

	return capacity;
}

// Find the slot number of the fuel cell and "compartment" number in that
// crew pod, where the Nth unit of fuel would be located.
// If the unit does not fit, false is returned, and *slotNr and
// *compartmentNr are unchanged.
// Pre: unitNr >= FUEL_RESERER
static bool
GetFuelTankForFuelUnit (DWORD unitNr, COUNT *slotNr, DWORD *compartmentNr)
{
	COUNT slotI;
	DWORD capacity = FUEL_RESERVE;

	assert (unitNr >= FUEL_RESERVE);

	slotI = NUM_MODULE_SLOTS;
	while (slotI--) {
		BYTE moduleType = GLOBAL_SIS (ModuleSlots[slotI]);
	
		capacity += GetModuleFuelCapacity (moduleType);
		if (unitNr < capacity)
		{
			*slotNr = slotI;
			*compartmentNr = capacity - unitNr;
			return true;
		}
	}

	return false;
}

// Get the point where to draw the next fuel unit,
// set the foreground color to the color for that unit,
// and return GetFuelTankCapacity ().
// TODO: Split of the parts of this function into separate functions.
DWORD
GetFTankCapacity (POINT *ppt)
{
	DWORD capacity;
	DWORD fuelAmount;
	COUNT slotNr;
	DWORD compartmentNr;
	BYTE moduleType;
	DWORD volume;

	DWORD rowNr;
	
	static const Color fuelColors[] = FUEL_COLOR_TABLE;
		
	capacity = GetFuelTankCapacity ();
	fuelAmount = GetFuelTotal ();
	if (fuelAmount < FUEL_RESERVE)
	{
		// Fuel is in the SIS reserve, not in a fuel tank.
		// *ppt is unchanged
		return capacity;
	}

	if (!GetFuelTankForFuelUnit (fuelAmount, &slotNr, &compartmentNr))
	{
		// Fuel does not fit. *ppt is unchanged.
		return capacity;
	}

	moduleType = GLOBAL_SIS (ModuleSlots[slotNr]);
	volume = GetModuleFuelCapacity (moduleType);

	rowNr = ((volume - compartmentNr) * MAX_FUEL_BARS / HEFUEL_TANK_CAPACITY);

	ppt->x = RES_SCALE (21) + (slotNr * SHIP_PIECE_OFFSET)
			+ IF_HD (OutfitOrShipyard == 2 ? 0 : 2);
	if (volume == FUEL_TANK_CAPACITY)
		ppt->y = RES_SCALE (27 - rowNr);
	else
		ppt->y = RES_SCALE (30 - rowNr);

	assert (rowNr + 1 < (COUNT)ARRAY_SIZE (fuelColors));
	SetContextForeGroundColor (fuelColors[rowNr]);
	SetContextBackGroundColor (fuelColors[rowNr + 1]);

	return capacity;
}


////////////////////////////////////////////////////////////////////////////

COUNT
CountSISPieces (BYTE piece_type)
{
	COUNT i, num_pieces;

	num_pieces = 0;
	if (piece_type == FUSION_THRUSTER)
	{
		for (i = 0; i < NUM_DRIVE_SLOTS; ++i)
		{
			if (GLOBAL_SIS (DriveSlots[i]) == piece_type)
				++num_pieces;
		}
	}
	else if (piece_type == TURNING_JETS)
	{
		for (i = 0; i < NUM_JET_SLOTS; ++i)
		{
			if (GLOBAL_SIS (JetSlots[i]) == piece_type)
				++num_pieces;
		}
	}
	else
	{
		for (i = 0; i < NUM_MODULE_SLOTS; ++i)
		{
			if (GLOBAL_SIS (ModuleSlots[i]) == piece_type)
				++num_pieces;
		}
	}

	return num_pieces;
}

void
DrawAutoPilotMessage (BOOLEAN Reset)
{
	static BOOLEAN LastPilot = FALSE;
	static TimeCount NextTime = 0;
	static DWORD cycle_index = 0;
	BOOLEAN OnAutoPilot;
	
	static const Color cycle_tab[] = AUTOPILOT_COLOR_CYCLE_TABLE;
	const size_t cycleCount = ARRAY_SIZE (cycle_tab);
#define BLINK_RATE (ONE_SECOND * 3 / 40) // 9 @ 120 ticks/second

	if (Reset)
	{	// Just a reset, not drawing
		LastPilot = FALSE;
		return;
	}

	OnAutoPilot = (GLOBAL (autopilot.x) != ~0 && GLOBAL (autopilot.y) != ~0)
			|| GLOBAL_SIS (FuelOnBoard) == 0;

	if (OnAutoPilot || LastPilot)
	{
		if (!OnAutoPilot)
		{	// AutoPilot aborted -- clear the AUTO-PILOT message
			DrawSISMessage (NULL);
			cycle_index = 0;
		}
		else if (GetTimeCounter () >= NextTime)
		{
			if (!(GLOBAL (CurrentActivity) & CHECK_ABORT)
					&& GLOBAL_SIS (CrewEnlisted) != (COUNT)~0)
			{
				CONTEXT OldContext;

				OldContext = SetContext (OffScreenContext);
				SetContextForeGroundColor (cycle_tab[cycle_index]);
				if (GLOBAL_SIS (FuelOnBoard) == 0)
				{
					DrawSISMessageEx (
							GAME_STRING (NAVIGATION_STRING_BASE + 2),
							-1, -1, DSME_MYCOLOR);   // "OUT OF FUEL"
				}
				else
				{
					DrawSISMessageEx (GAME_STRING (NAVIGATION_STRING_BASE + 3),
							-1, -1, DSME_MYCOLOR);   // "AUTO-PILOT"
				}
				SetContext (OldContext);
			}

			cycle_index = (cycle_index + 1) % cycleCount;
			NextTime = GetTimeCounter () + BLINK_RATE;
		}

		LastPilot = OnAutoPilot;
	}
}

#define MAX_NUM_RECTS 5 // 5 flashing rects at once should be enough
#define NUM_RECTS 1

static FlashContext *flashContext[MAX_NUM_RECTS] = { NULL, NULL, NULL, NULL, NULL };
static RECT flash_rect[MAX_NUM_RECTS];
static Alarm *flashAlarm = NULL;
static BOOLEAN flashPaused = FALSE;
static BYTE count_r = NUM_RECTS;

static void scheduleFlashAlarm (void);

static void
updateFlashRect (void *arg)
{
	if (flashContext[0] == NULL)
		return;

	for (COUNT i = 0; i < count_r; i++)
	{
		Flash_process (flashContext[i]);
	}
	scheduleFlashAlarm ();
	(void) arg;
}

static void
scheduleFlashAlarm (void)
{
	TimeCount nextTime = Flash_nextTime (flashContext[0]);
	DWORD nextTimeMs = (nextTime / ONE_SECOND) * 1000 +
			((nextTime % ONE_SECOND) * 1000 / ONE_SECOND);
			// Overflow-safe conversion.
	flashAlarm = Alarm_addAbsoluteMs (nextTimeMs, updateFlashRect, NULL);
}

void
SetAdditionalRect (const RECT *pRect, COUNT number)
{	// Add new flashing rect (Max 5)
	// Must be called one by one and in incremental order
	if (pRect != NULL && count_r != MAX_NUM_RECTS)
	{
		RECT clip_r = { {0, 0}, {0, 0} };
		GetContextClipRect (&clip_r);

		flash_rect[number] = *pRect;
		flash_rect[number].corner.x += clip_r.corner.x;
		flash_rect[number].corner.y += clip_r.corner.y;

		if (number == count_r)
			count_r++;
	}
}

void
DumpAdditionalRect (void)
{	// Dump all additional rects
	for (COUNT i = count_r; i > 0; i--)
	{
		if (flashContext[i] != NULL)
		{
			Flash_terminate (flashContext[i]);
			flashContext[i] = NULL;
		}
	}
	count_r = NUM_RECTS;
}

void
SetFlashRect (const RECT *pRect, BOOLEAN pcRect)
{
	RECT clip_r = {{0, 0}, {0, 0}};
	RECT temp_r;
	
	if (pRect != SFR_MENU_3DO && pRect != SFR_MENU_ANY)
	{	// The caller specified their own flash area, or NULL (stop flashing).
		GetContextClipRect (&clip_r);
	}
	else
	{
 		if (optWhichMenu == OPT_PC && pRect != SFR_MENU_ANY)
 		{
			// The player wants PC menus and this flash is not used
			// for a PC menu.
			// Don't flash.
 			pRect = 0;
 		}
 		else
 		{
			// The player wants 3DO menus, or the flash is used in both
			// 3DO and PC mode.
			CONTEXT OldContext = SetContext (StatusContext);
 			GetContextClipRect (&clip_r);
 			pRect = &temp_r;
 			temp_r.corner.x = RADAR_X - clip_r.corner.x;
 			temp_r.corner.y = RADAR_Y - clip_r.corner.y;
 			temp_r.extent.width = RADAR_WIDTH;
 			temp_r.extent.height = RADAR_HEIGHT;
 			SetContext (OldContext);
		}
	}

	if (pRect != 0 && pRect->extent.width != 0)
	{

		// Flash rectangle is not empty, start or continue flashing.
		flash_rect[0] = *pRect;

		flash_rect[0].corner.x += clip_r.corner.x;
		flash_rect[0].corner.y += clip_r.corner.y;

		// Create a new flash context(s).
		for (COUNT i = 0; i < count_r; i++)
		{
			if (flashContext[i] == NULL)
			{
				flashContext[i] = Flash_createHighlight (ScreenContext, &flash_rect[i]);
				Flash_setMergeFactors (flashContext[i], 3, 2, 2);
				Flash_setSpeed (flashContext[i], 0, ONE_SECOND / 16, 0, ONE_SECOND / 16);
				Flash_setFrameTime (flashContext[i], ONE_SECOND / 16);
				Flash_setPulseBox(flashContext[i], pcRect);
				Flash_start (flashContext[i]);
				if (i == (count_r - 1))
				scheduleFlashAlarm ();
			}
			else
			{
				Flash_setPulseBox(flashContext[i], pcRect);
				Flash_setRect (flashContext[i], &flash_rect[i]);
			}
		}
	}
	else
	{
		// Flash rectangle is empty. Stop flashing.
		if (flashContext[0] != NULL)
		{
			Alarm_remove (flashAlarm);
			flashAlarm = 0;
			
			for (COUNT i = 0; i < count_r; i++)
			{
				Flash_terminate (flashContext[i]);
				flashContext[i] = NULL;
			}
			count_r = NUM_RECTS;
		}
	}
}

COUNT updateFlashRectRecursion = 0;
// XXX This is necessary at least because DMS_AddEscortShip() calls
// DrawRaceStrings() in an UpdateFlashRect block, which calls
// ClearSISRect(), which calls DrawMenuStateStrings(), which starts its own
// UpdateFlashRect block. This should probably be cleaned up.

void
PreUpdateFlashRect (void)
{
	if (flashAlarm)
	{
		updateFlashRectRecursion++;
		if (updateFlashRectRecursion > 1)
			return;
		for (COUNT i = 0; i < count_r; i++)
		{
			Flash_preUpdate (flashContext[i]);
		}
	}
}

void
PostUpdateFlashRect (void)
{
	if (flashAlarm)
	{
		updateFlashRectRecursion--;
		if (updateFlashRectRecursion > 0)
			return;
		for (COUNT i = 0; i < count_r; i++)
		{
			Flash_postUpdate (flashContext[i]);
		}
	}
}

// Stop flashing if flashing is active.
void
PauseFlash (void)
{
	if (flashContext[0] != NULL)
	{
		Alarm_remove (flashAlarm);
		flashAlarm = 0;
		flashPaused = TRUE;
	}
	for (BYTE i = 0; i < count_r; i++)
	{
		if (flashContext[i] != NULL && Flash_getPulseBox (flashContext[i]))
				Flash_pause (flashContext[i]);
	}
}

// Continue flashing after PauseFlash (), if flashing was active.
void
ContinueFlash (void)
{
	if (flashPaused)
	{
		for (BYTE i = 0; i < count_r; i++)// need to do before setting clock
		{
			if (flashContext[i] != NULL && Flash_getPulseBox (flashContext[i]))
				Flash_continue (flashContext[i]);
		}

		scheduleFlashAlarm ();
		flashPaused = FALSE;
	}
}



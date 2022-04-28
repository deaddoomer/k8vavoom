//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2018-2022 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, version 3 of the License ONLY.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**  You should have received a copy of the GNU General Public License
//**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//**
//**************************************************************************
#ifndef VAVOOM_AUTOMAP_HEADER
#define VAVOOM_AUTOMAP_HEADER


void AM_Init ();
bool AM_Responder (event_t *ev);
void AM_Ticker ();
void AM_Drawer ();
// called to force the automap to quit if the level is completed while it is up
void AM_Stop ();

class VWidget;
void AM_DrawAtWidget (VWidget *w, float xc, float yc, float scale, float angle, float plrangle, float alpha);

void AM_Dirty (); // trigger line visibility update (autoresets)

bool AM_IsActive ();
bool AM_IsOverlay (); // returns `false` if automap is not active
bool AM_IsFullscreen (); // returns `false` if automap is not active

void AM_SetActive (bool act);

// automap marks API
int AM_GetMaxMarks ();
// for saving
bool AM_IsMarkActive (int index);
float AM_GetMarkX (int index);
float AM_GetMarkY (int index);
// for loading
void AM_ClearMarks ();
void AM_SetMarkXY (int index, float x, float y);

void AM_ClearAutomap ();

// this also remembers current map for marks
void AM_ClearMarksIfMapChanged (VLevel *currmap);

extern VCvarB am_always_update;


#endif

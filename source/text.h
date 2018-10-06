//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

// horisontal alignement
enum halign_e {
  hleft,   //Left
  hcentre, //Centred
  hright,  //Right
  hcenter = hcentre,
};

// vertical alignement
enum valign_e {
  vtop,    //Top
  vcentre, //Centre
  vbottom, //Bottom
  vcenter = vcentre,
};

// text colours, these must match the constants used in ACS
enum {
  CR_UNDEFINED = -1,
  CR_BRICK,
  CR_TAN,
  CR_GRAY,
  CR_GREEN,
  CR_BROWN,
  CR_GOLD,
  CR_RED,
  CR_BLUE,
  CR_ORANGE,
  CR_WHITE,
  CR_YELLOW,
  CR_UNTRANSLATED,
  CR_BLACK,
  CR_LIGHTBLUE,
  CR_CREAM,
  CR_OLIVE,
  CR_DARKGREEN,
  CR_DARKRED,
  CR_DARKBROWN,
  CR_PURPLE,
  CR_DARKGRAY,
  NUM_TEXT_COLOURS
};

class VFont;

void T_Init ();
void T_Shutdown ();

void T_SetFont (VFont *);
void T_SetAlign (halign_e, valign_e);

void T_DrawText (int, int, const VStr &, int);

void T_DrawCursor ();
void T_DrawCursorAt (int, int);

extern VFont *SmallFont;
extern VFont *ConFont;

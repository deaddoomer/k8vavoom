//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2009, 2010 Brendon Duncan
//**  Copyright (C) 2019-2023 Ketmar Dark
//**  Based on https://github.com/CO2/UDMF-Convert
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
#ifndef ZDOOM_MAPDATA_H
#define ZDOOM_MAPDATA_H

#include "sharedmap.h"


struct ZDoomThing : public thing {
  enum {
    TF_EASY = 0x1,    //Appears on skills 0 and 1
    TF_MEDIUM = 0x2,  //Ditto for skill 2
    TF_HARD = 0x4,    //Ditto for skills 3 and 4
    TF_AMBUSH = 0x8,  //This name is more awesome than "deaf"
    TF_DORMANT = 0x10,  //Must be activated with thing_activate
    TF_FIGHTER = 0x20,  //Appears for fighter class
    TF_CLERIC = 0x40, //Appears for cleric class
    TF_MAGE = 0x80,   //Appears for mage class
    TF_SINGLE = 0x100,  //Appears in singleplayer games
    TF_COOP = 0x200,  //Appears in cooperative games
    TF_DMATCH = 0x400,  //Appears in deathmatch games
    TF_TRANSLUCENT = 0x800,
    TF_INVISIBLE = 0x1000,
    TF_FRIENDLY = 0x2000,
    TF_STANDSTILL = 0x4000
  };

  short tid;
  short height; //'z' coordinate (sort of)
  int special;
  int arg[5];

  ZDoomThing () : thing(), tid(0), height(0), special(0) { memset(arg, 0, sizeof(arg)); }

  virtual void read (FILE *fl) override;
  virtual udmf::block convert (double xfactor=1.0, double yfactor=1.0, double zfactor=1.0) const override;
};

struct ZDoomLine : public linedef {
  enum {
    LDF_IMPASSIBLE = 0x1,
    LDF_BLOCKMONSTER = 0x2,
    LDF_TWOSIDED = 0x4,
    LDF_UPPERUNPEGGED = 0x8,
    LDF_LOWERUNPEGGED = 0x10,
    LDF_SECRET = 0x20,
    LDF_BLOCKSOUND = 0x40,
    LDF_NOAUTOMAP = 0x80,
    LDF_VISIBLE = 0x100,
    LDF_REPEATABLE = 0x200,
    LDF_MONSTERUSE = 0x2000,
    LDF_BLOCKPLAYERS = 0x4000,
    LDF_BLOCKALL = 0x8000
  };

  enum {
    LDFE_ZONEBOUNDARY = 0x1,
    LDFE_RAILING = 0x2,
    LDFE_BLOCKFLOATERS = 0x4,
    LDFE_CLIPMIDTEX = 0x8,
    LDFE_WRAPMIDTEX = 0x10,
    LDFE_3DMIDTEX = 0x20,
    LDFE_CHECKRANGE = 0x40,
    LDFE_FIRSTSIDE = 0x80
  };

  enum {
    LDA_PLAYERCROSS = 0x0,
    LDA_PLAYERUSE = 0x400,
    LDA_MONSTERCROSS = 0x800,
    LDA_PROJECTILEHIT = 0xC00,
    LDA_PLAYERBUMP = 0x1000,
    LDA_PROJECTILECROSS = 0x1400,
    LDA_PASSTHRUUSE = 0x1800,
    LDA_PROJECTILEANY = 0x1C00  //Seems to exist
  };

  int arg[5];

  ZDoomLine () : linedef() { memset(arg, 0, sizeof(arg)); }

  virtual void read (FILE *fl) override;
  virtual udmf::block convert (double xfactor=1.0, double yfactor=1.0, double zfactor=1.0) const override;
};


#endif

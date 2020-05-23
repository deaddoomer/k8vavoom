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
//**  Copyright (C) 2018-2020 Ketmar Dark
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

class VPlayerReplicationInfo : public VThinker {
  DECLARE_CLASS(VPlayerReplicationInfo, VThinker, 0)
  NO_DEFAULT_CONSTRUCTOR(VPlayerReplicationInfo);

  // player we are replicating
  VBasePlayer *Player;
  vint32 PlayerNum;

  VStr PlayerName;
  VStr UserInfo;

  vuint8 TranslStart;
  vuint8 TranslEnd;
  vint32 Color;

  vint32 Frags;
  vint32 Deaths;
  vint32 KillCount;
  vint32 ItemCount;
  vint32 SecretCount;
};

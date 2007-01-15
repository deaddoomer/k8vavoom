//**************************************************************************
//**
//**	##   ##    ##    ##   ##   ####     ####   ###     ###
//**	##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**	 ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**	 ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**	  ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**	   #    ##    ##    #      ####     ####   ##       ##
//**
//**	$Id: dehacked.cpp 1962 2007-01-11 23:29:44Z dj_jl $
//**
//**	Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**	This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**	This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
//**
//**	Dehacked patch parsing
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern char*				sprnames[];
extern char*				mobj_names[];
extern int					num_sfx;
/*extern state_t				states[];
extern mobjinfo_t			mobjinfo[];
extern weaponinfo_t			weaponinfo[];
extern sfxinfo_t			sfx[];
extern string_def_t			Strings[];
extern map_info_t			map_info1[];
extern map_info_t			map_info2[];*/
int					maxammo[4];
int					perammo[4];
int					initial_health;
int					initial_ammo;
int					bfg_cells;
int					soulsphere_max;
int					soulsphere_health;
int					megasphere_health;
int					god_health;

bool					Hacked;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int*		functions;

static char		*Patch;
static char		*PatchPtr;
static char 	*String;
static int 		value;

static TArray<FReplacedString>	SfxNames;
static TArray<FReplacedString>	MusicNames;
static TArray<FReplacedString>	SpriteNames;
static VLanguage*				EngStrings;

// CODE --------------------------------------------------------------------

//==========================================================================
//
//  GetLine
//
//==========================================================================

static bool GetLine()
{
	do
	{
		if (!*PatchPtr)
		{
			return false;
		}

		String = PatchPtr;

		while (*PatchPtr && *PatchPtr != '\n')
		{
			PatchPtr++;
		}
		if (*PatchPtr == '\n')
		{
			*PatchPtr = 0;
			PatchPtr++;
		}

		if (*String == '#')
		{
			*String = 0;
			continue;
		}

		while (*String && *String <= ' ')
		{
			String++;
		}
	} while (!*String);

	return true;
}

//==========================================================================
//
//  ParseParam
//
//==========================================================================

static bool ParseParam()
{
	char	*val;

	if (!GetLine())
	{
		return false;
	}

	val = strchr(String, '=');

	if (!val)
	{
		return false;
	}

	value = atoi(val + 1);

	do
	{
		*val = 0;
		val--;
	}
	while (val >= String && *val <= ' ');

	return true;
}

//==========================================================================
//
//  ReadThing
//
//==========================================================================

static void ReadThing(int num)
{
	num--; // begin at 0 not 1;
/*	if (num >= NUMMOBJTYPES || num < 0)
	{
		dprintf("WARNING! Invalid thing num %d\n", num);
		while (ParseParam());
		return;
	}*/

	while (ParseParam())
	{
/*		if (!strcmp(String ,"ID #"))	    			mobjinfo[num].doomednum   =value;
		else if (!strcmp(String, "Initial frame"))		mobjinfo[num].spawnstate  =value;
		else if (!strcmp(String, "Hit points"))	    	mobjinfo[num].spawnhealth =value;
		else if (!strcmp(String, "First moving frame"))	mobjinfo[num].seestate    =value;
		else if (!strcmp(String, "Alert sound"))	    mobjinfo[num].seesound    =value;
		else if (!strcmp(String, "Reaction time"))   	mobjinfo[num].reactiontime=value;
		else if (!strcmp(String, "Attack sound"))	    mobjinfo[num].attacksound =value;
		else if (!strcmp(String, "Injury frame"))	    mobjinfo[num].painstate   =value;
		else if (!strcmp(String, "Pain chance"))     	mobjinfo[num].painchance  =value;
		else if (!strcmp(String, "Pain sound")) 		mobjinfo[num].painsound   =value;
		else if (!strcmp(String, "Close attack frame"))	mobjinfo[num].meleestate  =value;
		else if (!strcmp(String, "Far attack frame"))	mobjinfo[num].missilestate=value;
		else if (!strcmp(String, "Death frame"))	    mobjinfo[num].deathstate  =value;
		else if (!strcmp(String, "Exploding frame"))	mobjinfo[num].xdeathstate =value;
		else if (!strcmp(String, "Death sound")) 		mobjinfo[num].deathsound  =value;
		else if (!strcmp(String, "Speed"))	    		mobjinfo[num].speed       =value;
		else if (!strcmp(String, "Width"))	    		mobjinfo[num].radius      =value;
		else if (!strcmp(String, "Height"))	    		mobjinfo[num].height      =value;
		else if (!strcmp(String, "Mass"))	    		mobjinfo[num].mass	      =value;
		else if (!strcmp(String, "Missile damage"))		mobjinfo[num].damage      =value;
		else if (!strcmp(String, "Action sound"))		mobjinfo[num].activesound =value;
		else if (!strcmp(String, "Bits"))	    		mobjinfo[num].flags       =value;
		else if (!strcmp(String, "Respawn frame"))		mobjinfo[num].raisestate  =value;
		else */dprintf("WARNING! Invalid mobj param %s\n", String);
	}
}

//==========================================================================
//
//  ReadSound
//
//==========================================================================

static void ReadSound(int num)
{
	while (ParseParam())
	{
/*		if (!strcmp(String, "Offset"));				//Lump name offset - can't handle
		else if (!strcmp(String, "Zero/One"));		//Singularity - removed
		else if (!strcmp(String, "Value"))			sfx[num].priority = value;
		else if (!strcmp(String, "Zero 1"));        //Lump num - can't be set
		else if (!strcmp(String, "Zero 2"));        //Data pointer - can't be set
		else if (!strcmp(String, "Zero 3"));		//Usefulness - removed
		else if (!strcmp(String, "Zero 4"));        //Link - removed
		else if (!strcmp(String, "Neg. One 1"));    //Link pitch - removed
		else if (!strcmp(String, "Neg. One 2"));    //Link volume - removed
		else */dprintf("WARNING! Invalid sound param %s\n", String);
	}
}

//==========================================================================
//
//  ReadState
//
//==========================================================================

static void ReadState(int num)
{
/*	if (num >= NUMSTATES || num < 0)
	{
		dprintf("WARNING! Invalid state num %d\n", num);
		while (ParseParam());
		return;
	}*/

	while (ParseParam())
	{
/*		if (!strcmp(String, "Sprite number"))     		states[num].sprite    = value;
		else if (!strcmp(String, "Sprite subnumber"))	states[num].frame	  = value;
		else if (!strcmp(String, "Duration"))    		states[num].tics	  = value;
		else if (!strcmp(String, "Next frame"))    		states[num].nextstate = value;
		else if (!strcmp(String, "Unknown 1"))    		states[num].misc1 	  = value;
		else if (!strcmp(String, "Unknown 2"))    		states[num].misc2 	  = value;
		else if (!strcmp(String, "Action pointer"))     dprintf("WARNING! Tried to set action pointer.\n");
		else */dprintf("WARNING! Invalid state param %s\n", String);
	}
}

//==========================================================================
//
//  ReadSpriteName
//
//==========================================================================

static void ReadSpriteName(int)
{
	while (ParseParam())
	{
		if (!VStr::ICmp(String, "Offset"));	//	Can't handle
		else dprintf("WARNING! Invalid sprite name param %s\n", String);
	}
}

//==========================================================================
//
//  ReadAmmo
//
//==========================================================================

static void ReadAmmo(int num)
{
	while (ParseParam())
	{
		if (!VStr::ICmp(String, "Max ammo"))		maxammo[num] = value;
		else if (!VStr::ICmp(String, "Per ammo"))	perammo[num] = value;
		else dprintf("WARNING! Invalid ammo param %s\n", String);
	}
}

//==========================================================================
//
//  ReadWeapon
//
//==========================================================================

static void ReadWeapon(int num)
{
	while (ParseParam())
	{
/*		if (!VStr::ICmp(String, "Ammo type"))				weaponinfo[num].ammo 		= value;
		else if (!VStr::ICmp(String, "Deselect frame"))	weaponinfo[num].upstate		= value;
		else if (!VStr::ICmp(String, "Select frame"))  	weaponinfo[num].downstate 	= value;
		else if (!VStr::ICmp(String, "Bobbing frame")) 	weaponinfo[num].readystate	= value;
		else if (!VStr::ICmp(String, "Shooting frame"))	weaponinfo[num].atkstate 	= value;
		else if (!VStr::ICmp(String, "Firing frame"))  	weaponinfo[num].flashstate	= value;
		else */dprintf("WARNING! Invalid weapon param %s\n", String);
	}
}

//==========================================================================
//
//  ReadPointer
//
//==========================================================================

static void ReadPointer(int num)
{
	int		statenum = -1;
	int		i;
	int		j;

/*	for (i=0, j=0; i < NUMSTATES; i++)
	{
		if (functions[i])
		{
			if (j == num)
			{
				statenum = i;
				break;
			}
			j++;
		}
	}*/

	if (statenum == -1)
	{
		dprintf("WARNING! Invalid pointer\n");
		while (ParseParam());
		return;
	}

	while (ParseParam())
	{
/*		if (!VStr::ICmp(String, "Codep Frame"))	states[statenum].action_num = functions[value];
		else */dprintf("WARNING! Invalid pointer param %s\n", String);
	}
}

//==========================================================================
//
//  ReadCheats
//
//==========================================================================

static void ReadCheats(int)
{
	//	Old cheat handling is removed
	while (ParseParam());
}

//==========================================================================
//
//  ReadMisc
//
//==========================================================================

static void ReadMisc(int)
{
	//	Not handled yet
	while (ParseParam())
	{
		if (!VStr::ICmp(String, "Initial Health"))			initial_health = value;
		else if (!VStr::ICmp(String, "Initial Bullets"))	initial_ammo = value;
		else if (!VStr::ICmp(String, "Max Health"));
		else if (!VStr::ICmp(String, "Max Armor"));
		else if (!VStr::ICmp(String, "Green Armor Class"));
		else if (!VStr::ICmp(String, "Blue Armor Class"));
		else if (!VStr::ICmp(String, "Max Soulsphere"))	soulsphere_max = value;
		else if (!VStr::ICmp(String, "Soulsphere Health"))	soulsphere_health = value;
		else if (!VStr::ICmp(String, "Megasphere Health"))	megasphere_health = value;
		else if (!VStr::ICmp(String, "God Mode Health"))   god_health = value;
		else if (!VStr::ICmp(String, "IDFA Armor"));		//	Cheat removed
		else if (!VStr::ICmp(String, "IDFA Armor Class"));	//	Cheat removed
		else if (!VStr::ICmp(String, "IDKFA Armor"));		//	Cheat removed
		else if (!VStr::ICmp(String, "IDKFA Armor Class"));//	Cheat removed
		else if (!VStr::ICmp(String, "BFG Cells/Shot"))	bfg_cells = value;
		else if (!VStr::ICmp(String, "Monsters Infight"));	//	What's that?
		else dprintf("WARNING! Invalid misc %s\n", String);
	}
}

//==========================================================================
//
//	FindString
//
//==========================================================================

static void FindString(const char* oldStr, const char* newStr)
{
	//	Sounds
	bool SoundFound = false;
	for (int i = 0; i < SfxNames.Num(); i++)
	{
		if (SfxNames[i].Old == oldStr)
		{
			SfxNames[i].New = newStr;
			SfxNames[i].Replaced = true;
			//	Continue, because other sounds can use the same sound
			SoundFound = true;
		}
	}
	if (SoundFound)
	{
		return;
	}

	//	Music
	bool SongFound = false;
	for (int i = 0; i < MusicNames.Num(); i++)
	{
		if (MusicNames[i].Old == oldStr)
		{
			MusicNames[i].New = newStr;
			MusicNames[i].Replaced = true;
			//	There could be duplicates
			SongFound = true;
		}
	}
	if (SongFound)
	{
		return;
	}

	//	Sprite names
	for (int i = 0; i < SpriteNames.Num(); i++)
	{
		if (SpriteNames[i].Old == oldStr)
		{
			SpriteNames[i].New = newStr;
			SpriteNames[i].Replaced = true;
			return;
		}
	}

	VName Id = EngStrings->GetStringId(oldStr);
	if (Id != NAME_None)
	{
		GLanguage.ReplaceString(Id, newStr);
		return;
	}

	dprintf("Not found old \"%s\" new \"%s\"\n", oldStr, newStr);
}

//==========================================================================
//
//  ReadText
//
//==========================================================================

static void ReadText(int oldSize)
{
	char	*lenPtr;
	int		newSize;
	char	*oldStr;
	char	*newStr;
	int		len;

	lenPtr = strtok(NULL, " ");
	if (!lenPtr)
	{
		return;
	}
	newSize = atoi(lenPtr);

	oldStr = new char[oldSize + 1];
	newStr = new char[newSize + 1];

	len = 0;
	while (*PatchPtr && len < oldSize)
	{
		if (*PatchPtr == '\r')
		{
			PatchPtr++;
			continue;
		}
		oldStr[len] = *PatchPtr;
		PatchPtr++;
		len++;
	}
	oldStr[len] = 0;

	len = 0;
	while (*PatchPtr && len < newSize)
	{
		if (*PatchPtr == '\r')
		{
			PatchPtr++;
			continue;
		}
		newStr[len] = *PatchPtr;
		PatchPtr++;
		len++;
	}
	newStr[len] = 0;

	FindString(oldStr, newStr);

	delete[] oldStr;
	delete[] newStr;

	GetLine();
}

//==========================================================================
//
//  LoadDehackedFile
//
//==========================================================================

static void LoadDehackedFile(const char *filename)
{
	char*	Section;
	char*	numStr;
	int		i = 0;

	dprintf("Hacking %s\n", filename);

	FILE* f = fopen(filename, "rb");
	fseek(f, 0, SEEK_END);
	size_t len = ftell(f);
	fseek(f, 0, SEEK_SET);
	Patch = new char[len + 1];
	fread(Patch, 1, len, f);
	Patch[len] = 0;
	fclose(f);
	PatchPtr = Patch;

	GetLine();
	while (*PatchPtr)
	{
		Section = strtok(String, " ");
		if (!Section)
			continue;

		numStr = strtok(NULL, " ");
		if (numStr)
		{
			i = atoi(numStr);
		}

		if (!strcmp(Section, "Thing"))
		{
			ReadThing(i);
		}
		else if (!strcmp(Section, "Sound"))
		{
			ReadSound(i);
		}
		else if (!strcmp(Section, "Frame"))
		{
			ReadState(i);
		}
		else if (!strcmp(Section, "Sprite"))
		{
			ReadSpriteName(i);
		}
		else if (!strcmp(Section, "Ammo"))
		{
			ReadAmmo(i);
		}
		else if (!strcmp(Section, "Weapon"))
		{
			ReadWeapon(i);
		}
		else if (!strcmp(Section, "Pointer"))
		{
			ReadPointer(i);
		}
		else if (!strcmp(Section, "Cheat"))
		{
			ReadCheats(i);
		}
		else if (!strcmp(Section, "Misc"))
		{
			ReadMisc(i);
		}
		else if (!strcmp(Section, "Text"))
		{
			ReadText(i);
		}
		else
		{
			dprintf("Don't know how to handle \"%s\"\n", String);
			GetLine();
		}
	}
	delete[] Patch;
}

//==========================================================================
//
//  ProcessDehackedFiles
//
//==========================================================================

void ProcessDehackedFiles()
{
	int		p;
	int		i;

	p = GArgs.CheckParm("-deh");
	if (!p)
	{
		return;
	}

	GSoundManager->GetSoundLumpNames(SfxNames);
	P_GetMusicLumpNames(MusicNames);
	VClass::GetSpriteNames(SpriteNames);
	EngStrings = new VLanguage();
	EngStrings->LoadStrings("en");

	Hacked = true;

/*	functions = (int*)malloc(NUMSTATES * 4);
	for (i = 0; i < NUMSTATES; i++)
		functions[i] = states[i].action_num;*/

	while (++p != GArgs.Count() && GArgs[p][0] != '-')
	{
		LoadDehackedFile(GArgs[p]);
	}

	GSoundManager->ReplaceSoundLumpNames(SfxNames);
	P_ReplaceMusicLumpNames(MusicNames);
	VClass::ReplaceSpriteNames(SpriteNames);

	SfxNames.Clear();
	MusicNames.Clear();
	SpriteNames.Clear();
	delete EngStrings;

//	free(functions);
}

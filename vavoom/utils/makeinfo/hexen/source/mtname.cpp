//**************************************************************************
//**
//**	##   ##    ##    ##   ##   ####     ####   ###     ###
//**	##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**	 ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**	 ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**	  ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**	   #    ##    ##    #      ####     ####   ##       ##
//**
//**	$Id$
//**
//**	Copyright (C) 1999-2002 J�nis Legzdi��
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
//** Info.h
//**
//**************************************************************************

char* mt_names[] =
{
    "MT_MAPSPOT",
    "MT_MAPSPOTGRAVITY",
    "MT_FIREBALL1",
    "MT_ARROW",
    "MT_DART",
    "MT_POISONDART",
    "MT_RIPPERBALL",
    "MT_PROJECTILE_BLADE",
    "MT_ICESHARD",
    "MT_FLAME_SMALL_TEMP",
    "MT_FLAME_LARGE_TEMP",
    "MT_FLAME_SMALL",
    "MT_FLAME_LARGE",
    "MT_HEALINGBOTTLE",
    "MT_HEALTHFLASK",
    "MT_ARTIFLY",
    "MT_ARTIINVULNERABILITY",
    "MT_SUMMONMAULATOR",
    "MT_SUMMON_FX",
    "MT_THRUSTFLOOR_UP",
    "MT_THRUSTFLOOR_DOWN",
    "MT_TELEPORTOTHER",
    "MT_TELOTHER_FX1",
    "MT_TELOTHER_FX2",
    "MT_TELOTHER_FX3",
    "MT_TELOTHER_FX4",
    "MT_TELOTHER_FX5",
    "MT_DIRT1",
    "MT_DIRT2",
    "MT_DIRT3",
    "MT_DIRT4",
    "MT_DIRT5",
    "MT_DIRT6",
    "MT_DIRTCLUMP",
    "MT_ROCK1",
    "MT_ROCK2",
    "MT_ROCK3",
    "MT_FOGSPAWNER",
    "MT_FOGPATCHS",
    "MT_FOGPATCHM",
    "MT_FOGPATCHL",
    "MT_QUAKE_FOCUS",
    "MT_SGSHARD1",
    "MT_SGSHARD2",
    "MT_SGSHARD3",
    "MT_SGSHARD4",
    "MT_SGSHARD5",
    "MT_SGSHARD6",
    "MT_SGSHARD7",
    "MT_SGSHARD8",
    "MT_SGSHARD9",
    "MT_SGSHARD0",
    "MT_ARTIEGG",
    "MT_EGGFX",
    "MT_ARTISUPERHEAL",
    "MT_ZWINGEDSTATUENOSKULL",
    "MT_ZGEMPEDESTAL",
    "MT_ARTIPUZZSKULL",
    "MT_ARTIPUZZGEMBIG",
    "MT_ARTIPUZZGEMRED",
    "MT_ARTIPUZZGEMGREEN1",
    "MT_ARTIPUZZGEMGREEN2",
    "MT_ARTIPUZZGEMBLUE1",
    "MT_ARTIPUZZGEMBLUE2",
    "MT_ARTIPUZZBOOK1",
    "MT_ARTIPUZZBOOK2",
    "MT_ARTIPUZZSKULL2",
    "MT_ARTIPUZZFWEAPON",
    "MT_ARTIPUZZCWEAPON",
    "MT_ARTIPUZZMWEAPON",
    "MT_ARTIPUZZGEAR",
    "MT_ARTIPUZZGEAR2",
    "MT_ARTIPUZZGEAR3",
    "MT_ARTIPUZZGEAR4",
    "MT_ARTITORCH",
    "MT_FIREBOMB",
    "MT_ARTITELEPORT",
    "MT_ARTIPOISONBAG",
    "MT_POISONBAG",
    "MT_POISONCLOUD",
    "MT_THROWINGBOMB",
    "MT_SPEEDBOOTS",
    "MT_BOOSTMANA",
    "MT_BOOSTARMOR",
    "MT_BLASTRADIUS",
    "MT_HEALRADIUS",
    "MT_SPLASH",
    "MT_SPLASHBASE",
    "MT_LAVASPLASH",
    "MT_LAVASMOKE",
    "MT_SLUDGECHUNK",
    "MT_SLUDGESPLASH",
    "MT_MISC0",
    "MT_MISC1",
    "MT_MISC2",
    "MT_MISC3",
    "MT_MISC4",
    "MT_MISC5",
    "MT_MISC6",
    "MT_MISC7",
    "MT_MISC8",
    "MT_TREEDESTRUCTIBLE",
    "MT_MISC9",
    "MT_MISC10",
    "MT_MISC11",
    "MT_MISC12",
    "MT_MISC13",
    "MT_MISC14",
    "MT_MISC15",
    "MT_MISC16",
    "MT_MISC17",
    "MT_MISC18",
    "MT_MISC19",
    "MT_MISC20",
    "MT_MISC21",
    "MT_MISC22",
    "MT_MISC23",
    "MT_MISC24",
    "MT_MISC25",
    "MT_MISC26",
    "MT_MISC27",
    "MT_MISC28",
    "MT_MISC29",
    "MT_MISC30",
    "MT_MISC31",
    "MT_MISC32",
    "MT_MISC33",
    "MT_MISC34",
    "MT_MISC35",
    "MT_MISC36",
    "MT_MISC37",
    "MT_MISC38",
    "MT_MISC39",
    "MT_MISC40",
    "MT_MISC41",
    "MT_MISC42",
    "MT_MISC43",
    "MT_MISC44",
    "MT_MISC45",
    "MT_MISC46",
    "MT_MISC47",
    "MT_MISC48",
    "MT_MISC49",
    "MT_MISC50",
    "MT_MISC51",
    "MT_MISC52",
    "MT_MISC53",
    "MT_MISC54",
    "MT_MISC55",
    "MT_MISC56",
    "MT_MISC57",
    "MT_MISC58",
    "MT_MISC59",
    "MT_MISC60",
    "MT_MISC61",
    "MT_MISC62",
    "MT_MISC63",
    "MT_MISC64",
    "MT_MISC65",
    "MT_MISC66",
    "MT_MISC67",
    "MT_MISC68",
    "MT_MISC69",
    "MT_MISC70",
    "MT_MISC71",
    "MT_MISC72",
    "MT_MISC73",
    "MT_MISC74",
    "MT_MISC75",
    "MT_MISC76",
    "MT_POTTERY1",
    "MT_POTTERY2",
    "MT_POTTERY3",
    "MT_POTTERYBIT1",
    "MT_MISC77",
    "MT_ZLYNCHED_NOHEART",
    "MT_MISC78",
    "MT_CORPSEBIT",
    "MT_CORPSEBLOODDRIP",
    "MT_BLOODPOOL",
    "MT_MISC79",
    "MT_MISC80",
    "MT_LEAF1",
    "MT_LEAF2",
    "MT_ZTWINEDTORCH",
    "MT_ZTWINEDTORCH_UNLIT",
    "MT_BRIDGE",
    "MT_BRIDGEBALL",
    "MT_ZWALLTORCH",
    "MT_ZWALLTORCH_UNLIT",
    "MT_ZBARREL",
    "MT_ZSHRUB1",
    "MT_ZSHRUB2",
    "MT_ZBUCKET",
    "MT_ZPOISONSHROOM",
    "MT_ZFIREBULL",
    "MT_ZFIREBULL_UNLIT",
    "MT_FIRETHING",
    "MT_BRASSTORCH",
    "MT_ZSUITOFARMOR",
    "MT_ZARMORCHUNK",
    "MT_ZBELL",
    "MT_ZBLUE_CANDLE",
    "MT_ZIRON_MAIDEN",
    "MT_ZXMAS_TREE",
    "MT_ZCAULDRON",
    "MT_ZCAULDRON_UNLIT",
    "MT_ZCHAINBIT32",
    "MT_ZCHAINBIT64",
    "MT_ZCHAINEND_HEART",
    "MT_ZCHAINEND_HOOK1",
    "MT_ZCHAINEND_HOOK2",
    "MT_ZCHAINEND_SPIKE",
    "MT_ZCHAINEND_SKULL",
    "MT_TABLE_SHIT1",
    "MT_TABLE_SHIT2",
    "MT_TABLE_SHIT3",
    "MT_TABLE_SHIT4",
    "MT_TABLE_SHIT5",
    "MT_TABLE_SHIT6",
    "MT_TABLE_SHIT7",
    "MT_TABLE_SHIT8",
    "MT_TABLE_SHIT9",
    "MT_TABLE_SHIT10",
    "MT_TFOG",
    "MT_MISC81",
    "MT_TELEPORTMAN",
    "MT_PUNCHPUFF",
    "MT_FW_AXE",
    "MT_AXEPUFF",
    "MT_AXEPUFF_GLOW",
    "MT_AXEBLOOD",
    "MT_FW_HAMMER",
    "MT_HAMMER_MISSILE",
    "MT_HAMMERPUFF",
    "MT_FSWORD_MISSILE",
    "MT_FSWORD_FLAME",
    "MT_CW_SERPSTAFF",
    "MT_CSTAFF_MISSILE",
    "MT_CSTAFFPUFF",
    "MT_CW_FLAME",
    "MT_CFLAMEFLOOR",
    "MT_FLAMEPUFF",
    "MT_FLAMEPUFF2",
    "MT_CIRCLEFLAME",
    "MT_CFLAME_MISSILE",
    "MT_HOLY_FX",
    "MT_HOLY_TAIL",
    "MT_HOLY_PUFF",
    "MT_HOLY_MISSILE",
    "MT_HOLY_MISSILE_PUFF",
    "MT_MWANDPUFF",
    "MT_MWANDSMOKE",
    "MT_MWAND_MISSILE",
    "MT_MW_LIGHTNING",
    "MT_LIGHTNING_CEILING",
    "MT_LIGHTNING_FLOOR",
    "MT_LIGHTNING_ZAP",
    "MT_MSTAFF_FX",
    "MT_MSTAFF_FX2",
    "MT_FW_SWORD1",
    "MT_FW_SWORD2",
    "MT_FW_SWORD3",
    "MT_CW_HOLY1",
    "MT_CW_HOLY2",
    "MT_CW_HOLY3",
    "MT_MW_STAFF1",
    "MT_MW_STAFF2",
    "MT_MW_STAFF3",
    "MT_SNOUTPUFF",
    "MT_MW_CONE",
    "MT_SHARDFX1",
    "MT_BLOOD",
    "MT_BLOODSPLATTER",
    "MT_GIBS",
    "MT_PLAYER_FIGHTER",
    "MT_BLOODYSKULL",
    "MT_PLAYER_SPEED",
    "MT_ICECHUNK",
    "MT_PLAYER_CLERIC",
    "MT_PLAYER_MAGE",
    "MT_PIGPLAYER",
    "MT_PIG",
    "MT_CENTAUR",
    "MT_CENTAURLEADER",
    "MT_CENTAUR_FX",
    "MT_CENTAUR_SHIELD",
    "MT_CENTAUR_SWORD",
    "MT_DEMON",
    "MT_DEMONCHUNK1",
    "MT_DEMONCHUNK2",
    "MT_DEMONCHUNK3",
    "MT_DEMONCHUNK4",
    "MT_DEMONCHUNK5",
    "MT_DEMONFX1",
    "MT_DEMON2",
    "MT_DEMON2CHUNK1",
    "MT_DEMON2CHUNK2",
    "MT_DEMON2CHUNK3",
    "MT_DEMON2CHUNK4",
    "MT_DEMON2CHUNK5",
    "MT_DEMON2FX1",
    "MT_WRAITHB",
    "MT_WRAITH",
    "MT_WRAITHFX1",
    "MT_WRAITHFX2",
    "MT_WRAITHFX3",
    "MT_WRAITHFX4",
    "MT_WRAITHFX5",
    "MT_MINOTAUR",
    "MT_MNTRFX1",
    "MT_MNTRFX2",
    "MT_MNTRFX3",
    "MT_MNTRSMOKE",
    "MT_MNTRSMOKEEXIT",
    "MT_SERPENT",
    "MT_SERPENTLEADER",
    "MT_SERPENTFX",
    "MT_SERPENT_HEAD",
    "MT_SERPENT_GIB1",
    "MT_SERPENT_GIB2",
    "MT_SERPENT_GIB3",
    "MT_BISHOP",
    "MT_BISHOP_PUFF",
    "MT_BISHOPBLUR",
    "MT_BISHOPPAINBLUR",
    "MT_BISH_FX",
    "MT_DRAGON",
    "MT_DRAGON_FX",
    "MT_DRAGON_FX2",
    "MT_ARMOR_1",
    "MT_ARMOR_2",
    "MT_ARMOR_3",
    "MT_ARMOR_4",
    "MT_MANA1",
    "MT_MANA2",
    "MT_MANA3",
    "MT_KEY1",
    "MT_KEY2",
    "MT_KEY3",
    "MT_KEY4",
    "MT_KEY5",
    "MT_KEY6",
    "MT_KEY7",
    "MT_KEY8",
    "MT_KEY9",
    "MT_KEYA",
    "MT_KEYB",
    "MT_SOUNDWIND",
    "MT_SOUNDWATERFALL",
    "MT_ETTIN",
    "MT_ETTIN_MACE",
    "MT_FIREDEMON",
    "MT_FIREDEMON_SPLOTCH1",
    "MT_FIREDEMON_SPLOTCH2",
    "MT_FIREDEMON_FX1",
    "MT_FIREDEMON_FX2",
    "MT_FIREDEMON_FX3",
    "MT_FIREDEMON_FX4",
    "MT_FIREDEMON_FX5",
    "MT_FIREDEMON_FX6",
    "MT_ICEGUY",
    "MT_ICEGUY_FX",
    "MT_ICEFX_PUFF",
    "MT_ICEGUY_FX2",
    "MT_ICEGUY_BIT",
    "MT_ICEGUY_WISP1",
    "MT_ICEGUY_WISP2",
    "MT_FIGHTER_BOSS",
    "MT_CLERIC_BOSS",
    "MT_MAGE_BOSS",
    "MT_SORCBOSS",
    "MT_SORCBALL1",
    "MT_SORCBALL2",
    "MT_SORCBALL3",
    "MT_SORCFX1",
    "MT_SORCFX2",
    "MT_SORCFX2_T1",
    "MT_SORCFX3",
    "MT_SORCFX3_EXPLOSION",
    "MT_SORCFX4",
    "MT_SORCSPARK1",
    "MT_BLASTEFFECT",
    "MT_WATER_DRIP",
    "MT_KORAX",
    "MT_KORAX_SPIRIT1",
    "MT_KORAX_SPIRIT2",
    "MT_KORAX_SPIRIT3",
    "MT_KORAX_SPIRIT4",
    "MT_KORAX_SPIRIT5",
    "MT_KORAX_SPIRIT6",
    "MT_DEMON_MASH",
    "MT_DEMON2_MASH",
    "MT_ETTIN_MASH",
    "MT_CENTAUR_MASH",
    "MT_KORAX_BOLT",
    "MT_BAT_SPAWNER",
    "MT_BAT",
};

//**************************************************************************
//
//	$Log$
//	Revision 1.4  2002/01/07 12:30:06  dj_jl
//	Changed copyright year
//
//	Revision 1.3  2001/09/20 16:36:47  dj_jl
//	Beautification
//	
//	Revision 1.2  2001/07/27 14:27:55  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************

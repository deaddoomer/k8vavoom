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
//**	Copyright (C) 1999-2001 J�nis Legzdi��
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

enum
{
SPR2_IMPX = 0,
SPR2_ACLO,
SPR2_PTN1,
SPR2_SHLD,
SPR2_SHD2,
SPR2_BAGH,
SPR2_SPMP,
SPR2_INVS,
SPR2_PTN2,
SPR2_SOAR,
SPR2_INVU,
SPR2_PWBK,
SPR2_EGGC,
SPR2_EGGM,
SPR2_FX01,
SPR2_SPHL,
SPR2_TRCH,
SPR2_FBMB,
SPR2_XPL1,
SPR2_ATLP,
SPR2_PPOD,
SPR2_AMG1,
SPR2_SPSH,
SPR2_LVAS,
SPR2_SLDG,
SPR2_SKH1,
SPR2_SKH2,
SPR2_SKH3,
SPR2_SKH4,
SPR2_CHDL,
SPR2_SRTC,
SPR2_SMPL,
SPR2_STGS,
SPR2_STGL,
SPR2_STCS,
SPR2_STCL,
SPR2_KFR1,
SPR2_BARL,
SPR2_BRPL,
SPR2_MOS1,
SPR2_MOS2,
SPR2_WTRH,
SPR2_HCOR,
SPR2_KGZ1,
SPR2_KGZB,
SPR2_KGZG,
SPR2_KGZY,
SPR2_VLCO,
SPR2_VFBL,
SPR2_VTFB,
SPR2_SFFI,
SPR2_TGLT,
SPR2_TELE,
SPR2_STFF,
SPR2_PUF3,
SPR2_PUF4,
SPR2_BEAK,
SPR2_WGNT,
SPR2_GAUN,
SPR2_PUF1,
SPR2_WBLS,
SPR2_BLSR,
SPR2_FX18,
SPR2_FX17,
SPR2_WMCE,
SPR2_MACE,
SPR2_FX02,
SPR2_WSKL,
SPR2_HROD,
SPR2_FX00,
SPR2_FX20,
SPR2_FX21,
SPR2_FX22,
SPR2_FX23,
SPR2_GWND,
SPR2_PUF2,
SPR2_WPHX,
SPR2_PHNX,
SPR2_FX04,
SPR2_FX08,
SPR2_FX09,
SPR2_WBOW,
SPR2_CRBW,
SPR2_FX03,
SPR2_BLOD,
SPR2_PLAY,
SPR2_FDTH,
SPR2_BSKL,
SPR2_CHKN,
SPR2_MUMM,
SPR2_FX15,
SPR2_BEAS,
SPR2_FRB1,
SPR2_SNKE,
SPR2_SNFX,
SPR2_HEAD,
SPR2_FX05,
SPR2_FX06,
SPR2_FX07,
SPR2_CLNK,
SPR2_WZRD,
SPR2_FX11,
SPR2_FX10,
SPR2_KNIG,
SPR2_SPAX,
SPR2_RAXE,
SPR2_SRCR,
SPR2_FX14,
SPR2_SOR2,
SPR2_SDTH,
SPR2_FX16,
SPR2_MNTR,
SPR2_FX12,
SPR2_FX13,
SPR2_AKYY,
SPR2_BKYY,
SPR2_CKYY,
SPR2_AMG2,
SPR2_AMM1,
SPR2_AMM2,
SPR2_AMC1,
SPR2_AMC2,
SPR2_AMS1,
SPR2_AMS2,
SPR2_AMP1,
SPR2_AMP2,
SPR2_AMB1,
SPR2_AMB2,

NUMHERETICSPRITES
};

enum
{
	SA2_NULL,
    SA2_FreeTargMobj,
    SA2_RestoreSpecialThing1,
    SA2_RestoreSpecialThing2,
    SA2_HideThing,
    SA2_UnHideThing,
    SA2_RestoreArtifact,
    SA2_Scream,
    SA2_Explode,
    SA2_PodPain,
    SA2_RemovePod,
    SA2_MakePod,
    SA2_InitKeyGizmo,
    SA2_VolcanoSet,
    SA2_VolcanoBlast,
    SA2_BeastPuff,
    SA2_VolcBallImpact,
    SA2_SpawnTeleGlitter,
    SA2_SpawnTeleGlitter2,
    SA2_AccTeleGlitter,
    SA2_Light0,
    SA2_WeaponReady,
    SA2_Lower,
    SA2_Raise,
    SA2_StaffAttackPL1,
    SA2_ReFire,
    SA2_StaffAttackPL2,
    SA2_BeakReady,
    SA2_BeakRaise,
    SA2_BeakAttackPL1,
    SA2_BeakAttackPL2,
    SA2_GauntletAttack,
    SA2_FireBlasterPL1,
    SA2_FireBlasterPL2,
    SA2_SpawnRippers,
    SA2_FireMacePL1,
    SA2_FireMacePL2,
    SA2_MacePL1Check,
    SA2_MaceBallImpact,
    SA2_MaceBallImpact2,
    SA2_DeathBallImpact,
    SA2_FireSkullRodPL1,
    SA2_FireSkullRodPL2,
    SA2_SkullRodPL2Seek,
    SA2_AddPlayerRain,
    SA2_HideInCeiling,
    SA2_SkullRodStorm,
    SA2_RainImpact,
    SA2_FireGoldWandPL1,
    SA2_FireGoldWandPL2,
    SA2_FirePhoenixPL1,
    SA2_InitPhoenixPL2,
    SA2_FirePhoenixPL2,
    SA2_ShutdownPhoenixPL2,
    SA2_PhoenixPuff,
    SA2_FlameEnd,
    SA2_FloatPuff,
    SA2_FireCrossbowPL1,
    SA2_FireCrossbowPL2,
    SA2_BoltSpark,
    SA2_Pain,
    SA2_NoBlocking,
    SA2_AddPlayerCorpse,
    SA2_SkullPop,
    SA2_FlameSnd,
    SA2_CheckBurnGone,
    SA2_CheckSkullFloor,
    SA2_CheckSkullDone,
    SA2_Feathers,
    SA2_ChicLook,
    SA2_ChicChase,
    SA2_ChicPain,
    SA2_FaceTarget,
    SA2_ChicAttack,
    SA2_Look,
    SA2_Chase,
    SA2_MummyAttack,
    SA2_MummyAttack2,
    SA2_MummySoul,
    SA2_ContMobjSound,
    SA2_MummyFX1Seek,
    SA2_BeastAttack,
    SA2_SnakeAttack,
    SA2_SnakeAttack2,
    SA2_HeadAttack,
    SA2_BossDeath,
    SA2_HeadIceImpact,
    SA2_HeadFireGrow,
    SA2_WhirlwindSeek,
    SA2_ClinkAttack,
    SA2_WizAtk1,
    SA2_WizAtk2,
    SA2_WizAtk3,
    SA2_GhostOff,
    SA2_ImpMeAttack,
    SA2_ImpMsAttack,
    SA2_ImpMsAttack2,
    SA2_ImpDeath,
    SA2_ImpXDeath1,
    SA2_ImpXDeath2,
    SA2_ImpExplode,
    SA2_KnightAttack,
    SA2_DripBlood,
    SA2_Sor1Chase,
    SA2_Sor1Pain,
    SA2_Srcr1Attack,
    SA2_SorZap,
    SA2_SorcererRise,
    SA2_SorRise,
    SA2_SorSightSnd,
    SA2_Srcr2Decide,
    SA2_Srcr2Attack,
    SA2_Sor2DthInit,
    SA2_SorDSph,
    SA2_Sor2DthLoop,
    SA2_SorDExp,
    SA2_SorDBon,
    SA2_BlueSpark,
    SA2_GenWizard,
    SA2_MinotaurAtk1,
    SA2_MinotaurDecide,
    SA2_MinotaurAtk2,
    SA2_MinotaurAtk3,
    SA2_MinotaurCharge,
    SA2_MntrFloorFire,
    SA2_ESound,

    NUM_HERETIC_STATE_ACTIONS
};

enum
{
S2_NULL = 0,
S2_FREETARGMOBJ,
S2_ITEM_PTN1_1,
S2_ITEM_PTN1_2,
S2_ITEM_PTN1_3,
S2_ITEM_SHLD1,
S2_ITEM_SHD2_1,
S2_ITEM_BAGH1,
S2_ITEM_SPMP1,
S2_HIDESPECIAL1,
S2_HIDESPECIAL2,
S2_HIDESPECIAL3,
S2_HIDESPECIAL4,
S2_HIDESPECIAL5,
S2_HIDESPECIAL6,
S2_HIDESPECIAL7,
S2_HIDESPECIAL8,
S2_HIDESPECIAL9,
S2_HIDESPECIAL10,
S2_HIDESPECIAL11,
S2_DORMANTARTI1,
S2_DORMANTARTI2,
S2_DORMANTARTI3,
S2_DORMANTARTI4,
S2_DORMANTARTI5,
S2_DORMANTARTI6,
S2_DORMANTARTI7,
S2_DORMANTARTI8,
S2_DORMANTARTI9,
S2_DORMANTARTI10,
S2_DORMANTARTI11,
S2_DORMANTARTI12,
S2_DORMANTARTI13,
S2_DORMANTARTI14,
S2_DORMANTARTI15,
S2_DORMANTARTI16,
S2_DORMANTARTI17,
S2_DORMANTARTI18,
S2_DORMANTARTI19,
S2_DORMANTARTI20,
S2_DORMANTARTI21,
S2_DEADARTI1,
S2_DEADARTI2,
S2_DEADARTI3,
S2_DEADARTI4,
S2_DEADARTI5,
S2_DEADARTI6,
S2_DEADARTI7,
S2_DEADARTI8,
S2_DEADARTI9,
S2_DEADARTI10,
S2_ARTI_INVS1,
S2_ARTI_PTN2_1,
S2_ARTI_PTN2_2,
S2_ARTI_PTN2_3,
S2_ARTI_SOAR1,
S2_ARTI_SOAR2,
S2_ARTI_SOAR3,
S2_ARTI_SOAR4,
S2_ARTI_INVU1,
S2_ARTI_INVU2,
S2_ARTI_INVU3,
S2_ARTI_INVU4,
S2_ARTI_PWBK1,
S2_ARTI_EGGC1,
S2_ARTI_EGGC2,
S2_ARTI_EGGC3,
S2_ARTI_EGGC4,
S2_EGGFX1,
S2_EGGFX2,
S2_EGGFX3,
S2_EGGFX4,
S2_EGGFX5,
S2_EGGFXI1_1,
S2_EGGFXI1_2,
S2_EGGFXI1_3,
S2_EGGFXI1_4,
S2_ARTI_SPHL1,
S2_ARTI_TRCH1,
S2_ARTI_TRCH2,
S2_ARTI_TRCH3,
S2_ARTI_FBMB1,
S2_FIREBOMB1,
S2_FIREBOMB2,
S2_FIREBOMB3,
S2_FIREBOMB4,
S2_FIREBOMB5,
S2_FIREBOMB6,
S2_FIREBOMB7,
S2_FIREBOMB8,
S2_FIREBOMB9,
S2_FIREBOMB10,
S2_FIREBOMB11,
S2_ARTI_ATLP1,
S2_ARTI_ATLP2,
S2_ARTI_ATLP3,
S2_ARTI_ATLP4,
S2_POD_WAIT1,
S2_POD_PAIN1,
S2_POD_DIE1,
S2_POD_DIE2,
S2_POD_DIE3,
S2_POD_DIE4,
S2_POD_GROW1,
S2_POD_GROW2,
S2_POD_GROW3,
S2_POD_GROW4,
S2_POD_GROW5,
S2_POD_GROW6,
S2_POD_GROW7,
S2_POD_GROW8,
S2_PODGOO1,
S2_PODGOO2,
S2_PODGOOX,
S2_PODGENERATOR,
S2_SPLASH1,
S2_SPLASH2,
S2_SPLASH3,
S2_SPLASH4,
S2_SPLASHX,
S2_SPLASHBASE1,
S2_SPLASHBASE2,
S2_SPLASHBASE3,
S2_SPLASHBASE4,
S2_SPLASHBASE5,
S2_SPLASHBASE6,
S2_SPLASHBASE7,
S2_LAVASPLASH1,
S2_LAVASPLASH2,
S2_LAVASPLASH3,
S2_LAVASPLASH4,
S2_LAVASPLASH5,
S2_LAVASPLASH6,
S2_LAVASMOKE1,
S2_LAVASMOKE2,
S2_LAVASMOKE3,
S2_LAVASMOKE4,
S2_LAVASMOKE5,
S2_SLUDGECHUNK1,
S2_SLUDGECHUNK2,
S2_SLUDGECHUNK3,
S2_SLUDGECHUNK4,
S2_SLUDGECHUNKX,
S2_SLUDGESPLASH1,
S2_SLUDGESPLASH2,
S2_SLUDGESPLASH3,
S2_SLUDGESPLASH4,
S2_SKULLHANG70_1,
S2_SKULLHANG60_1,
S2_SKULLHANG45_1,
S2_SKULLHANG35_1,
S2_CHANDELIER1,
S2_CHANDELIER2,
S2_CHANDELIER3,
S2_SERPTORCH1,
S2_SERPTORCH2,
S2_SERPTORCH3,
S2_SMALLPILLAR,
S2_STALAGMITESMALL,
S2_STALAGMITELARGE,
S2_STALACTITESMALL,
S2_STALACTITELARGE,
S2_FIREBRAZIER1,
S2_FIREBRAZIER2,
S2_FIREBRAZIER3,
S2_FIREBRAZIER4,
S2_FIREBRAZIER5,
S2_FIREBRAZIER6,
S2_FIREBRAZIER7,
S2_FIREBRAZIER8,
S2_BARREL,
S2_BRPILLAR,
S2_MOSS1,
S2_MOSS2,
S2_WALLTORCH1,
S2_WALLTORCH2,
S2_WALLTORCH3,
S2_HANGINGCORPSE,
S2_KEYGIZMO1,
S2_KEYGIZMO2,
S2_KEYGIZMO3,
S2_KGZ_START,
S2_KGZ_BLUEFLOAT1,
S2_KGZ_GREENFLOAT1,
S2_KGZ_YELLOWFLOAT1,
S2_VOLCANO1,
S2_VOLCANO2,
S2_VOLCANO3,
S2_VOLCANO4,
S2_VOLCANO5,
S2_VOLCANO6,
S2_VOLCANO7,
S2_VOLCANO8,
S2_VOLCANO9,
S2_VOLCANOBALL1,
S2_VOLCANOBALL2,
S2_VOLCANOBALLX1,
S2_VOLCANOBALLX2,
S2_VOLCANOBALLX3,
S2_VOLCANOBALLX4,
S2_VOLCANOBALLX5,
S2_VOLCANOBALLX6,
S2_VOLCANOTBALL1,
S2_VOLCANOTBALL2,
S2_VOLCANOTBALLX1,
S2_VOLCANOTBALLX2,
S2_VOLCANOTBALLX3,
S2_VOLCANOTBALLX4,
S2_VOLCANOTBALLX5,
S2_VOLCANOTBALLX6,
S2_VOLCANOTBALLX7,
S2_TELEGLITGEN1,
S2_TELEGLITGEN2,
S2_TELEGLITTER1_1,
S2_TELEGLITTER1_2,
S2_TELEGLITTER1_3,
S2_TELEGLITTER1_4,
S2_TELEGLITTER1_5,
S2_TELEGLITTER2_1,
S2_TELEGLITTER2_2,
S2_TELEGLITTER2_3,
S2_TELEGLITTER2_4,
S2_TELEGLITTER2_5,
S2_TFOG1,
S2_TFOG2,
S2_TFOG3,
S2_TFOG4,
S2_TFOG5,
S2_TFOG6,
S2_TFOG7,
S2_TFOG8,
S2_TFOG9,
S2_TFOG10,
S2_TFOG11,
S2_TFOG12,
S2_TFOG13,
S2_LIGHTDONE,
S2_STAFFREADY,
S2_STAFFDOWN,
S2_STAFFUP,
S2_STAFFREADY2_1,
S2_STAFFREADY2_2,
S2_STAFFREADY2_3,
S2_STAFFDOWN2,
S2_STAFFUP2,
S2_STAFFATK1_1,
S2_STAFFATK1_2,
S2_STAFFATK1_3,
S2_STAFFATK2_1,
S2_STAFFATK2_2,
S2_STAFFATK2_3,
S2_STAFFPUFF1,
S2_STAFFPUFF2,
S2_STAFFPUFF3,
S2_STAFFPUFF4,
S2_STAFFPUFF2_1,
S2_STAFFPUFF2_2,
S2_STAFFPUFF2_3,
S2_STAFFPUFF2_4,
S2_STAFFPUFF2_5,
S2_STAFFPUFF2_6,
S2_BEAKREADY,
S2_BEAKDOWN,
S2_BEAKUP,
S2_BEAKATK1_1,
S2_BEAKATK2_1,
S2_WGNT,
S2_GAUNTLETREADY,
S2_GAUNTLETDOWN,
S2_GAUNTLETUP,
S2_GAUNTLETREADY2_1,
S2_GAUNTLETREADY2_2,
S2_GAUNTLETREADY2_3,
S2_GAUNTLETDOWN2,
S2_GAUNTLETUP2,
S2_GAUNTLETATK1_1,
S2_GAUNTLETATK1_2,
S2_GAUNTLETATK1_3,
S2_GAUNTLETATK1_4,
S2_GAUNTLETATK1_5,
S2_GAUNTLETATK1_6,
S2_GAUNTLETATK1_7,
S2_GAUNTLETATK2_1,
S2_GAUNTLETATK2_2,
S2_GAUNTLETATK2_3,
S2_GAUNTLETATK2_4,
S2_GAUNTLETATK2_5,
S2_GAUNTLETATK2_6,
S2_GAUNTLETATK2_7,
S2_GAUNTLETPUFF1_1,
S2_GAUNTLETPUFF1_2,
S2_GAUNTLETPUFF1_3,
S2_GAUNTLETPUFF1_4,
S2_GAUNTLETPUFF2_1,
S2_GAUNTLETPUFF2_2,
S2_GAUNTLETPUFF2_3,
S2_GAUNTLETPUFF2_4,
S2_BLSR,
S2_BLASTERREADY,
S2_BLASTERDOWN,
S2_BLASTERUP,
S2_BLASTERATK1_1,
S2_BLASTERATK1_2,
S2_BLASTERATK1_3,
S2_BLASTERATK1_4,
S2_BLASTERATK1_5,
S2_BLASTERATK1_6,
S2_BLASTERATK2_1,
S2_BLASTERATK2_2,
S2_BLASTERATK2_3,
S2_BLASTERATK2_4,
S2_BLASTERATK2_5,
S2_BLASTERATK2_6,
S2_BLASTERFX1_1,
S2_BLASTERFXI1_1,
S2_BLASTERFXI1_2,
S2_BLASTERFXI1_3,
S2_BLASTERFXI1_4,
S2_BLASTERFXI1_5,
S2_BLASTERFXI1_6,
S2_BLASTERFXI1_7,
S2_BLASTERSMOKE1,
S2_BLASTERSMOKE2,
S2_BLASTERSMOKE3,
S2_BLASTERSMOKE4,
S2_BLASTERSMOKE5,
S2_RIPPER1,
S2_RIPPER2,
S2_RIPPERX1,
S2_RIPPERX2,
S2_RIPPERX3,
S2_RIPPERX4,
S2_RIPPERX5,
S2_BLASTERPUFF1_1,
S2_BLASTERPUFF1_2,
S2_BLASTERPUFF1_3,
S2_BLASTERPUFF1_4,
S2_BLASTERPUFF1_5,
S2_BLASTERPUFF2_1,
S2_BLASTERPUFF2_2,
S2_BLASTERPUFF2_3,
S2_BLASTERPUFF2_4,
S2_BLASTERPUFF2_5,
S2_BLASTERPUFF2_6,
S2_BLASTERPUFF2_7,
S2_WMCE,
S2_MACEREADY,
S2_MACEDOWN,
S2_MACEUP,
S2_MACEATK1_1,
S2_MACEATK1_2,
S2_MACEATK1_3,
S2_MACEATK1_4,
S2_MACEATK1_5,
S2_MACEATK1_6,
S2_MACEATK1_7,
S2_MACEATK1_8,
S2_MACEATK1_9,
S2_MACEATK1_10,
S2_MACEATK2_1,
S2_MACEATK2_2,
S2_MACEATK2_3,
S2_MACEATK2_4,
S2_MACEFX1_1,
S2_MACEFX1_2,
S2_MACEFXI1_1,
S2_MACEFXI1_2,
S2_MACEFXI1_3,
S2_MACEFXI1_4,
S2_MACEFXI1_5,
S2_MACEFX2_1,
S2_MACEFX2_2,
S2_MACEFXI2_1,
S2_MACEFX3_1,
S2_MACEFX3_2,
S2_MACEFX4_1,
S2_MACEFXI4_1,
S2_WSKL,
S2_HORNRODREADY,
S2_HORNRODDOWN,
S2_HORNRODUP,
S2_HORNRODATK1_1,
S2_HORNRODATK1_2,
S2_HORNRODATK1_3,
S2_HORNRODATK2_1,
S2_HORNRODATK2_2,
S2_HORNRODATK2_3,
S2_HORNRODATK2_4,
S2_HORNRODATK2_5,
S2_HORNRODATK2_6,
S2_HORNRODATK2_7,
S2_HORNRODATK2_8,
S2_HORNRODATK2_9,
S2_HRODFX1_1,
S2_HRODFX1_2,
S2_HRODFXI1_1,
S2_HRODFXI1_2,
S2_HRODFXI1_3,
S2_HRODFXI1_4,
S2_HRODFXI1_5,
S2_HRODFXI1_6,
S2_HRODFX2_1,
S2_HRODFX2_2,
S2_HRODFX2_3,
S2_HRODFX2_4,
S2_HRODFXI2_1,
S2_HRODFXI2_2,
S2_HRODFXI2_3,
S2_HRODFXI2_4,
S2_HRODFXI2_5,
S2_HRODFXI2_6,
S2_HRODFXI2_7,
S2_HRODFXI2_8,
S2_RAINPLR1_1,
S2_RAINPLR2_1,
S2_RAINPLR3_1,
S2_RAINPLR4_1,
S2_RAINPLR1X_1,
S2_RAINPLR1X_2,
S2_RAINPLR1X_3,
S2_RAINPLR1X_4,
S2_RAINPLR1X_5,
S2_RAINPLR2X_1,
S2_RAINPLR2X_2,
S2_RAINPLR2X_3,
S2_RAINPLR2X_4,
S2_RAINPLR2X_5,
S2_RAINPLR3X_1,
S2_RAINPLR3X_2,
S2_RAINPLR3X_3,
S2_RAINPLR3X_4,
S2_RAINPLR3X_5,
S2_RAINPLR4X_1,
S2_RAINPLR4X_2,
S2_RAINPLR4X_3,
S2_RAINPLR4X_4,
S2_RAINPLR4X_5,
S2_RAINAIRXPLR1_1,
S2_RAINAIRXPLR2_1,
S2_RAINAIRXPLR3_1,
S2_RAINAIRXPLR4_1,
S2_RAINAIRXPLR1_2,
S2_RAINAIRXPLR2_2,
S2_RAINAIRXPLR3_2,
S2_RAINAIRXPLR4_2,
S2_RAINAIRXPLR1_3,
S2_RAINAIRXPLR2_3,
S2_RAINAIRXPLR3_3,
S2_RAINAIRXPLR4_3,
S2_GOLDWANDREADY,
S2_GOLDWANDDOWN,
S2_GOLDWANDUP,
S2_GOLDWANDATK1_1,
S2_GOLDWANDATK1_2,
S2_GOLDWANDATK1_3,
S2_GOLDWANDATK1_4,
S2_GOLDWANDATK2_1,
S2_GOLDWANDATK2_2,
S2_GOLDWANDATK2_3,
S2_GOLDWANDATK2_4,
S2_GWANDFX1_1,
S2_GWANDFX1_2,
S2_GWANDFXI1_1,
S2_GWANDFXI1_2,
S2_GWANDFXI1_3,
S2_GWANDFXI1_4,
S2_GWANDFX2_1,
S2_GWANDFX2_2,
S2_GWANDPUFF1_1,
S2_GWANDPUFF1_2,
S2_GWANDPUFF1_3,
S2_GWANDPUFF1_4,
S2_GWANDPUFF1_5,
S2_WPHX,
S2_PHOENIXREADY,
S2_PHOENIXDOWN,
S2_PHOENIXUP,
S2_PHOENIXATK1_1,
S2_PHOENIXATK1_2,
S2_PHOENIXATK1_3,
S2_PHOENIXATK1_4,
S2_PHOENIXATK1_5,
S2_PHOENIXATK2_1,
S2_PHOENIXATK2_2,
S2_PHOENIXATK2_3,
S2_PHOENIXATK2_4,
S2_PHOENIXFX1_1,
S2_PHOENIXFXI1_1,
S2_PHOENIXFXI1_2,
S2_PHOENIXFXI1_3,
S2_PHOENIXFXI1_4,
S2_PHOENIXFXI1_5,
S2_PHOENIXFXI1_6,
S2_PHOENIXFXI1_7,
S2_PHOENIXFXI1_8,
S2_PHOENIXPUFF1,
S2_PHOENIXPUFF2,
S2_PHOENIXPUFF3,
S2_PHOENIXPUFF4,
S2_PHOENIXPUFF5,
S2_PHOENIXFX2_1,
S2_PHOENIXFX2_2,
S2_PHOENIXFX2_3,
S2_PHOENIXFX2_4,
S2_PHOENIXFX2_5,
S2_PHOENIXFX2_6,
S2_PHOENIXFX2_7,
S2_PHOENIXFX2_8,
S2_PHOENIXFX2_9,
S2_PHOENIXFX2_10,
S2_PHOENIXFXI2_1,
S2_PHOENIXFXI2_2,
S2_PHOENIXFXI2_3,
S2_PHOENIXFXI2_4,
S2_PHOENIXFXI2_5,
S2_WBOW,
S2_CRBOW1,
S2_CRBOW2,
S2_CRBOW3,
S2_CRBOW4,
S2_CRBOW5,
S2_CRBOW6,
S2_CRBOW7,
S2_CRBOW8,
S2_CRBOW9,
S2_CRBOW10,
S2_CRBOW11,
S2_CRBOW12,
S2_CRBOW13,
S2_CRBOW14,
S2_CRBOW15,
S2_CRBOW16,
S2_CRBOW17,
S2_CRBOW18,
S2_CRBOWDOWN,
S2_CRBOWUP,
S2_CRBOWATK1_1,
S2_CRBOWATK1_2,
S2_CRBOWATK1_3,
S2_CRBOWATK1_4,
S2_CRBOWATK1_5,
S2_CRBOWATK1_6,
S2_CRBOWATK1_7,
S2_CRBOWATK1_8,
S2_CRBOWATK2_1,
S2_CRBOWATK2_2,
S2_CRBOWATK2_3,
S2_CRBOWATK2_4,
S2_CRBOWATK2_5,
S2_CRBOWATK2_6,
S2_CRBOWATK2_7,
S2_CRBOWATK2_8,
S2_CRBOWFX1,
S2_CRBOWFXI1_1,
S2_CRBOWFXI1_2,
S2_CRBOWFXI1_3,
S2_CRBOWFX2,
S2_CRBOWFX3,
S2_CRBOWFXI3_1,
S2_CRBOWFXI3_2,
S2_CRBOWFXI3_3,
S2_CRBOWFX4_1,
S2_CRBOWFX4_2,
S2_BLOOD1,
S2_BLOOD2,
S2_BLOOD3,
S2_BLOODSPLATTER1,
S2_BLOODSPLATTER2,
S2_BLOODSPLATTER3,
S2_BLOODSPLATTERX,
S2_PLAY,
S2_PLAY_RUN1,
S2_PLAY_RUN2,
S2_PLAY_RUN3,
S2_PLAY_RUN4,
S2_PLAY_ATK1,
S2_PLAY_ATK2,
S2_PLAY_PAIN,
S2_PLAY_PAIN2,
S2_PLAY_DIE1,
S2_PLAY_DIE2,
S2_PLAY_DIE3,
S2_PLAY_DIE4,
S2_PLAY_DIE5,
S2_PLAY_DIE6,
S2_PLAY_DIE7,
S2_PLAY_DIE8,
S2_PLAY_DIE9,
S2_PLAY_XDIE1,
S2_PLAY_XDIE2,
S2_PLAY_XDIE3,
S2_PLAY_XDIE4,
S2_PLAY_XDIE5,
S2_PLAY_XDIE6,
S2_PLAY_XDIE7,
S2_PLAY_XDIE8,
S2_PLAY_XDIE9,
S2_PLAY_FDTH1,
S2_PLAY_FDTH2,
S2_PLAY_FDTH3,
S2_PLAY_FDTH4,
S2_PLAY_FDTH5,
S2_PLAY_FDTH6,
S2_PLAY_FDTH7,
S2_PLAY_FDTH8,
S2_PLAY_FDTH9,
S2_PLAY_FDTH10,
S2_PLAY_FDTH11,
S2_PLAY_FDTH12,
S2_PLAY_FDTH13,
S2_PLAY_FDTH14,
S2_PLAY_FDTH15,
S2_PLAY_FDTH16,
S2_PLAY_FDTH17,
S2_PLAY_FDTH18,
S2_PLAY_FDTH19,
S2_PLAY_FDTH20,
S2_BLOODYSKULL1,
S2_BLOODYSKULL2,
S2_BLOODYSKULL3,
S2_BLOODYSKULL4,
S2_BLOODYSKULL5,
S2_BLOODYSKULLX1,
S2_BLOODYSKULLX2,
S2_CHICPLAY,
S2_CHICPLAY_RUN1,
S2_CHICPLAY_RUN2,
S2_CHICPLAY_RUN3,
S2_CHICPLAY_RUN4,
S2_CHICPLAY_ATK1,
S2_CHICPLAY_PAIN,
S2_CHICPLAY_PAIN2,
S2_CHICKEN_LOOK1,
S2_CHICKEN_LOOK2,
S2_CHICKEN_WALK1,
S2_CHICKEN_WALK2,
S2_CHICKEN_PAIN1,
S2_CHICKEN_PAIN2,
S2_CHICKEN_ATK1,
S2_CHICKEN_ATK2,
S2_CHICKEN_DIE1,
S2_CHICKEN_DIE2,
S2_CHICKEN_DIE3,
S2_CHICKEN_DIE4,
S2_CHICKEN_DIE5,
S2_CHICKEN_DIE6,
S2_CHICKEN_DIE7,
S2_CHICKEN_DIE8,
S2_FEATHER1,
S2_FEATHER2,
S2_FEATHER3,
S2_FEATHER4,
S2_FEATHER5,
S2_FEATHER6,
S2_FEATHER7,
S2_FEATHER8,
S2_FEATHERX,
S2_MUMMY_LOOK1,
S2_MUMMY_LOOK2,
S2_MUMMY_WALK1,
S2_MUMMY_WALK2,
S2_MUMMY_WALK3,
S2_MUMMY_WALK4,
S2_MUMMY_ATK1,
S2_MUMMY_ATK2,
S2_MUMMY_ATK3,
S2_MUMMYL_ATK1,
S2_MUMMYL_ATK2,
S2_MUMMYL_ATK3,
S2_MUMMYL_ATK4,
S2_MUMMYL_ATK5,
S2_MUMMYL_ATK6,
S2_MUMMY_PAIN1,
S2_MUMMY_PAIN2,
S2_MUMMY_DIE1,
S2_MUMMY_DIE2,
S2_MUMMY_DIE3,
S2_MUMMY_DIE4,
S2_MUMMY_DIE5,
S2_MUMMY_DIE6,
S2_MUMMY_DIE7,
S2_MUMMY_DIE8,
S2_MUMMY_SOUL1,
S2_MUMMY_SOUL2,
S2_MUMMY_SOUL3,
S2_MUMMY_SOUL4,
S2_MUMMY_SOUL5,
S2_MUMMY_SOUL6,
S2_MUMMY_SOUL7,
S2_MUMMYFX1_1,
S2_MUMMYFX1_2,
S2_MUMMYFX1_3,
S2_MUMMYFX1_4,
S2_MUMMYFXI1_1,
S2_MUMMYFXI1_2,
S2_MUMMYFXI1_3,
S2_MUMMYFXI1_4,
S2_BEAST_LOOK1,
S2_BEAST_LOOK2,
S2_BEAST_WALK1,
S2_BEAST_WALK2,
S2_BEAST_WALK3,
S2_BEAST_WALK4,
S2_BEAST_WALK5,
S2_BEAST_WALK6,
S2_BEAST_ATK1,
S2_BEAST_ATK2,
S2_BEAST_PAIN1,
S2_BEAST_PAIN2,
S2_BEAST_DIE1,
S2_BEAST_DIE2,
S2_BEAST_DIE3,
S2_BEAST_DIE4,
S2_BEAST_DIE5,
S2_BEAST_DIE6,
S2_BEAST_DIE7,
S2_BEAST_DIE8,
S2_BEAST_DIE9,
S2_BEAST_XDIE1,
S2_BEAST_XDIE2,
S2_BEAST_XDIE3,
S2_BEAST_XDIE4,
S2_BEAST_XDIE5,
S2_BEAST_XDIE6,
S2_BEAST_XDIE7,
S2_BEAST_XDIE8,
S2_BEASTBALL1,
S2_BEASTBALL2,
S2_BEASTBALL3,
S2_BEASTBALL4,
S2_BEASTBALL5,
S2_BEASTBALL6,
S2_BEASTBALLX1,
S2_BEASTBALLX2,
S2_BEASTBALLX3,
S2_BEASTBALLX4,
S2_BEASTBALLX5,
S2_BURNBALL1,
S2_BURNBALL2,
S2_BURNBALL3,
S2_BURNBALL4,
S2_BURNBALL5,
S2_BURNBALL6,
S2_BURNBALL7,
S2_BURNBALL8,
S2_BURNBALLFB1,
S2_BURNBALLFB2,
S2_BURNBALLFB3,
S2_BURNBALLFB4,
S2_BURNBALLFB5,
S2_BURNBALLFB6,
S2_BURNBALLFB7,
S2_BURNBALLFB8,
S2_PUFFY1,
S2_PUFFY2,
S2_PUFFY3,
S2_PUFFY4,
S2_PUFFY5,
S2_SNAKE_LOOK1,
S2_SNAKE_LOOK2,
S2_SNAKE_WALK1,
S2_SNAKE_WALK2,
S2_SNAKE_WALK3,
S2_SNAKE_WALK4,
S2_SNAKE_ATK1,
S2_SNAKE_ATK2,
S2_SNAKE_ATK3,
S2_SNAKE_ATK4,
S2_SNAKE_ATK5,
S2_SNAKE_ATK6,
S2_SNAKE_ATK7,
S2_SNAKE_ATK8,
S2_SNAKE_ATK9,
S2_SNAKE_PAIN1,
S2_SNAKE_PAIN2,
S2_SNAKE_DIE1,
S2_SNAKE_DIE2,
S2_SNAKE_DIE3,
S2_SNAKE_DIE4,
S2_SNAKE_DIE5,
S2_SNAKE_DIE6,
S2_SNAKE_DIE7,
S2_SNAKE_DIE8,
S2_SNAKE_DIE9,
S2_SNAKE_DIE10,
S2_SNAKEPRO_A1,
S2_SNAKEPRO_A2,
S2_SNAKEPRO_A3,
S2_SNAKEPRO_A4,
S2_SNAKEPRO_AX1,
S2_SNAKEPRO_AX2,
S2_SNAKEPRO_AX3,
S2_SNAKEPRO_AX4,
S2_SNAKEPRO_AX5,
S2_SNAKEPRO_B1,
S2_SNAKEPRO_B2,
S2_SNAKEPRO_BX1,
S2_SNAKEPRO_BX2,
S2_SNAKEPRO_BX3,
S2_SNAKEPRO_BX4,
S2_HEAD_LOOK,
S2_HEAD_FLOAT,
S2_HEAD_ATK1,
S2_HEAD_ATK2,
S2_HEAD_PAIN1,
S2_HEAD_PAIN2,
S2_HEAD_DIE1,
S2_HEAD_DIE2,
S2_HEAD_DIE3,
S2_HEAD_DIE4,
S2_HEAD_DIE5,
S2_HEAD_DIE6,
S2_HEAD_DIE7,
S2_HEADFX1_1,
S2_HEADFX1_2,
S2_HEADFX1_3,
S2_HEADFXI1_1,
S2_HEADFXI1_2,
S2_HEADFXI1_3,
S2_HEADFXI1_4,
S2_HEADFX2_1,
S2_HEADFX2_2,
S2_HEADFX2_3,
S2_HEADFXI2_1,
S2_HEADFXI2_2,
S2_HEADFXI2_3,
S2_HEADFXI2_4,
S2_HEADFX3_1,
S2_HEADFX3_2,
S2_HEADFX3_3,
S2_HEADFX3_4,
S2_HEADFX3_5,
S2_HEADFX3_6,
S2_HEADFXI3_1,
S2_HEADFXI3_2,
S2_HEADFXI3_3,
S2_HEADFXI3_4,
S2_HEADFX4_1,
S2_HEADFX4_2,
S2_HEADFX4_3,
S2_HEADFX4_4,
S2_HEADFX4_5,
S2_HEADFX4_6,
S2_HEADFX4_7,
S2_HEADFXI4_1,
S2_HEADFXI4_2,
S2_HEADFXI4_3,
S2_HEADFXI4_4,
S2_CLINK_LOOK1,
S2_CLINK_LOOK2,
S2_CLINK_WALK1,
S2_CLINK_WALK2,
S2_CLINK_WALK3,
S2_CLINK_WALK4,
S2_CLINK_ATK1,
S2_CLINK_ATK2,
S2_CLINK_ATK3,
S2_CLINK_PAIN1,
S2_CLINK_PAIN2,
S2_CLINK_DIE1,
S2_CLINK_DIE2,
S2_CLINK_DIE3,
S2_CLINK_DIE4,
S2_CLINK_DIE5,
S2_CLINK_DIE6,
S2_CLINK_DIE7,
S2_WIZARD_LOOK1,
S2_WIZARD_LOOK2,
S2_WIZARD_WALK1,
S2_WIZARD_WALK2,
S2_WIZARD_WALK3,
S2_WIZARD_WALK4,
S2_WIZARD_WALK5,
S2_WIZARD_WALK6,
S2_WIZARD_WALK7,
S2_WIZARD_WALK8,
S2_WIZARD_ATK1,
S2_WIZARD_ATK2,
S2_WIZARD_ATK3,
S2_WIZARD_ATK4,
S2_WIZARD_ATK5,
S2_WIZARD_ATK6,
S2_WIZARD_ATK7,
S2_WIZARD_ATK8,
S2_WIZARD_ATK9,
S2_WIZARD_PAIN1,
S2_WIZARD_PAIN2,
S2_WIZARD_DIE1,
S2_WIZARD_DIE2,
S2_WIZARD_DIE3,
S2_WIZARD_DIE4,
S2_WIZARD_DIE5,
S2_WIZARD_DIE6,
S2_WIZARD_DIE7,
S2_WIZARD_DIE8,
S2_WIZFX1_1,
S2_WIZFX1_2,
S2_WIZFXI1_1,
S2_WIZFXI1_2,
S2_WIZFXI1_3,
S2_WIZFXI1_4,
S2_WIZFXI1_5,
S2_IMP_LOOK1,
S2_IMP_LOOK2,
S2_IMP_LOOK3,
S2_IMP_LOOK4,
S2_IMP_FLY1,
S2_IMP_FLY2,
S2_IMP_FLY3,
S2_IMP_FLY4,
S2_IMP_FLY5,
S2_IMP_FLY6,
S2_IMP_FLY7,
S2_IMP_FLY8,
S2_IMP_MEATK1,
S2_IMP_MEATK2,
S2_IMP_MEATK3,
S2_IMP_MSATK1_1,
S2_IMP_MSATK1_2,
S2_IMP_MSATK1_3,
S2_IMP_MSATK1_4,
S2_IMP_MSATK1_5,
S2_IMP_MSATK1_6,
S2_IMP_MSATK2_1,
S2_IMP_MSATK2_2,
S2_IMP_MSATK2_3,
S2_IMP_PAIN1,
S2_IMP_PAIN2,
S2_IMP_DIE1,
S2_IMP_DIE2,
S2_IMP_XDIE1,
S2_IMP_XDIE2,
S2_IMP_XDIE3,
S2_IMP_XDIE4,
S2_IMP_XDIE5,
S2_IMP_CRASH1,
S2_IMP_CRASH2,
S2_IMP_CRASH3,
S2_IMP_CRASH4,
S2_IMP_XCRASH1,
S2_IMP_XCRASH2,
S2_IMP_XCRASH3,
S2_IMP_CHUNKA1,
S2_IMP_CHUNKA2,
S2_IMP_CHUNKA3,
S2_IMP_CHUNKB1,
S2_IMP_CHUNKB2,
S2_IMP_CHUNKB3,
S2_IMPFX1,
S2_IMPFX2,
S2_IMPFX3,
S2_IMPFXI1,
S2_IMPFXI2,
S2_IMPFXI3,
S2_IMPFXI4,
S2_KNIGHT_STND1,
S2_KNIGHT_STND2,
S2_KNIGHT_WALK1,
S2_KNIGHT_WALK2,
S2_KNIGHT_WALK3,
S2_KNIGHT_WALK4,
S2_KNIGHT_ATK1,
S2_KNIGHT_ATK2,
S2_KNIGHT_ATK3,
S2_KNIGHT_ATK4,
S2_KNIGHT_ATK5,
S2_KNIGHT_ATK6,
S2_KNIGHT_PAIN1,
S2_KNIGHT_PAIN2,
S2_KNIGHT_DIE1,
S2_KNIGHT_DIE2,
S2_KNIGHT_DIE3,
S2_KNIGHT_DIE4,
S2_KNIGHT_DIE5,
S2_KNIGHT_DIE6,
S2_KNIGHT_DIE7,
S2_SPINAXE1,
S2_SPINAXE2,
S2_SPINAXE3,
S2_SPINAXEX1,
S2_SPINAXEX2,
S2_SPINAXEX3,
S2_REDAXE1,
S2_REDAXE2,
S2_REDAXEX1,
S2_REDAXEX2,
S2_REDAXEX3,
S2_SRCR1_LOOK1,
S2_SRCR1_LOOK2,
S2_SRCR1_WALK1,
S2_SRCR1_WALK2,
S2_SRCR1_WALK3,
S2_SRCR1_WALK4,
S2_SRCR1_PAIN1,
S2_SRCR1_ATK1,
S2_SRCR1_ATK2,
S2_SRCR1_ATK3,
S2_SRCR1_ATK4,
S2_SRCR1_ATK5,
S2_SRCR1_ATK6,
S2_SRCR1_ATK7,
S2_SRCR1_DIE1,
S2_SRCR1_DIE2,
S2_SRCR1_DIE3,
S2_SRCR1_DIE4,
S2_SRCR1_DIE5,
S2_SRCR1_DIE6,
S2_SRCR1_DIE7,
S2_SRCR1_DIE8,
S2_SRCR1_DIE9,
S2_SRCR1_DIE10,
S2_SRCR1_DIE11,
S2_SRCR1_DIE12,
S2_SRCR1_DIE13,
S2_SRCR1_DIE14,
S2_SRCR1_DIE15,
S2_SRCR1_DIE16,
S2_SRCR1_DIE17,
S2_SRCRFX1_1,
S2_SRCRFX1_2,
S2_SRCRFX1_3,
S2_SRCRFXI1_1,
S2_SRCRFXI1_2,
S2_SRCRFXI1_3,
S2_SRCRFXI1_4,
S2_SRCRFXI1_5,
S2_SOR2_RISE1,
S2_SOR2_RISE2,
S2_SOR2_RISE3,
S2_SOR2_RISE4,
S2_SOR2_RISE5,
S2_SOR2_RISE6,
S2_SOR2_RISE7,
S2_SOR2_LOOK1,
S2_SOR2_LOOK2,
S2_SOR2_WALK1,
S2_SOR2_WALK2,
S2_SOR2_WALK3,
S2_SOR2_WALK4,
S2_SOR2_PAIN1,
S2_SOR2_PAIN2,
S2_SOR2_ATK1,
S2_SOR2_ATK2,
S2_SOR2_ATK3,
S2_SOR2_TELE1,
S2_SOR2_TELE2,
S2_SOR2_TELE3,
S2_SOR2_TELE4,
S2_SOR2_TELE5,
S2_SOR2_TELE6,
S2_SOR2_DIE1,
S2_SOR2_DIE2,
S2_SOR2_DIE3,
S2_SOR2_DIE4,
S2_SOR2_DIE5,
S2_SOR2_DIE6,
S2_SOR2_DIE7,
S2_SOR2_DIE8,
S2_SOR2_DIE9,
S2_SOR2_DIE10,
S2_SOR2_DIE11,
S2_SOR2_DIE12,
S2_SOR2_DIE13,
S2_SOR2_DIE14,
S2_SOR2_DIE15,
S2_SOR2FX1_1,
S2_SOR2FX1_2,
S2_SOR2FX1_3,
S2_SOR2FXI1_1,
S2_SOR2FXI1_2,
S2_SOR2FXI1_3,
S2_SOR2FXI1_4,
S2_SOR2FXI1_5,
S2_SOR2FXI1_6,
S2_SOR2FXSPARK1,
S2_SOR2FXSPARK2,
S2_SOR2FXSPARK3,
S2_SOR2FX2_1,
S2_SOR2FX2_2,
S2_SOR2FX2_3,
S2_SOR2FXI2_1,
S2_SOR2FXI2_2,
S2_SOR2FXI2_3,
S2_SOR2FXI2_4,
S2_SOR2FXI2_5,
S2_SOR2TELEFADE1,
S2_SOR2TELEFADE2,
S2_SOR2TELEFADE3,
S2_SOR2TELEFADE4,
S2_SOR2TELEFADE5,
S2_SOR2TELEFADE6,
S2_MNTR_LOOK1,
S2_MNTR_LOOK2,
S2_MNTR_WALK1,
S2_MNTR_WALK2,
S2_MNTR_WALK3,
S2_MNTR_WALK4,
S2_MNTR_ATK1_1,
S2_MNTR_ATK1_2,
S2_MNTR_ATK1_3,
S2_MNTR_ATK2_1,
S2_MNTR_ATK2_2,
S2_MNTR_ATK2_3,
S2_MNTR_ATK3_1,
S2_MNTR_ATK3_2,
S2_MNTR_ATK3_3,
S2_MNTR_ATK3_4,
S2_MNTR_ATK4_1,
S2_MNTR_PAIN1,
S2_MNTR_PAIN2,
S2_MNTR_DIE1,
S2_MNTR_DIE2,
S2_MNTR_DIE3,
S2_MNTR_DIE4,
S2_MNTR_DIE5,
S2_MNTR_DIE6,
S2_MNTR_DIE7,
S2_MNTR_DIE8,
S2_MNTR_DIE9,
S2_MNTR_DIE10,
S2_MNTR_DIE11,
S2_MNTR_DIE12,
S2_MNTR_DIE13,
S2_MNTR_DIE14,
S2_MNTR_DIE15,
S2_MNTRFX1_1,
S2_MNTRFX1_2,
S2_MNTRFXI1_1,
S2_MNTRFXI1_2,
S2_MNTRFXI1_3,
S2_MNTRFXI1_4,
S2_MNTRFXI1_5,
S2_MNTRFXI1_6,
S2_MNTRFX2_1,
S2_MNTRFXI2_1,
S2_MNTRFXI2_2,
S2_MNTRFXI2_3,
S2_MNTRFXI2_4,
S2_MNTRFXI2_5,
S2_MNTRFX3_1,
S2_MNTRFX3_2,
S2_MNTRFX3_3,
S2_MNTRFX3_4,
S2_MNTRFX3_5,
S2_MNTRFX3_6,
S2_MNTRFX3_7,
S2_MNTRFX3_8,
S2_MNTRFX3_9,
S2_AKYY1,
S2_AKYY2,
S2_AKYY3,
S2_AKYY4,
S2_AKYY5,
S2_AKYY6,
S2_AKYY7,
S2_AKYY8,
S2_AKYY9,
S2_AKYY10,
S2_BKYY1,
S2_BKYY2,
S2_BKYY3,
S2_BKYY4,
S2_BKYY5,
S2_BKYY6,
S2_BKYY7,
S2_BKYY8,
S2_BKYY9,
S2_BKYY10,
S2_CKYY1,
S2_CKYY2,
S2_CKYY3,
S2_CKYY4,
S2_CKYY5,
S2_CKYY6,
S2_CKYY7,
S2_CKYY8,
S2_CKYY9,
S2_AMG1,
S2_AMG2_1,
S2_AMG2_2,
S2_AMG2_3,
S2_AMM1,
S2_AMM2,
S2_AMC1,
S2_AMC2_1,
S2_AMC2_2,
S2_AMC2_3,
S2_AMS1_1,
S2_AMS1_2,
S2_AMS2_1,
S2_AMS2_2,
S2_AMP1_1,
S2_AMP1_2,
S2_AMP1_3,
S2_AMP2_1,
S2_AMP2_2,
S2_AMP2_3,
S2_AMB1_1,
S2_AMB1_2,
S2_AMB1_3,
S2_AMB2_1,
S2_AMB2_2,
S2_AMB2_3,
S2_SND_WIND,
S2_SND_WATERFALL,

NUMHERETICSTATES
};

enum
{
MT2_MISC0 = 0,
MT2_ITEMSHIELD1,
MT2_ITEMSHIELD2,
MT2_MISC1,
MT2_MISC2,
MT2_ARTIINVISIBILITY,
MT2_MISC3,
MT2_ARTIFLY,
MT2_ARTIINVULNERABILITY,
MT2_ARTITOMEOFPOWER,
MT2_ARTIEGG,
MT2_EGGFX,
MT2_ARTISUPERHEAL,
MT2_MISC4,
MT2_MISC5,
MT2_FIREBOMB,
MT2_ARTITELEPORT,
MT2_POD,
MT2_PODGOO,
MT2_PODGENERATOR,
MT2_SPLASH,
MT2_SPLASHBASE,
MT2_LAVASPLASH,
MT2_LAVASMOKE,
MT2_SLUDGECHUNK,
MT2_SLUDGESPLASH,
MT2_SKULLHANG70,
MT2_SKULLHANG60,
MT2_SKULLHANG45,
MT2_SKULLHANG35,
MT2_CHANDELIER,
MT2_SERPTORCH,
MT2_SMALLPILLAR,
MT2_STALAGMITESMALL,
MT2_STALAGMITELARGE,
MT2_STALACTITESMALL,
MT2_STALACTITELARGE,
MT2_MISC6,
MT2_BARREL,
MT2_MISC7,
MT2_MISC8,
MT2_MISC9,
MT2_MISC10,
MT2_MISC11,
MT2_KEYGIZMOBLUE,
MT2_KEYGIZMOGREEN,
MT2_KEYGIZMOYELLOW,
MT2_KEYGIZMOFLOAT,
MT2_MISC12,
MT2_VOLCANOBLAST,
MT2_VOLCANOTBLAST,
MT2_TELEGLITGEN,
MT2_TELEGLITGEN2,
MT2_TELEGLITTER,
MT2_TELEGLITTER2,
MT2_TFOG,
MT2_TELEPORTMAN,
MT2_STAFFPUFF,
MT2_STAFFPUFF2,
MT2_BEAKPUFF,
MT2_MISC13,
MT2_GAUNTLETPUFF1,
MT2_GAUNTLETPUFF2,
MT2_MISC14,
MT2_BLASTERFX1,
MT2_BLASTERSMOKE,
MT2_RIPPER,
MT2_BLASTERPUFF1,
MT2_BLASTERPUFF2,
MT2_WMACE,
MT2_MACEFX1,
MT2_MACEFX2,
MT2_MACEFX3,
MT2_MACEFX4,
MT2_WSKULLROD,
MT2_HORNRODFX1,
MT2_HORNRODFX2,
MT2_RAINPLR1,
MT2_RAINPLR2,
MT2_RAINPLR3,
MT2_RAINPLR4,
MT2_GOLDWANDFX1,
MT2_GOLDWANDFX2,
MT2_GOLDWANDPUFF1,
MT2_GOLDWANDPUFF2,
MT2_WPHOENIXROD,
MT2_PHOENIXFX1,
MT2_PHOENIXPUFF,
MT2_PHOENIXFX2,
MT2_MISC15,
MT2_CRBOWFX1,
MT2_CRBOWFX2,
MT2_CRBOWFX3,
MT2_CRBOWFX4,
MT2_BLOOD,
MT2_BLOODSPLATTER,
MT2_PLAYER,
MT2_BLOODYSKULL,
MT2_CHICPLAYER,
MT2_CHICKEN,
MT2_FEATHER,
MT2_MUMMY,
MT2_MUMMYLEADER,
MT2_MUMMYGHOST,
MT2_MUMMYLEADERGHOST,
MT2_MUMMYSOUL,
MT2_MUMMYFX1,
MT2_BEAST,
MT2_BEASTBALL,
MT2_BURNBALL,
MT2_BURNBALLFB,
MT2_PUFFY,
MT2_SNAKE,
MT2_SNAKEPRO_A,
MT2_SNAKEPRO_B,
MT2_HEAD,
MT2_HEADFX1,
MT2_HEADFX2,
MT2_HEADFX3,
MT2_WHIRLWIND,
MT2_CLINK,
MT2_WIZARD,
MT2_WIZFX1,
MT2_IMP,
MT2_IMPLEADER,
MT2_IMPCHUNK1,
MT2_IMPCHUNK2,
MT2_IMPBALL,
MT2_KNIGHT,
MT2_KNIGHTGHOST,
MT2_KNIGHTAXE,
MT2_REDAXE,
MT2_SORCERER1,
MT2_SRCRFX1,
MT2_SORCERER2,
MT2_SOR2FX1,
MT2_SOR2FXSPARK,
MT2_SOR2FX2,
MT2_SOR2TELEFADE,
MT2_MINOTAUR,
MT2_MNTRFX1,
MT2_MNTRFX2,
MT2_MNTRFX3,
MT2_AKYY,
MT2_BKYY,
MT2_CKEY,
MT2_AMGWNDWIMPY,
MT2_AMGWNDHEFTY,
MT2_AMMACEWIMPY,
MT2_AMMACEHEFTY,
MT2_AMCBOWWIMPY,
MT2_AMCBOWHEFTY,
MT2_AMSKRDWIMPY,
MT2_AMSKRDHEFTY,
MT2_AMPHRDWIMPY,
MT2_AMPHRDHEFTY,
MT2_AMBLSRWIMPY,
MT2_AMBLSRHEFTY,
MT2_SOUNDWIND,
MT2_SOUNDWATERFALL,

NUMHERETICMOBJTYPES
};

enum
{
	sfx2_None = 0,
	sfx2_gldhit,
	sfx2_gntful,
	sfx2_gnthit,
	sfx2_gntpow,
	sfx2_gntact,
	sfx2_gntuse,
	sfx2_phosht,
	sfx2_phohit,
	sfx2_phopow,
	sfx2_lobsht,
	sfx2_lobhit,
	sfx2_lobpow,
	sfx2_hrnsht,
	sfx2_hrnhit,
	sfx2_hrnpow,
	sfx2_ramphit,
	sfx2_ramrain,
	sfx2_bowsht,
	sfx2_stfhit,
	sfx2_stfpow,
	sfx2_stfcrk,
	sfx2_impsit,
	sfx2_impat1,
	sfx2_impat2,
	sfx2_impdth,
	sfx2_impact,
	sfx2_imppai,
	sfx2_mumsit,
	sfx2_mumat1,
	sfx2_mumat2,
	sfx2_mumdth,
	sfx2_mumact,
	sfx2_mumpai,
	sfx2_mumhed,
	sfx2_bstsit,
	sfx2_bstatk,
	sfx2_bstdth,
	sfx2_bstact,
	sfx2_bstpai,
	sfx2_clksit,
	sfx2_clkatk,
	sfx2_clkdth,
	sfx2_clkact,
	sfx2_clkpai,
	sfx2_snksit,
	sfx2_snkatk,
	sfx2_snkdth,
	sfx2_snkact,
	sfx2_snkpai,
	sfx2_kgtsit,
	sfx2_kgtatk,
	sfx2_kgtat2,
	sfx2_kgtdth,
	sfx2_kgtact,
	sfx2_kgtpai,
	sfx2_wizsit,
	sfx2_wizatk,
	sfx2_wizdth,
	sfx2_wizact,
	sfx2_wizpai,
	sfx2_minsit,
	sfx2_minat1,
	sfx2_minat2,
	sfx2_minat3,
	sfx2_mindth,
	sfx2_minact,
	sfx2_minpai,
	sfx2_hedsit,
	sfx2_hedat1,
	sfx2_hedat2,
	sfx2_hedat3,
	sfx2_heddth,
	sfx2_hedact,
	sfx2_hedpai,
	sfx2_sorzap,
	sfx2_sorrise,
	sfx2_sorsit,
	sfx2_soratk,
	sfx2_soract,
	sfx2_sorpai,
	sfx2_sordsph,
	sfx2_sordexp,
	sfx2_sordbon,
	sfx2_sbtsit,
	sfx2_sbtatk,
	sfx2_sbtdth,
	sfx2_sbtact,
	sfx2_sbtpai,
	sfx2_plroof,
	sfx2_plrpai,
	sfx2_plrdth,		// Normal
	sfx2_gibdth,		// Extreme
	sfx2_plrwdth,	// Wimpy
	sfx2_plrcdth,	// Crazy
	sfx2_itemup,
	sfx2_wpnup,
	sfx2_telept,
	sfx2_doropn,
	sfx2_dorcls,
	sfx2_dormov,
	sfx2_artiup,
	sfx2_switch,
	sfx2_pstart,
	sfx2_pstop,
	sfx2_stnmov,
	sfx2_chicpai,
	sfx2_chicatk,
	sfx2_chicdth,
	sfx2_chicact,
	sfx2_chicpk1,
	sfx2_chicpk2,
	sfx2_chicpk3,
	sfx2_keyup,
	sfx2_ripslop,
	sfx2_newpod,
	sfx2_podexp,
	sfx2_bounce,
	sfx2_volsht,
	sfx2_volhit,
	sfx2_burn,
	sfx2_splash,
	sfx2_gloop,
	sfx2_respawn,
	sfx2_blssht,
	sfx2_blshit,
	sfx2_chat,
	sfx2_artiuse,
	sfx2_gfrag,
	sfx2_waterfl,

	// Monophonic sounds

	sfx2_wind,
	sfx2_amb1,
	sfx2_amb2,
	sfx2_amb3,
	sfx2_amb4,
	sfx2_amb5,
	sfx2_amb6,
	sfx2_amb7,
	sfx2_amb8,
	sfx2_amb9,
	sfx2_amb10,
	sfx2_amb11,

	NUMHERETICSFX
};

//**************************************************************************
//
//	$Log$
//	Revision 1.3  2001/09/20 16:34:58  dj_jl
//	Beautification
//
//	Revision 1.2  2001/07/27 14:27:55  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************

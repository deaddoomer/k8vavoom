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

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "sv_local.h"
#ifdef CLIENT
#include "cl_local.h"
#endif
#include "render/r_local.h" // for decals

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------
extern VCvarB decals_enabled;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

IMPLEMENT_CLASS(V, Level);

VLevel*     GLevel;
VLevel*     GClLevel;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static VCvarI decal_onetype_max("decal_onetype_max", "128", "Maximum decals of one decaltype on a wall segment", CVAR_Archive);

// CODE --------------------------------------------------------------------

//==========================================================================
//
//  VLevel::PointInSubsector
//
//==========================================================================

subsector_t* VLevel::PointInSubsector(const TVec &point) const
{
  guard(VLevel::PointInSubsector);
  // single subsector is a special case
  if (!NumNodes)
  {
    return Subsectors;
  }

  int nodenum = NumNodes - 1;
  do
  {
    const node_t* node = Nodes + nodenum;
    nodenum = node->children[node->PointOnSide(point)];
  }
  while (!(nodenum & NF_SUBSECTOR));
  return &Subsectors[nodenum & ~NF_SUBSECTOR];
  unguard;
}

//==========================================================================
//
//  VLevel::LeafPVS
//
//==========================================================================

byte *VLevel::LeafPVS(const subsector_t *ss) const
{
  guard(VLevel::LeafPVS);
  int sub = ss - Subsectors;
  if (VisData)
  {
    return VisData + (((NumSubsectors + 7) >> 3) * sub);
  }
  else
  {
    return NoVis;
  }
  unguard;
}

//==========================================================================
//
//  DecalIO
//
//==========================================================================

static void DecalIO (VStream& Strm, decal_t *dc) {
  if (!dc) return;
  char namebuf[64];
  vuint32 namelen = 0;
  if (Strm.IsLoading()) {
    // load picture name
    Strm << namelen;
    if (namelen == 0 || namelen > 63) Host_Error("Level load: invalid decal name length");
    memset(namebuf, 0, sizeof(namebuf));
    Strm.Serialise(namebuf, namelen);
    dc->picname = VName(namebuf);
    dc->texture = GTextureManager.AddPatch(dc->picname, TEXTYPE_Pic);
    // load decal type
    Strm << namelen;
    if (namelen == 0 || namelen > 63) Host_Error("Level load: invalid decal name length");
    memset(namebuf, 0, sizeof(namebuf));
    Strm.Serialise(namebuf, namelen);
    dc->dectype = VName(namebuf);
  } else {
    // save picture name
    namelen = (vuint32)strlen(*dc->picname);
    if (namelen == 0 || namelen > 63) Sys_Error("Level save: invalid decal name length");
    Strm << namelen;
    memcpy(namebuf, *dc->picname, namelen);
    Strm.Serialise(namebuf, namelen);
    // save decal type
    namelen = (vuint32)strlen(*dc->dectype);
    if (namelen == 0 || namelen > 63) Sys_Error("Level save: invalid decal name length");
    Strm << namelen;
    memcpy(namebuf, *dc->dectype, namelen);
    Strm.Serialise(namebuf, namelen);
  }
  Strm << dc->flags;
  Strm << dc->orgz;
  Strm << dc->curz;
  Strm << dc->xdist;
  Strm << dc->linelen;
  Strm << dc->shade[0];
  Strm << dc->shade[1];
  Strm << dc->shade[2];
  Strm << dc->shade[3];
  Strm << dc->ofsX;
  Strm << dc->ofsY;
  Strm << dc->origScaleX;
  Strm << dc->origScaleY;
  Strm << dc->scaleX;
  Strm << dc->scaleY;
  Strm << dc->origAlpha;
  Strm << dc->alpha;
  Strm << dc->addAlpha;
  VDecalAnim::Serialise(Strm, dc->animator);
}

//==========================================================================
//
//  VLevel::Serialise
//
//==========================================================================

static void writeOrCheckName (VStream& Strm, const VName& value, const char *errmsg) {
  if (Strm.IsLoading()) {
    auto slen = strlen(*value);
    vuint32 v;
    Strm << v;
    if (v != slen) Host_Error(va("Save loader: invalid string length for %s", errmsg));
    if (v > 0) {
      auto buf = new char[v];
      memset(buf, 0, v);
      Strm.Serialise(buf, v);
      int res = memcmp(buf, *value, v);
      delete[] buf;
      if (res != 0) Host_Error(va("Save loader: invalid string value for %s", errmsg));
    }
  } else {
    vuint32 slen = (vuint32)strlen(*value);
    Strm << slen;
    if (slen) Strm.Serialise((char*)*value, slen);
  }
}

static void writeOrCheckUInt (VStream& Strm, vuint32 value, const char *errmsg) {
  if (Strm.IsLoading()) {
    vuint32 v;
    Strm << v;
    if (v != value) Host_Error(va("Save loader: invalid value for %s; got %d, but expected %d", errmsg, v, value));
  } else {
    Strm << value;
  }
}

static void writeOrCheckInt (VStream& Strm, int value, const char *errmsg) {
  if (Strm.IsLoading()) {
    int v;
    Strm << v;
    if (v != value) Host_Error(va("Save loader: invalid value for %s; got %d, but expected %d", errmsg, v, value));
  } else {
    Strm << value;
  }
}

static void writeOrCheckFloat (VStream& Strm, float value, const char *errmsg) {
  if (Strm.IsLoading()) {
    float v;
    Strm << v;
    if (v != value) Host_Error(va("Save loader: invalid value for %s; got %f, but expected %f", errmsg, v, value));
  } else {
    Strm << value;
  }
}

void VLevel::Serialise(VStream& Strm)
{
  guard(VLevel::Serialise);
  int i;
  sector_t* sec;
  line_t* li;
  side_t* si;

  // write/check various numbers, so we won't load invalid save accidentally
  // this is not the best or most reliable way to check it, but it is better
  // than nothing...

  writeOrCheckName(Strm, MapName, "map name");
  writeOrCheckUInt(Strm, LevelFlags, "level flags");

  writeOrCheckInt(Strm, NumVertexes, "vertex count");
  writeOrCheckInt(Strm, NumSectors, "sector count");
  writeOrCheckInt(Strm, NumSides, "side count");
  writeOrCheckInt(Strm, NumLines, "line count");
  writeOrCheckInt(Strm, NumSegs, "seg count");
  writeOrCheckInt(Strm, NumSubsectors, "subsector count");
  writeOrCheckInt(Strm, NumNodes, "node count");
  writeOrCheckInt(Strm, NumPolyObjs, "polyobj count");
  writeOrCheckInt(Strm, NumZones, "zone count");

  writeOrCheckFloat(Strm, BlockMapOrgX, "blocmap x origin");
  writeOrCheckFloat(Strm, BlockMapOrgY, "blocmap y origin");

  //
  //  Decals
  //

  if (Strm.IsLoading()) decanimlist = nullptr;
  if (Segs && NumSegs) {
    vuint32 sgc = (vuint32)NumSegs;
    vuint32 dctotal = 0;
    if (Strm.IsLoading()) {
      // load decals
      sgc = 0;
      Strm << sgc; // just to be sure
      if (sgc != (vuint32)NumSegs) Host_Error("Level load: invalid number of segs");
      for (int f = 0; f < NumSegs; ++f) {
        vuint32 dcount = 0;
        // remove old decals
        decal_t* decal = Segs[f].decals;
        while (decal) {
          decal_t* c = decal;
          decal = c->next;
          delete c->animator;
          delete c;
        }
        Segs[f].decals = nullptr;
        // load decal count for this seg
        Strm << dcount;
        decal = nullptr; // previous
        while (dcount-- > 0) {
          decal_t* dc = new decal_t;
          memset(dc, 0, sizeof(decal_t));
          dc->seg = &Segs[f];
          DecalIO(Strm, dc);
          if (dc->alpha <= 0 || dc->scaleX <= 0 || dc->scaleY <= 0 || dc->texture < 0) {
            delete dc->animator;
            delete dc;
          } else {
            // fix backsector
            if (dc->flags&(decal_t::SlideFloor|decal_t::SlideCeil)) {
              line_t* li = Segs[f].linedef;
              if (!li) Sys_Error("Save loader: invalid seg linedef (0)!");
              int bsidenum = (dc->flags&decal_t::SideDefOne ? 1 : 0);
              if (li->sidenum[bsidenum] < 0) Sys_Error("Save loader: invalid seg linedef (1)!");
              side_t *sb = &Sides[li->sidenum[bsidenum]];
              dc->bsec = sb->Sector;
              if (!dc->bsec) Sys_Error("Save loader: invalid seg linedef (2)!");
            }
            // add to decal list
            if (decal) decal->next = dc; else Segs[f].decals = dc;
            if (dc->animator) {
              if (decanimlist) decanimlist->prevanimated = dc;
              dc->nextanimated = decanimlist;
              decanimlist = dc;
            }
            decal = dc;
          }
          ++dctotal;
        }
      }
      GCon->Logf("%u decals loaded", dctotal);
    } else {
      // save decals
      Strm << sgc; // just to be sure
      for (int f = 0; f < NumSegs; ++f) {
        // count decals
        vuint32 dcount = 0;
        for (decal_t* decal = Segs[f].decals; decal; decal = decal->next) ++dcount;
        Strm << dcount;
        for (decal_t* decal = Segs[f].decals; decal; decal = decal->next) {
          DecalIO(Strm, decal);
          ++dctotal;
        }
      }
      GCon->Logf("%u decals saved", dctotal);
    }
  }

  //
  //  Sectors
  //
  guard(VLevel::Serialise::Sectors);
  for (i = 0, sec = Sectors; i < NumSectors; i++, sec++)
  {
    Strm << sec->floor.dist
      << sec->floor.TexZ
      << sec->floor.pic
      << sec->floor.xoffs
      << sec->floor.yoffs
      << sec->floor.XScale
      << sec->floor.YScale
      << sec->floor.Angle
      << sec->floor.BaseAngle
      << sec->floor.BaseYOffs
      << sec->floor.flags
      << sec->floor.Alpha
      << sec->floor.MirrorAlpha
      << sec->floor.LightSourceSector
      << sec->floor.SkyBox
      << sec->ceiling.dist
      << sec->ceiling.TexZ
      << sec->ceiling.pic
      << sec->ceiling.xoffs
      << sec->ceiling.yoffs
      << sec->ceiling.XScale
      << sec->ceiling.YScale
      << sec->ceiling.Angle
      << sec->ceiling.BaseAngle
      << sec->ceiling.BaseYOffs
      << sec->ceiling.flags
      << sec->ceiling.Alpha
      << sec->ceiling.MirrorAlpha
      << sec->ceiling.LightSourceSector
      << sec->ceiling.SkyBox
      << sec->params.lightlevel
      << sec->params.LightColour
      << sec->params.Fade
      << sec->params.contents
      << sec->special
      << sec->tag
      << sec->seqType
      << sec->SectorFlags
      << sec->SoundTarget
      << sec->FloorData
      << sec->CeilingData
      << sec->LightingData
      << sec->AffectorData
      << sec->ActionList
      << sec->Damage
      << sec->Friction
      << sec->MoveFactor
      << sec->Gravity
      << sec->Sky;
    if (Strm.IsLoading())
    {
      CalcSecMinMaxs(sec);
      sec->ThingList = NULL;
    }
  }
  if (Strm.IsLoading())
  {
    HashSectors();
  }
  unguard;

  //
  //  Lines
  //
  guard(VLevel::Serialise::Lines);
  for (i = 0, li = Lines; i < NumLines; i++, li++)
  {
    Strm << li->flags
      << li->SpacFlags
      << li->special
      << li->arg1
      << li->arg2
      << li->arg3
      << li->arg4
      << li->arg5
      << li->LineTag
      << li->alpha;
    for (int j = 0; j < 2; j++)
    {
      if (li->sidenum[j] == -1)
      {
        continue;
      }
      si = &Sides[li->sidenum[j]];
      Strm << si->TopTextureOffset
        << si->BotTextureOffset
        << si->MidTextureOffset
        << si->TopRowOffset
        << si->BotRowOffset
        << si->MidRowOffset
        << si->TopTexture
        << si->BottomTexture
        << si->MidTexture
        << si->Flags
        << si->Light;
    }
  }
  if (Strm.IsLoading())
  {
    HashLines();
  }
  unguard;

  //
  //  Polyobjs
  //
  guard(VLevel::Serialise::Polyobjs);
  for (i = 0; i < NumPolyObjs; i++)
  {
    if (Strm.IsLoading())
    {
      float angle, polyX, polyY;

      Strm << angle
        << polyX
        << polyY;
      RotatePolyobj(PolyObjs[i].tag, angle);
      MovePolyobj(PolyObjs[i].tag,
        polyX - PolyObjs[i].startSpot.x,
        polyY - PolyObjs[i].startSpot.y);
    }
    else
    {
      Strm << PolyObjs[i].angle
        << PolyObjs[i].startSpot.x
        << PolyObjs[i].startSpot.y;
    }
    Strm << PolyObjs[i].SpecialData;
  }
  unguard;

  //
  //  Static lights
  //
  guard(VLevel::Serialise::StaticLights);
  Strm << STRM_INDEX(NumStaticLights);
  if (Strm.IsLoading())
  {
    if (StaticLights)
    {
      delete[] StaticLights;
      StaticLights = NULL;
    }
    if (NumStaticLights)
    {
      StaticLights = new rep_light_t[NumStaticLights];
    }
  }
  for (i = 0; i < NumStaticLights; i++)
  {
    Strm << StaticLights[i].Origin
      << StaticLights[i].Radius
      << StaticLights[i].Colour;
  }
  unguard;

  //
  //  ACS
  //
  guard(VLevel::Serialise::ACS);
  Acs->Serialise(Strm);
  unguard;

  //
  //  Camera textures
  //
  guard(VLevel::Serialise::CameraTextures);
  int NumCamTex = CameraTextures.Num();
  Strm << STRM_INDEX(NumCamTex);
  if (Strm.IsLoading())
  {
    CameraTextures.SetNum(NumCamTex);
  }
  for (i = 0; i < NumCamTex; i++)
  {
    Strm << CameraTextures[i].Camera
      << CameraTextures[i].TexNum
      << CameraTextures[i].FOV;
  }
  unguard;

  //
  //  Translation tables
  //
  guard(VLevel::Serialise::Translations);
  int NumTrans = Translations.Num();
  Strm << STRM_INDEX(NumTrans);
  if (Strm.IsLoading())
  {
    Translations.SetNum(NumTrans);
  }
  for (i = 0; i < NumTrans; i++)
  {
    vuint8 Present = !!Translations[i];
    Strm << Present;
    if (Strm.IsLoading())
    {
      if (Present)
      {
        Translations[i] = new VTextureTranslation;
      }
      else
      {
        Translations[i] = NULL;
      }
    }
    if (Present)
    {
      Translations[i]->Serialise(Strm);
    }
  }
  unguard;

  //
  //  Body queue translation tables
  //
  guard(VLevel::Serialise::BodyQueueTranslations);
  int NumTrans = BodyQueueTrans.Num();
  Strm << STRM_INDEX(NumTrans);
  if (Strm.IsLoading())
  {
    BodyQueueTrans.SetNum(NumTrans);
  }
  for (i = 0; i < NumTrans; i++)
  {
    vuint8 Present = !!BodyQueueTrans[i];
    Strm << Present;
    if (Strm.IsLoading())
    {
      if (Present)
      {
        BodyQueueTrans[i] = new VTextureTranslation;
      }
      else
      {
        BodyQueueTrans[i] = NULL;
      }
    }
    if (Present)
    {
      BodyQueueTrans[i]->Serialise(Strm);
    }
  }
  unguard;

  //
  //  Zones
  //
  guard(VLevel::Serialise::Zones);
  for (i = 0; i < NumZones; i++)
  {
    Strm << STRM_INDEX(Zones[i]);
  }
  unguard;
  unguard;
}

//==========================================================================
//
//  VLevel::ClearReferences
//
//==========================================================================

void VLevel::ClearReferences()
{
  guard(VLevel::ClearReferences);
  Super::ClearReferences();
  for (int i = 0; i < NumSectors; i++)
  {
    sector_t* sec = Sectors + i;
    if (sec->SoundTarget && sec->SoundTarget->GetFlags() & _OF_CleanupRef)
      sec->SoundTarget = NULL;
    if (sec->FloorData && sec->FloorData->GetFlags() & _OF_CleanupRef)
      sec->FloorData = NULL;
    if (sec->CeilingData && sec->CeilingData->GetFlags() & _OF_CleanupRef)
      sec->CeilingData = NULL;
    if (sec->LightingData && sec->LightingData->GetFlags() & _OF_CleanupRef)
      sec->LightingData = NULL;
    if (sec->AffectorData && sec->AffectorData->GetFlags() & _OF_CleanupRef)
      sec->AffectorData = NULL;
    if (sec->ActionList && sec->ActionList->GetFlags() & _OF_CleanupRef)
      sec->ActionList = NULL;
  }
  for (int i = 0; i < NumPolyObjs; i++)
  {
    if (PolyObjs[i].SpecialData && PolyObjs[i].SpecialData->GetFlags() & _OF_CleanupRef)
      PolyObjs[i].SpecialData = NULL;
  }
  for (int i = 0; i < CameraTextures.Num(); i++)
  {
    if (CameraTextures[i].Camera && CameraTextures[i].Camera->GetFlags() & _OF_CleanupRef)
      CameraTextures[i].Camera = NULL;
  }
  unguard;
}

//==========================================================================
//
//  VLevel::Destroy
//
//==========================================================================

void VLevel::Destroy()
{
  guard(VLevel::Destroy);

  decanimlist = nullptr; // why not?

  //  Destroy all thinkers.
  DestroyAllThinkers();

  while (HeadSecNode)
  {
    msecnode_t* Node = HeadSecNode;
    HeadSecNode = Node->SNext;
    delete Node;
    Node = NULL;
  }

  //  Free level data.
  if (RenderData)
  {
    delete RenderData;
    RenderData = NULL;
  }

  for (int i = 0; i < NumPolyObjs; i++)
  {
    delete[] PolyObjs[i].segs;
    PolyObjs[i].segs = NULL;
    delete[] PolyObjs[i].originalPts;
    PolyObjs[i].originalPts = NULL;
    if (PolyObjs[i].prevPts)
    {
      delete[] PolyObjs[i].prevPts;
      PolyObjs[i].prevPts = NULL;
    }
  }
  if (PolyBlockMap)
  {
    for (int i = 0; i < BlockMapWidth * BlockMapHeight; i++)
    {
      for (polyblock_t* pb = PolyBlockMap[i]; pb;)
      {
        polyblock_t* Next = pb->next;
        delete pb;
        if (Next)
        {
          pb = Next;
        }
        else
        {
          pb = NULL;
        }
      }
    }
    delete[] PolyBlockMap;
    PolyBlockMap = NULL;
  }
  if (PolyObjs)
  {
    delete[] PolyObjs;
    PolyObjs = NULL;
  }
  if (PolyAnchorPoints)
  {
    delete[] PolyAnchorPoints;
    PolyAnchorPoints = NULL;
  }

  if (Sectors)
  {
    for (int i = 0; i < NumSectors; i++)
    {
      sec_region_t* Next;
      for (sec_region_t* r = Sectors[i].botregion; r; r = Next)
      {
        Next = r->next;
        delete r;
        r = NULL;
      }
    }
    delete[] Sectors[0].lines;
    Sectors[0].lines = NULL;
  }

  if (Segs && NumSegs) {
    for (int f = 0; f < NumSegs; ++f) {
      decal_t* decal = Segs[f].decals;
      while (decal) {
        decal_t* c = decal;
        decal = c->next;
        delete c->animator;
        delete c;
      }
    }
  }

  delete[] Vertexes;
  Vertexes = NULL;
  delete[] Sectors;
  Sectors = NULL;
  delete[] Sides;
  Sides = NULL;
  delete[] Lines;
  Lines = NULL;
  delete[] Segs;
  Segs = NULL;
  delete[] Subsectors;
  Subsectors = NULL;
  delete[] Nodes;
  Nodes = NULL;
  if (VisData)
  {
    delete[] VisData;
    VisData = NULL;
  }
  else
  {
    delete[] NoVis;
    NoVis = NULL;
  }
  delete[] BlockMapLump;
  BlockMapLump = NULL;
  delete[] BlockLinks;
  BlockLinks = NULL;
  delete[] RejectMatrix;
  RejectMatrix = NULL;
  delete[] Things;
  Things = NULL;
  delete[] Zones;
  Zones = NULL;

  delete[] BaseLines;
  BaseLines = NULL;
  delete[] BaseSides;
  BaseSides = NULL;
  delete[] BaseSectors;
  BaseSectors = NULL;
  delete[] BasePolyObjs;
  BasePolyObjs = NULL;

  if (Acs)
  {
    delete Acs;
    Acs = NULL;
  }
  if (GenericSpeeches)
  {
    delete[] GenericSpeeches;
    GenericSpeeches = NULL;
  }
  if (LevelSpeeches)
  {
    delete[] LevelSpeeches;
    LevelSpeeches = NULL;
  }
  if (StaticLights)
  {
    delete[] StaticLights;
    StaticLights = NULL;
  }

  ActiveSequences.Clear();

  for (int i = 0; i < Translations.Num(); i++)
  {
    if (Translations[i])
    {
      delete Translations[i];
      Translations[i] = NULL;
    }
  }
  Translations.Clear();
  for (int i = 0; i < BodyQueueTrans.Num(); i++)
  {
    if (BodyQueueTrans[i])
    {
      delete BodyQueueTrans[i];
      BodyQueueTrans[i] = NULL;
    }
  }
  BodyQueueTrans.Clear();

  //  Call parent class's Destroy method.
  Super::Destroy();
  unguard;
}

//==========================================================================
//
//  VLevel::SetCameraToTexture
//
//==========================================================================

void VLevel::SetCameraToTexture(VEntity* Ent, VName TexName, int FOV)
{
  guard(VLevel::SetCameraToTexture);
  if (!Ent)
  {
    return;
  }

  //  Get texture index.
  int TexNum = GTextureManager.CheckNumForName(TexName, TEXTYPE_Wall,
    true, false);
  if (TexNum < 0)
  {
    GCon->Logf("SetCameraToTexture: %s is not a valid texture",
      *TexName);
    return;
  }

  //  Make camera to be always relevant
  Ent->ThinkerFlags |= VEntity::TF_AlwaysRelevant;

  for (int i = 0; i < CameraTextures.Num(); i++)
  {
    if (CameraTextures[i].TexNum == TexNum)
    {
      CameraTextures[i].Camera = Ent;
      CameraTextures[i].FOV = FOV;
      return;
    }
  }
  VCameraTextureInfo& C = CameraTextures.Alloc();
  C.Camera = Ent;
  C.TexNum = TexNum;
  C.FOV = FOV;
  unguard;
}

//=============================================================================
//
//  VLevel::AddSecnode
//
// phares 3/16/98
//
// Searches the current list to see if this sector is
// already there. If not, it adds a sector node at the head of the list of
// sectors this object appears in. This is called when creating a list of
// nodes that will get linked in later. Returns a pointer to the new node.
//
//=============================================================================

msecnode_t* VLevel::AddSecnode(sector_t* Sec, VEntity* Thing,
  msecnode_t* NextNode)
{
  guard(VLevel::AddSecnode);
  msecnode_t* Node;

  if (!Sec)
  {
    Sys_Error("AddSecnode of 0 for %s\n", Thing->GetClass()->GetName());
  }

  Node = NextNode;
  while (Node)
  {
    if (Node->Sector == Sec)  // Already have a node for this sector?
    {
      Node->Thing = Thing;  // Yes. Setting m_thing says 'keep it'.
      return NextNode;
    }
    Node = Node->TNext;
  }

  // Couldn't find an existing node for this sector. Add one at the head
  // of the list.

  // Retrieve a node from the freelist.
  if (HeadSecNode)
  {
    Node = HeadSecNode;
    HeadSecNode = HeadSecNode->SNext;
  }
  else
  {
    Node = new msecnode_t;
  }

  // killough 4/4/98, 4/7/98: mark new nodes unvisited.
  Node->Visited = false;

  Node->Sector = Sec;     // sector
  Node->Thing = Thing;    // mobj
  Node->TPrev = NULL;     // prev node on Thing thread
  Node->TNext = NextNode;   // next node on Thing thread
  if (NextNode)
  {
    NextNode->TPrev = Node; // set back link on Thing
  }

  // Add new node at head of sector thread starting at Sec->TouchingThingList

  Node->SPrev = NULL;     // prev node on sector thread
  Node->SNext = Sec->TouchingThingList; // next node on sector thread
  if (Sec->TouchingThingList)
  {
    Node->SNext->SPrev = Node;
  }
  Sec->TouchingThingList = Node;
  return Node;
  unguard;
}

//=============================================================================
//
//  VLevel::DelSecnode
//
//  Deletes a sector node from the list of sectors this object appears in.
// Returns a pointer to the next node on the linked list, or NULL.
//
//=============================================================================

msecnode_t* VLevel::DelSecnode(msecnode_t* Node)
{
  guard(VLevel::DelSecnode);
  msecnode_t*   tp;  // prev node on thing thread
  msecnode_t*   tn;  // next node on thing thread
  msecnode_t*   sp;  // prev node on sector thread
  msecnode_t*   sn;  // next node on sector thread

  if (Node)
  {
    // Unlink from the Thing thread. The Thing thread begins at
    // sector_list and not from VEntiy->TouchingSectorList.

    tp = Node->TPrev;
    tn = Node->TNext;
    if (tp)
    {
      tp->TNext = tn;
    }
    if (tn)
    {
      tn->TPrev = tp;
    }

    // Unlink from the sector thread. This thread begins at
    // sector_t->TouchingThingList.

    sp = Node->SPrev;
    sn = Node->SNext;
    if (sp)
    {
      sp->SNext = sn;
    }
    else
    {
      Node->Sector->TouchingThingList = sn;
    }
    if (sn)
    {
      sn->SPrev = sp;
    }

    // Return this node to the freelist

    Node->SNext = HeadSecNode;
    HeadSecNode = Node;
    return tn;
  }
  return NULL;
  unguard;
}                             // phares 3/13/98

//=============================================================================
//
//  VLevel::DelSectorList
//
//  Deletes the sector_list and NULLs it.
//
//=============================================================================

void VLevel::DelSectorList()
{
  guard(VLevel::DelSectorList);
  if (SectorList)
  {
    msecnode_t* Node = SectorList;
    while (Node)
    {
      Node = DelSecnode(Node);
    }
    SectorList = NULL;
  }
  unguard;
}

//==========================================================================
//
//  VLevel::SetBodyQueueTrans
//
//==========================================================================

int VLevel::SetBodyQueueTrans(int Slot, int Trans)
{
  guard(VLevel::SetBodyQueueTrans);
  int Type = Trans >> TRANSL_TYPE_SHIFT;
  int Index = Trans & ((1 << TRANSL_TYPE_SHIFT) - 1);
  if (Type != TRANSL_Player)
  {
    return Trans;
  }
  if (Slot < 0 || Slot > MAX_BODY_QUEUE_TRANSLATIONS || Index < 0 ||
    Index >= MAXPLAYERS || !LevelInfo->Game->Players[Index])
  {
    return Trans;
  }

  //  Add it.
  while (BodyQueueTrans.Num() <= Slot)
  {
    BodyQueueTrans.Append(NULL);
  }
  VTextureTranslation* Tr = BodyQueueTrans[Slot];
  if (!Tr)
  {
    Tr = new VTextureTranslation;
    BodyQueueTrans[Slot] = Tr;
  }
  Tr->Clear();
  VBasePlayer* P = LevelInfo->Game->Players[Index];
  Tr->BuildPlayerTrans(P->TranslStart, P->TranslEnd, P->Colour);
  return (TRANSL_BodyQueue << TRANSL_TYPE_SHIFT) + Slot;
  unguard;
}

//==========================================================================
//
//  VLevel::FindSectorFromTag
//
//==========================================================================

int VLevel::FindSectorFromTag(int tag, int start)
{
  guard(VLevel::FindSectorFromTag);
  for (int i = start  < 0 ? Sectors[(vuint32)tag %
    (vuint32)NumSectors].HashFirst : Sectors[start].HashNext;
    i >= 0; i = Sectors[i].HashNext)
  {
    if (Sectors[i].tag == tag)
    {
      return i;
    }
  }
  return -1;
  unguard;
}

//==========================================================================
//
//  VLevel::FindLine
//
//==========================================================================

line_t* VLevel::FindLine(int lineTag, int* searchPosition)
{
  guard(VLevel::FindLine);
  for (int i = *searchPosition < 0 ? Lines[(vuint32)lineTag %
    (vuint32)NumLines].HashFirst : Lines[*searchPosition].HashNext;
    i >= 0; i = Lines[i].HashNext)
  {
    if (Lines[i].LineTag == lineTag)
    {
      *searchPosition = i;
      return &Lines[i];
    }
  }
  *searchPosition = -1;
  return NULL;
  unguard;
}

//==========================================================================
//
// VLevel::FindAdjacentLine
//
// dir<0: previous; dir>0: next
//
//==========================================================================

line_t* VLevel::FindAdjacentLine (line_t* srcline, int side, int dir) {
  if (!srcline || dir == 0 || side < 0 || side > 1) return nullptr; // sanity checks
  sector_t* sec = (side == 0 ? srcline->frontsector : srcline->backsector);
  if (!sec) return nullptr; // wtf?!
  TVec *lv1 = (side == 0 ? srcline->v1 : srcline->v2);
  TVec *lv2 = (side == 0 ? srcline->v2 : srcline->v1);
  // loop over all sector lines, trying to match prev/next
  for (int lidx = 0; lidx < sec->linecount; ++lidx) {
    line_t* line = sec->lines[lidx];
    // get vertices
    TVec *v1, *v2;
    if (line->frontsector == sec) {
      v1 = line->v1;
      v2 = line->v2;
    } else if (line->backsector == sec) {
      v1 = line->v2;
      v2 = line->v1;
    } else {
      // wtf?!
      continue;
    }
    // check vertices
    if (dir < 0) {
      // want previous
      if (v2 == lv1) return line;
    } else {
      // want next
      if (v1 == lv2) return line;
    }
  }
  return nullptr;
}

//==========================================================================
//
// VLevel::AddAnimatedDecal
//
//==========================================================================

void VLevel::AddAnimatedDecal (decal_t* dc) {
  if (!dc || dc->prevanimated || dc->nextanimated || decanimlist == dc || !dc->animator) return;
  if (decanimlist) decanimlist->prevanimated = dc;
  dc->nextanimated = decanimlist;
  decanimlist = dc;
}

//==========================================================================
//
// VLevel::RemoveAnimatedDecal
//
// this will also kill animator
//
//==========================================================================

void VLevel::RemoveAnimatedDecal (decal_t* dc) {
  if (!dc || (!dc->prevanimated && !dc->nextanimated && decanimlist != dc)) return;
  if (dc->prevanimated) dc->prevanimated->nextanimated = dc->nextanimated; else decanimlist = dc->nextanimated;
  if (dc->nextanimated) dc->nextanimated->prevanimated = dc->prevanimated;
  delete dc->animator;
  dc->animator = nullptr;
  dc->prevanimated = dc->nextanimated = nullptr;
}

//==========================================================================
//
// VLevel::PutDecalAtLine
//
// prevdir<0: `segdist` is negative, left offset
// prevdir=0: `segdist` is normal dist
// prevdir>0: `segdist` is positive, right offset
//
//==========================================================================

static bool isDecalsOverlap (VDecalDef *dec, float segd0, float segd1, float orgz, decal_t* cur, const picinfo_t& tinf) {
  float cco = cur->xdist-tinf.xoffset*cur->scaleX+cur->ofsX;
  float ccz = cur->curz-cur->ofsY+tinf.yoffset*cur->scaleY;
  orgz += tinf.yoffset*dec->scaleY;
  return
    segd1 > cco && segd0 < cco+tinf.width*cur->scaleX &&
    orgz > ccz-tinf.height*cur->scaleY && orgz-tinf.height*dec->scaleY < ccz;
}


void VLevel::PutDecalAtLine (int tex, float orgz, float segdist, VDecalDef *dec, sector_t *sec, line_t *li, int prevdir, vuint32 flips) {
  guard(VLevel::PutDecalAtLine);

  picinfo_t tinf;
  GTextureManager.GetTextureInfo(tex, &tinf);
  float tw = tinf.width*dec->scaleX;
  float tw2 = tw*0.5f;
  //float th2 = tinf.height*dec->scaleY*0.5f;

  int sidenum = 0;
  vertex_t* v1;
  vertex_t* v2;

  if (li->frontsector == sec) {
    v1 = li->v1;
    v2 = li->v2;
  } else {
    sidenum = 1;
    v1 = li->v2;
    v2 = li->v1;
  }

  TVec lv1 = *v1, lv2 = *v2;
  lv1.z = 0;
  lv2.z = 0;
  float linelen = Length(lv2-lv1);

  float segd0, segd1;
  if (prevdir < 0) {
    if (segdist >= 0) return; // just in case
    segd0 = segdist+linelen;
    segd1 = segd0+tw;
    segdist = (segd0+segd1)*0.5f;
    //GCon->Logf("left spread; segdist=%f; segd0=%f; segd1=%f", segdist, segd0, segd1);
  } else if (prevdir > 0) {
    if (segdist <= 0) return; // just in case
    segd1 = segdist;
    segd0 = segd1-tw;
    segdist = (segd0+segd1)*0.5f;
    //GCon->Logf("right spread; segdist=%f; segd0=%f; segd1=%f", segdist, segd0, segd1);
  } else {
    segd0 = segdist-tw2;
    segd1 = segd0+tw;
  }

  // find segs for this decal (there may be several segs)
  for (seg_t *seg = li->firstseg; seg; seg = seg->lsnext) {
    if (/*seg->linedef == li &&*/ seg->frontsector == sec) {
      if (segd0 >= seg->offset+seg->length || segd1 < seg->offset) continue;
      //if (prevdir < 0) GCon->Logf("  found seg #%d (segd=%f:%f; seg=%f:%f)", sidx, segd0, segd1, seg->offset, seg->offset+seg->length);
      int dcmaxcount = decal_onetype_max;
           if (tinf.width >= 128 || tinf.height >= 128) dcmaxcount = 8;
      else if (tinf.width >= 64 || tinf.height >= 64) dcmaxcount = 16;
      else if (tinf.width >= 32 || tinf.height >= 32) dcmaxcount = 32;
      // remove old same-typed decals, if necessary
      if (dcmaxcount > 0 && dcmaxcount < 10000) {
        int count = 0;
        decal_t* prev = nullptr;
        decal_t* first = nullptr;
        decal_t* cur = seg->decals;
        while (cur) {
          // also, check if this decal is touching our one
          if (cur->dectype == dec->name && isDecalsOverlap(dec, segd0, segd1, orgz, cur, tinf)) {
            if (!first) first = cur;
            ++count;
          }
          if (!first) prev = cur;
          cur = cur->next;
        }
        if (count >= dcmaxcount) {
          //GCon->Logf("removing %d extra '%s' decals", count-dcmaxcount+1, *dec->name);
          // do removal
          decal_t* cur = first;
          if (prev) {
            if (prev->next != cur) Sys_Error("decal oops(0)");
          } else {
            if (seg->decals != cur) Sys_Error("decal oops(1)");
          }
          while (cur) {
            decal_t* n = cur->next;
            if (cur->dectype == dec->name && isDecalsOverlap(dec, segd0, segd1, orgz, cur, tinf)) {
              /*
              GCon->Logf("removing extra '%s' decal; org=(%f:%f,%f:%f); neworg=(%f:%f,%f:%f)", *dec->name,
                cur->xdist-tw2, cur->xdist+tw2, cur->orgz-th2, cur->orgz+th2,
                segd0, segd1, orgz-th2, orgz+th2
              );
              */
              if (prev) prev->next = n; else seg->decals = n;
              RemoveAnimatedDecal(cur);
              delete cur;
              if (--count < dcmaxcount) break;
            }
            cur = n;
          }
        }
      }
      // create decals
      decal_t* decal = new decal_t;
      memset(decal, 0, sizeof(decal_t));
      decal_t* cdec = seg->decals;
      if (cdec) {
        while (cdec->next) cdec = cdec->next;
        cdec->next = decal;
      } else {
        seg->decals = decal;
      }
      decal->seg = seg;
      decal->dectype = dec->name;
      decal->picname = dec->pic;
      decal->texture = tex;
      decal->orgz = decal->curz = orgz;
      decal->xdist = segd0+tinf.width*0.5f;
      decal->linelen = linelen;
      decal->shade[0] = dec->shade[0];
      decal->shade[1] = dec->shade[1];
      decal->shade[2] = dec->shade[2];
      decal->shade[3] = dec->shade[3];
      decal->ofsX = decal->ofsY = 0;
      decal->scaleX = decal->origScaleX = dec->scaleX;
      decal->scaleY = decal->origScaleY = dec->scaleY;
      decal->alpha = decal->origAlpha = dec->alpha;
      decal->addAlpha = dec->addAlpha;
      decal->animator = (dec->animator ? dec->animator->clone() : nullptr);
      if (decal->animator) AddAnimatedDecal(decal);

      // setup misc flags
      decal->flags = flips|(dec->fullbright ? decal_t::Fullbright : 0)|(dec->fuzzy ? decal_t::Fuzzy : 0);

      // setup curz and pegs
      // if we have the other side, check for top/bottom hit
      if (li->sidenum[1-sidenum] >= 0) {
        side_t *sb = &Sides[li->sidenum[1-sidenum]];
        sector_t *bsec = sb->Sector;
        // this check is just in case of something is really fucked up
        if (bsec) {
          // back floor has higher z: has bottom, check for [myfloorz..backfloorz]
          if (bsec->floor.TexZ > sec->floor.TexZ && orgz >= sec->floor.TexZ && orgz <= bsec->floor.TexZ) {
            if ((li->flags&ML_DONTPEGTOP) == 0) {
              decal->curz -= bsec->floor.TexZ;
              decal->flags |= decal_t::SlideFloor|(sidenum == 0 ? decal_t::SideDefOne : 0);
              decal->bsec = bsec;
            }
          } else if (bsec->ceiling.TexZ < sec->ceiling.TexZ && orgz >= bsec->ceiling.TexZ && orgz <= sec->ceiling.TexZ) {
            // back ceil has lower z: has top, check for [backceilz..myceilz]
            if ((li->flags&ML_DONTPEGBOTTOM) == 0) {
              decal->curz -= bsec->ceiling.TexZ;
              decal->flags |= decal_t::SlideCeil|(sidenum == 0 ? decal_t::SideDefOne : 0);
              decal->bsec = bsec;
            }
          }
        }
      }
    }
  }

  // if our decal is not completely at linedef, spread it to adjacent linedefs
  // FIXME: this is not right, 'cause we want not a linedef at the same sector,
  //        but linedef we can use for spreading. the difference is that it can
  //        belong to any other sector, it just has to have the texture at the
  //        given z point. i.e. linedef without midtexture (for example) can't
  //        be used, but another linedef from another sector with such texture
  //        is ok.

  // left spread?
  if (segd0 < 0 && prevdir <= 0) {
    line_t* spline = FindAdjacentLine(li, sidenum, -1);
    if (spline) {
      //GCon->Logf("  WANT LEFT SPREAD, found linedef");
      PutDecalAtLine(tex, orgz, segd0, dec, sec, spline, -1, flips);
    } else {
      //GCon->Logf("  WANT LEFT SPREAD, but no left line found");
    }
  }

  //if (prevdir == 0) GCon->Logf("RSP TEST: segd1=%f; linelen=%f", segd1, linelen);

  // right spread?
  if (segd1 > linelen && prevdir >= 0) {
    line_t* spline = FindAdjacentLine(li, sidenum, 1);
    if (spline) {
      //GCon->Logf("  WANT RIGHT SPREAD, found linedef");
      PutDecalAtLine(tex, orgz, segd1-linelen, dec, sec, spline, 1, flips);
    } else {
      //GCon->Logf("  WANT RIGHT SPREAD, but no right line found");
    }
  }

  unguard;
}

//==========================================================================
//
// VLevel::AddOneDecal
//
//==========================================================================

void VLevel::AddOneDecal (int level, TVec org, VDecalDef *dec, sector_t *sec, line_t *li) {
  if (!dec || !sec || !li) return;

  if (level > 64) {
    GCon->Logf("WARNING: too many lower decals!");
    return;
  }

  if (dec->lowername != NAME_None) AddDecal(org, dec->lowername, (sec == li->backsector ? 1 : 0), li, level+1);

  if (dec->scaleX <= 0 || dec->scaleY <= 0) {
    GCon->Logf("Decal '%s' has zero scale", *dec->name);
    return;
  }

  // actually, we should check animator here, but meh...
  if (dec->alpha <= 0.1) {
    GCon->Logf("Decal '%s' has zero alpha", *dec->name);
    return;
  }

  int tex = GTextureManager.AddPatch(dec->pic, TEXTYPE_Pic);
  //if (dec->pic == VName("scorch1")) tex = GTextureManager.AddPatch(VName("bulde1"), TEXTYPE_Pic);
  if (tex < 0) {
    // no decal gfx, nothing to do
    GCon->Logf("Decal '%s' has no pic (%s)", *dec->name, *dec->pic);
    return;
  }

  // get picture size, so we can spread it over segs and linedefs
  picinfo_t tinf;
  GTextureManager.GetTextureInfo(tex, &tinf);
  if (tinf.width < 1 || tinf.height < 1) {
    // invisible picture
    GCon->Logf("Decal '%s' has pic without pixels (%s)", *dec->name, *dec->pic);
  }

  //GCon->Logf("Picture '%s' size: %d, %d  offsets: %d, %d", *dec->pic, tinf.width, tinf.height, tinf.xoffset, tinf.yoffset);

  // setup flips
  vuint32 flips = 0;
  if (dec->flipX == VDecalDef::FlipRandom) {
    if (Random() < 0.5) flips |= decal_t::FlipX;
  } else if (dec->flipX == VDecalDef::FlipAlways) {
    flips |= decal_t::FlipX;
  }
  if (dec->flipY == VDecalDef::FlipRandom) {
    if (Random() < 0.5) flips |= decal_t::FlipY;
  } else if (dec->flipY == VDecalDef::FlipAlways) {
    flips |= decal_t::FlipY;
  }

  // calculate `dist` -- distance from wall start
  //int sidenum = 0;
  vertex_t* v1;
  vertex_t* v2;

  if (li->frontsector == sec) {
    v1 = li->v1;
    v2 = li->v2;
  } else {
    //sidenum = 1;
    v1 = li->v2;
    v2 = li->v1;
  }

  float dx = v2->x-v1->x;
  float dy = v2->y-v1->y;
  float dist = 0; // distance from wall start
       if (fabs(dx) > fabs(dy)) dist = (org.x-v1->x)/dx;
  else if (dy != 0) dist = (org.y-v1->y)/dy;
  else dist = 0;

  TVec lv1 = *v1, lv2 = *v2;
  lv1.z = 0;
  lv2.z = 0;
  float linelen = Length(lv2-lv1);
  float segdist = dist*linelen;

  //GCon->Logf("Want to spawn decal '%s' (%s); pic=<%s>; dist=%f (linelen=%f)", *dec->name, (li->frontsector == sec ? "front" : "back"), *dec->pic, dist, linelen);

  float segd0 = segdist-tinf.width/2.0f;
  float segd1 = segd0+tinf.width;

  if (segd1 <= 0 || segd0 >= linelen) return; // out of linedef

  PutDecalAtLine(tex, org.z, segdist, dec, sec, li, 0, flips);
}

//==========================================================================
//
// VLevel::AddDecal
//
//==========================================================================

void VLevel::AddDecal (TVec org, const VName& dectype, int side, line_t *li, int level) {
  guard(VLevel::AddDecal);

  if (!decals_enabled) return;
  if (!li || dectype == NAME_None) return; // just in case

  sector_t *sec = (side ? li->backsector : li->frontsector);
  if (!sec) return; // just in case

  // ignore slopes
  if (sec->floor.minz != sec->floor.maxz || sec->ceiling.minz != sec->ceiling.maxz) return;

  /*
  auto op = SV_LineOpenings(li, org, SPF_NOBLOCKING);
  if (op) {
    GCon->Logf("========== OPENINGS ==========");
    while (op) {
      GCon->Logf("  bottom:%f; top:%f; range:%f; lowfloor:%f; highceiling:%f", op->bottom, op->top, op->range, op->lowfloor, op->highceiling);
      op = op->next;
    }
    GCon->Logf("---------------");
  }
  */

  //TVec oorg = org;
  org = P_SectorClosestPoint(sec, org);
  //org.z = oorg.z;
  //GCon->Logf("oorg:(%f,%f,%f); org:(%f,%f,%f)", oorg.x, oorg.y, oorg.z, org.x, org.y, org.z);

  int sidenum = (int)(li->backsector == sec);
  if (li->sidenum[sidenum] < 0) Sys_Error("decal engine: invalid linedef (0)!");

#if 0
  side_t *side = &Sides[li->sidenum[sidenum]];
  if (side->Sector != sec) Sys_Error("decal engine: invalid linedef (1)!");
  GCon->Logf("z=%f; tfloor=%f; tceil=%f; floor=%f; ceil=%f; btex=%d; mtex=%d; ttex=%d; flags=0x%04x; hasback:%s",
    org.z, sec->floor.TexZ, sec->ceiling.TexZ,
    sec->floor.minz, sec->ceiling.minz,
    side->BottomTexture, side->MidTexture, side->TopTexture,
    li->flags,
    (li->sidenum[1-sidenum] >= 0 ? "TAN" : "ona"));
  if (li->sidenum[1-sidenum] >= 0) {
    side_t *sb = &Sides[li->sidenum[1-sidenum]];
    GCon->Logf("z=%f; tfloor=%f; tceil=%f; floor=%f; ceil=%f; btex=%d; mtex=%d; ttex=%d; flags=0x%04x; hasback:%s",
      org.z, sb->Sector->floor.TexZ, sb->Sector->ceiling.TexZ,
      sb->Sector->floor.minz, sb->Sector->ceiling.minz,
      sb->BottomTexture, sb->MidTexture, sb->TopTexture,
      li->flags,
      (li->sidenum[1-sidenum] >= 0 ? "TAN" : "ona"));
  }

  GCon->Logf("=== top regions ===");
  for (sec_region_t *reg = sec->topregion; reg; reg = reg->next) {
    GCon->Logf("  floor: texz=%f; minz=%f; maxz=%f", reg->floor->TexZ, reg->floor->minz, reg->floor->maxz);
    GCon->Logf("  ceil : texz=%f; minz=%f; maxz=%f", reg->ceiling->TexZ, reg->ceiling->minz, reg->ceiling->maxz);
  }

  GCon->Logf("=== bot regions ===");
  for (sec_region_t *reg = sec->botregion; reg; reg = reg->next) {
    GCon->Logf("  floor: texz=%f; minz=%f; maxz=%f", reg->floor->TexZ, reg->floor->minz, reg->floor->maxz);
    GCon->Logf("  ceil : texz=%f; minz=%f; maxz=%f", reg->ceiling->TexZ, reg->ceiling->minz, reg->ceiling->maxz);
  }

  // find segs for this decal (there may be several segs)
  GCon->Logf("=== segs and drawsegs ===");
  for (seg_t *seg = li->firstseg; seg; seg = seg->lsnext) {
    if (seg->frontsector == sec) {
      GCon->Logf(" seg: ofs=%f; len=%f", seg->offset, seg->length);
      for (drawseg_t *ds = seg->drawsegs; ds; ds = ds->next) {
        if (ds->bot) {
          GCon->Logf("  bottom: fdist=(%f,%f); bdist=(%f,%f)", ds->bot->frontTopDist, ds->bot->frontBotDist, ds->bot->backTopDist, ds->bot->backBotDist);
        } else {
          GCon->Logf("  bottom: NONE");
        }
        if (ds->mid) {
          GCon->Logf("  middle: fdist=(%f,%f); bdist=(%f,%f)", ds->mid->frontTopDist, ds->mid->frontBotDist, ds->mid->backTopDist, ds->mid->backBotDist);
        } else {
          GCon->Logf("  middle: NONE");
        }
        if (ds->top) {
          GCon->Logf("  top   : fdist=(%f,%f); bdist=(%f,%f)", ds->top->frontTopDist, ds->top->frontBotDist, ds->top->backTopDist, ds->top->backBotDist);
        } else {
          GCon->Logf("  top   : NONE");
        }
      }
    }
  }
#endif

  static TStrSet baddecals;

  VDecalDef *dec = VDecalDef::getDecal(dectype);
  if (dec) {
    AddOneDecal(level, org, dec, sec, li);
  } else {
    if (!baddecals.put(*dectype)) GCon->Logf("NO DECAL: '%s'", *dectype);
  }

  unguard;
}

//==========================================================================
//
//  CalcLine
//
//==========================================================================

void CalcLine(line_t *line)
{
  guard(CalcLine);
  //  Calc line's slopetype
  line->dir = *line->v2 - *line->v1;
  if (!line->dir.x)
  {
    line->slopetype = ST_VERTICAL;
  }
  else if (!line->dir.y)
  {
    line->slopetype = ST_HORIZONTAL;
  }
  else
  {
    if (line->dir.y / line->dir.x > 0)
    {
      line->slopetype = ST_POSITIVE;
    }
    else
    {
      line->slopetype = ST_NEGATIVE;
    }
  }

  line->SetPointDir(*line->v1, line->dir);

  //  Calc line's bounding box
  if (line->v1->x < line->v2->x)
  {
    line->bbox[BOXLEFT] = line->v1->x;
    line->bbox[BOXRIGHT] = line->v2->x;
  }
  else
  {
    line->bbox[BOXLEFT] = line->v2->x;
    line->bbox[BOXRIGHT] = line->v1->x;
  }
  if (line->v1->y < line->v2->y)
  {
    line->bbox[BOXBOTTOM] = line->v1->y;
    line->bbox[BOXTOP] = line->v2->y;
  }
  else
  {
    line->bbox[BOXBOTTOM] = line->v2->y;
    line->bbox[BOXTOP] = line->v1->y;
  }
  unguard;
}

//==========================================================================
//
//  CalcSeg
//
//==========================================================================

void CalcSeg(seg_t *seg)
{
  guardSlow(CalcSeg);
  seg->Set2Points(*seg->v1, *seg->v2);
  unguardSlow;
}

#ifdef SERVER

//==========================================================================
//
//  SV_LoadLevel
//
//==========================================================================

void SV_LoadLevel(VName MapName)
{
  guard(SV_LoadLevel);
#ifdef CLIENT
  GClLevel = NULL;
#endif
  if (GLevel)
  {
    delete GLevel;
    GLevel = NULL;
  }

  GLevel = Spawn<VLevel>();
  GLevel->LevelFlags |= VLevel::LF_ForServer;

  GLevel->LoadMap(MapName);
  unguard;
}

#endif
#ifdef CLIENT

//==========================================================================
//
//  CL_LoadLevel
//
//==========================================================================

void CL_LoadLevel(VName MapName)
{
  guard(CL_LoadLevel);
  if (GClLevel)
  {
    delete GClLevel;
    GClLevel = NULL;
  }

  GClLevel = Spawn<VLevel>();
  GClGame->GLevel = GClLevel;

  GClLevel->LoadMap(MapName);
  unguard;
}

#endif

//==========================================================================
//
//  AddExtraFloor
//
//==========================================================================

sec_region_t *AddExtraFloor(line_t *line, sector_t *dst)
{
  guard(AddExtraFloor);
  sec_region_t *region;
  sec_region_t *inregion;
  sector_t *src;

  src = line->frontsector;
  src->SectorFlags |= sector_t::SF_ExtrafloorSource;
  dst->SectorFlags |= sector_t::SF_HasExtrafloors;

  float floorz = src->floor.GetPointZ(dst->soundorg);
  float ceilz = src->ceiling.GetPointZ(dst->soundorg);

  // Swap planes for 3d floors like those of GZDoom
  if (floorz < ceilz)
  {
    SwapPlanes(src);
    floorz = src->floor.GetPointZ(dst->soundorg);
    ceilz = src->ceiling.GetPointZ(dst->soundorg);
    GCon->Logf("Swapped planes for tag: %d, ceilz: %f, floorz: %f", line->arg1, ceilz, floorz);
  }

  for (inregion = dst->botregion; inregion; inregion = inregion->next)
  {
    float infloorz = inregion->floor->GetPointZ(dst->soundorg);
    float inceilz = inregion->ceiling->GetPointZ(dst->soundorg);

    if (infloorz <= floorz && inceilz >= ceilz)
    {
      region = new sec_region_t;
      memset(region, 0, sizeof(*region));
      region->floor = inregion->floor;
      region->ceiling = &src->ceiling;
      region->params = &src->params;
      region->extraline = line;
      inregion->floor = &src->floor;

      if (inregion->prev)
      {
        inregion->prev->next = region;
      }
      else
      {
        dst->botregion = region;
      }
      region->prev = inregion->prev;
      region->next = inregion;
      inregion->prev = region;

      return region;
    }
    // Check for sloped floor
    else if (inregion->floor->normal.z != 1.0)
    {
      if (inregion->floor->maxz <= src->ceiling.minz && inregion->ceiling->maxz >= src->floor.minz)
      {
        region = new sec_region_t;
        memset(region, 0, sizeof(*region));
        region->floor = inregion->floor;
        region->ceiling = &src->ceiling;
        region->params = &src->params;
        region->extraline = line;
        inregion->floor = &src->floor;

        if (inregion->prev)
        {
          inregion->prev->next = region;
        }
        else
        {
          dst->botregion = region;
        }
        region->prev = inregion->prev;
        region->next = inregion;
        inregion->prev = region;

        return region;
      }
      /*else
      {
        GCon->Logf("tag: %d, floor->maxz: %f, ceiling.minz: %f, ceiling->maxz: %f, floor.minz: %f", line->arg1, inregion->floor->maxz, src->ceiling.minz, inregion->ceiling->maxz, src->floor.minz);
      }*/
    }
    // Check for sloped ceiling
    else if (inregion->ceiling->normal.z != -1.0)
    {
      if (inregion->floor->minz <= src->ceiling.maxz && inregion->ceiling->minz >= src->floor.maxz)
      {
        region = new sec_region_t;
        memset(region, 0, sizeof(*region));
        region->floor = inregion->floor;
        region->ceiling = &src->ceiling;
        region->params = &src->params;
        region->extraline = line;
        inregion->floor = &src->floor;

        if (inregion->prev)
        {
          inregion->prev->next = region;
        }
        else
        {
          dst->botregion = region;
        }
        region->prev = inregion->prev;
        region->next = inregion;
        inregion->prev = region;

        return region;
      }
      /*else
      {
        GCon->Logf("tag: %d, floor->minz: %f, ceiling.maxz: %f, ceiling->minz: %f, floor.maxz: %f", line->arg1, inregion->floor->minz, src->ceiling.maxz, inregion->ceiling->minz, src->floor.maxz);
      }*/
    }
    /*else
    {
      GCon->Logf("tag: %d, infloorz: %f, ceilz: %f, inceilz: %f, floorz: %f", line->arg1, infloorz, ceilz, inceilz, floorz);
    }*/
  }
  GCon->Logf("Invalid extra floor, tag %d", dst->tag);

  return NULL;
  unguard;
}

//==========================================================================
//
//  SwapPlanes
//
//==========================================================================

void SwapPlanes(sector_t *s)
{
  guard(SwapPlanes);
  float tempHeight;
  int tempTexture;

  tempHeight = s->floor.TexZ;
  tempTexture = s->floor.pic;

  //  Floor
  s->floor.TexZ = s->ceiling.TexZ;
  s->floor.dist = s->floor.TexZ;
  s->floor.minz = s->floor.TexZ;
  s->floor.maxz = s->floor.TexZ;

  s->ceiling.TexZ = tempHeight;
  s->ceiling.dist = -s->ceiling.TexZ;
  s->ceiling.minz = s->ceiling.TexZ;
  s->ceiling.maxz = s->ceiling.TexZ;

  s->floor.pic = s->ceiling.pic;
  s->ceiling.pic = tempTexture;
  unguard;
}

//==========================================================================
//
//  CalcSecMinMaxs
//
//==========================================================================

void CalcSecMinMaxs(sector_t *sector)
{
  guard(CalcSecMinMaxs);
  float minz;
  float maxz;
  int   i;

  if (sector->floor.normal.z == 1.0)
  {
    //  Horisontal floor
    sector->floor.minz = sector->floor.dist;
    sector->floor.maxz = sector->floor.dist;
  }
  else
  {
    //  Sloped floor
    minz = 99999.0;
    maxz = -99999.0;
    for (i = 0; i < sector->linecount; i++)
    {
      float z;
      z = sector->floor.GetPointZ(*sector->lines[i]->v1);
      if (minz > z)
        minz = z;
      if (maxz < z)
        maxz = z;
      z = sector->floor.GetPointZ(*sector->lines[i]->v2);
      if (minz > z)
        minz = z;
      if (maxz < z)
        maxz = z;
    }
    sector->floor.minz = minz;
    sector->floor.maxz = maxz;
  }

  if (sector->ceiling.normal.z == -1.0)
  {
    //  Horisontal ceiling
    sector->ceiling.minz = -sector->ceiling.dist;
    sector->ceiling.maxz = -sector->ceiling.dist;
  }
  else
  {
    //  Sloped ceiling
    minz = 99999.0;
    maxz = -99999.0;
    for (i = 0; i < sector->linecount; i++)
    {
      float z;
      z = sector->ceiling.GetPointZ(*sector->lines[i]->v1);
      if (minz > z)
        minz = z;
      if (maxz < z)
        maxz = z;
      z = sector->ceiling.GetPointZ(*sector->lines[i]->v2);
      if (minz > z)
        minz = z;
      if (maxz < z)
        maxz = z;
    }
    sector->ceiling.minz = minz;
    sector->ceiling.maxz = maxz;
  }
  unguard;
}

//==========================================================================
//
//  Natives
//
//==========================================================================

IMPLEMENT_FUNCTION(VLevel, PointInSector)
{
  P_GET_VEC(Point);
  P_GET_SELF;
  RET_PTR(Self->PointInSubsector(Point)->sector);
}

IMPLEMENT_FUNCTION(VLevel, ChangeSector)
{
  P_GET_INT(crunch);
  P_GET_PTR(sector_t, sec);
  P_GET_SELF;
  RET_BOOL(Self->ChangeSector(sec, crunch));
}

IMPLEMENT_FUNCTION(VLevel, AddExtraFloor)
{
  P_GET_PTR(sector_t, dst);
  P_GET_PTR(line_t, line);
  P_GET_SELF;
  (void)Self;
  RET_PTR(AddExtraFloor(line, dst));
}

IMPLEMENT_FUNCTION(VLevel, SwapPlanes)
{
  P_GET_PTR(sector_t, s);
  P_GET_SELF;
  (void)Self;
  SwapPlanes(s);
}

IMPLEMENT_FUNCTION(VLevel, SetFloorLightSector)
{
  P_GET_PTR(sector_t, SrcSector);
  P_GET_PTR(sector_t, Sector);
  P_GET_SELF;
  Sector->floor.LightSourceSector = SrcSector - Self->Sectors;
}

IMPLEMENT_FUNCTION(VLevel, SetCeilingLightSector)
{
  P_GET_PTR(sector_t, SrcSector);
  P_GET_PTR(sector_t, Sector);
  P_GET_SELF;
  Sector->ceiling.LightSourceSector = SrcSector - Self->Sectors;
}

IMPLEMENT_FUNCTION(VLevel, SetHeightSector)
{
  P_GET_INT(Flags);
  P_GET_PTR(sector_t, SrcSector);
  P_GET_PTR(sector_t, Sector);
  P_GET_SELF;
  (void)Flags;
  (void)SrcSector;
  if (Self->RenderData)
  {
    Self->RenderData->SetupFakeFloors(Sector);
  }
}

IMPLEMENT_FUNCTION(VLevel, FindSectorFromTag)
{
  P_GET_INT(start);
  P_GET_INT(tag);
  P_GET_SELF;
  RET_INT(Self->FindSectorFromTag(tag, start));
}

IMPLEMENT_FUNCTION(VLevel, FindLine)
{
  P_GET_PTR(int, searchPosition);
  P_GET_INT(lineTag);
  P_GET_SELF;
  RET_PTR(Self->FindLine(lineTag, searchPosition));
}

IMPLEMENT_FUNCTION(VLevel, SetBodyQueueTrans)
{
  P_GET_INT(Trans);
  P_GET_INT(Slot);
  P_GET_SELF;
  RET_INT(Self->SetBodyQueueTrans(Slot, Trans));
}

//native final void AddDecal (TVec org, name dectype, int side, line_t *li);
IMPLEMENT_FUNCTION(VLevel, AddDecal)
{
  P_GET_PTR(line_t, li);
  P_GET_INT(side);
  P_GET_NAME(dectype);
  P_GET_VEC(org);
  P_GET_SELF;
  Self->AddDecal(org, dectype, side, li);
}

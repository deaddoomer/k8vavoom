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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#include "../gamedefs.h"
#include "../host.h"
#include "../server/sv_local.h"
#include "../psim/p_entity.h"
#include "../psim/p_levelinfo.h"
#include "../psim/p_player.h"
#ifdef CLIENT
# include "../client/cl_local.h"
# include "../drawer.h"
#endif


IMPLEMENT_CLASS_NAMED(VLevel, VLevel);

VLevel *GLevel;
VLevel *GClLevel;

// for entity state management
vint64 validcountState = 1;
// increment every time a check is made
int validcount = 1;


//==========================================================================
//
//  SwapPlanes
//
//==========================================================================
void SwapPlanes (sector_t *s) {
  float tempHeight = s->floor.TexZ;
  int tempTexture = s->floor.pic;

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
}


//==========================================================================
//
//  VLevelScriptThinker::~VLevelScriptThinker
//
//==========================================================================
VLevelScriptThinker::~VLevelScriptThinker () {
  if (!destroyed) Sys_Error("trying to delete unfinalized Acs script");
}



//==========================================================================
//
//  VLevel::VAllBlockLines::VAllBlockLines
//
//==========================================================================
VLevel::VAllBlockLines::VAllBlockLines (const VLevel *ALevel, int x, int y, unsigned pobjMode) noexcept
  : Level(ALevel)
  , PolyLink(nullptr)
  , PolySegIdx(0)
  , List(nullptr)
  , Line(nullptr)
{
  if (x < 0 || x >= Level->BlockMapWidth || y < 0 || y >= Level->BlockMapHeight) return; // off the map

  int offset = y*Level->BlockMapWidth+x;
  if (pobjMode&BLINES_POBJ) PolyLink = Level->PolyBlockMap[offset];

  if (pobjMode&BLINES_LINES) {
    offset = *(Level->BlockMap+offset);
    List = Level->BlockMapLump+offset+1; // first item is always 0 (dunno why, prolly because vanilla did it this way)
  }

  advance(); // advance to the first item
}


//==========================================================================
//
//  VLevel::VAllBlockLines::advance
//
//==========================================================================
void VLevel::VAllBlockLines::advance () noexcept {
  // check polyobj blockmap
  while (PolyLink) {
    polyobj_t *po = PolyLink->polyobj;
    if (po) {
      while (PolySegIdx < po->numlines) {
        line_t *linedef = po->lines[PolySegIdx++];
        if (linedef->validcount == validcount) continue;
        linedef->validcount = validcount;
        Line = linedef;
        return;
      }
    }
    PolySegIdx = 0;
    PolyLink = PolyLink->next;
  }

  if (List) {
    while (*List != -1) {
      #ifdef PARANOID
      if (*List < 0 || *List >= Level->NumLines) Host_Error("Broken blockmap - line %d", *List);
      #endif
      line_t *linedef = &Level->Lines[*List];
      ++List;
      if (linedef->validcount == validcount) continue; // line has already been checked
      linedef->validcount = validcount;
      Line = linedef;
      return;
    }
  }

  // just in case
  PolyLink = nullptr;
  PolySegIdx = 0;
  List = nullptr;
  Line = nullptr;
}



//==========================================================================
//
//  VLevel::GetFirstBlockmapEntity
//
//==========================================================================
VEntity *VLevel::GetFirstBlockmapEntity (VEntity *ent) noexcept {
  while (ent && ent->IsGoingToDie()) ent = ent->BlockMapNext;
  return ent;
}


//==========================================================================
//
//  VLevel::GetNextBlockmapEntity
//
//==========================================================================
VEntity *VLevel::GetNextBlockmapEntity (VEntity *ent) noexcept {
  if (ent) do { ent = ent->BlockMapNext; } while (ent && ent->IsGoingToDie());
  return ent;
}


//==========================================================================
//
//  VLevel::PostCtor
//
//==========================================================================
void VLevel::PostCtor () {
  lineTags = tagHashAlloc();
  sectorTags = tagHashAlloc();
  PolyTagMap = new TMapNC<int, int>();
  suid2ent = new TMapNC<vuint32, VEntity *>();
  Super::PostCtor();
}


//==========================================================================
//
//  VLevel::ResetValidCount
//
//==========================================================================
void VLevel::ResetValidCount () {
  validcount = 1;
  for (auto &&it : allLines()) it.validcount = 0;
  for (auto &&it : allSectors()) it.validcount = 0;
  for (auto &&it : allPolyobjects()) it->validcount = 0;
  if (processedBMCellsSize) memset(processedBMCells, 0, processedBMCellsSize*sizeof(processedBMCells[0]));
  for (VThinker *th = ThinkerHead; th; th = th->Next) th->ValidCount = 0;
}


//==========================================================================
//
//  VLevel::ResetSZValidCount
//
//==========================================================================
void VLevel::ResetSZValidCount () {
  validcountSZCache = 1;
  for (auto &&it : allSectors()) it.ZExtentsCacheId = 0;
}


/* reference code from GZDoom
static int R_PointOnSideSlow(double xx, double yy, node_t *node)
{
  // [RH] This might have been faster than two multiplies and an
  // add on a 386/486, but it certainly isn't on anything newer than that.
  auto x = FloatToFixed(xx);
  auto y = FloatToFixed(yy);
  double  left;
  double  right;

  if (!node->dx)
  {
    if (x <= node->x)
      return node->dy > 0;

    return node->dy < 0;
  }
  if (!node->dy)
  {
    if (y <= node->y)
      return node->dx < 0;

    return node->dx > 0;
  }

  auto dx = (x - node->x);
  auto dy = (y - node->y);

  // Try to quickly decide by looking at sign bits.
  if ((node->dy ^ node->dx ^ dx ^ dy) & 0x80000000)
  {
    if ((node->dy ^ dx) & 0x80000000)
    {
      // (left is negative)
      return 1;
    }
    return 0;
  }

  // we must use doubles here because the fixed point code will produce errors due to loss of precision for extremely short linedefs.
  // Note that this function is used for all map spawned actors and not just a compatibility fallback!
  left = (double)node->dy * (double)dx;
  right = (double)dy * (double)node->dx;

  if (right < left)
  {
    // front side
    return 0;
  }
  // back side
  return 1;
}
*/


//==========================================================================
//
//  VLevel::PointInSubsector_Buggy
//
//  vanilla buggy algo
//
//==========================================================================
subsector_t *VLevel::PointInSubsector_Buggy (const TVec &point) const noexcept {
  // single subsector is a special case
  if (!NumNodes || NumSubsectors < 2) return Subsectors;
  int nodenum = NumNodes-1;
  do {
    const node_t *node = Nodes+nodenum;
    const float dist = node->PointDistance(point);
    // hack for back subsector
    // try to emulate original Doom buggy code
    // if we are exactly on a two-sided linedef, choose node that leads to back sector
    // this is what vanilla does, and some map authors rely on that fact
    // this usually matters mostly for horizontal, vertical, or strictly diagonal lines, so checking for zero should be ok here
    if (dist == 0.0f) {
      //nodenum = node->children[0];
      //normal = TVec(dir.y, -dir.x, 0.0f);
      // dy:  node.normal.x
      // dx: -node.normal.y
      const float fdx = -node->normal.y;
      const float fdy = +node->normal.x;
           if (fdx == 0) nodenum = node->children[(unsigned)(fdy > 0)];
      else if (fdy == 0) nodenum = node->children[(unsigned)(fdx < 0)];
      else {
        //nodenum = node->children[1/*(unsigned)(dist <= 0.0f)*/]; // is this right?
        vint32 dx = (vint32)(point.x*65536.0)-node->sx;
        vint32 dy = (vint32)(point.y*65536.0)-node->sy;
        // try to quickly decide by looking at sign bits
        if ((node->dy^node->dx^dx^dy)&0x80000000) {
          if ((node->dy^dx)&0x80000000) {
            // (left is negative)
            nodenum = node->children[1];
          } else {
            nodenum = node->children[0];
          }
        } else {
          const double left = (double)node->dy*(double)dx;
          const double right = (double)dy*(double)node->dx;
          nodenum = node->children[(unsigned)(right >= left)];
        }
      }
    } else {
      nodenum = node->children[/*node->PointOnSide(point)*/(unsigned)(dist <= 0.0f)];
    }
  } while (BSPIDX_IS_NON_LEAF(nodenum));
  return &Subsectors[BSPIDX_LEAF_SUBSECTOR(nodenum)];
}


//==========================================================================
//
//  VLevel::PointInSubsector
//
//  bugfixed algo
//
//==========================================================================
subsector_t *VLevel::PointInSubsector (const TVec &point) const noexcept {
  // single subsector is a special case
  if (!NumNodes || NumSubsectors < 2) return Subsectors;
  int nodenum = NumNodes-1;
  do {
    const node_t *node = Nodes+nodenum;
    nodenum = node->children[node->PointOnSide(point)];
  } while (BSPIDX_IS_NON_LEAF(nodenum));
  return &Subsectors[BSPIDX_LEAF_SUBSECTOR(nodenum)];
}


//==========================================================================
//
//  VLevel::ClearReferences
//
//==========================================================================
void VLevel::ClearReferences () {
  Super::ClearReferences();
  // clear script refs
  for (int scidx = scriptThinkers.length()-1; scidx >= 0; --scidx) {
    VLevelScriptThinker *sth = scriptThinkers[scidx];
    if (sth && !sth->destroyed) sth->ClearReferences();
  }
  // clear sector refs
  sector_t *sec = Sectors;
  for (int i = NumSectors; i--; ++sec) {
    if (sec->SoundTarget && sec->SoundTarget->IsRefToCleanup()) sec->SoundTarget = nullptr;
    if (sec->FloorData && sec->FloorData->IsRefToCleanup()) sec->FloorData = nullptr;
    if (sec->CeilingData && sec->CeilingData->IsRefToCleanup()) sec->CeilingData = nullptr;
    if (sec->LightingData && sec->LightingData->IsRefToCleanup()) sec->LightingData = nullptr;
    if (sec->AffectorData && sec->AffectorData->IsRefToCleanup()) sec->AffectorData = nullptr;
    if (sec->ActionList && sec->ActionList->IsRefToCleanup()) sec->ActionList = nullptr;
  }
  // clear polyobject refs
  polyobj_t **pop = PolyObjs;
  for (int i = NumPolyObjs; i--; ++pop) {
    polyobj_t *po = *pop;
    if (po->SpecialData && po->SpecialData->IsRefToCleanup()) po->SpecialData = nullptr;
  }
  // cameras
  if (CameraTextures.length()) {
    VCameraTextureInfo *cam = CameraTextures.ptr();
    for (int i = CameraTextures.length(); i--; ++cam) {
      if (cam->Camera && cam->Camera->IsRefToCleanup()) cam->Camera = nullptr;
    }
  }
  // static lights will be cleaned in thinker remover
  // renderer
  #ifdef CLIENT
  if (Renderer) Renderer->ClearReferences();
  #endif
}


//==========================================================================
//
//  VLevel::KillAllMapDecals
//
//==========================================================================
void VLevel::KillAllMapDecals () {
  // remove all wall decals
  if (Segs) for (auto &&seg : allSegs()) seg.killAllDecals(this);
  // remove all flat decals
  KillAllSubsectorDecals();
  // decal animation list should be empty too
  vassert(!decanimlist);
  // clear subsector decal list (it should be empty anyway)
  if (subsectorDecalList) {
    delete[] subsectorDecalList;
    subsectorDecalList = nullptr;
  }
}


//==========================================================================
//
//  VLevel::ClearAllMapData
//
//  this is also used in dtor
//
//==========================================================================
void VLevel::ClearAllMapData () {
  delete UserLineKeyInfo;
  UserLineKeyInfo = nullptr;

  delete UserThingKeyInfo;
  UserThingKeyInfo = nullptr;

  delete UserSectorKeyInfo;
  UserSectorKeyInfo = nullptr;

  delete UserSideKeyInfo0;
  UserSideKeyInfo0 = nullptr;

  delete UserSideKeyInfo1;
  UserSideKeyInfo1 = nullptr;

  delete UserLineIdxKeyInfo;
  UserLineIdxKeyInfo = nullptr;

  delete UserSideIdxKeyInfo;
  UserSideIdxKeyInfo = nullptr;

  KillAllMapDecals();

  if (Sectors) {
    for (auto &&sec : allSectors()) {
      sec.DeleteAllRegions();
      sec.moreTags.clear();
    }
    // line buffer is shared, so this correctly deletes it
    delete[] Sectors[0].lines;
    Sectors[0].lines = nullptr;
    delete[] Sectors[0].nbsecs;
    Sectors[0].nbsecs = nullptr;
  }

  if (Lines) {
    for (auto &&line : allLines()) {
      delete[] line.v1lines;
      delete[] line.v2lines;
      line.moreTags.clear();
    }
  }

  delete[] Vertexes;
  Vertexes = nullptr;
  NumVertexes = 0;

  delete[] Sectors;
  Sectors = nullptr;
  NumSectors = 0;

  delete[] Sides;
  Sides = nullptr;
  NumSides = 0;

  delete[] Lines;
  Lines = nullptr;
  NumLines = 0;

  delete[] Segs;
  Segs = nullptr;
  NumSegs = 0;

  delete[] Subsectors;
  Subsectors = nullptr;
  NumSubsectors = 0;

  delete[] Nodes;
  Nodes = nullptr;
  NumNodes = 0;

  delete[] BlockMapLump;
  BlockMapLump = nullptr;
  BlockMapLumpSize = 0;

  delete[] BlockLinks;
  BlockLinks = nullptr;

  delete[] RejectMatrix;
  RejectMatrix = nullptr;
  RejectMatrixSize = 0;

  delete[] Things;
  Things = nullptr;
  NumThings = 0;

  delete[] Zones;
  Zones = nullptr;
  NumZones = 0;

  GTextureManager.ResetMapTextures();

  dcSubTouchMark.resetNoDtor();
  dcPobjTouched.reset();
  dcSubTouchCounter = 0;
  dcPobjTouchedReset = false;

  dcLineTouchMark.resetNoDtor();
  dcSegTouchMark.resetNoDtor();
  dcLineTouchCounter = 0;
}


//==========================================================================
//
//  VLevel::Destroy
//
//==========================================================================
void VLevel::Destroy () {
  // destroy all thinkers (including scripts)
  DestroyAllThinkers();

  // free fake floor data
  for (auto &&sector : allSectors()) {
    if (sector.fakefloors) {
      delete sector.fakefloors;
      sector.fakefloors = nullptr;
    }
  }

  tagHashFree(lineTags);
  tagHashFree(sectorTags);

  if (csTouched) Z_Free(csTouched);
  csTouchCount = 0;
  csTouched = nullptr;

  while (HeadSecNode) {
    msecnode_t *Node = HeadSecNode;
    HeadSecNode = Node->SNext;
    delete Node;
    Node = nullptr;
  }

  // free render data
  #ifdef CLIENT
  if (Renderer) {
    delete Renderer;
    Renderer = nullptr;
  }
  #else
  vassert(!Renderer);
  #endif

  if (PolyBlockMap) {
    for (int i = 0; i < BlockMapWidth*BlockMapHeight; ++i) {
      polyblock_t *n;
      for (polyblock_t *pb = PolyBlockMap[i]; pb; pb = n) {
        n = pb->next;
        delete pb;
      }
    }
    delete[] PolyBlockMap;
    PolyBlockMap = nullptr;
  }

  if (PolyObjs) {
    for (int i = 0; i < NumPolyObjs; ++i) {
      PolyObjs[i]->Free();
      delete PolyObjs[i];
    }
    delete[] PolyObjs;
    PolyObjs = nullptr;
  }
  NumPolyObjs = 0;

  if (PolyAnchorPoints) {
    delete[] PolyAnchorPoints;
    PolyAnchorPoints = nullptr;
  }
  NumPolyAnchorPoints = 0;

  if (PolyLinks3D) {
    delete[] PolyLinks3D;
    PolyLinks3D = nullptr;
  }
  NumPolyLinks3D = 0;

  if (PolyTagMap) {
    PolyTagMap->clear();
    delete PolyTagMap;
    PolyTagMap = nullptr;
  }

  if (suid2ent) {
    suid2ent->clear();
    delete suid2ent;
    suid2ent = nullptr;
  }

  ClearAllMapData(); // this also clears decals

  delete[] BaseLines;
  BaseLines = nullptr;
  delete[] BaseSides;
  BaseSides = nullptr;
  delete[] BaseSectors;
  BaseSectors = nullptr;
  delete[] BasePolyObjs;
  BasePolyObjs = nullptr;

  if (Acs) {
    delete Acs;
    Acs = nullptr;
  }

  if (GenericSpeeches) {
    delete[] GenericSpeeches;
    GenericSpeeches = nullptr;
  }

  if (LevelSpeeches) {
    delete[] LevelSpeeches;
    LevelSpeeches = nullptr;
  }

  StaticLights.clear();
  if (StaticLightsMap) {
    StaticLightsMap->clear();
    delete StaticLightsMap;
    StaticLightsMap = nullptr;
  }

  ActiveSequences.Clear();

  for (int i = 0; i < Translations.length(); ++i) {
    if (Translations[i]) {
      delete Translations[i];
      Translations[i] = nullptr;
    }
  }
  Translations.Clear();

  for (int i = 0; i < BodyQueueTrans.length(); ++i) {
    if (BodyQueueTrans[i]) {
      delete BodyQueueTrans[i];
      BodyQueueTrans[i] = nullptr;
    }
  }
  BodyQueueTrans.Clear();

  if (processedBMCells) Z_Free(processedBMCells);
  processedBMCells = nullptr;
  processedBMCellsSize = 0;

  if (pathIntercepts) Z_Free(pathIntercepts);
  pathIntercepts = nullptr;
  pathInterceptsAlloted = pathInterceptsUsed = 0;

  if (tempPathIntercepts) Z_Free(tempPathIntercepts);
  tempPathIntercepts = nullptr;
  tempPathInterceptsAlloted = tempPathInterceptsUsed = 0;

  GTextureManager.ResetMapTextures();

  // call parent class' `Destroy()` method
  Super::Destroy();
}


//==========================================================================
//
//  VLevel::ResetStaticLights
//
//==========================================================================
void VLevel::ResetStaticLights () {
  StaticLights.clear();
  if (StaticLightsMap) StaticLightsMap->clear();
}


//==========================================================================
//
//  VLevel::AddStaticLightRGB
//
//==========================================================================
void VLevel::AddStaticLightRGB (vuint32 owneruid, const VLightParams &lpar, const vuint32 flags) {
  if (owneruid && !StaticLightsMap) StaticLightsMap = new TMapNC<vuint32, int>();
  const int idx = StaticLights.length();
  rep_light_t &L = StaticLights.alloc();
  L.OwnerUId = owneruid;
  L.Origin = lpar.Origin;
  L.Radius = lpar.Radius;
  L.Color = lpar.Color;
  L.ConeDir = lpar.coneDirection;
  L.ConeAngle = lpar.coneAngle;
  L.LevelSector = lpar.LevelSector;
  L.LevelScale = lpar.LevelScale;
  if (L.LevelSector) {
    if (L.LevelScale == 0.0f) L.LevelScale = 1.0f;
    const float intensity = clampval(lpar.LevelSector->params.lightlevel*(fabsf(lpar.LevelScale)*0.125f), 0.0f, 255.0f);
    L.Radius = VLevelInfo::GZSizeToRadius(intensity, (lpar.LevelScale < 0.0f), 2.0f);
  }
  L.Flags = flags|rep_light_t::LightChanged|rep_light_t::LightActive;
  if (owneruid) {
    vassert(StaticLightsMap);
    auto oidxp = StaticLightsMap->find(owneruid);
    if (oidxp) StaticLights[*oidxp].OwnerUId = 0; //FIXME!
    StaticLightsMap->put(owneruid, idx);
  }
  #ifdef CLIENT
  if (Renderer) Renderer->AddStaticLightRGB(owneruid, lpar, flags);
  #endif
}


//==========================================================================
//
//  VLevel::MoveStaticLightByOwner
//
//==========================================================================
void VLevel::MoveStaticLightByOwner (vuint32 owneruid, const TVec &Origin) {
  if (!owneruid) return;
  if (!StaticLightsMap) return; // no owned lights
  auto oidxp = StaticLightsMap->find(owneruid);
  if (!oidxp) return; // no such owned light
  // check if it is moved far enough
  rep_light_t &sl = StaticLights[*oidxp];
  if (fabsf(sl.Origin.x-Origin.x) <= 4.0f &&
      fabsf(sl.Origin.y-Origin.y) <= 4.0f &&
      fabsf(sl.Origin.z-Origin.z) <= 4.0f)
  {
    return;
  }
  StaticLights[*oidxp].Origin = Origin;
  #ifdef CLIENT
  if (Renderer) Renderer->MoveStaticLightByOwner(owneruid, Origin);
  #endif
}


//==========================================================================
//
//  VLevel::RemoveStaticLightByOwner
//
//==========================================================================
void VLevel::RemoveStaticLightByOwner (vuint32 owneruid) {
  if (!owneruid) return;
  if (!StaticLightsMap) return; // no owned lights
  auto oidxp = StaticLightsMap->find(owneruid);
  if (!oidxp) return; // no such owned light
  rep_light_t &sl = StaticLights[*oidxp];
  sl.Flags = rep_light_t::LightChanged;
  sl.Flags &= ~rep_light_t::LightActive;
  sl.OwnerUId = 0;
  StaticLightsMap->del(owneruid);
  #ifdef CLIENT
  if (Renderer) Renderer->RemoveStaticLightByOwner(owneruid);
  #endif
}


//==========================================================================
//
//  VLevel::AddStaticLightRGB
//
//==========================================================================
void VLevel::AddStaticLightRGB (VEntity *Ent, const VLightParams &lpar, const vuint32 flags) {
  return AddStaticLightRGB((Ent ? Ent->/*GetUniqueId()*/ServerUId : 0u), lpar, flags);
}


//==========================================================================
//
//  VLevel::MoveStaticLightByOwner
//
//==========================================================================
void VLevel::MoveStaticLightByOwner (VEntity *Ent, const TVec &Origin) {
  if (!Ent) return;
  if (Ent->IsGoingToDie()) return; // the engine will remove it on GC
  MoveStaticLightByOwner(Ent->/*GetUniqueId()*/ServerUId, Origin);
}


//==========================================================================
//
//  VLevel::RemoveStaticLightByOwner
//
//==========================================================================
void VLevel::RemoveStaticLightByOwner (VEntity *Ent) {
  if (!Ent) return;
  if (Ent->IsGoingToDie()) return; // the engine will remove it on GC
  RemoveStaticLightByOwner(Ent->/*GetUniqueId()*/ServerUId);
}


//==========================================================================
//
//  VLevel::SetCameraToTexture
//
//==========================================================================
void VLevel::SetCameraToTexture (VEntity *Ent, VName TexName, int FOV) {
  if (!Ent) return;

  // get texture index
  int TexNum = GTextureManager.CheckNumForName(TexName, TEXTYPE_Wall, true);
  if (TexNum < 0) {
    GCon->Logf("SetCameraToTexture: %s is not a valid texture", *TexName);
    return;
  }

  // make camera to be always relevant
  Ent->ThinkerFlags |= VEntity::TF_AlwaysRelevant;

  for (int i = 0; i < CameraTextures.length(); ++i) {
    if (CameraTextures[i].TexNum == TexNum) {
      CameraTextures[i].Camera = Ent;
      CameraTextures[i].FOV = FOV;
      return;
    }
  }

  VCameraTextureInfo &C = CameraTextures.Alloc();
  C.Camera = Ent;
  C.TexNum = TexNum;
  C.FOV = FOV;
}


//==========================================================================
//
//  VLevel::SetBodyQueueTrans
//
//==========================================================================
int VLevel::SetBodyQueueTrans (int Slot, int Trans) {
  int Type = Trans>>TRANSL_TYPE_SHIFT;
  int Index = Trans&((1<<TRANSL_TYPE_SHIFT)-1);
  if (Type != TRANSL_Player) return Trans;
  if (Slot < 0 || Slot > MAX_BODY_QUEUE_TRANSLATIONS || Index < 0 ||
      Index >= MAXPLAYERS || !LevelInfo->Game->Players[Index])
  {
    return Trans;
  }

  // add it
  while (BodyQueueTrans.length() <= Slot) BodyQueueTrans.Append(nullptr);
  VTextureTranslation *Tr = BodyQueueTrans[Slot];
  if (!Tr) {
    Tr = new VTextureTranslation;
    BodyQueueTrans[Slot] = Tr;
  }
  Tr->Clear();
  VBasePlayer *P = LevelInfo->Game->Players[Index];
  Tr->BuildPlayerTrans(P->TranslStart, P->TranslEnd, P->Color);
  return (TRANSL_BodyQueue<<TRANSL_TYPE_SHIFT)+Slot;
}


//==========================================================================
//
//  VLevel::FindSectorFromTag
//
//==========================================================================
int VLevel::FindSectorFromTag (sector_t *&sector, int tag, int start) {
  //k8: just in case
  if (tag == -1 || NumSubsectors < 1) {
    sector = nullptr;
    return -1;
  }

  if (tag == 0) {
    // this has to be special
    if (start < -1) start = -1; // first
    // find next untagged sector
    for (++start; start < NumSectors; ++start) {
      sector_t *sec = &Sectors[start];
      if (sec->sectorTag || sec->moreTags.length()) continue;
      sector = sec;
      return start;
    }
    sector = nullptr;
    return -1;
  }

  if (start < 0) {
    // first
    start = tagHashFirst(sectorTags, tag);
  } else {
    // next
    start = tagHashNext(sectorTags, start, tag);
  }
  sector = (sector_t *)tagHashPtr(sectorTags, start);
  return start;

  /*
  if (tag == 0 || NumSectors < 1) return -1; //k8: just in case
  for (int i = start < 0 ? Sectors[(vuint32)tag%(vuint32)NumSectors].HashFirst : Sectors[start].HashNext;
       i >= 0;
       i = Sectors[i].HashNext)
  {
    if (Sectors[i].tag == tag) return i;
  }
  return -1;
  */
}


//==========================================================================
//
//  VLevel::FindLine
//
//==========================================================================
line_t *VLevel::FindLine (int lineTag, int *searchPosition) {
  if (!searchPosition) return nullptr;
  //k8: should zero tag be allowed here?
  if (lineTag == -1 || NumLines < 1) { *searchPosition = -1; return nullptr; }

  if (lineTag == 0) {
    int start = (searchPosition ? *searchPosition : -1);
    if (start < -1) start = -1;
    // find next untagged line
    for (++start; start < NumLines; ++start) {
      line_t *ldef = &Lines[start];
      if (ldef->lineTag || ldef->moreTags.length()) continue;
      *searchPosition = start;
      return ldef;
    }
    *searchPosition = -1;
    return nullptr;
  }

  if (*searchPosition < 0) {
    // first
    *searchPosition = tagHashFirst(lineTags, lineTag);
  } else {
    // next
    *searchPosition = tagHashNext(lineTags, *searchPosition, lineTag);
  }
  return (line_t *)tagHashPtr(lineTags, *searchPosition);

  /*
  if (NumLines > 0) {
    for (int i = *searchPosition < 0 ? Lines[(vuint32)lineTag%(vuint32)NumLines].HashFirst : Lines[*searchPosition].HashNext;
         i >= 0;
         i = Lines[i].HashNext)
    {
      if (Lines[i].LineTag == lineTag) {
        *searchPosition = i;
        return &Lines[i];
      }
    }
  }
  *searchPosition = -1;
  return nullptr;
  */
}


//==========================================================================
//
//  VLevel::SectorSetLink
//
//==========================================================================
void VLevel::SectorSetLink (int controltag, int tag, int surface, int movetype) {
  if (controltag <= 0) return;
  if (tag <= 0) return;
  if (tag == controltag) return;
  //FIXME: just enough to let annie working
  if (surface != 0 || movetype != 1) {
    GCon->Logf(NAME_Warning, "UNIMPLEMENTED: setting sector link: controltag=%d; tag=%d; surface=%d; movetype=%d", controltag, tag, surface, movetype);
    return;
  }
  sector_t *ccsector;
  for (int cshidx = FindSectorFromTag(ccsector, controltag); cshidx >= 0; cshidx = FindSectorFromTag(ccsector, controltag, cshidx)) {
    const int csi = (int)(ptrdiff_t)(ccsector-&Sectors[0]);
    sector_t *sssector;
    for (int lshidx = FindSectorFromTag(sssector, tag); lshidx >= 0; lshidx = FindSectorFromTag(sssector, tag, lshidx)) {
      const int lsi = (int)(ptrdiff_t)(sssector-&Sectors[0]);
      if (lsi == csi) continue;
      if (csi < sectorlinkStart.length()) {
        int f = sectorlinkStart[csi];
        while (f >= 0) {
          if (sectorlinks[f].index == lsi) break;
          f = sectorlinks[f].next;
        }
        if (f >= 0) continue;
      }
      // add it
      //GCon->Logf("linking sector #%d (tag=%d) to sector #%d (controltag=%d)", lsi, tag, csi, controltag);
      while (csi >= sectorlinkStart.length()) sectorlinkStart.append(-1);
      //GCon->Logf("  csi=%d; len=%d", csi, sectorlinkStart.length());
      //GCon->Logf("  csi=%d; sl=%d", csi, sectorlinkStart[csi]);
      // allocate sectorlink
      int slidx = sectorlinks.length();
      VSectorLink &sl = sectorlinks.alloc();
      sl.index = lsi;
      sl.mts = (movetype&0x0f)|(surface ? 1<<30 : 0);
      sl.next = sectorlinkStart[csi];
      sectorlinkStart[csi] = slidx;
    }
  }
}


//==========================================================================
//
//  VLevel::AppendControlLink
//
//  returns `false` for duplicate/invalid link
//
//==========================================================================
bool VLevel::AppendControlLink (const sector_t *src, const sector_t *dest) {
  if (!src || !dest || src == dest) return false; // just in case

  if (ControlLinks.length() == 0) {
    // first time, create empty array
    ControlLinks.setLength(NumSectors);
    for (auto &&link : ControlLinks) {
      link.src = -1;
      link.dest = -1;
      link.next = -1;
    }
  }

  const int srcidx = (int)(ptrdiff_t)(src-Sectors);
  const int destidx = (int)(ptrdiff_t)(dest-Sectors);
  VCtl2DestLink *lnk = &ControlLinks[srcidx];
  if (lnk->dest < 0) {
    // first slot
    vassert(lnk->src == -1);
    vassert(lnk->next == -1);
    lnk->src = srcidx;
    lnk->dest = destidx;
    lnk->next = -1;
  } else {
    // find list tail
    int lastidx = srcidx;
    for (;;) {
      vassert(ControlLinks[lastidx].src == srcidx);
      if (ControlLinks[lastidx].dest == destidx) return false;
      int nli = ControlLinks[lastidx].next;
      if (nli < 0) break;
      lastidx = nli;
    }
    // append to list
    int newidx = ControlLinks.length();
    VCtl2DestLink *newlnk = &ControlLinks.alloc();
    lnk = &ControlLinks[lastidx]; // refresh lnk, because array might be reallocated
    vassert(lnk->next == -1);
    lnk->next = newidx;
    newlnk->src = srcidx;
    newlnk->dest = destidx;
    newlnk->next = -1;
  }

  #if 0
  GCon->Logf(NAME_Debug, "=== AppendControlLink (src=%d; dst=%d) ===", srcidx, destidx);
  for (auto it = IterControlLinks(src); !it.isEmpty(); it.next()) {
    GCon->Logf(NAME_Debug, "   dest=%d", it.getDestSectorIndex());
  }
  #endif

  return true;
}


//==========================================================================
//
//  VLevel::dumpSectorRegions
//
//==========================================================================
void VLevel::dumpRegion (const sec_region_t *reg) {
  if (!reg) return;
  char xflags[128];
  xflags[0] = 0;
  if (reg->regflags&sec_region_t::RF_BaseRegion) strcat(xflags, " [base]");
  if (reg->regflags&sec_region_t::RF_SaneRegion) strcat(xflags, " [sane]");
  if (reg->regflags&sec_region_t::RF_NonSolid) strcat(xflags, " [non-solid]");
  if (reg->regflags&sec_region_t::RF_OnlyVisual) strcat(xflags, " [visual]");
  if (reg->regflags&sec_region_t::RF_SkipFloorSurf) strcat(xflags, " [skip-floor]");
  if (reg->regflags&sec_region_t::RF_SkipCeilSurf) strcat(xflags, " [skip-ceil]");
  if (reg->params && reg->params->contents) strcat(xflags, va(" [contents:%u]", reg->params->contents));
  GCon->Logf("  %p: floor=(%g,%g,%g:%g)%s; (%g : %g), flags=0x%04x; ceil=(%g,%g,%g:%g)%s; (%g : %g), flags=0x%04x; eline=%d; rflags=0x%02x%s",
    reg,
    reg->efloor.GetNormal().x, reg->efloor.GetNormal().y, reg->efloor.GetNormal().z, reg->efloor.GetDist(), (reg->efloor.isFloor() ? " OK" : ""),
    reg->efloor.splane->minz, reg->efloor.splane->maxz,
    reg->efloor.splane->flags,
    reg->eceiling.GetNormal().x, reg->eceiling.GetNormal().y, reg->eceiling.GetNormal().z, reg->eceiling.GetDist(), (reg->eceiling.isCeiling() ? " OK" : ""),
    reg->eceiling.splane->minz, reg->eceiling.splane->maxz,
    reg->eceiling.splane->flags,
    (reg->extraline ? 1 : 0),
    reg->regflags, xflags);
}


//==========================================================================
//
//  VLevel::dumpSectorRegions
//
//==========================================================================
void VLevel::dumpSectorRegions (const sector_t *dst) {
  GCon->Logf(" === bot -> top (sector: %p) ===", dst);
  for (const sec_region_t *reg = dst->eregions; reg; reg = reg->next) dumpRegion(reg);
  GCon->Log("--------");
}


//==========================================================================
//
//  VLevel::CalcEntityLight
//
//==========================================================================
vuint32 VLevel::CalcEntityLight (VEntity *lowner, unsigned flags) {
  if (!lowner) return 0u;
#ifdef CLIENT
  if (Drawer) return Drawer->CalcEntityLight(lowner, flags);
#endif
  return 0u;
}


//==========================================================================
//
//  Natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VLevel, GetLineIndex) {
  P_GET_PTR(line_t, line);
  P_GET_SELF;
  int idx = -1;
  if (line) idx = (int)(ptrdiff_t)(line-Self->Lines);
  RET_INT(idx);
}

IMPLEMENT_FUNCTION(VLevel, PointInSector) {
  P_GET_VEC(Point);
  P_GET_SELF;
  RET_PTR(Self->PointInSubsector_Buggy(Point)->sector);
}

IMPLEMENT_FUNCTION(VLevel, PointInSubsector) {
  P_GET_VEC(Point);
  P_GET_SELF;
  RET_PTR(Self->PointInSubsector_Buggy(Point));
}

IMPLEMENT_FUNCTION(VLevel, PointInSectorRender) {
  P_GET_VEC(Point);
  P_GET_SELF;
  RET_PTR(Self->PointInSubsector(Point)->sector);
}

IMPLEMENT_FUNCTION(VLevel, PointInSubsectorRender) {
  P_GET_VEC(Point);
  P_GET_SELF;
  RET_PTR(Self->PointInSubsector(Point));
}

IMPLEMENT_FUNCTION(VLevel, ChangeSector) {
  P_GET_INT(crunch);
  P_GET_PTR(sector_t, sec);
  P_GET_SELF;
  RET_BOOL(Self->ChangeSector(sec, crunch));
}

IMPLEMENT_FUNCTION(VLevel, ChangeOneSectorInternal) {
  P_GET_PTR(sector_t, sec);
  P_GET_SELF;
  Self->ChangeOneSectorInternal(sec);
}

IMPLEMENT_FUNCTION(VLevel, AddExtraFloor) {
  line_t *line;
  sector_t *dst;
  vobjGetParamSelf(line, dst);
  if (dst && line && line->frontsector) Self->AddExtraFloor(line, dst);
}

IMPLEMENT_FUNCTION(VLevel, SwapPlanes) {
  P_GET_PTR(sector_t, s);
  P_GET_SELF;
  (void)Self;
  SwapPlanes(s);
}

IMPLEMENT_FUNCTION(VLevel, SetFloorLightSector) {
  P_GET_PTR(sector_t, SrcSector);
  P_GET_PTR(sector_t, Sector);
  P_GET_SELF;
  Sector->floor.LightSourceSector = SrcSector-Self->Sectors;
}

IMPLEMENT_FUNCTION(VLevel, SetCeilingLightSector) {
  P_GET_PTR(sector_t, SrcSector);
  P_GET_PTR(sector_t, Sector);
  P_GET_SELF;
  Sector->ceiling.LightSourceSector = SrcSector-Self->Sectors;
}

// native final bool SetHeightSector (sector_t *Sector, sector_t *SrcSector);
// see https://zdoom.org/wiki/Transfer_Heights for flags
IMPLEMENT_FUNCTION(VLevel, SetHeightSector) {
  sector_t *Sector, *SrcSector;
  vobjGetParamSelf(Sector, SrcSector);
  if (!Sector || !SrcSector || Sector->heightsec == SrcSector/*nothing to do*/) {
    RET_BOOL(false);
    return;
  }
  const int destidx = (int)(ptrdiff_t)(Sector-&Self->Sectors[0]);
  const int srcidx = (int)(ptrdiff_t)(SrcSector-&Self->Sectors[0]);
  if (Sector->heightsec) GCon->Logf(NAME_Warning, "tried to set height sector for already set height sector: dest=%d; src=%d", destidx, srcidx);
  if (Sector->fakefloors) Sector->eregions->params = &Sector->params; // this may be called for already inited sector, so restore params
  Sector->heightsec = SrcSector;
  // add to list
  bool found = false;
  for (int xidx = 0; xidx < Self->FakeFCSectors.length(); ++xidx) if (Self->FakeFCSectors[xidx] == srcidx) { found = true; break; }
  if (!found) Self->FakeFCSectors.append(srcidx);
  #ifdef CLIENT
  if (Self->Renderer) Self->Renderer->SetupFakeFloors(Sector);
  #endif
  RET_BOOL(true);
}

// native final int FindSectorFromTag (out sector_t *Sector, int tag, optional int start);
IMPLEMENT_FUNCTION(VLevel, FindSectorFromTag) {
  P_GET_INT_OPT(start, -1);
  P_GET_INT(tag);
  P_GET_PTR(sector_t *, osectorp);
  P_GET_SELF;
  sector_t *sector;
  start = Self->FindSectorFromTag(sector, tag, start);
  if (osectorp) *osectorp = sector;
  RET_INT(start);
  //RET_INT(Self->FindSectorFromTag(tag, start));
}

IMPLEMENT_FUNCTION(VLevel, FindLine) {
  P_GET_PTR(int, searchPosition);
  P_GET_INT(lineTag);
  P_GET_SELF;
  RET_PTR(Self->FindLine(lineTag, searchPosition));
}

//native final void SectorSetLink (int controltag, int tag, int surface, int movetype);
IMPLEMENT_FUNCTION(VLevel, SectorSetLink) {
  P_GET_INT(movetype);
  P_GET_INT(surface);
  P_GET_INT(tag);
  P_GET_INT(controltag);
  P_GET_SELF;
  Self->SectorSetLink(controltag, tag, surface, movetype);
}

IMPLEMENT_FUNCTION(VLevel, SetBodyQueueTrans) {
  P_GET_INT(Trans);
  P_GET_INT(Slot);
  P_GET_SELF;
  RET_INT(Self->SetBodyQueueTrans(Slot, Trans));
}


// native final bool GetUDMFLineInt (int id, name keyname, out int res);
IMPLEMENT_FUNCTION(VLevel, GetUDMFLineInt) {
  vint32 id;
  VName name;
  vint32 *res;
  vobjGetParamSelf(id, name, res);
  const VCustomKeyInfo::Value *val = Self->findLineKey(id, *name);
  if (val && res) *res = val->i;
  RET_BOOL(!!val);
}

// native final bool GetUDMFLineFloat (int id, name keyname, out float res);
IMPLEMENT_FUNCTION(VLevel, GetUDMFLineFloat) {
  vint32 id;
  VName name;
  float *res;
  vobjGetParamSelf(id, name, res);
  const VCustomKeyInfo::Value *val = Self->findLineKey(id, *name);
  if (val && res) *res = val->f;
  RET_BOOL(!!val);
}

// native final bool GetUDMFThingInt (int id, name keyname, out int res);
IMPLEMENT_FUNCTION(VLevel, GetUDMFThingInt) {
  vint32 id;
  VName name;
  vint32 *res;
  vobjGetParamSelf(id, name, res);
  const VCustomKeyInfo::Value *val = Self->findThingKey(id, *name);
  if (val && res) *res = val->i;
  RET_BOOL(!!val);
}

// native final bool GetUDMFThingFloat (int id, name keyname, out float res);
IMPLEMENT_FUNCTION(VLevel, GetUDMFThingFloat) {
  vint32 id;
  VName name;
  float *res;
  vobjGetParamSelf(id, name, res);
  const VCustomKeyInfo::Value *val = Self->findThingKey(id, *name);
  if (val && res) *res = val->f;
  RET_BOOL(!!val);
}

// native final bool GetUDMFSectorInt (int id, name keyname, out int res);
IMPLEMENT_FUNCTION(VLevel, GetUDMFSectorInt) {
  vint32 id;
  VName name;
  vint32 *res;
  vobjGetParamSelf(id, name, res);
  const VCustomKeyInfo::Value *val = Self->findSectorKey(id, *name);
  if (val && res) *res = val->i;
  RET_BOOL(!!val);
}

// native final bool GetUDMFSectorFloat (int id, name keyname, out float res);
IMPLEMENT_FUNCTION(VLevel, GetUDMFSectorFloat) {
  vint32 id;
  VName name;
  float *res;
  vobjGetParamSelf(id, name, res);
  const VCustomKeyInfo::Value *val = Self->findSectorKey(id, *name);
  if (val && res) *res = val->f;
  RET_BOOL(!!val);
}

// native final bool GetUDMFSideInt (int sidenum, int id, name keyname, out int res);
IMPLEMENT_FUNCTION(VLevel, GetUDMFSideInt) {
  vint32 sidenum, id;
  VName name;
  vint32 *res;
  vobjGetParamSelf(sidenum, id, name, res);
  const VCustomKeyInfo::Value *val = Self->findSideKey(sidenum, id, *name);
  if (val && res) *res = val->i;
  RET_BOOL(!!val);
}

// native final bool GetUDMFSideFloat (int sidenum, int id, name keyname, out float res);
IMPLEMENT_FUNCTION(VLevel, GetUDMFSideFloat) {
  vint32 sidenum, id;
  VName name;
  float *res;
  vobjGetParamSelf(sidenum, id, name, res);
  const VCustomKeyInfo::Value *val = Self->findSideKey(sidenum, id, *name);
  if (val && res) *res = val->f;
  RET_BOOL(!!val);
}

// native final int GetNextValidCount ();
IMPLEMENT_FUNCTION(VLevel, GetNextValidCount) {
  vobjGetParamSelf();
  Self->IncrementValidCount();
  RET_INT(validcount);
}

// native final int GetNextVisitedCount ();
IMPLEMENT_FUNCTION(VLevel, GetNextVisitedCount) {
  vobjGetParamSelf();
  const vint32 vc = Self->nextVisitedCount();
  RET_INT(vc);
}


#ifdef SERVER
//==========================================================================
//
//  SV_LoadLevel
//
//==========================================================================
void SV_LoadLevel (VName MapName) {
#ifdef CLIENT
  GClLevel = nullptr;
#endif
  if (GLevel) {
    delete GLevel;
    GLevel = nullptr;
  }

  GLevel = SpawnWithReplace<VLevel>();
  GLevel->LevelFlags |= VLevel::LF_ForServer;

  GLevel->LoadMap(MapName);
  Host_ResetSkipFrames();

  // reset valid counts
  validcount = 0;
  validcountState = 0;
}
#endif


#ifdef CLIENT
//==========================================================================
//
//  CL_LoadLevel
//
//==========================================================================
void CL_LoadLevel (VName MapName) {
  if (GClLevel) {
    delete GClLevel;
    GClLevel = nullptr;
  }

  GClLevel = SpawnWithReplace<VLevel>();
  GClGame->GLevel = GClLevel;

  GClLevel->LoadMap(MapName);
  Host_ResetSkipFrames();
}
#endif

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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
#include "gamedefs.h"


struct VViewClipper::VClipNode {
  float From;
  float To;
  VClipNode *Prev;
  VClipNode *Next;
};


#ifdef CLIENT
extern VCvarF r_lights_radius;
#else
// light tracing is not used in dedicated server, but we still need this var
// it is better than throwing more ifdefs all over the source
static float r_lights_radius;
#endif

static VCvarB clip_bsp("clip_bsp", true, "Clip geometry behind some BSP nodes?"/*, CVAR_Archive*/);
static VCvarB clip_enabled("clip_enabled", true, "Do geometry cliping optimizations?"/*, CVAR_Archive*/);
static VCvarB clip_trans_hack("clip_trans_hack", true, "Do translucent clipping hack?"/*, CVAR_Archive*/);
static VCvarB clip_with_polyobj("clip_with_polyobj", true, "Do clipping with polyobjects?"/*, CVAR_Archive*/);

static VCvarB clip_platforms("clip_platforms", true, "Clip geometry behind some closed doors and lifts?"/*, CVAR_Archive*/);
//static VCvarB clip_platforms_doors("clip_platforms_doors", true, "Clip geometry behind some closed doors?"/*, CVAR_Archive*/);
//static VCvarB clip_platforms_lift_lo("clip_platforms_lift_lo", true, "Clip geometry behind lowered lifts?"/*, CVAR_Archive*/);
//static VCvarB clip_platforms_lift_hi("clip_platforms_lift_hi", true, "Clip geometry behind raised lifts?"/*, CVAR_Archive*/);


/*
  // Mirrors and horizons always block the view
  //if (linedef->special==Line_Mirror || linedef->special==Line_Horizon) return true;

  // Lines with stacked sectors must never block!

  if (backsector->portals[sector_t::ceiling] != NULL || backsector->portals[sector_t::floor] != NULL ||
    frontsector->portals[sector_t::ceiling] != NULL || frontsector->portals[sector_t::floor] != NULL)
  {
    return false;
  }

  // on large levels this distinction can save some time
  // That's a lot of avoided multiplications if there's a lot to see!

  if (frontsector->ceilingplane.a | frontsector->ceilingplane.b)
  {
    fs_ceilingheight1=frontsector->ceilingplane.ZatPoint(linedef->v1);
    fs_ceilingheight2=frontsector->ceilingplane.ZatPoint(linedef->v2);
  }
  else
  {
    fs_ceilingheight2=fs_ceilingheight1=frontsector->ceilingplane.d;
  }

  if (frontsector->floorplane.a | frontsector->floorplane.b)
  {
    fs_floorheight1=frontsector->floorplane.ZatPoint(linedef->v1);
    fs_floorheight2=frontsector->floorplane.ZatPoint(linedef->v2);
  }
  else
  {
    fs_floorheight2=fs_floorheight1=-frontsector->floorplane.d;
  }

  if (backsector->ceilingplane.a | backsector->ceilingplane.b)
  {
    bs_ceilingheight1=backsector->ceilingplane.ZatPoint(linedef->v1);
    bs_ceilingheight2=backsector->ceilingplane.ZatPoint(linedef->v2);
  }
  else
  {
    bs_ceilingheight2=bs_ceilingheight1=backsector->ceilingplane.d;
  }

  if (backsector->floorplane.a | backsector->floorplane.b)
  {
    bs_floorheight1=backsector->floorplane.ZatPoint(linedef->v1);
    bs_floorheight2=backsector->floorplane.ZatPoint(linedef->v2);
  }
  else
  {
    bs_floorheight2=bs_floorheight1=-backsector->floorplane.d;
  }

  // now check for closed sectors!
  if (bs_ceilingheight1<=fs_floorheight1 && bs_ceilingheight2<=fs_floorheight2)
  {
    FTexture * tex = TexMan(sidedef->GetTexture(side_t::top));
    if (!tex || tex->UseType==FTexture::TEX_Null) return false;
    if (backsector->GetTexture(sector_t::ceiling)==skyflatnum &&
      frontsector->GetTexture(sector_t::ceiling)==skyflatnum) return false;
    return true;
  }

  if (fs_ceilingheight1<=bs_floorheight1 && fs_ceilingheight2<=bs_floorheight2)
  {
    FTexture * tex = TexMan(sidedef->GetTexture(side_t::bottom));
    if (!tex || tex->UseType==FTexture::TEX_Null) return false;

    // properly render skies (consider door "open" if both floors are sky):
    if (backsector->GetTexture(sector_t::ceiling)==skyflatnum &&
      frontsector->GetTexture(sector_t::ceiling)==skyflatnum) return false;
    return true;
  }

  if (bs_ceilingheight1<=bs_floorheight1 && bs_ceilingheight2<=bs_floorheight2)
  {
    // preserve a kind of transparent door/lift special effect:
    if (bs_ceilingheight1 < fs_ceilingheight1 || bs_ceilingheight2 < fs_ceilingheight2)
    {
      FTexture * tex = TexMan(sidedef->GetTexture(side_t::top));
      if (!tex || tex->UseType==FTexture::TEX_Null) return false;
    }
    if (bs_floorheight1 > fs_floorheight1 || bs_floorheight2 > fs_floorheight2)
    {
      FTexture * tex = TexMan(sidedef->GetTexture(side_t::bottom));
      if (!tex || tex->UseType==FTexture::TEX_Null) return false;
    }
    if (backsector->GetTexture(sector_t::ceiling)==skyflatnum &&
      frontsector->GetTexture(sector_t::ceiling)==skyflatnum) return false;
    if (backsector->GetTexture(sector_t::floor)==skyflatnum && frontsector->GetTexture(sector_t::floor)
      ==skyflatnum) return false;
    return true;
  }

  return false;
*/


//==========================================================================
//
//  IsSegAClosedSomething
//
//==========================================================================
static inline bool IsSegAClosedSomething (VLevel *Level, const seg_t *seg) {
  if (!clip_platforms) return false;

  if (seg->linedef->flags&ML_3DMIDTEX) return false; // 3dmidtex never blocks anything

  //TODO: check `heightsec` here (see `gl_CheckViewArea()` in Zandronum)
  auto fsec = seg->linedef->frontsector;
  auto bsec = seg->linedef->backsector;

  //if (!fsec || !bsec) return false;

  // only apply this to sectors without slopes
  if (fsec->floor.normal.z == 1.0f && bsec->floor.normal.z == 1.0f &&
      fsec->ceiling.normal.z == -1.0f && bsec->ceiling.normal.z == -1.0f)
  {
    if (seg->sidedef->TopTexture != -1 || // a seg without top texture isn't a door
        seg->sidedef->BottomTexture != -1 || // a seg without bottom texture isn't an elevator/plat
        seg->sidedef->MidTexture != -1) // a seg without mid texture isn't a polyobj door
    {
      const TVec vv1 = *seg->linedef->v1;
      const TVec vv2 = *seg->linedef->v2;

      const float frontcz1 = fsec->ceiling.GetPointZ(vv1);
      const float frontcz2 = fsec->ceiling.GetPointZ(vv2);
      check(frontcz1 == frontcz2);
      const float frontfz1 = fsec->floor.GetPointZ(vv1);
      const float frontfz2 = fsec->floor.GetPointZ(vv2);
      check(frontfz1 == frontfz2);

      const float backcz1 = bsec->ceiling.GetPointZ(vv1);
      const float backcz2 = bsec->ceiling.GetPointZ(vv2);
      check(backcz1 == backcz2);
      const float backfz1 = bsec->floor.GetPointZ(vv1);
      const float backfz2 = bsec->floor.GetPointZ(vv2);
      check(backfz1 == backfz2);

      // taken from Zandronum
      // now check for closed sectors
      if (backcz1 <= frontfz1 && backcz2 <= frontfz2) {
        // preserve a kind of transparent door/lift special effect:
        if (GTextureManager.IsEmptyTexture(seg->sidedef->TopTexture)) return false;
        // properly render skies (consider door "open" if both floors are sky):
        if (bsec->ceiling.pic == skyflatnum && fsec->ceiling.pic == skyflatnum) return false;
        return true;
      }

      if (frontcz1 <= backfz1 && frontcz2 <= backfz2) {
        // preserve a kind of transparent door/lift special effect:
        if (GTextureManager.IsEmptyTexture(seg->sidedef->BottomTexture)) return false;
        // properly render skies (consider door "open" if both floors are sky):
        if (bsec->ceiling.pic == skyflatnum && fsec->ceiling.pic == skyflatnum) return false;
        return true;
      }

      if (backcz1 <= backfz1 && backcz2 <= backfz2) {
        // preserve a kind of transparent door/lift special effect:
        if (backcz1 < frontcz1 || backcz2 < frontcz2) {
          if (GTextureManager.IsEmptyTexture(seg->sidedef->TopTexture)) return false;
        }
        if (backfz1 > frontfz1 || backfz2 > frontfz2) {
          if (GTextureManager.IsEmptyTexture(seg->sidedef->BottomTexture)) return false;
        }
        // properly render skies
        if (bsec->ceiling.pic == skyflatnum && fsec->ceiling.pic == skyflatnum) return false;
        if (bsec->floor.pic == skyflatnum && fsec->floor.pic == skyflatnum) return false;
        return true;
      }

#if 0 // k8 failed attempts

      int snum = (seg->sidedef == &Level->Sides[seg->linedef->sidenum[0]] ? 0 :
                  seg->sidedef == &Level->Sides[seg->linedef->sidenum[1]] ? 1 :
                  -1);
      check(snum >= 0);
      const side_t *s0 = &Level->Sides[seg->linedef->sidenum[0]];
      const side_t *s1 = &Level->Sides[seg->linedef->sidenum[1]];
      //const side_t *s2 = &Level->Sides[seg->linedef->sidenum[snum^1]];
#if 0
      {
        int lnum = (int)(ptrdiff_t)(seg->linedef-Level->Lines);
        if (/*lnum == 243 || lnum == 244*/ /*lnum == 336 || lnum == 337*/ lnum == 458 || lnum == 299)
        {
          int snum = (seg->sidedef == &Level->Sides[seg->linedef->sidenum[0]] ? 0 :
                      seg->sidedef == &Level->Sides[seg->linedef->sidenum[1]] ? 1 :
                      -1);
          GCon->Logf("%d: opened(%d:%d)! t=%d; b=%d; m=%d; fcz=(%f,%f); ffz=(%f,%f); bcz=(%f,%f); bfz=(%f,%f); alpha=%f",
            lnum,
            (int)(ptrdiff_t)(seg->sidedef-Level->Sides), snum,
            seg->sidedef->TopTexture.id, seg->sidedef->BottomTexture.id, seg->sidedef->MidTexture.id,
            frontcz1, frontcz2, frontfz1, frontfz2, backcz1, backcz2, backfz1, backfz2,
            seg->linedef->alpha);
        }
      }
#endif

      if (clip_platforms_doors) {
        if (backcz1 <= backfz1 && backcz1 <= backfz2 &&
            backcz2 <= backfz1 && backcz2 <= backfz2)
        {
          // this looks like a closed door
          // if front ceiling is higher than front floor, and we have top texture, this is a closed door
          if (frontcz1 > frontfz1 && frontcz1 > frontfz2 &&
              frontcz2 > frontfz1 && frontcz2 > frontfz2)
          {
            if (seg->sidedef->TopTexture > 0) return true;
            // check other side, this can be a back side of a door
            if (snum >= 0) {
              const side_t *s2 = &Level->Sides[seg->linedef->sidenum[snum^1]];
              if (s2->TopTexture > 0) return true;
            }
          }
        }
      }

      if (clip_platforms_lift_lo) {
        // check for lowered lift
        if (snum == 0 && s0->MidTexture == 0 && s1->MidTexture == 0 && // no midtex
            seg->sidedef->BottomTexture > 0) // has bottex
        {
          if (backfz1 >= /*seg->sidedef->Sector*/fsec->ceiling.maxz) {
            // this looks like a raised lift, yay
            return true;
          }
        }
      }

      if (clip_platforms_lift_hi) {
        // check for raised lift
        if (snum == 1 && s0->MidTexture == 0 && s1->MidTexture == 0 && // no midtex
            seg->sidedef->TopTexture > 0) // has bottex
        {
          if (backfz1 >= /*seg->sidedef->Sector*/fsec->ceiling.maxz) {
            // this looks like a raised lift, yay
            return true;
          }
        }
      }

      /*
      if (s0->MidTexture == 0 && s1->MidTexture == 0 && // no midtex
          s0->BottomTexture > 0) // has bottex
      {
        if (backfz1 >= bsec->ceiling.maxz) {
          // this looks like a raised lift, yay
          return true;
        }
      }
      */

      if ((backcz2 <= frontfz2 && backcz2 <= frontfz1 && backcz1 <= frontfz2 && backcz1 <= frontfz1) &&
          (frontcz2 <= backfz2 && frontcz2 <= backfz1 && frontcz1 <= backfz2 && frontcz1 <= backfz1))
      {
        // it's a closed door/elevator/polydoor
        /*
        int lnum = (int)(ptrdiff_t)(seg->linedef-Level->Lines);
        if (lnum == 47 || lnum == 68) {
          GCon->Logf("%d: closed!", lnum);
        }
        */
        return true;
      }

      /*
      {
        int lnum = (int)(ptrdiff_t)(seg->linedef-Level->Lines);
        if (lnum == 47 || lnum == 68) {
          int snum = (seg->sidedef == &Level->Sides[seg->linedef->sidenum[0]] ? 0 :
                      seg->sidedef == &Level->Sides[seg->linedef->sidenum[1]] ? 1 :
                      -1);
          GCon->Logf("%d: opened(%d:%d)! t=%d; b=%d; m=%d; fcz=(%f,%f); bcz=(%f,%f); ffz=(%f,%f); bfz=(%f,%f)",
            lnum,
            (int)(ptrdiff_t)(seg->sidedef-Level->Sides), snum,
            seg->sidedef->TopTexture.id, seg->sidedef->BottomTexture.id, seg->sidedef->MidTexture.id,
            frontcz1, frontcz2, backcz1, backcz2, frontfz1, frontfz2, backfz1, backfz2);
        }
      }
      */
#endif
    }
  } else {
    // sloped
    if (((fsec->floor.maxz > bsec->ceiling.minz && fsec->ceiling.maxz < bsec->floor.minz) ||
         (fsec->floor.minz > bsec->ceiling.maxz && fsec->ceiling.minz < bsec->floor.maxz)) ||
        ((bsec->floor.maxz > fsec->ceiling.minz && bsec->ceiling.maxz < fsec->floor.minz) ||
         (bsec->floor.minz > fsec->ceiling.maxz && bsec->ceiling.minz < fsec->floor.maxz)))
    {
      return true;
    }
  }

  return false;
}

/*
//==========================================================================
//
//  IsSegAnOpenedSomething
//
//==========================================================================
static inline bool IsSegAnOpenedSomething (VLevel *Level, const seg_t *seg) {
  if (seg->linedef->flags&ML_3DMIDTEX) return true; // 3dmidtex never blocks anything

  auto fsec = seg->linedef->frontsector;
  auto bsec = seg->linedef->backsector;

  //if (!fsec || !bsec) return false;

  // only apply this to sectors without slopes
  if (fsec->floor.normal.z == 1.0f && bsec->floor.normal.z == 1.0f &&
      fsec->ceiling.normal.z == -1.0f && bsec->ceiling.normal.z == -1.0f)
  {
    if (seg->sidedef->TopTexture != -1 || // a seg without top texture isn't a door
        seg->sidedef->BottomTexture != -1 || // a seg without bottom texture isn't an elevator/plat
        seg->sidedef->MidTexture != -1) // a seg without mid texture isn't a polyobj door
    {
      const TVec vv1 = *seg->linedef->v1;
      const TVec vv2 = *seg->linedef->v2;

      const float frontcz1 = fsec->ceiling.GetPointZ(vv1);
      const float frontcz2 = fsec->ceiling.GetPointZ(vv2);
      const float frontfz1 = fsec->floor.GetPointZ(vv1);
      const float frontfz2 = fsec->floor.GetPointZ(vv2);

      const float backcz1 = bsec->ceiling.GetPointZ(vv1);
      const float backcz2 = bsec->ceiling.GetPointZ(vv2);
      const float backfz1 = bsec->floor.GetPointZ(vv1);
      const float backfz2 = bsec->floor.GetPointZ(vv2);

      if ((backcz2 > frontfz2 && backcz2 > frontfz1 && backcz1 > frontfz2 && backcz1 > frontfz1) &&
          (frontcz2 > backfz2 && frontcz2 > backfz1 && frontcz1 > backfz2 && frontcz1 > backfz1))
      {
        // it's an opened door/elevator/polydoor
        return true;
      }
    }
  } else {
    // sloped
    if (((fsec->floor.maxz <= bsec->ceiling.minz && fsec->ceiling.maxz >= bsec->floor.minz) ||
         (fsec->floor.minz <= bsec->ceiling.maxz && fsec->ceiling.minz >= bsec->floor.maxz)) ||
        ((bsec->floor.maxz <= fsec->ceiling.minz && bsec->ceiling.maxz >= fsec->floor.minz) ||
         (bsec->floor.minz <= fsec->ceiling.maxz && bsec->ceiling.minz >= fsec->floor.maxz)))
    {
      return true;
    }
  }

  return false;
}
*/


//==========================================================================
//
//  VViewClipper::VViewClipper
//
//==========================================================================
VViewClipper::VViewClipper ()
  : FreeClipNodes(nullptr)
  , ClipHead(nullptr)
  , ClipTail(nullptr)
{
}


//==========================================================================
//
//  VViewClipper::~VViewClipper
//
//==========================================================================
VViewClipper::~VViewClipper () {
  ClearClipNodes(TVec(), nullptr);
  VClipNode *Node = FreeClipNodes;
  while (Node) {
    VClipNode *Next = Node->Next;
    delete Node;
    Node = Next;
  }
}


//==========================================================================
//
//  VViewClipper::NewClipNode
//
//==========================================================================
VViewClipper::VClipNode *VViewClipper::NewClipNode () {
  guard(VViewClipper::NewClipNode);
  VClipNode *Ret = FreeClipNodes;
  if (Ret) FreeClipNodes = Ret->Next; else Ret = new VClipNode();
  return Ret;
  unguard;
}


//==========================================================================
//
//  VViewClipper::RemoveClipNode
//
//==========================================================================
void VViewClipper::RemoveClipNode (VViewClipper::VClipNode *Node) {
  guard(VViewClipper::RemoveClipNode);
  if (Node->Next) Node->Next->Prev = Node->Prev;
  if (Node->Prev) Node->Prev->Next = Node->Next;
  if (Node == ClipHead) ClipHead = Node->Next;
  if (Node == ClipTail) ClipTail = Node->Prev;
  Node->Next = FreeClipNodes;
  FreeClipNodes = Node;
  unguard;
}


//==========================================================================
//
//  VViewClipper::ClearClipNodes
//
//==========================================================================
void VViewClipper::ClearClipNodes (const TVec &AOrigin, VLevel *ALevel) {
  guard(VViewClipper::ClearClipNodes);
  if (ClipHead) {
    ClipTail->Next = FreeClipNodes;
    FreeClipNodes = ClipHead;
  }
  ClipHead = nullptr;
  ClipTail = nullptr;
  Origin = AOrigin;
  Level = ALevel;
  unguard;
}


//==========================================================================
//
//  VViewClipper::ClipInitFrustrumRange
//
//==========================================================================
void VViewClipper::ClipInitFrustrumRange (const TAVec &viewangles, const TVec &viewforward,
                                          const TVec &viewright, const TVec &viewup,
                                          float fovx, float fovy)
{
  guard(VViewClipper::ClipInitFrustrumRange);
  check(!ClipHead);

  //if (viewforward.z > 0.9f || viewforward.z < -0.9f) return; // looking up or down, can see behind
  if (viewforward.z >= 0.985f || viewforward.z <= -0.985f) return; // looking up or down, can see behind

  TVec Pts[4];
  TVec TransPts[4];
  Pts[0] = TVec(1, fovx, fovy);
  Pts[1] = TVec(1, fovx, -fovy);
  Pts[2] = TVec(1, -fovx, fovy);
  Pts[3] = TVec(1, -fovx, -fovy);
  TVec clipforward = Normalise(TVec(viewforward.x, viewforward.y, 0.0f));
  float d1 = 0;
  float d2 = 0;

  for (int i = 0; i < 4; ++i) {
    TransPts[i].x = Pts[i].y*viewright.x+Pts[i].z*viewup.x+Pts[i].x*viewforward.x;
    TransPts[i].y = Pts[i].y*viewright.y+Pts[i].z*viewup.y+Pts[i].x*viewforward.y;
    TransPts[i].z = Pts[i].y*viewright.z+Pts[i].z*viewup.z+Pts[i].x*viewforward.z;

    if (DotProduct(TransPts[i], clipforward) <= 0) return; // player can see behind

    float a = matan(TransPts[i].y, TransPts[i].x);
    if (a < 0.0f) a += 360.0f;

    float d = AngleMod180(a-viewangles.yaw);
    if (d1 > d) d1 = d;
    if (d2 < d) d2 = d;
  }
  float a1 = AngleMod(viewangles.yaw+d1);
  float a2 = AngleMod(viewangles.yaw+d2);

  if (a1 > a2) {
    ClipHead = NewClipNode();
    ClipTail = ClipHead;
    ClipHead->From = a2;
    ClipHead->To = a1;
    ClipHead->Prev = nullptr;
    ClipHead->Next = nullptr;
  } else {
    ClipHead = NewClipNode();
    ClipHead->From = 0.0f;
    ClipHead->To = a1;
    ClipTail = NewClipNode();
    ClipTail->From = a2;
    ClipTail->To = 360.0f;
    ClipHead->Prev = nullptr;
    ClipHead->Next = ClipTail;
    ClipTail->Prev = ClipHead;
    ClipTail->Next = nullptr;
  }
  unguard;
}


//==========================================================================
//
//  VViewClipper::ClipToRanges
//
//==========================================================================
void VViewClipper::ClipToRanges (const VViewClipper &Range) {
  guard(VViewClipper::ClipToRanges);
  if (!Range.ClipHead) {
    // no ranges, everything is clipped away
    DoAddClipRange(0.0f, 360.0f);
    return;
  }

  // add head and tail ranges
  if (Range.ClipHead->From > 0.0f) DoAddClipRange(0.0f, Range.ClipHead->From);
  if (Range.ClipTail->To < 360.0f) DoAddClipRange(Range.ClipTail->To, 360.0f);

  // add middle ranges
  for (VClipNode *N = Range.ClipHead; N->Next; N = N->Next) DoAddClipRange(N->To, N->Next->From);
  unguard;
}


//==========================================================================
//
//  VViewClipper::DoAddClipRange
//
//==========================================================================
void VViewClipper::DoAddClipRange (float From, float To) {
  guard(VViewClipper::DoAddClipRange);
  if (!ClipHead) {
    ClipHead = NewClipNode();
    ClipTail = ClipHead;
    ClipHead->From = From;
    ClipHead->To = To;
    ClipHead->Prev = nullptr;
    ClipHead->Next = nullptr;
    return;
  }

  for (VClipNode *Node = ClipHead; Node; Node = Node->Next) {
    if (Node->To < From) continue; // before this range

    if (To < Node->From) {
      // insert a new clip range before current one
      VClipNode *N = NewClipNode();
      N->From = From;
      N->To = To;
      N->Prev = Node->Prev;
      N->Next = Node;

      if (Node->Prev) Node->Prev->Next = N; else ClipHead = N;
      Node->Prev = N;

      return;
    }

    if (Node->From <= From && Node->To >= To) return; // it contains this range

    if (From < Node->From) Node->From = From; // extend start of the current range

    if (To <= Node->To) return; // end is included, so we are done here

    // merge with following nodes if needed
    while (Node->Next && Node->Next->From <= To) {
      Node->To = Node->Next->To;
      RemoveClipNode(Node->Next);
    }

    if (To > Node->To) Node->To = To; // extend end

    // we are done here
    return;
  }

  // if we are here it means it's a new range at the end
  VClipNode *NewTail = NewClipNode();
  NewTail->From = From;
  NewTail->To = To;
  NewTail->Prev = ClipTail;
  NewTail->Next = nullptr;
  ClipTail->Next = NewTail;
  ClipTail = NewTail;
  unguard;
}


//==========================================================================
//
//  VViewClipper::AddClipRange
//
//==========================================================================
void VViewClipper::AddClipRange (float From, float To) {
  guard(VViewClipper::AddClipRange);
  if (From > To) {
    DoAddClipRange(0.0f, To);
    DoAddClipRange(From, 360.0f);
  } else {
    DoAddClipRange(From, To);
  }
  unguard;
}


//==========================================================================
//
//  VViewClipper::DoIsRangeVisible
//
//==========================================================================
bool VViewClipper::DoIsRangeVisible (float From, float To) {
  guard(VViewClipper::DoIsRangeVisible);
  for (VClipNode *N = ClipHead; N; N = N->Next) {
    if (From >= N->From && To <= N->To) return false;
  }
  return true;
  unguard;
}


//==========================================================================
//
//  VViewClipper::IsRangeVisible
//
//==========================================================================
bool VViewClipper::IsRangeVisible (float From, float To) {
  guard(VViewClipper::IsRangeVisible);
  if (From > To) return (DoIsRangeVisible(0.0f, To) || DoIsRangeVisible(From, 360.0f));
  return DoIsRangeVisible(From, To);
  unguard;
}


//==========================================================================
//
//  VViewClipper::ClipIsFull
//
//==========================================================================
bool VViewClipper::ClipIsFull () {
  guard(VViewClipper::ClipIsFull);
  return (ClipHead && ClipHead->From == 0.0f && ClipHead->To == 360.0f);
  unguard;
}


//==========================================================================
//
//  VViewClipper::PointToClipAngle
//
//==========================================================================
/*
float VViewClipper::PointToClipAngle (const TVec &Pt) {
  float Ret = matan(Pt.y-Origin.y, Pt.x-Origin.x);
  if (Ret < 0.0f) Ret += 360.0f;
  return Ret;
}
*/


//==========================================================================
//
//  CreateBBVerts
//
//==========================================================================
inline static void CreateBBVerts (const float *BBox, const TVec origin, TVec *v1, TVec *v2) {
  v1->z = v2->z = 0;
  if (BBox[0] > origin.x) {
    if (BBox[1] > origin.y) {
      v1->x = BBox[3];
      v1->y = BBox[1];
      v2->x = BBox[0];
      v2->y = BBox[4];
    } else if (BBox[4] < origin.y) {
      v1->x = BBox[0];
      v1->y = BBox[1];
      v2->x = BBox[3];
      v2->y = BBox[4];
    } else {
      v1->x = BBox[0];
      v1->y = BBox[1];
      v2->x = BBox[0];
      v2->y = BBox[4];
    }
  } else if (BBox[3] < origin.x) {
    if (BBox[1] > origin.y) {
      v1->x = BBox[3];
      v1->y = BBox[4];
      v2->x = BBox[0];
      v2->y = BBox[1];
    } else if (BBox[4] < origin.y) {
      v1->x = BBox[0];
      v1->y = BBox[4];
      v2->x = BBox[3];
      v2->y = BBox[1];
    } else {
      v1->x = BBox[3];
      v1->y = BBox[4];
      v2->x = BBox[3];
      v2->y = BBox[1];
    }
  } else {
    if (BBox[1] > origin.y) {
      v1->x = BBox[3];
      v1->y = BBox[1];
      v2->x = BBox[0];
      v2->y = BBox[1];
    } else {
      v1->x = BBox[0];
      v1->y = BBox[4];
      v2->x = BBox[3];
      v2->y = BBox[4];
    }
  }
}


//==========================================================================
//
//  VViewClipper::ClipIsBBoxVisible
//
//==========================================================================
bool VViewClipper::ClipIsBBoxVisible (const float *BBox, bool shadowslight, const TVec &CurrLightPos, float CurrLightRadius) {
  guard(VViewClipper::ClipIsBBoxVisible);
  if (!ClipHead) return true; // no clip nodes yet

  if (!clip_enabled) return true;

  if (shadowslight) {
    if (BBox[0] <= CurrLightPos.x && BBox[3] >= CurrLightPos.x &&
        BBox[1] <= CurrLightPos.y && BBox[4] >= CurrLightPos.y)
    {
      // viewer is inside the box
      return true;
    }
  } else if (BBox[0] <= Origin.x && BBox[3] >= Origin.x &&
             BBox[1] <= Origin.y && BBox[4] >= Origin.y)
  {
    // viewer is inside the box
    return true;
  }

  TVec v1(0, 0, 0), v2(0, 0, 0);
  CreateBBVerts(BBox, (shadowslight ? CurrLightPos : Origin), &v1, &v2);

  // clip sectors that are behind rendered segs
  TVec r1 = Origin-v1;
  TVec r2 = Origin-v2;
  float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), Origin);
  float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), Origin);

  if (shadowslight) {
    TVec rLight1 = CurrLightPos-v1;
    TVec rLight2 = CurrLightPos-v2;
    float DLight1 = DotProduct(Normalise(CrossProduct(rLight1, rLight2)), CurrLightPos);
    float DLight2 = DotProduct(Normalise(CrossProduct(rLight2, rLight1)), CurrLightPos);

    TVec rView1 = Origin-v1-CurrLightPos;
    TVec rView2 = Origin-v2-CurrLightPos;
    float DView1 = DotProduct(Normalise(CrossProduct(rView1, rView2)), Origin);
    float DView2 = DotProduct(Normalise(CrossProduct(rView2, rView1)), Origin);

    if (D1 < 0.0f && D2 < 0.0f && DView1 < -CurrLightRadius && DView2 < -CurrLightRadius) return false;
    if (D1 > r_lights_radius && D2 > r_lights_radius) return false;

    if ((DLight1 > CurrLightRadius && DLight2 > CurrLightRadius) ||
        (DLight1 < -CurrLightRadius && DLight2 < -CurrLightRadius))
    {
      return false;
    }

    // there might be a better method of doing this, but this one works for now...
         if (DLight1 > 0.0f && DLight2 <= 0.0f) v2 += (v2-v1)*D1/(D1-D2);
    else if (DLight2 > 0.0f && DLight1 <= 0.0f) v1 += (v1-v2)*D2/(D2-D1);
  } else {
    if (D1 < 0.0f && D2 < 0.0f) return false;

    // there might be a better method of doing this, but this one works for now...
         if (D1 > 0.0f && D2 <= 0.0f) v2 += (v2-v1)*D1/(D1-D2);
    else if (D2 > 0.0f && D1 <= 0.0f) v1 += (v1-v2)*D2/(D2-D1);
  }

  return IsRangeVisible(PointToClipAngle(v1), PointToClipAngle(v2));
  unguard;
}


//==========================================================================
//
//  VViewClipper::ClipCheckRegion
//
//==========================================================================
bool VViewClipper::ClipCheckRegion (subregion_t *region, subsector_t *sub, bool shadowslight, const TVec &CurrLightPos, float CurrLightRadius) {
  guard(VViewClipper::ClipCheckRegion);
  if (!ClipHead) return true;

  if (!clip_enabled) return true;

  vint32 count = sub->numlines;
  drawseg_t *ds = region->lines;

  for (; count--; ++ds) {
    TVec v1 = *ds->seg->v1;
    TVec v2 = *ds->seg->v2;

    if (!ds->seg->linedef) {
      // miniseg
      if (!IsRangeVisible(PointToClipAngle(v2), PointToClipAngle(v1))) {
        //++ds;
        continue;
      }
    }

    // clip sectors that are behind rendered segs
    TVec rLight1;
    TVec rLight2;
    TVec r1 = Origin-v1;
    TVec r2 = Origin-v2;
    float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), Origin);
    float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), Origin);

    float DLight1;
    float DLight2;

    if (shadowslight) {
      rLight1 = CurrLightPos-v1;
      rLight2 = CurrLightPos-v2;
      DLight1 = DotProduct(Normalise(CrossProduct(rLight1, rLight2)), CurrLightPos);
      DLight2 = DotProduct(Normalise(CrossProduct(rLight2, rLight1)), CurrLightPos);

      TVec rView1 = Origin-v1-CurrLightPos;
      TVec rView2 = Origin-v2-CurrLightPos;
      float DView1 = DotProduct(Normalise(CrossProduct(rView1, rView2)), Origin);
      float DView2 = DotProduct(Normalise(CrossProduct(rView2, rView1)), Origin);

      if (D1 <= 0.0f && D2 <= 0.0f && DView1 < -CurrLightRadius && DView2 < -CurrLightRadius) { /*++ds;*/ continue; }
      if (D1 > r_lights_radius && D2 > r_lights_radius) { /*++ds;*/ continue; }

      if ((DLight1 > CurrLightRadius && DLight2 > CurrLightRadius) ||
          (DLight1 < -CurrLightRadius && DLight2 < -CurrLightRadius))
      {
        //++ds;
        continue;
      }
    } else {
      if (D1 <= 0.0f && D2 <= 0.0f) { /*++ds;*/ continue; }
    }

    if (!ds->seg->backsector) {
      if (shadowslight) {
        // there might be a better method of doing this, but this one works for now...
             if (DLight1 > CurrLightRadius && DLight2 < -CurrLightRadius) v2 += (v2-v1)*DLight1/(DLight1-DLight2);
        else if (DLight2 > CurrLightRadius && DLight1 < -CurrLightRadius) v1 += (v1-v2)*DLight2/(DLight2-DLight1);
      } else {
             if (D1 > 0.0f && D2 <= 0.0f) v2 += (v2-v1)*D1/(D1-D2);
        else if (D2 > 0.0f && D1 <= 0.0f) v1 += (v1-v2)*D2/(D2-D1);
      }

      if (IsRangeVisible(PointToClipAngle(v2), PointToClipAngle(v1))) return true;
    } else if (ds->seg->linedef) {
      /*
      if (ds->seg->backsector) {
        // 2-sided line, determine if it can be skipped
        if (ds->seg->linedef->backsector) {
          if (IsSegAClosedSomething(Level, ds->seg)) { / *++ds;* / continue; }
        }
      }
      */

      TVec vv1 = *ds->seg->linedef->v1;
      TVec vv2 = *ds->seg->linedef->v2;

      // clip sectors that are behind rendered segs
      TVec rr1 = Origin-vv1;
      TVec rr2 = Origin-vv2;
      float DD1 = DotProduct(Normalise(CrossProduct(rr1, rr2)), Origin);
      float DD2 = DotProduct(Normalise(CrossProduct(rr2, rr1)), Origin);

      if (shadowslight) {
        rLight1 = CurrLightPos-vv1;
        rLight2 = CurrLightPos-vv2;
        DLight1 = DotProduct(Normalise(CrossProduct(rLight1, rLight2)), CurrLightPos);
        DLight2 = DotProduct(Normalise(CrossProduct(rLight2, rLight1)), CurrLightPos);

        TVec rView1 = Origin-v1-CurrLightPos;
        TVec rView2 = Origin-v2-CurrLightPos;
        float DView1 = DotProduct(Normalise(CrossProduct(rView1, rView2)), Origin);
        float DView2 = DotProduct(Normalise(CrossProduct(rView2, rView1)), Origin);

        if (D1 <= 0.0f && D2 <= 0.0f && DView1 < -CurrLightRadius && DView2 < -CurrLightRadius) { /*++ds;*/ continue; }
        if (DD1 > r_lights_radius && DD2 > r_lights_radius) { /*++ds;*/ continue; }

        if ((DLight1 > CurrLightRadius && DLight2 > CurrLightRadius) ||
            (DLight1 < -CurrLightRadius && DLight2 < -CurrLightRadius))
        {
          //++ds;
          continue;
        }
      } else {
        if (DD1 <= 0.0f && DD2 <= 0.0f) { /*++ds;*/ continue; }
      }

      if (shadowslight) {
        // there might be a better method of doing this, but this one works for now...
             if (DLight1 > CurrLightRadius && DLight2 < -CurrLightRadius) vv2 += (vv2-vv1)*DLight1/(DLight1-DLight2);
        else if (DLight2 > CurrLightRadius && DLight1 < -CurrLightRadius) vv1 += (vv1-vv2)*DLight2/(DLight2-DLight1);
      } else {
             if (DD1 > 0.0f && DD2 <= 0.0f) vv2 += (vv2-vv1)*DD1/(DD1-DD2);
        else if (DD2 > 0.0f && DD1 <= 0.0f) vv1 += (vv1-vv2)*DD2/(DD2-DD1);
      }

      if (!ds->seg->linedef->backsector || IsSegAClosedSomething(Level, ds->seg)) {
        if (IsRangeVisible(PointToClipAngle(vv2), PointToClipAngle(vv1))) return true;
      } else {
        return true;
      }
    }
    //++ds;
  }
  return false;
  unguard;
}


//==========================================================================
//
//  VViewClipper::ClipCheckSubsector
//
//==========================================================================
bool VViewClipper::ClipCheckSubsector (subsector_t *Sub, bool shadowslight, const TVec &CurrLightPos, float CurrLightRadius) {
  guard(VViewClipper::ClipCheckSubsector);
  if (!ClipHead) return true;

  if (!clip_enabled) return true;

  //k8: moved here and initialized, 'cause gcc is unable to see that it is safe to skip init for those
  float DLight1 = 0, DLight2 = 0;
  float DView1 = 0, DView2 = 0;
  TVec rLight1 = TVec(0, 0, 0), rLight2 = TVec(0, 0, 0);
  TVec rView1 = TVec(0, 0, 0), rView2 = TVec(0, 0, 0);

  for (int i = 0; i < Sub->numlines; ++i) {
    seg_t *seg = &Level->Segs[Sub->firstline+i];

    TVec v1 = *seg->v1;
    TVec v2 = *seg->v2;

    if (!seg->linedef) {
      // miniseg
      if (!IsRangeVisible(PointToClipAngle(v2), PointToClipAngle(v1))) continue;
    }

    // clip sectors that are behind rendered segs
    TVec r1 = Origin-v1;
    TVec r2 = Origin-v2;
    float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), Origin);
    float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), Origin);

    if (shadowslight) {
      rLight1 = CurrLightPos-v1;
      rLight2 = CurrLightPos-v2;

      DLight1 = DotProduct(Normalise(CrossProduct(rLight1, rLight2)), CurrLightPos);
      DLight2 = DotProduct(Normalise(CrossProduct(rLight2, rLight1)), CurrLightPos);

      rView1 = Origin-v1-CurrLightPos;
      rView2 = Origin-v2-CurrLightPos;
      DView1 = DotProduct(Normalise(CrossProduct(rView1, rView2)), Origin);
      DView2 = DotProduct(Normalise(CrossProduct(rView2, rView1)), Origin);

      if (D1 < 0.0f && D2 < 0.0f && DView1 < -CurrLightRadius && DView2 < -CurrLightRadius) continue;
      if (D1 > r_lights_radius && D2 > r_lights_radius) continue;

      if ((DLight1 > CurrLightRadius && DLight2 > CurrLightRadius) ||
          (DLight1 < -CurrLightRadius && DLight2 < -CurrLightRadius))
      {
        continue;
      }
    } else {
      if (D1 < 0.0f && D2 < 0.0f) continue;
    }

    if (!seg->backsector) {
      // only apply this to sectors without slopes
      if (seg->frontsector && seg->frontsector->floor.normal.z == 1.0f && seg->frontsector->ceiling.normal.z == -1.0f) {
        if (shadowslight) {
          // there might be a better method of doing this, but this one works for now...
               if (DLight1 > CurrLightRadius && DLight2 < -CurrLightRadius) v2 += (v2-v1)*DLight1/(DLight1-DLight2);
          else if (DLight2 > CurrLightRadius && DLight1 < -CurrLightRadius) v1 += (v1-v2)*DLight2/(DLight2-DLight1);
        } else {
               if (D1 > 0.0f && D2 < 0.0f) v2 += (v2-v1)*D1/(D1-D2);
          else if (D2 > 0.0f && D1 < 0.0f) v1 += (v1-v2)*D2/(D2-D1);
        }
        if (IsRangeVisible(PointToClipAngle(v2), PointToClipAngle(v1))) return true;
      } else {
        // sloped sector is always visible
        // k8: why?
        return true;
      }
    } else if (seg->linedef) {
      // 2-sided line, determine if it can be skipped
      /*
      if (seg->backsector) {
        if (IsSegAClosedSomething(Level, seg)) continue;
      }
      */

      // clip sectors that are behind rendered segs
      if (shadowslight) {
        if (D1 < 0.0f && D2 < 0.0f && DView1 < -CurrLightRadius && DView2 < -CurrLightRadius) continue;
        if (D1 > r_lights_radius && D2 > r_lights_radius) continue;

        if ((DLight1 > CurrLightRadius && DLight2 > CurrLightRadius) ||
            (DLight1 < -CurrLightRadius && DLight2 < -CurrLightRadius))
        {
          continue;
        }
      } else {
        if (D1 < 0.0f && D2 < 0.0f) continue;
      }

      if (!seg->linedef->backsector || IsSegAClosedSomething(Level, seg)) {
        if (shadowslight) {
          // there might be a better method of doing this, but this one works for now...
               if (DLight1 > CurrLightRadius && DLight2 < -CurrLightRadius) v2 += (v2-v1)*DLight1/(DLight1-DLight2);
          else if (DLight2 > CurrLightRadius && DLight1 < -CurrLightRadius) v1 += (v1-v2)*DLight2/(DLight2-DLight1);
        } else {
               if (D1 > 0.0f && D2 < 0.0f) v2 += (v2-v1)*D1/(D1-D2);
          else if (D2 > 0.0f && D1 < 0.0f) v1 += (v1-v2)*D2/(D2-D1);
        }
        if (IsRangeVisible(PointToClipAngle(v2), PointToClipAngle(v1))) return true;
      } else {
        return true;
      }
    }
  }
  return false;
  unguard;
}


//==========================================================================
//
//  VViewClipper::CheckAddClipSeg
//
//==========================================================================
void VViewClipper::CheckAddClipSeg (const seg_t *seg, bool shadowslight, TPlane *Mirror, const TVec &CurrLightPos, float CurrLightRadius) {
  if (!seg->linedef) return; // miniseg
  if (seg->PointOnSide(Origin)) return; // viewer is in back side or on plane

  TVec v1 = *seg->v1;
  TVec v2 = *seg->v2;

  // only apply this to sectors without slopes
  // k8: originally, slopes were checked only for polyobjects; wtf?!
  if (seg->frontsector->floor.normal.z == 1.0f && seg->frontsector->ceiling.normal.z == -1.0f) {
    TVec r1 = Origin-v1;
    TVec r2 = Origin-v2;
    float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), Origin);
    float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), Origin);

    if (shadowslight) {
      TVec rLight1 = CurrLightPos-v1;
      TVec rLight2 = CurrLightPos-v2;
      float DLight1 = DotProduct(Normalise(CrossProduct(rLight1, rLight2)), CurrLightPos);
      float DLight2 = DotProduct(Normalise(CrossProduct(rLight2, rLight1)), CurrLightPos);

      TVec rView1 = Origin-v1-CurrLightPos;
      TVec rView2 = Origin-v2-CurrLightPos;
      float DView1 = DotProduct(Normalise(CrossProduct(rView1, rView2)), Origin);
      float DView2 = DotProduct(Normalise(CrossProduct(rView2, rView1)), Origin);

      if (D1 <= 0.0f && D2 <= 0.0f && DView1 < -CurrLightRadius && DView2 < -CurrLightRadius) return;
      if (D1 > r_lights_radius && D2 > r_lights_radius) return;

      if ((DLight1 > CurrLightRadius && DLight2 > CurrLightRadius) ||
          (DLight1 < -CurrLightRadius && DLight2 < -CurrLightRadius))
      {
        return;
      }
    } else {
      if (D1 <= 0.0f && D2 <= 0.0f) return;
    }
  }

  if (Mirror) {
    // clip seg with mirror plane
    const float Dist1 = DotProduct(v1, Mirror->normal)-Mirror->dist;
    const float Dist2 = DotProduct(v2, Mirror->normal)-Mirror->dist;

    if (Dist1 <= 0.0f && Dist2 <= 0.0f) return;

    // and clip it while we are here
         if (Dist1 > 0.0f && Dist2 <= 0.0f) v2 = v1+(v2-v1)*Dist1/(Dist1-Dist2);
    else if (Dist2 > 0.0f && Dist1 <= 0.0f) v1 = v2+(v1-v2)*Dist2/(Dist2-Dist1);
  }

  // 2-sided line, determine if it can be skipped
  if (seg->backsector) {
    if (clip_trans_hack) {
      if (seg->linedef->alpha != 1.0f) return; //k8: skip translucent walls (for now)
      // do normal clipping check
      /*
      if (seg->sidedef->MidTexture <= 0) {
        // we can see the opposite sector through midtex (it seems)
        //const sector_t *ops = (seg->side ? seg->frontsector : seg->backsector);
        return;
      }
      */
    }
    //if (IsSegAnOpenedSomething(Level, seg)) return;
    if (!IsSegAClosedSomething(Level, seg)) return;
  }

  //k8: this is hack for boom translucency
  //    midtexture 0 *SHOULD* mean "transparent", but let's play safe
#if 0
  if (clip_trans_hack && seg->linedef && seg->frontsector && seg->backsector && seg->sidedef->MidTexture <= 0) {
    // ok, we can possibly see thru it, now check the OPPOSITE sector ceiling and floor heights
    const sector_t *ops = (seg->side ? seg->frontsector : seg->backsector);
    // ceiling is higher than the floor?
    if (ops->ceiling.minz >= ops->floor.maxz) {
      // check if that sector has any translucent walls
      bool hasTrans = false;
      int lcount = ops->linecount;
      for (int lidx = 0; lidx < lcount; ++lidx) if (ops->lines[lidx]->alpha < 1.0f) { hasTrans = true; break; }
      if (hasTrans) {
        //fprintf(stderr, "!!! SKIPPED 2-SIDED LINE W/O MIDTEX (%d); side=%d; origz=%f!\n", seg->sidedef->LineNum, seg->side, Origin.z);
        //fprintf(stderr, "  (f,c)=(%f:%f,%f:%f)\n", ops->floor.minz, ops->floor.maxz, ops->ceiling.minz, ops->ceiling.maxz);
        return;
      } else {
        //fprintf(stderr, "!!! ALLOWED 2-SIDED LINE W/O MIDTEX (%d); side=%d; origz=%f!\n", seg->sidedef->LineNum, seg->side, Origin.z);
      }
    } else {
      //fprintf(stderr, "!!! (1) SKIPPED 2-SIDED LINE W/O MIDTEX (%d); side=%d; origz=%f!\n", seg->sidedef->LineNum, seg->side, Origin.z);
      //fprintf(stderr, "  (f,c)=(%f:%f,%f:%f)\n", ops->floor.minz, ops->floor.maxz, ops->ceiling.minz, ops->ceiling.maxz);
    }
    //continue;
  }
#endif

  AddClipRange(PointToClipAngle(v2), PointToClipAngle(v1));
}


//==========================================================================
//
//  VViewClipper::ClipAddSubsectorSegs
//
//==========================================================================
void VViewClipper::ClipAddSubsectorSegs (subsector_t *Sub, bool shadowslight, TPlane *Mirror, const TVec &CurrLightPos, float CurrLightRadius) {
  guard(VViewClipper::ClipAddSubsectorSegs);

  if (!clip_enabled) return;

  for (int i = 0; i < Sub->numlines; ++i) {
    const seg_t *seg = &Level->Segs[Sub->firstline+i];
    CheckAddClipSeg(seg, shadowslight, Mirror, CurrLightPos, CurrLightRadius);
  }

  if (Sub->poly && clip_with_polyobj) {
    seg_t **polySeg = Sub->poly->segs;
    for (int polyCount = Sub->poly->numsegs; --polyCount; ++polySeg) {
      const seg_t *seg = *polySeg;
      CheckAddClipSeg(seg, shadowslight, nullptr, CurrLightPos, CurrLightRadius);
    }
  }
  unguard;
}

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
#include "../filesys/files.h"


// ////////////////////////////////////////////////////////////////////////// //
class DebugExportError : public VavoomError {
public:
  explicit DebugExportError (const char *text) : VavoomError(text) {}
};


// ////////////////////////////////////////////////////////////////////////// //
struct VertexPool {
public:
  TMapNC<vuint64, int> map; // key: two floats; value: index
  TArray<TVec> list;

public:
  VV_DISABLE_COPY(VertexPool)
  VertexPool () {}

  void clear () {
    map.clear();
    list.clear();
  }

  // returns index
  vint32 put (const TVec v) {
    union __attribute__((packed)) {
      struct __attribute__((packed)) { float f1, f2; };
      vuint64 i64;
    } u;
    static_assert(sizeof(u) == sizeof(vuint64), "oops");
    u.f1 = v.x;
    u.f2 = v.y;
    auto ip = map.find(u.i64);
    if (ip) return *ip;
    int idx = list.length();
    list.append(TVec(v.x, v.y));
    map.put(u.i64, idx);
    return idx;
  }
};


//==========================================================================
//
//  VLevel::DebugSaveLevel
//
//  this saves everything except thinkers, so i can load it for
//  further experiments
//
//==========================================================================
void VLevel::DebugSaveLevel (VStream &strm) {
  strm.writef("Namespace = \"VavoomDebug\";\n");

  VertexPool vpool;

  // collect vertices
  for (int f = 0; f < NumLines; ++f) {
    const line_t *line = &Lines[f];
    vpool.put(*line->v1);
    vpool.put(*line->v2);
  }

  // write vertices
  strm.writef("\n// --- vertices (%d) ---\n", vpool.list.length());
  for (int f = 0; f < vpool.list.length(); ++f) {
    strm.writef("\nvertex // %d\n", f);
    strm.writef("{\n");
    if ((int)vpool.list[f].x == vpool.list[f].x) {
      strm.writef("  x = %g.0;\n", vpool.list[f].x);
    } else {
      strm.writef("  x = %g;\n", vpool.list[f].x);
    }
    if ((int)vpool.list[f].y == vpool.list[f].y) {
      strm.writef("  y = %g.0;\n", vpool.list[f].y);
    } else {
      strm.writef("  y = %g;\n", vpool.list[f].y);
    }
    strm.writef("}\n");
  }

  // write lines
  strm.writef("\n\n// --- lines (%d) ---\n", NumLines);
  for (int f = 0; f < NumLines; ++f) {
    const line_t *line = &Lines[f];
    strm.writef("\nlinedef // %d\n", f);
    strm.writef("{\n");
    if (line->lineTag && line->lineTag != -1) strm.writef("  id = %d;\n", line->lineTag);
    strm.writef("  v1 = %d;\n", vpool.put(*line->v1));
    strm.writef("  v2 = %d;\n", vpool.put(*line->v2));
    //vassert(line->sidenum[0] >= 0);
    if (line->sidenum[0] >= 0) strm.writef("  sidefront = %d;\n", line->sidenum[0]);
    if (line->sidenum[1] >= 0) strm.writef("  sideback = %d;\n", line->sidenum[1]);
    // flags
    if (line->flags&ML_BLOCKING) strm.writef("  blocking = true;\n");
    if (line->flags&ML_BLOCKMONSTERS) strm.writef("  blockmonsters = true;\n");
    if (line->flags&ML_TWOSIDED) strm.writef("  twosided = true;\n");
    if (line->flags&ML_DONTPEGTOP) strm.writef("  dontpegtop = true;\n");
    if (line->flags&ML_DONTPEGBOTTOM) strm.writef("  dontpegbottom = true;\n");
    if (line->flags&ML_SECRET) strm.writef("  secret = true;\n");
    if (line->flags&ML_SOUNDBLOCK) strm.writef("  blocksound = true;\n");
    if (line->flags&ML_DONTDRAW) strm.writef("  dontdraw = true;\n");
    if (line->flags&ML_MAPPED) strm.writef("  mapped = true;\n");
    if (line->flags&ML_REPEAT_SPECIAL) strm.writef("  repeatspecial = true;\n");
    if (line->flags&ML_MONSTERSCANACTIVATE) strm.writef("  monsteractivate = true;\n");
    if (line->flags&ML_BLOCKPLAYERS) strm.writef("  blockplayers = true;\n");
    if (line->flags&ML_BLOCKEVERYTHING) strm.writef("  blockeverything = true;\n");
    if (line->flags&ML_ZONEBOUNDARY) strm.writef("  zoneboundary = true;\n");
    if (line->flags&ML_ADDITIVE) strm.writef("  renderstyle = \"add\";\n");
    if (line->flags&ML_RAILING) strm.writef("  jumpover = true;\n");
    if (line->flags&ML_BLOCK_FLOATERS) strm.writef("  blockfloaters = true;\n");
    if (line->flags&ML_CLIP_MIDTEX) strm.writef("  clipmidtex = true;\n");
    if (line->flags&ML_WRAP_MIDTEX) strm.writef("  wrapmidtex = true;\n");
    if (line->flags&ML_3DMIDTEX) strm.writef("  midtex3d = true;\n");
    if (line->flags&ML_3DMIDTEX_IMPASS) strm.writef("  midtex3dimpassible = true;\n");
    if (line->flags&ML_CHECKSWITCHRANGE) strm.writef("  checkswitchrange = true;\n");
    if (line->flags&ML_FIRSTSIDEONLY) strm.writef("  firstsideonly = true;\n");
    if (line->flags&ML_BLOCKPROJECTILE) strm.writef("  blockprojectiles = true;\n");
    if (line->flags&ML_BLOCKUSE) strm.writef("  blockuse = true;\n");
    if (line->flags&ML_BLOCKSIGHT) strm.writef("  blocksight = true;\n");
    if (line->flags&ML_BLOCKHITSCAN) strm.writef("  blockhitscan = true;\n");
    if (line->flags&ML_KEEPDATA) strm.writef("  keepdata = true;\n"); // k8vavoom
    if (line->flags&ML_NODECAL) strm.writef("  nodecal = true;\n"); // k8vavoom
    // spac flags
    if (line->SpacFlags&SPAC_Cross) strm.writef("  playercross = true;\n");
    if (line->SpacFlags&SPAC_Use) strm.writef("  playeruse = true;\n");
    if (line->SpacFlags&SPAC_MCross) strm.writef("  monstercross = true;\n");
    if (line->SpacFlags&SPAC_Impact) strm.writef("  impact = true;\n");
    if (line->SpacFlags&SPAC_Push) strm.writef("  playerpush = true;\n");
    if (line->SpacFlags&SPAC_PCross) strm.writef("  missilecross = true;\n");
    if (line->SpacFlags&SPAC_UseThrough) strm.writef("  usethrough = true;\n"); // k8vavoom
    if (line->SpacFlags&SPAC_AnyCross) strm.writef("  anycross = true;\n");
    if (line->SpacFlags&SPAC_MUse) strm.writef("  monsteruse = true;\n");
    if (line->SpacFlags&SPAC_MPush) strm.writef("  monsterpush = true;\n"); // k8vavoom
    if (line->SpacFlags&SPAC_UseBack) strm.writef("  playeruseback = true;\n"); // k8vavoom
    // other
    if (line->alpha < 1.0f) strm.writef("  alpha = %g;\n", line->alpha);
    // special
    if (line->special) strm.writef("  special = %d;\n", line->special);
    if (line->arg1) strm.writef("  arg1 = %d;\n", line->arg1);
    if (line->arg2) strm.writef("  arg2 = %d;\n", line->arg2);
    if (line->arg3) strm.writef("  arg3 = %d;\n", line->arg3);
    if (line->arg4) strm.writef("  arg4 = %d;\n", line->arg4);
    if (line->arg5) strm.writef("  arg5 = %d;\n", line->arg5);
    if (line->locknumber) strm.writef("  locknumber = %d;\n", line->locknumber);
    strm.writef("}\n");
  }

  // write sides
  strm.writef("\n\n// --- sides (%d) ---\n", NumSides);
  for (int f = 0; f < NumSides; ++f) {
    const side_t *side = &Sides[f];
    strm.writef("\nsidedef // %d\n", f);
    strm.writef("{\n");
    if (side->Sector) strm.writef("  sector = %d;\n", (int)(ptrdiff_t)(side->Sector-&Sectors[0]));
    if (side->TopTexture.id > 0) strm.writef("  texturetop = \"%s\";\n", *VStr(GTextureManager.GetTextureName(side->TopTexture.id)).quote());
    if (side->BottomTexture.id > 0) strm.writef("  texturebottom = \"%s\";\n", *VStr(GTextureManager.GetTextureName(side->BottomTexture.id)).quote());
    if (side->MidTexture.id > 0) strm.writef("  texturemiddle = \"%s\";\n", *VStr(GTextureManager.GetTextureName(side->MidTexture.id)).quote());
    // offset
    if (side->Top.TextureOffset == side->Bot.TextureOffset && side->Top.TextureOffset == side->Mid.TextureOffset) {
      if (side->Top.TextureOffset) strm.writef("  offsetx = %g;\n", side->Top.TextureOffset);
    } else {
      if (side->Top.TextureOffset) strm.writef("  offsetx_top = %g;\n", side->Top.TextureOffset);
      if (side->Bot.TextureOffset) strm.writef("  offsetx_bottom = %g;\n", side->Bot.TextureOffset);
      if (side->Mid.TextureOffset) strm.writef("  offsetx_mid = %g;\n", side->Mid.TextureOffset);
    }
    if (side->Top.RowOffset == side->Bot.RowOffset && side->Top.RowOffset == side->Mid.RowOffset) {
      if (side->Top.RowOffset) strm.writef("  offsety = %g;\n", side->Top.RowOffset);
    } else {
      if (side->Top.RowOffset) strm.writef("  offsety_top = %g;\n", side->Top.RowOffset);
      if (side->Bot.RowOffset) strm.writef("  offsety_bottom = %g;\n", side->Bot.RowOffset);
      if (side->Mid.RowOffset) strm.writef("  offsety_mid = %g;\n", side->Mid.RowOffset);
    }
    // scale
    if (side->Top.ScaleX != 1.0f) strm.writef("  scaley_top = %g;\n", side->Top.ScaleX);
    if (side->Top.ScaleY != 1.0f) strm.writef("  scaley_top = %g;\n", side->Top.ScaleY);
    if (side->Bot.ScaleX != 1.0f) strm.writef("  scaley_bottom = %g;\n", side->Bot.ScaleX);
    if (side->Bot.ScaleY != 1.0f) strm.writef("  scaley_bottom = %g;\n", side->Bot.ScaleY);
    if (side->Mid.ScaleX != 1.0f) strm.writef("  scaley_mid = %g;\n", side->Mid.ScaleX);
    if (side->Mid.ScaleY != 1.0f) strm.writef("  scaley_mid = %g;\n", side->Mid.ScaleY);
    // other
    strm.writef("  nofakecontrast = true;\n"); // k8vavoom, not right
    if (side->Light) strm.writef("  light = %d;\n", side->Light); // k8vavoom, not right
    // flags
    if (side->Flags&SDF_ABSLIGHT) strm.writef("  lightabsolute = true;\n");
    if (side->Flags&SDF_WRAPMIDTEX) strm.writef("  wrapmidtex = true;\n");
    if (side->Flags&SDF_CLIPMIDTEX) strm.writef("  clipmidtex = true;\n");
    strm.writef("}\n");
  }

  // sectors
  strm.writef("\n\n// --- sectors (%d) ---\n", NumSectors);
  for (int f = 0; f < NumSectors; ++f) {
    const sector_t *sector = &Sectors[f];
    strm.writef("\nsector // %d\n", f);
    strm.writef("{\n");
    if (sector->sectorTag) strm.writef("  id = %d;\n", sector->sectorTag);
    if (sector->special) strm.writef("  special = %d;\n", sector->special);
    if (sector->floor.normal.z == 1.0f) {
      // normal
      strm.writef("  heightfloor = %g;\n", sector->floor.minz);
    } else {
      // slope
      strm.writef("  floornormal_x = %g;\n", sector->floor.normal.x); // k8vavoom
      strm.writef("  floornormal_y = %g;\n", sector->floor.normal.y); // k8vavoom
      strm.writef("  floornormal_z = %g;\n", sector->floor.normal.z); // k8vavoom
      strm.writef("  floordist = %g;\n", sector->floor.dist); // k8vavoom
    }
    if (sector->ceiling.normal.z == -1.0f) {
      // normal
      strm.writef("  heightceiling = %g;\n", sector->ceiling.minz);
    } else {
      // slope
      strm.writef("  ceilingnormal_x = %g;\n", sector->ceiling.normal.x); // k8vavoom
      strm.writef("  ceilingnormal_y = %g;\n", sector->ceiling.normal.y); // k8vavoom
      strm.writef("  ceilingnormal_z = %g;\n", sector->ceiling.normal.z); // k8vavoom
      strm.writef("  ceilingdist = %g;\n", sector->ceiling.dist); // k8vavoom
    }
    // textures
    strm.writef("  texturefloor = \"%s\";\n", (sector->floor.pic.id > 0 ? *VStr(GTextureManager.GetTextureName(sector->floor.pic.id)).quote() : "-"));
    strm.writef("  textureceiling = \"%s\";\n", (sector->ceiling.pic.id > 0 ? *VStr(GTextureManager.GetTextureName(sector->ceiling.pic.id)).quote() : "-"));
    //TODO: write other floor/ceiling parameters
    // light
    strm.writef("  lightlevel = %d;\n", sector->params.lightlevel);
    if ((sector->params.LightColor&0xffffff) != 0xffffff) strm.writef("  lightcolor = 0x%06x;\n", sector->params.LightColor);
    if (sector->params.Fade) strm.writef("  fadecolor = 0x%08x;\n", sector->params.Fade);
    // other
    if (sector->Damage) {
      strm.writef("  damageamount = %d;\n", sector->Damage);
      if (sector->DamageType != NAME_None) strm.writef("  damagetype = \"%s\";\n", *VStr(sector->DamageType).quote());
      if (sector->DamageInterval != 32) strm.writef("  damageinterval = %d;\n", sector->DamageInterval);
      if (sector->DamageLeaky != 0) strm.writef("  leakiness = %d;\n", sector->DamageLeaky);
    }
    // write other crap
    strm.writef("}\n");
  }

  //*// non-standard sections //*//
  strm.writef("\n\n// === NON-STANDARD SECTIONS ===\n");

  // segs
  strm.writef("\n\n// --- segs (%d) ---\n", NumSegs);
  for (int f = 0; f < NumSegs; ++f) {
    const seg_t *seg = &Segs[f];
    strm.writef("\nseg // %d\n", f);
    strm.writef("{\n");
    strm.writef("  v1_x = %g;\n", seg->v1->x);
    strm.writef("  v1_y = %g;\n", seg->v1->y);
    strm.writef("  v2_x = %g;\n", seg->v2->x);
    strm.writef("  v2_y = %g;\n", seg->v2->y);
    strm.writef("  offset = %g;\n", seg->offset);
    strm.writef("  length = %g;\n", seg->length);
    if (seg->linedef) {
      strm.writef("  miniseg = false;\n");
      strm.writef("  side = %d;\n", seg->side);
      // not a miniseg
      vassert(seg->sidedef);
      strm.writef("  sidedef = %d;\n", (int)(ptrdiff_t)(seg->sidedef-&Sides[0]));
      vassert(seg->linedef);
      strm.writef("  linedef = %d;\n", (int)(ptrdiff_t)(seg->linedef-&Lines[0]));
    } else {
      strm.writef("  miniseg = true;\n");
    }
    if (seg->partner) strm.writef("  partner = %d;\n", (int)(ptrdiff_t)(seg->partner-&Segs[0]));
    vassert(seg->frontsub);
    strm.writef("  frontsub = %d;\n", (int)(ptrdiff_t)(seg->frontsub-&Subsectors[0]));
    strm.writef("}\n");
  }

  // subsectors
  strm.writef("\n\n// --- subsectors (%d) ---\n", NumSubsectors);
  for (int f = 0; f < NumSubsectors; ++f) {
    const subsector_t *sub = &Subsectors[f];
    strm.writef("\nsubsector // %d\n", f);
    strm.writef("{\n");
    vassert(sub->sector);
    strm.writef("  sector = %d;\n", (int)(ptrdiff_t)(sub->sector-&Sectors[0]));
    strm.writef("  firstseg = %d;\n", sub->firstline);
    strm.writef("  numsegs = %d;\n", sub->numlines);
    if (sub->parent) {
      strm.writef("  bspnode = %d;\n", (int)(ptrdiff_t)(sub->parent-&Nodes[0]));
    }
    strm.writef("}\n");
  }

  // bsp nodes
  strm.writef("\n\n// --- nodes (%d) ---\n", NumNodes);
  for (int f = 0; f < NumNodes; ++f) {
    const node_t *node = &Nodes[f];
    strm.writef("\nbspnode // %d\n", f);
    strm.writef("{\n");
    // plane
    strm.writef("  plane_normal_x = %g;\n", node->normal.x);
    strm.writef("  plane_normal_y = %g;\n", node->normal.y);
    strm.writef("  plane_normal_z = %g;\n", node->normal.z);
    strm.writef("  plane_dist = %g;\n", node->dist);
    // child 0
    strm.writef("  bbox_child0_min_x = %g;\n", node->bbox[0][0]);
    strm.writef("  bbox_child0_min_y = %g;\n", node->bbox[0][1]);
    strm.writef("  bbox_child0_min_z = %g;\n", node->bbox[0][2]);
    strm.writef("  bbox_child0_max_x = %g;\n", node->bbox[0][3]);
    strm.writef("  bbox_child0_max_y = %g;\n", node->bbox[0][4]);
    strm.writef("  bbox_child0_max_z = %g;\n", node->bbox[0][5]);
    // child 1
    strm.writef("  bbox_child1_min_x = %g;\n", node->bbox[1][0]);
    strm.writef("  bbox_child1_min_y = %g;\n", node->bbox[1][1]);
    strm.writef("  bbox_child1_min_z = %g;\n", node->bbox[1][2]);
    strm.writef("  bbox_child1_max_x = %g;\n", node->bbox[1][3]);
    strm.writef("  bbox_child1_max_y = %g;\n", node->bbox[1][4]);
    strm.writef("  bbox_child1_max_z = %g;\n", node->bbox[1][5]);
    // children
    if (BSPIDX_IS_LEAF(node->children[0])) {
      strm.writef("  subsector0 = %d;\n", BSPIDX_LEAF_SUBSECTOR(node->children[0]));
    } else {
      strm.writef("  node0 = %d;\n", node->children[0]);
    }
    if (BSPIDX_IS_LEAF(node->children[1])) {
      strm.writef("  subsector1 = %d;\n", BSPIDX_LEAF_SUBSECTOR(node->children[1]));
    } else {
      strm.writef("  node1 = %d;\n", node->children[1]);
    }
    // parent (if any)
    if (node->parent) strm.writef("  parent = %d;\n", (int)(ptrdiff_t)(node->parent-&Nodes[0]));
    strm.writef("}\n");
  }

  // write player starts
  int psidx[8];
  for (int f = 0; f < 8; ++f) psidx[f] = -1;
  int startCount = 0;
  for (int f = 0; f < NumThings; ++f) {
    const mthing_t *thing = &Things[f];
    int idx = -1;
    if (thing->type >= 1 && thing->type <= 4) idx = (thing->type-1);
    if (thing->type >= 4001 && thing->type <= 4004) idx = (thing->type-4001+4);
    if (idx >= 0) {
      ++startCount;
      psidx[idx] = f;
    }
  }

  if (startCount) {
    strm.writef("\n\n// --- player starts (%d) ---\n", startCount);
    for (int f = 0; f < 8; ++f) {
      int idx = psidx[f];
      if (idx < 0) continue;
      strm.writef("\nplayerstart // %d\n", f);
      strm.writef("{\n");
      strm.writef("  player = %d;\n", idx);
      strm.writef("  x = %g;\n", Things[idx].x);
      strm.writef("  y = %g;\n", Things[idx].y);
      strm.writef("  angle = %d;\n", (int)Things[idx].angle);
      if (Things[idx].pitch) strm.writef("  pitch = %g;\n", Things[idx].pitch);
      if (Things[idx].roll) strm.writef("  roll = %g;\n", Things[idx].roll);
      strm.writef("}\n");
    }
  }
}


COMMAND(DebugExportLevel) {
  if (Args.length() != 2) {
    GCon->Log("(only) file name expected!");
    return;
  }

  if (!GLevel) {
    GCon->Log("no level loaded");
    return;
  }

  // find a file name to save it to
  if (!FL_IsSafeDiskFileName(Args[1])) {
    GCon->Logf(NAME_Error, "unsafe file name '%s'", *Args[1]);
    return;
  }

  VStr fname = va("%s.udmf", *Args[1]);
  auto strm = FL_OpenFileWrite(fname, true); // as full name
  if (!strm) {
    GCon->Logf(NAME_Error, "cannot create file '%s'", *fname);
    return;
  }

  try {
    GLevel->DebugSaveLevel(*strm);
    VStream::Destroy(strm);
    GCon->Logf("Level exported to '%s'", *fname);
  } catch (DebugExportError &werr) {
    VStream::Destroy(strm);
    GCon->Logf(NAME_Error, "Cannot write level to '%s'", *fname);
  } catch (...) {
    VStream::Destroy(strm);
    throw;
  }
}

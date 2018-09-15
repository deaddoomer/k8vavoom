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
#ifndef GLVISINT_H
#define GLVISINT_H

#include <math.h>

#include "cmdlib.h"
#include "wadlib.h"
#include "glvis.h"

namespace VavoomUtils {

#include "vector.h"

#define ON_EPSILON  (0.1)

// indicate a leaf
#define NF_SUBSECTOR  (0x80000000U)


struct winding_t {
  bool original; // don't free, it's part of the portal
  TVec2D points[2];
};

enum vstatus_t { stat_none, stat_working, stat_done };

// normal pointing into neighbor
struct portal_t : TPlane2D {
  int leaf; // neighbor
  winding_t winding;
  vstatus_t status;
  vuint8 *visbits;
  vuint8 *mightsee;
  int nummightsee;
  int numcansee;
};

#define MAX_PORTALS_ON_LEAF   (128)
struct leaf_t {
  int secnum;
  int numportals;
  portal_t *portals[MAX_PORTALS_ON_LEAF];
};

struct pstack_t {
  pstack_t *next;
  leaf_t *leaf;
  portal_t *portal; // portal exiting
  winding_t *source;
  winding_t *pass;
  TPlane2D portalplane;
  vuint8 *mightsee; // bit string
};

struct threaddata_t {
  vuint8 *leafvis; // bit string
  portal_t *base;
  pstack_t pstack_head;
};

typedef TVec2D vertex_t;

typedef leaf_t subsector_t;

struct seg_t : public TPlane2D {
  vertex_t *v1;
  vertex_t *v2;

  seg_t *partner;
  int leaf;
  int secnum;
};

struct line_t {
  int secnum[2];
};


class TVisBuilder {
private:
  TGLVis &Owner;

  TIWadFile inwad;
  TIWadFile gwa;
  TOWadFile outwad;

  TIWadFile *mainwad;
  TIWadFile *glwad;

  bool GLNodesV5;

  int numvertexes;
  vertex_t *vertexes;
  vertex_t *gl_vertexes;

  int numsectors;
  line_t *lines;
  int numlines;
  int *sidesecs;
  int numsides;

  int numsegs;
  seg_t *segs;

  int numsubsectors;
  subsector_t *subsectors;

  int numportals;
  portal_t* portals;

  int vissize;
  vuint8* vis;

  int rejectsize;
  vuint8* reject;

  int bitbytes; // (portalleafs+63)>>3
  int bitlongs;

  vuint8* portalsee;
  int c_leafsee, c_portalsee;

  int c_chains;
  int c_portalskip, c_leafskip;
  int c_vistest, c_mighttest;
  int c_portaltest, c_portalpass, c_portalcheck;

  int totalvis;
  int rowbytes;

private:
  bool IsLevelName (int lump);
  bool IsGLLevelName (int lump);

  bool IsInverseMode (const char *levname);

  void LoadVertexes (int lump, int gl_lump);
  void LoadSectors (int lump);
  void LoadSideDefs (int lump);
  void LoadLineDefs1 (int lump);
  void LoadLineDefs2 (int lump);
  void LoadSegs (int lump);
  void LoadSubsectors (int lump);
  void CreatePortals ();
  void CopyLump (int i);
  void LoadLevel (int lumpnum, int gl_lumpnum);
  void FreeLevel ();
  void BuildGWA ();
  void BuildWAD ();

  void SimpleFlood (portal_t *srcportal, int leafnum);
  void BasePortalVis ();
  void CheckStack (leaf_t *leaf, threaddata_t *thread);
  void FreeWinding (winding_t *w);
  winding_t *CopyWinding (winding_t *w);
  winding_t *ClipWinding (winding_t *in, TPlane2D *split);
  winding_t *ClipToSeperators (winding_t *source, winding_t *pass, winding_t *target);
  void RecursiveLeafFlow (int leafnum, threaddata_t *thread, pstack_t *prevstack, int StackDepth);
  void PortalFlow (portal_t *p);
  portal_t *GetNextPortal ();
  void CalcPortalVis ();
  void LeafFlow (int leafnum);
  void BuildPVS ();
  void BuildReject ();

public:
  TVisBuilder (TGLVis &);
  void Run (const char *srcfile, const char *gwafile);
};


} // namespace VavoomUtils

#endif

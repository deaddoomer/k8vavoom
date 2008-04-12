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
//**	BSP traversal, handling of LineSegs for rendering.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "r_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static subsector_t*		r_sub;
static sec_region_t*	r_region;

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	VRenderLevel::SetUpFrustumIndexes
//
//==========================================================================

void VRenderLevel::SetUpFrustumIndexes()
{
	guard(VRenderLevel::SetUpFrustumIndexes);
	for (int i = 0; i < 4; i++)
	{
		int *pindex = FrustumIndexes[i];
		for (int j = 0; j < 3; j++)
		{
			if (view_clipplanes[i].normal[j] < 0)
			{
				pindex[j] = j;
				pindex[j + 3] = j + 3;
			}
			else
			{
				pindex[j] = j + 3;
				pindex[j + 3] = j;
			}
		}
	}
	unguard;
}

//==========================================================================
//
//	VRenderLevel::DrawSurfaces
//
//==========================================================================

void VRenderLevel::DrawSurfaces(surface_t* InSurfs, texinfo_t *texinfo,
	int clipflags, int LightSourceSector)
{
	guard(VRenderLevel::DrawSurfaces);
	surface_t* surfs = InSurfs;
	if (!surfs)
	{
		return;
	}

	if (texinfo->Tex->Type == TEXTYPE_Null)
	{
		return;
	}

	if (texinfo->Tex == GTextureManager[skyflatnum])
	{
		SkyIsVisible = true;
		VSky* Sky = NULL;
		if (r_sub->sector->Sky & SKY_FROM_SIDE)
		{
			side_t* Side = &Level->Sides[r_sub->sector->Sky &
				(SKY_FROM_SIDE - 1)];
			int Tex = Side->toptexture;
			bool Flip = !!Level->Lines[Side->LineNum].arg3;
			if (GTextureManager[Tex]->Type != TEXTYPE_Null)
			{
				for (int i = 0; i < SideSkies.Num(); i++)
				{
					if (SideSkies[i]->SideTex == Tex &&
						SideSkies[i]->SideFlip == Flip)
					{
						Sky = SideSkies[i];
						break;
					}
				}
				if (!Sky)
				{
					Sky = new VSky;
					Sky->Init(Tex, Tex, 0, 0, false,
						!!(Level->LevelInfo->LevelInfoFlags &
						VLevelInfo::LIF_ForceNoSkyStretch), Flip, false);
					SideSkies.Append(Sky);
				}
			}
		}
		if (!Sky)
		{
			InitSky();
			Sky = &BaseSky;
		}

		VPortal* Portal = NULL;
		for (int i = 0; i < Portals.Num(); i++)
		{
			if (Portals[i] && Portals[i]->MatchSky(Sky))
			{
				Portal = Portals[i];
				break;
			}
		}
		if (!Portal)
		{
			Portal = new VSkyPortal(this, Sky);
			Portals.Append(Portal);
		}
		Portal->Surfs.Append(surfs);

		if (!Drawer->HasStencil)
		{
			Drawer->DrawSkyPortal(surfs, clipflags);
		}
		return;
	}

	sec_params_t* LightParams = LightSourceSector == -1 ? r_region->params :
		&Level->Sectors[LightSourceSector].params;
	int lLev = FixedLight ? FixedLight :
			MIN(255, LightParams->lightlevel + ExtraLight);
	if (r_darken)
	{
		lLev = light_remap[lLev];
	}
	vuint32 Fade = GetFade(r_sub);
	do
	{
		surfs->Light = (lLev << 24) | LightParams->LightColour;
		surfs->Fade = Fade;
		surfs->dlightframe = r_sub->dlightframe;
		surfs->dlightbits = r_sub->dlightbits;

		if (texinfo->Alpha > 1.0)
		{
			Drawer->DrawPolygon(surfs, clipflags);
		}
		else
		{
			DrawTranslucentPoly(surfs, surfs->verts, surfs->count,
				0, texinfo->Alpha, texinfo->Additive, 0, false, 0, 0,
				TVec(), 0, TVec(), TVec(), TVec());
		}
		surfs = surfs->next;
	} while (surfs);
	unguard;
}

//==========================================================================
//
//	VRenderLevel::RenderLine
//
// 	Clips the given segment and adds any visible pieces to the line list.
//
//==========================================================================

void VRenderLevel::RenderLine(drawseg_t* dseg, int clipflags)
{
	guard(VRenderLevel::RenderLine);
	seg_t *line = dseg->seg;

	if (!line->linedef)
	{
		//	Miniseg
		return;
	}

	float dist = DotProduct(vieworg, line->normal) - line->dist;
	if (dist <= 0)
	{
		//	Viewer is in back side or on plane
		return;
	}

	float a1 = ViewClip.PointToClipAngle(*line->v2);
	float a2 = ViewClip.PointToClipAngle(*line->v1);
	if (!ViewClip.IsRangeVisible(a1, a2))
	{
		return;
	}

	line_t *linedef = line->linedef;

	//FIXME this marks all lines
	// mark the segment as visible for auto map
	linedef->flags |= ML_MAPPED;

	if (!line->backsector)
	{
		// single sided line
		DrawSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, clipflags);
		DrawSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, clipflags);
	}
	else
	{
		// two sided line
		DrawSurfaces(dseg->top->surfs, &dseg->top->texinfo, clipflags);
		DrawSurfaces(dseg->topsky->surfs, &dseg->topsky->texinfo, clipflags);
		DrawSurfaces(dseg->bot->surfs, &dseg->bot->texinfo, clipflags);
		DrawSurfaces(dseg->mid->surfs, &dseg->mid->texinfo, clipflags);
		for (segpart_t *sp = dseg->extra; sp; sp = sp->next)
		{
			DrawSurfaces(sp->surfs, &sp->texinfo, clipflags);
		}
	}
	unguard;
}

//==========================================================================
//
//	VRenderLevel::RenderSecSurface
//
//==========================================================================

void VRenderLevel::RenderSecSurface(sec_surface_t* ssurf, int clipflags)
{
	guard(VRenderLevel::RenderSecSurface);
	sec_plane_t& plane = *ssurf->secplane;

	if (!plane.pic)
	{
		return;
	}

	float dist = DotProduct(vieworg, plane.normal) - plane.dist;
	if (dist <= 0)
	{
		//	Viewer is in back side or on plane
		return;
	}

	DrawSurfaces(ssurf->surfs, &ssurf->texinfo, clipflags,
		plane.LightSourceSector);
	unguard;
}

//==========================================================================
//
//	VRenderLevel::RenderSubRegion
//
// 	Determine floor/ceiling planes.
// 	Draw one or more line segments.
//
//==========================================================================

void VRenderLevel::RenderSubRegion(subregion_t* region, int clipflags)
{
	guard(VRenderLevel::RenderSubRegion);
	int				count;
	int 			polyCount;
	seg_t**			polySeg;
	float			d;

	d = DotProduct(vieworg, region->floor->secplane->normal) -
		region->floor->secplane->dist;
	if (region->next && d <= 0.0)
	{
		RenderSubRegion(region->next, clipflags);
	}

	r_region = region->secregion;

	if (r_sub->poly)
	{
		//	Render the polyobj in the subsector first
		polyCount = r_sub->poly->numsegs;
		polySeg = r_sub->poly->segs;
		while (polyCount--)
		{
			RenderLine((*polySeg)->drawsegs, clipflags);
			polySeg++;
		}
	}

	count = r_sub->numlines;
	drawseg_t *ds = region->lines;
	while (count--)
	{
		RenderLine(ds, clipflags);
		ds++;
	}

	RenderSecSurface(region->floor, clipflags);
	RenderSecSurface(region->ceil, clipflags);

	if (region->next && d > 0.0)
	{
		RenderSubRegion(region->next, clipflags);
	}
	unguard;
}

//==========================================================================
//
//	VRenderLevel::RenderSubsector
//
//==========================================================================

void VRenderLevel::RenderSubsector(int num, int clipflags)
{
	guard(VRenderLevel::RenderSubsector);
	subsector_t* Sub = &Level->Subsectors[num];
	r_sub = Sub;

	if (Sub->VisFrame != r_visframecount)
	{
		return;
	}

	if (!Sub->sector->linecount)
	{
		//	Skip sectors containing original polyobjs
		return;
	}

	if (!ViewClip.ClipCheckSubsector(Sub))
	{
		return;
	}

	BspVis[num >> 3] |= 1 << (num & 7);

	RenderSubRegion(Sub->regions, clipflags);

	ViewClip.ClipAddSubsectorSegs(Sub);
	unguard;
}

//==========================================================================
//
//	VRenderLevel::RenderBSPNode
//
//	Renders all subsectors below a given node, traversing subtree
// recursively. Just call with BSP root.
//
//==========================================================================

void VRenderLevel::RenderBSPNode(int bspnum, float* bbox, int AClipflags)
{
	guard(VRenderLevel::RenderBSPNode);
	if (ViewClip.ClipIsFull())
		return;
	int clipflags = AClipflags;
	// cull the clipping planes if not trivial accept
	if (clipflags)
	{
		for (int i = 0; i < 4; i++)
		{
			if (!(clipflags & view_clipplanes[i].clipflag))
				continue;	// don't need to clip against it

			// generate accept and reject points

			int *pindex = FrustumIndexes[i];

			TVec rejectpt;

			rejectpt[0] = bbox[pindex[0]];
			rejectpt[1] = bbox[pindex[1]];
			rejectpt[2] = bbox[pindex[2]];

			float d;

			d = DotProduct(rejectpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= 0)
				return;

			TVec acceptpt;

			acceptpt[0] = bbox[pindex[3+0]];
			acceptpt[1] = bbox[pindex[3+1]];
			acceptpt[2] = bbox[pindex[3+2]];

			d = DotProduct(acceptpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d >= 0)
				clipflags ^= view_clipplanes[i].clipflag;	// node is entirely on screen
		}
	}

	if (!ViewClip.ClipIsBBoxVisible(bbox))
	{
		return;
	}

	// Found a subsector?
	if (bspnum & NF_SUBSECTOR)
	{
		if (bspnum == -1)
			RenderSubsector(0, clipflags);
		else
			RenderSubsector(bspnum & (~NF_SUBSECTOR), clipflags);
		return;
	}

	node_t* bsp = &Level->Nodes[bspnum];

	if (bsp->VisFrame != r_visframecount)
	{
		return;
	}

	// Decide which side the view point is on.
	int side = bsp->PointOnSide(vieworg);

	// Recursively divide front space.
	RenderBSPNode(bsp->children[side], bsp->bbox[side], clipflags);

	// Divide back space.
	RenderBSPNode(bsp->children[side ^ 1], bsp->bbox[side ^ 1], clipflags);
	unguard;
}

//==========================================================================
//
//	VRenderLevel::RenderWorld
//
//==========================================================================

void VRenderLevel::RenderWorld(const refdef_t* rd)
{
	guard(VRenderLevel::RenderWorld);
	float	dummy_bbox[6] = {-99999, -99999, -99999, 99999, 9999, 99999};

	SetUpFrustumIndexes();
	ViewClip.ClearClipNodes(vieworg, Level);
	ViewClip.ClipInitFrustrumRange(viewangles, viewforward, viewright, viewup,
		rd->fovx, rd->fovy);
	memset(BspVis, 0, VisSize);

	SkyIsVisible = false;

	RenderBSPNode(Level->NumNodes - 1, dummy_bbox, 15);	// head node is the last node output

	if (SkyIsVisible && !Drawer->HasStencil)
	{
		DrawSky();
	}

	Drawer->WorldDrawing();

	if (Drawer->HasStencil)
	{
		for (int i = 0; i < Portals.Num(); i++)
		{
			if (!Drawer->StartPortal(Portals[i]))
			{
				//	All portal polygons are clipped away.
				continue;
			}
			Portals[i]->DrawContents();
			Drawer->EndPortal(Portals[i]);
		}
	}
	for (int i = 0; i < Portals.Num(); i++)
	{
		delete Portals[i];
	}
	Portals.Clear();
	unguard;
}

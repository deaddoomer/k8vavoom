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

#define HORIZON_SURF_SIZE	(sizeof(surface_t) + sizeof(TVec) * 3)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static subsector_t*		r_sub;
static subregion_t*		r_subregion;
static sec_region_t*	r_region;
static bool				MirrorClipSegs;

static VCvarI			r_maxmirrors("r_maxmirrors", "4", "Maximum allowed mirrors.", CVAR_Archive);

// CODE --------------------------------------------------------------------

static void addDecalsToSurf (surface_t* surf, seg_t* seg) {
  if (!surf) return;
  surf->decals = nullptr;
  if (seg && seg->decals) {
    // append decals
    //printf("queuing surface with decals!\n");
    decal_t* last = nullptr;
    for (decal_t* dc = seg->decals; dc; dc = dc->next) {
      if (last) last->surfnext = dc; else surf->decals = dc;
      last = dc;
    }
    if (last) last->surfnext = nullptr;
  }
}


//==========================================================================
//
//	VRenderLevelShared::SetUpFrustumIndexes
//
//==========================================================================

void VRenderLevelShared::SetUpFrustumIndexes()
{
	guard(VRenderLevelShared::SetUpFrustumIndexes);
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
//	VRenderLevel::QueueWorldSurface
//
//==========================================================================

void VRenderLevel::QueueWorldSurface(seg_t* seg, surface_t* surf)
{
	guard(VRenderLevel::QueueWorldSurface);
	bool lightmaped = (surf->lightmap != NULL || surf->dlightframe == r_dlightframecount);
	surf->decals = nullptr;

	if (lightmaped)
	{
		CacheSurface(surf);
		if (Drawer->HaveMultiTexture)
		{
			addDecalsToSurf(surf, seg);
			return;
		}
	}

	QueueSimpleSurf(seg, surf);
	unguard;
}

//==========================================================================
//
//	VAdvancedRenderLevel::QueueWorldSurface
//
//==========================================================================

void VAdvancedRenderLevel::QueueWorldSurface(seg_t* seg, surface_t* surf)
{
	guard(VAdvancedRenderLevel::QueueWorldSurface);
	QueueSimpleSurf(seg, surf);
	unguard;
}

//==========================================================================
//
//	VRenderLevelShared::QueueSimpleSurf
//
//==========================================================================

void VRenderLevelShared::QueueSimpleSurf(seg_t* seg, surface_t* surf)
{
	guard(VRenderLevelShared::QueueSimpleSurf);
	if (SimpleSurfsTail)
	{
		SimpleSurfsTail->DrawNext = surf;
		SimpleSurfsTail = surf;
	}
	else
	{
		SimpleSurfsHead = surf;
		SimpleSurfsTail = surf;
	}
	surf->DrawNext = NULL;
	addDecalsToSurf(surf, seg);
	unguard;
}

//==========================================================================
//
//	VRenderLevelShared::QueueSkyPortal
//
//==========================================================================

void VRenderLevelShared::QueueSkyPortal(surface_t* surf)
{
	guard(VRenderLevelShared::QueueSkyPortal);
	if (SkyPortalsTail)
	{
		SkyPortalsTail->DrawNext = surf;
		SkyPortalsTail = surf;
	}
	else
	{
		SkyPortalsHead = surf;
		SkyPortalsTail = surf;
	}
	surf->DrawNext = NULL;
	unguard;
}

//==========================================================================
//
//	VRenderLevelShared::QueueHorizonPortal
//
//==========================================================================

void VRenderLevelShared::QueueHorizonPortal(surface_t* surf)
{
	guard(VRenderLevelShared::QueueHorizonPortal);
	if (HorizonPortalsTail)
	{
		HorizonPortalsTail->DrawNext = surf;
		HorizonPortalsTail = surf;
	}
	else
	{
		HorizonPortalsHead = surf;
		HorizonPortalsTail = surf;
	}
	surf->DrawNext = NULL;
	unguard;
}

//==========================================================================
//
//	VRenderLevelShared::DrawSurfaces
//
//==========================================================================

void VRenderLevelShared::DrawSurfaces(seg_t* seg, surface_t* InSurfs, texinfo_t *texinfo,
	VEntity* SkyBox, int LightSourceSector, int SideLight, bool AbsSideLight,
	bool CheckSkyBoxAlways)
{
	guard(VRenderLevelShared::DrawSurfaces);
	surface_t* surfs = InSurfs;
	if (!surfs)
	{
		return;
	}

	if (texinfo->Tex->Type == TEXTYPE_Null)
	{
		return;
	}

	sec_params_t* LightParams = LightSourceSector == -1 ? r_region->params :
		&Level->Sectors[LightSourceSector].params;
	int lLev = (AbsSideLight ? 0 : LightParams->lightlevel) + SideLight;
	lLev = FixedLight ? FixedLight : lLev + ExtraLight;
	lLev = MID(0, lLev, 255);
	if (r_darken)
	{
		lLev = light_remap[lLev];
	}
	vuint32 Fade = GetFade(r_region);

	if (SkyBox && (SkyBox->EntityFlags & VEntity::EF_FixedModel))
	{
		SkyBox = NULL;
	}
	bool IsStack = SkyBox && SkyBox->eventSkyBoxGetAlways();
	if (texinfo->Tex == GTextureManager[skyflatnum] ||
		(IsStack && CheckSkyBoxAlways)) //k8: i hope that the parens are right here
	{
		VSky* Sky = NULL;
		if (!SkyBox && r_sub->sector->Sky & SKY_FROM_SIDE)
		{
			int Tex;
			bool Flip;
			if (r_sub->sector->Sky == SKY_FROM_SIDE)
			{
				Tex = Level->LevelInfo->Sky2Texture;
				Flip = true;
			}
			else
			{
				side_t* Side = &Level->Sides[(r_sub->sector->Sky &
					(SKY_FROM_SIDE - 1)) - 1];
				Tex = Side->TopTexture;
				Flip = !!Level->Lines[Side->LineNum].arg3;
			}
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
		if (!Sky && !SkyBox)
		{
			InitSky();
			Sky = &BaseSky;
		}

		VPortal* Portal = NULL;
		if (SkyBox)
		{
			for (int i = 0; i < Portals.Num(); i++)
			{
				if (Portals[i] && Portals[i]->MatchSkyBox(SkyBox))
				{
					Portal = Portals[i];
					break;
				}
			}
			if (!Portal)
			{
				if (IsStack)
				{
					Portal = new VSectorStackPortal(this, SkyBox);
				}
				else
				{
					Portal = new VSkyBoxPortal(this, SkyBox);
				}
				Portals.Append(Portal);
			}
		}
		else
		{
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
		}
		do
		{
			if (surfs->plane->PointOnSide(vieworg))
			{
				//	Viewer is in back side or on plane
				continue;
			}

			Portal->Surfs.Append(surfs);
			if (IsStack && CheckSkyBoxAlways &&
				SkyBox->eventSkyBoxGetPlaneAlpha())
			{
				surfs->Light = (lLev << 24) | LightParams->LightColour;
				surfs->Fade = Fade;
				surfs->dlightframe = r_sub->dlightframe;
				surfs->dlightbits = r_sub->dlightbits;
				DrawTranslucentPoly(surfs, surfs->verts, surfs->count,
					0, SkyBox->eventSkyBoxGetPlaneAlpha(), false, 0,
					false, 0, 0, TVec(), 0, TVec(), TVec(), TVec());
			}

			if (!Drawer->HaveStencil)
			{
				if (PortalLevel == 0)
				{
					world_surf_t& S = WorldSurfs.Alloc();
					S.Surf = surfs;
					S.Type = 1;
					surfs->decals = nullptr;
				}
				else
				{
					QueueSkyPortal(surfs);
				}
			}

			surfs = surfs->next;
		} while (surfs);
		return;
	} // done skybox rendering

	do
	{
		if (surfs->plane->PointOnSide(vieworg))
		{
			//	Viewer is in back side or on plane
			continue;
		}

		surfs->Light = (lLev << 24) | LightParams->LightColour;
		surfs->Fade = Fade;
		surfs->dlightframe = r_sub->dlightframe;
		surfs->dlightbits = r_sub->dlightbits;

		if (texinfo->Alpha > 1.0)
		{
			if (PortalLevel == 0)
			{
				world_surf_t& S = WorldSurfs.Alloc();
				S.Surf = surfs;
				S.Type = 0;
				addDecalsToSurf(surfs, seg);
			}
			else
			{
				QueueWorldSurface(seg, surfs);
			}
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
//	VRenderLevelShared::RenderHorizon
//
//==========================================================================

void VRenderLevelShared::RenderHorizon(drawseg_t* dseg)
{
	guard(VRenderLevelShared::RenderHorizon);
	seg_t* Seg = dseg->seg;

	if (!dseg->HorizonTop)
	{
		dseg->HorizonTop = (surface_t*)Z_Malloc(HORIZON_SURF_SIZE);
		dseg->HorizonBot = (surface_t*)Z_Malloc(HORIZON_SURF_SIZE);
		memset(dseg->HorizonTop, 0, HORIZON_SURF_SIZE);
		memset(dseg->HorizonBot, 0, HORIZON_SURF_SIZE);
	}

	//	Horizon is not supported in sectors with slopes, so just use TexZ.
	float TopZ = r_region->ceiling->TexZ;
	float BotZ = r_region->floor->TexZ;
	float HorizonZ = vieworg.z;

	//	Handle top part.
	if (TopZ > HorizonZ)
	{
		sec_surface_t* Ceil = r_subregion->ceil;

		//	Calculate light and fade.
		sec_params_t* LightParams = Ceil->secplane->LightSourceSector != -1 ?
			&Level->Sectors[Ceil->secplane->LightSourceSector].params :
			r_region->params;
		int lLev = FixedLight ? FixedLight :
			MIN(255, LightParams->lightlevel + ExtraLight);
		if (r_darken)
		{
			lLev = light_remap[lLev];
		}
		vuint32 Fade = GetFade(r_region);

		surface_t* Surf = dseg->HorizonTop;
		Surf->plane = dseg->seg;
		Surf->texinfo = &Ceil->texinfo;
		Surf->HorizonPlane = Ceil->secplane;
		Surf->Light = (lLev << 24) | LightParams->LightColour;
		Surf->Fade = Fade;
		Surf->count = 4;
		TVec* svs = &Surf->verts[0];
		svs[0] = *Seg->v1;
		svs[0].z = MAX(BotZ, HorizonZ);
		svs[1] = *Seg->v1;
		svs[1].z = TopZ;
		svs[2] = *Seg->v2;
		svs[2].z = TopZ;
		svs[3] = *Seg->v2;
		svs[3].z = MAX(BotZ, HorizonZ);
		if (Ceil->secplane->pic == skyflatnum)
		{
			//	If it's a sky, render it as a regular sky surface.
			DrawSurfaces(nullptr, Surf, &Ceil->texinfo, r_region->ceiling->SkyBox, -1,
				Seg->sidedef->Light, !!(Seg->sidedef->Flags & SDF_ABSLIGHT),
				false);
		}
		else
		{
			if (PortalLevel == 0)
			{
				world_surf_t& S = WorldSurfs.Alloc();
				S.Surf = Surf;
				S.Type = 2;
				Surf->decals = nullptr;
			}
			else
			{
				QueueHorizonPortal(Surf);
			}
		}
	}

	//	Handle bottom part.
	if (BotZ < HorizonZ)
	{
		sec_surface_t* Floor = r_subregion->floor;

		//	Calculate light and fade.
		sec_params_t* LightParams = Floor->secplane->LightSourceSector != -1 ?
			&Level->Sectors[Floor->secplane->LightSourceSector].params :
			r_region->params;
		int lLev = FixedLight ? FixedLight :
			MIN(255, LightParams->lightlevel + ExtraLight);
		if (r_darken)
		{
			lLev = light_remap[lLev];
		}
		vuint32 Fade = GetFade(r_region);

		surface_t* Surf = dseg->HorizonBot;
		Surf->plane = dseg->seg;
		Surf->texinfo = &Floor->texinfo;
		Surf->HorizonPlane = Floor->secplane;
		Surf->Light = (lLev << 24) | LightParams->LightColour;
		Surf->Fade = Fade;
		Surf->count = 4;
		TVec* svs = &Surf->verts[0];
		svs[0] = *Seg->v1;
		svs[0].z = BotZ;
		svs[1] = *Seg->v1;
		svs[1].z = MIN(TopZ, HorizonZ);
		svs[2] = *Seg->v2;
		svs[2].z = MIN(TopZ, HorizonZ);
		svs[3] = *Seg->v2;
		svs[3].z = BotZ;
		if (Floor->secplane->pic == skyflatnum)
		{
			//	If it's a sky, render it as a regular sky surface.
			DrawSurfaces(nullptr, Surf, &Floor->texinfo, r_region->floor->SkyBox, -1,
				Seg->sidedef->Light, !!(Seg->sidedef->Flags & SDF_ABSLIGHT),
				false);
		}
		else
		{
			if (PortalLevel == 0)
			{
				world_surf_t& S = WorldSurfs.Alloc();
				S.Surf = Surf;
				S.Type = 2;
				Surf->decals = nullptr;
			}
			else
			{
				QueueHorizonPortal(Surf);
			}
		}
	}
	unguard;
}

//==========================================================================
//
//	VRenderLevelShared::RenderMirror
//
//==========================================================================

void VRenderLevelShared::RenderMirror(drawseg_t* dseg)
{
	guard(VRenderLevelShared::RenderMirror);
	seg_t* Seg = dseg->seg;

	if (Drawer->HaveStencil && MirrorLevel < r_maxmirrors)
	{
		VPortal* Portal = NULL;
		for (int i = 0; i < Portals.Num(); i++)
		{
			if (Portals[i] && Portals[i]->MatchMirror(Seg))
			{
				Portal = Portals[i];
				break;
			}
		}
		if (!Portal)
		{
			Portal = new VMirrorPortal(this, Seg);
			Portals.Append(Portal);
		}

		surface_t* surfs = dseg->mid->surfs;
		do
		{
			Portal->Surfs.Append(surfs);
			surfs = surfs->next;
		} while (surfs);
	}
	else
	{
		DrawSurfaces(Seg, dseg->mid->surfs, &dseg->mid->texinfo,
			r_region->ceiling->SkyBox, -1, Seg->sidedef->Light,
			!!(Seg->sidedef->Flags & SDF_ABSLIGHT), false);
	}
	unguard;
}

//==========================================================================
//
//	VRenderLevelShared::RenderLine
//
// 	Clips the given segment and adds any visible pieces to the line list.
//
//==========================================================================

void VRenderLevelShared::RenderLine(drawseg_t* dseg)
{
	guard(VRenderLevelShared::RenderLine);
	seg_t *line = dseg->seg;

	if (!line->linedef)
	{
		//	Miniseg
		return;
	}

	if (line->PointOnSide(vieworg))
	{
		//	Viewer is in back side or on plane
		return;
	}

	if (MirrorClipSegs)
	{
		//	Clip away segs that are behind mirror.
		if (view_clipplanes[4].PointOnSide(*line->v1) && view_clipplanes[4].PointOnSide(*line->v2))
		{
			//	Behind mirror.
			return;
		}
	}

	if (line->backsector)
	{
		// Just apply this to sectors without slopes
		if (line->frontsector->floor.normal.z == 1.0 && line->backsector->floor.normal.z == 1.0 &&
			line->frontsector->ceiling.normal.z == -1.0 && line->backsector->ceiling.normal.z == -1.0)
		{
			// Clip sectors that are behind rendered segs
			TVec v1 = *line->v1;
			TVec v2 = *line->v2;
			TVec r1 = vieworg - v1;
			TVec r2 = vieworg - v2;
			float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), vieworg);
			float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), vieworg);

			// There might be a better method of doing this, but
			// this one works for now...
			if (D1 > 0.0 && D2 < 0.0)
			{
				v2 += ((v2 - v1) * D1) / (D1 - D2);
			}
			else if (D2 > 0.0 && D1 < 0.0)
			{
				v1 += ((v2 - v1) * D1) / (D2 - D1);
			}

			if (!ViewClip.IsRangeVisible(ViewClip.PointToClipAngle(v2),
				ViewClip.PointToClipAngle(v1)))
			{
				return;
			}
		}
	}
	else
	{
		// Clip sectors that are behind rendered segs
		TVec v1 = *line->v1;
		TVec v2 = *line->v2;
		TVec r1 = vieworg - v1;
		TVec r2 = vieworg - v2;
		float D1 = DotProduct(Normalise(CrossProduct(r1, r2)), vieworg);
		float D2 = DotProduct(Normalise(CrossProduct(r2, r1)), vieworg);

		// There might be a better method of doing this, but
		// this one works for now...
		if (D1 > 0.0 && D2 < 0.0)
		{
			v2 += ((v2 - v1) * D1) / (D1 - D2);
		}
		else if (D2 > 0.0 && D1 < 0.0)
		{
			v1 += ((v2 - v1) * D1) / (D2 - D1);
		}

		if (!ViewClip.IsRangeVisible(ViewClip.PointToClipAngle(v2),
			ViewClip.PointToClipAngle(v1)))
		{
			return;
		}
	}

	line_t *linedef = line->linedef;
	side_t *sidedef = line->sidedef;

	//FIXME this marks all lines
	// mark the segment as visible for auto map
	linedef->flags |= ML_MAPPED;

	if (!line->backsector)
	{
		// single sided line
		if (line->linedef->special == LNSPEC_LineHorizon)
		{
			RenderHorizon(dseg);
		}
		else if (line->linedef->special == LNSPEC_LineMirror)
		{
			RenderMirror(dseg);
		}
		else
		{
			DrawSurfaces(line, dseg->mid->surfs, &dseg->mid->texinfo,
				r_region->ceiling->SkyBox, -1, sidedef->Light,
				!!(sidedef->Flags & SDF_ABSLIGHT), false);
		}
		DrawSurfaces(nullptr, dseg->topsky->surfs, &dseg->topsky->texinfo,
			r_region->ceiling->SkyBox, -1, sidedef->Light,
			!!(sidedef->Flags & SDF_ABSLIGHT), false);
	}
	else
	{
		// two sided line
		DrawSurfaces(line, dseg->top->surfs, &dseg->top->texinfo,
			r_region->ceiling->SkyBox, -1, sidedef->Light,
			!!(sidedef->Flags & SDF_ABSLIGHT), false);
		DrawSurfaces(nullptr, dseg->topsky->surfs, &dseg->topsky->texinfo,
			r_region->ceiling->SkyBox, -1, sidedef->Light,
			!!(sidedef->Flags & SDF_ABSLIGHT), false);
		DrawSurfaces(line, dseg->bot->surfs, &dseg->bot->texinfo,
			r_region->ceiling->SkyBox, -1, sidedef->Light,
			!!(sidedef->Flags & SDF_ABSLIGHT), false);
		DrawSurfaces(line, dseg->mid->surfs, &dseg->mid->texinfo,
			r_region->ceiling->SkyBox, -1, sidedef->Light,
			!!(sidedef->Flags & SDF_ABSLIGHT), false);
		for (segpart_t *sp = dseg->extra; sp; sp = sp->next)
		{
			DrawSurfaces(line, sp->surfs, &sp->texinfo, r_region->ceiling->SkyBox,
				-1, sidedef->Light, !!(sidedef->Flags & SDF_ABSLIGHT), false);
		}
	}
	unguard;
}

//==========================================================================
//
//	VRenderLevelShared::RenderSecSurface
//
//==========================================================================

void VRenderLevelShared::RenderSecSurface(sec_surface_t* ssurf,
	VEntity* SkyBox)
{
	guard(VRenderLevelShared::RenderSecSurface);
	sec_plane_t& plane = *ssurf->secplane;

	if (!plane.pic)
	{
		return;
	}

	if (plane.PointOnSide(vieworg))
	{
		//	Viewer is in back side or on plane
		return;
	}

	if (plane.MirrorAlpha < 1.0 && Drawer->HaveStencil &&
		MirrorLevel < r_maxmirrors)
	{
		VPortal* Portal = NULL;
		for (int i = 0; i < Portals.Num(); i++)
		{
			if (Portals[i] && Portals[i]->MatchMirror(&plane))
			{
				Portal = Portals[i];
				break;
			}
		}
		if (!Portal)
		{
			Portal = new VMirrorPortal(this, &plane);
			Portals.Append(Portal);
		}

		surface_t* surfs = ssurf->surfs;
		do
		{
			Portal->Surfs.Append(surfs);
			surfs = surfs->next;
		} while (surfs);

		if (plane.MirrorAlpha <= 0.0)
		{
			return;
		}
		ssurf->texinfo.Alpha = plane.MirrorAlpha;
	}

	DrawSurfaces(nullptr, ssurf->surfs, &ssurf->texinfo, SkyBox,
		plane.LightSourceSector, 0, false, true);
	unguard;
}

//==========================================================================
//
//	VRenderLevelShared::RenderSubRegion
//
// 	Determine floor/ceiling planes.
// 	Draw one or more line segments.
//
//==========================================================================

void VRenderLevelShared::RenderSubRegion(subregion_t* region)
{
	guard(VRenderLevelShared::RenderSubRegion);
	int				count;
	int 			polyCount;
	seg_t**			polySeg;
	float			d;

	d = DotProduct(vieworg, region->floor->secplane->normal)-region->floor->secplane->dist;
	if (region->next && d <= 0.0)
	{
		if (!ViewClip.ClipCheckRegion(region->next, r_sub, false))
		{
			return;
		}
		RenderSubRegion(region->next);
	}

	check(r_sub->sector != NULL);

	r_subregion = region;
	r_region = region->secregion;

	if (r_sub->poly)
	{
		//	Render the polyobj in the subsector first
		polyCount = r_sub->poly->numsegs;
		polySeg = r_sub->poly->segs;
		while (polyCount--)
		{
			RenderLine((*polySeg)->drawsegs);
			polySeg++;
		}
	}

	count = r_sub->numlines;
	drawseg_t *ds = region->lines;
	while (count--)
	{
		RenderLine(ds);
		ds++;
	}

	RenderSecSurface(region->floor, r_region->floor->SkyBox);
	RenderSecSurface(region->ceil, r_region->ceiling->SkyBox);

	if (region->next && d > 0.0)
	{
		if (!ViewClip.ClipCheckRegion(region->next, r_sub, false))
		{
			return;
		}
		RenderSubRegion(region->next);
	}
	unguard;
}

//==========================================================================
//
//	VRenderLevelShared::RenderSubsector
//
//==========================================================================

void VRenderLevelShared::RenderSubsector(int num)
{
	guard(VRenderLevelShared::RenderSubsector);
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

	if (!ViewClip.ClipCheckSubsector(Sub, false))
	{
		return;
	}

	BspVis[num >> 3] |= 1 << (num & 7);

	RenderSubRegion(Sub->regions);

	//	Add subsector's segs to the clipper. Clipping against mirror
	// is done only for vertical mirror planes.
	ViewClip.ClipAddSubsectorSegs(Sub, false, MirrorClipSegs ? &view_clipplanes[4] :
		NULL);
	unguard;
}

//==========================================================================
//
//	VRenderLevelShared::RenderBSPNode
//
//	Renders all subsectors below a given node, traversing subtree
// recursively. Just call with BSP root.
//
//==========================================================================

void VRenderLevelShared::RenderBSPNode(int bspnum, float* bbox, int AClipflags)
{
	guard(VRenderLevelShared::RenderBSPNode);
	if (ViewClip.ClipIsFull())
	{
		return;
	}
	int clipflags = AClipflags;
	// cull the clipping planes if not trivial accept
	if (clipflags)
	{
		for (int i = 0; i < 5; i++)
		{
			if (!(clipflags & view_clipplanes[i].clipflag))
			{
				continue;	// don't need to clip against it
			}
			if (view_clipplanes[i].PointOnSide(vieworg))
			{
				continue;
			}

			// generate accept and reject points
			int *pindex = FrustumIndexes[i];

			TVec rejectpt;

			rejectpt[0] = bbox[pindex[0]];
			rejectpt[1] = bbox[pindex[1]];
			rejectpt[2] = bbox[pindex[2]];

			if (view_clipplanes[i].PointOnSide(rejectpt))
			{
				continue;
			}

			TVec acceptpt;

			acceptpt[0] = bbox[pindex[3+0]];
			acceptpt[1] = bbox[pindex[3+1]];
			acceptpt[2] = bbox[pindex[3+2]];

			if (!view_clipplanes[i].PointOnSide(acceptpt))
			{
				clipflags ^= view_clipplanes[i].clipflag;	// node is entirely on screen
			}
		}
	}

	if (!ViewClip.ClipIsBBoxVisible(bbox, false))
	{
		return;
	}

	if (bspnum == -1)
	{
		RenderSubsector(0);
		return;
	}

	// Found a subsector?
	if (!(bspnum & NF_SUBSECTOR))
	{
		node_t* bsp = &Level->Nodes[bspnum];

		if (bsp->VisFrame != r_visframecount)
		{
			return;
		}

		// Decide which side the view point is on.
		int side = bsp->PointOnSide(vieworg);

		// Recursively divide front space (toward the viewer).
		RenderBSPNode(bsp->children[side], bsp->bbox[side], clipflags);

		// Possibly divide back space (away from the viewer).
		if (!ViewClip.ClipIsBBoxVisible(bsp->bbox[side ^ 1], false))
		{
			return;
		}

		RenderBSPNode(bsp->children[side ^ 1], bsp->bbox[side ^ 1], clipflags);
		return;
	}

	RenderSubsector(bspnum & (~NF_SUBSECTOR));
	unguard;
}

//==========================================================================
//
//	VRenderLevelShared::RenderBspWorld
//
//==========================================================================

void VRenderLevelShared::RenderBspWorld(const refdef_t* rd, const VViewClipper* Range)
{
	guard(VRenderLevelShared::RenderBspWorld);
	float	dummy_bbox[6] = { -99999, -99999, -99999, 99999, 99999, 99999 };

	SetUpFrustumIndexes();
	ViewClip.ClearClipNodes(vieworg, Level);
	ViewClip.ClipInitFrustrumRange(viewangles, viewforward, viewright, viewup,
		rd->fovx, rd->fovy);
	if (Range)
	{
		//	Range contains a valid range, so we must clip away holes in it.
		ViewClip.ClipToRanges(*Range);
	}
	memset(BspVis, 0, VisSize);
	if (PortalLevel == 0)
	{
		WorldSurfs.Resize(4096);
	}
	MirrorClipSegs = MirrorClip && !view_clipplanes[4].normal.z;

	// head node is the last node output
	RenderBSPNode(Level->NumNodes - 1, dummy_bbox, MirrorClip ? 31 : 15);

	if (PortalLevel == 0)
	{
		guard(VRenderLevelShared::RenderBspWorld::Best sky);
		//	Draw the most complex sky portal behind the scene first, without
		// the need to use stencil buffer.
		VPortal* BestSky = NULL;
		int BestSkyIndex = -1;
		for (int i = 0; i < Portals.Num(); i++)
		{
			if (Portals[i] && Portals[i]->IsSky() &&
				(!BestSky || BestSky->Surfs.Num() < Portals[i]->Surfs.Num()))
			{
				BestSky = Portals[i];
				BestSkyIndex = i;
			}
		}
		if (BestSky)
		{
			PortalLevel = 1;
			BestSky->Draw(false);
			delete BestSky;
			BestSky = NULL;
			Portals.RemoveIndex(BestSkyIndex);
			PortalLevel = 0;
		}
		unguard;

		guard(VRenderLevelShared::RenderBspWorld::World surfaces);
		for (int i = 0; i < WorldSurfs.Num(); i++)
		{
			switch (WorldSurfs[i].Type)
			{
			case 0:
				{
					seg_t* dcseg = (WorldSurfs[i].Surf->decals ? WorldSurfs[i].Surf->decals->seg : nullptr);
					//if (dcseg) printf("world surface! %p\n", dcseg);
					QueueWorldSurface(dcseg, WorldSurfs[i].Surf);
				}
				break;
			case 1:
				QueueSkyPortal(WorldSurfs[i].Surf);
				break;
			case 2:
				QueueHorizonPortal(WorldSurfs[i].Surf);
				break;
			}
		}
		WorldSurfs.Clear();
		unguard;
	}
	unguard;
}

//==========================================================================
//
//	VRenderLevelShared::RenderPortals
//
//==========================================================================

void VRenderLevelShared::RenderPortals()
{
	guard(VRenderLevelShared::RenderPortals);
	PortalLevel++;
	if (Drawer->HaveStencil)
	{
		for (int i = 0; i < Portals.Num(); i++)
		{
			if (Portals[i] && Portals[i]->Level == PortalLevel)
			{
				Portals[i]->Draw(true);
			}
		}
	}
	for (int i = 0; i < Portals.Num(); i++)
	{
		if (Portals[i] && Portals[i]->Level == PortalLevel)
		{
			delete Portals[i];
			Portals[i] = NULL;
		}
	}
	PortalLevel--;
	if (PortalLevel == 0)
	{
		Portals.Clear();
	}
	unguard;
}

//==========================================================================
//
//	VRenderLevel::RenderWorld
//
//==========================================================================

void VRenderLevel::RenderWorld(const refdef_t* rd, const VViewClipper* Range)
{
	guard(VRenderLevel::RenderWorld);
	RenderBspWorld(rd, Range);

	Drawer->WorldDrawing();

	RenderPortals();
	unguard;
}

//==========================================================================
//
//	VAdvancedRenderLevel::RenderWorld
//
//==========================================================================

void VAdvancedRenderLevel::RenderWorld(const refdef_t* rd, const VViewClipper* Range)
{
	guard(VAdvancedRenderLevel::RenderWorld);
	RenderBspWorld(rd, Range);

	Drawer->DrawWorldAmbientPass();

	RenderPortals();
	unguard;
}

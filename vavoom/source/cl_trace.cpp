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

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "cl_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int					cl_validcount;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static TVec			TraceStart;
static TVec			TraceEnd;
static TVec			TraceDelta;
static TPlane		TracePlane;			// from t1 to t2

static TVec			LineStart;
static TVec			LineEnd;

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	PlaneSide2
//
//	Returns side 0 (front), 1 (back), or 2 (on).
//
//==========================================================================

static int PlaneSide2(const TVec &point, const TPlane* plane)
{
	float dot = DotProduct(point, plane->normal) - plane->dist;
	return dot < -0.1 ? 1 : dot > 0.1 ? 0 : 2;
}

//==========================================================================
//
//	CheckPlane
//
//==========================================================================

static bool CheckPlane(const sec_plane_t *plane)
{
	guard(CheckPlane);
	float		org_dist;
	float		hit_dist;

	if (plane->flags & SPF_NOBLOCKSIGHT)
	{
		//	Plane doesn't block sight
		return true;
	}
	org_dist = DotProduct(LineStart, plane->normal) - plane->dist;
	if (org_dist < -0.1)
	{
		//	Ignore back side
		return true;
	}
	hit_dist = DotProduct(LineEnd, plane->normal) - plane->dist;
	if (hit_dist >= -0.1)
	{
		//	Didn't cross plane
		return true;
	}

	//	Crosses plane
	return false;
	unguard;
}

//==========================================================================
//
//	CheckPlanes
//
//==========================================================================

static bool CheckPlanes(sector_t *sec)
{
	guard(CheckPlanes);
	sec_region_t	*reg;

	for (reg = sec->topregion; reg; reg = reg->prev)
	{
		if (!CheckPlane(reg->floor))
		{
			//	Hit floor
			return false;
		}
		if (!CheckPlane(reg->ceiling))
		{
			//	Hit ceiling
			return false;
		}
	}
	return true;
	unguard;
}

//==========================================================================
//
//	CheckLine
//
//==========================================================================

static bool CheckLine(seg_t* seg)
{
	guard(CheckLine);
	line_t*			line;
	int				s1;
	int				s2;
	sector_t*		front;
	float			opentop;
	float			openbottom;
	float			frac;
	float			num;
	float			den;
	TVec			hit_point;

	line = seg->linedef;
	if (!line)
		return true;

	// allready checked other side?
	if (line->validcount == cl_validcount)
		return true;
	
	line->validcount = cl_validcount;

	s1 = PlaneSide2(*line->v1, &TracePlane);
	s2 = PlaneSide2(*line->v2, &TracePlane);

	// line isn't crossed?
	if (s1 == s2)
		return true;

	s1 = PlaneSide2(TraceStart, line);
	s2 = PlaneSide2(TraceEnd, line);

	// line isn't crossed?
	if (s1 == s2 || (s1 == 2 && s2 == 0))
		return true;

	// stop because it is not two sided anyway
	if (!(line->flags & ML_TWOSIDED))
		return false;
	
	// crosses a two sided line
	if (s1 == 0)
	{
		front = line->frontsector;
	}
	else
	{
		front = line->backsector;
	}

	// Intercept vector.
	// Don't need to check if den == 0, because then planes are paralel
	// (they will never cross) or it's the same plane (also rejected)
	den = DotProduct(TraceDelta, line->normal);
	num = line->dist - DotProduct(TraceStart, line->normal);
	frac = num / den;
	hit_point = TraceStart + frac * TraceDelta;

	LineEnd = hit_point;
	if (!CheckPlanes(front))
	{
		return false;
	}
	LineStart = LineEnd;

	sec_region_t	*frontreg;
	sec_region_t	*backreg;
	float			frontfloorz;
	float			backfloorz;
	float			frontceilz;
	float			backceilz;

	frontreg = line->frontsector->botregion;
	backreg = line->backsector->botregion;

	while (frontreg && backreg)
	{
		frontfloorz = frontreg->floor->GetPointZ(hit_point);
		backfloorz = backreg->floor->GetPointZ(hit_point);
		frontceilz = frontreg->ceiling->GetPointZ(hit_point);
		backceilz = backreg->ceiling->GetPointZ(hit_point);
		if (frontfloorz >= backceilz)
		{
			backreg = backreg->next;
			continue;
		}
		if (backfloorz >= frontceilz)
		{
			frontreg = frontreg->next;
			continue;
		}

		if (frontfloorz > backfloorz)
		{
			openbottom = frontfloorz;
		}
		else
		{
			openbottom = backfloorz;
		}
		if (frontceilz < backceilz)
		{
			opentop = frontceilz;
			frontreg = frontreg->next;
		}
		else
		{
			opentop = backceilz;
			backreg = backreg->next;
		}
		if (hit_point.z >= openbottom && hit_point.z <= opentop)
		{
			return true;
		}
	}

	return false;		// stop
	unguard;
}

//==========================================================================
//
//	CrossSubsector
//
//	Returns true if TracePlane crosses the given subsector successfully.
//
//==========================================================================

static bool CrossSubsector(int num)
{
	guard(CrossSubsector);
	subsector_t*	sub;
	int				count;
	seg_t*			seg;
	int 			polyCount;
	seg_t**			polySeg;

	sub = &GClLevel->Subsectors[num];
	
	if (sub->poly)
	{
		// Check the polyobj in the subsector first
		polyCount = sub->poly->numsegs;
		polySeg = sub->poly->segs;
		while (polyCount--)
		{
			if (!CheckLine(*polySeg++))
			{
				return false;
			}
		}
	}

	// check lines
	count = sub->numlines;
	seg = &GClLevel->Segs[sub->firstline];

	for ( ; count ; seg++, count--)
	{
		if (!CheckLine(seg))
		{
			return false;
		}
	}
	// passed the subsector ok
	return true;
	unguard;
}

//==========================================================================
//
//	CrossBSPNode
//
//	Returns true if TracePlane crosses the given node successfully.
//
//==========================================================================

static bool CrossBSPNode(int bspnum)
{
	guard(CrossBSPNode);
	node_t*	bsp;
	int		side;

	if (bspnum & NF_SUBSECTOR)
	{
		if (bspnum == -1)
			return CrossSubsector(0);
		else
			return CrossSubsector(bspnum & (~NF_SUBSECTOR));
	}

	bsp = &GClLevel->Nodes[bspnum];
	
	// decide which side the start point is on
	side = PlaneSide2(TraceStart, bsp);
	if (side == 2)
		side = 0;	// an "on" should cross both sides

	// cross the starting side
	if (!CrossBSPNode(bsp->children[side]))
		return false;
	
	// the partition plane is crossed here
	if (side == PlaneSide2(TraceEnd, bsp))
	{
		// the line doesn't touch the other side
		return true;
	}

	// cross the ending side		
	return CrossBSPNode(bsp->children[side^1]);
	unguard;
}

//==========================================================================
//
//	CL_TraceLine
//
//==========================================================================

bool CL_TraceLine(const TVec &start, const TVec &end)
{
	guard(CL_TraceLine);
	cl_validcount++;

	TraceStart = start;
	TraceEnd = end;

	TraceDelta = TraceEnd - TraceStart;
	TracePlane.SetPointDir(start, TraceDelta);

	LineStart = TraceStart;

	// the head node is the last node output
	if (CrossBSPNode(GClLevel->NumNodes - 1))
	{
		LineEnd = TraceEnd;
		return CheckPlanes(GClLevel->PointInSubsector(end)->sector);
	}
	return false;
	unguard;
}

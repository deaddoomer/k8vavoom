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

static TVec			trace_start;
static TVec			trace_end;
static TVec			trace_delta;
static TPlane		trace;			// from t1 to t2

static TVec			linestart;
static TVec			lineend;

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
	org_dist = DotProduct(linestart, plane->normal) - plane->dist;
	if (org_dist < -0.1)
	{
		//	Ignore back side
		return true;
	}
	hit_dist = DotProduct(lineend, plane->normal) - plane->dist;
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

	s1 = PlaneSide2(*line->v1, &trace);
	s2 = PlaneSide2(*line->v2, &trace);

	// line isn't crossed?
	if (s1 == s2)
	    return true;

	s1 = PlaneSide2(trace_start, line);
	s2 = PlaneSide2(trace_end, line);

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
    den = DotProduct(trace_delta, line->normal);
	num = line->dist - DotProduct(trace_start, line->normal);
	frac = num / den;
	hit_point = trace_start + frac * trace_delta;

	lineend = hit_point;
	if (!CheckPlanes(front))
	{
		return false;
	}
	linestart = lineend;

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
//	Returns true if trace crosses the given subsector successfully.
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

    sub = &cl_level.subsectors[num];
    
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
    seg = &cl_level.segs[sub->firstline];

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
//	Returns true if trace crosses the given node successfully.
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
		
    bsp = &cl_level.nodes[bspnum];
    
    // decide which side the start point is on
    side = PlaneSide2(trace_start, bsp);
    if (side == 2)
		side = 0;	// an "on" should cross both sides

    // cross the starting side
    if (!CrossBSPNode(bsp->children[side]))
		return false;
	
    // the partition plane is crossed here
    if (side == PlaneSide2(trace_end, bsp))
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

	trace_start = start;
	trace_end = end;

	trace_delta = trace_end - trace_start;
	trace.SetPointDir(start, trace_delta);

	linestart = trace_start;

    // the head node is the last node output
    if (CrossBSPNode(cl_level.numnodes - 1))
	{
		lineend = trace_end;
	    return CheckPlanes(CL_PointInSubsector(end.x, end.y)->sector);
	}
	return false;
	unguard;
}

//**************************************************************************
//
//	$Log$
//	Revision 1.8  2002/08/05 17:20:00  dj_jl
//	Added guarding.
//
//	Revision 1.7  2002/04/11 16:44:44  dj_jl
//	Got rid of some warnings.
//	
//	Revision 1.6  2002/01/07 12:16:41  dj_jl
//	Changed copyright year
//	
//	Revision 1.5  2001/10/27 07:51:27  dj_jl
//	Beautification
//	
//	Revision 1.4  2001/10/08 17:34:57  dj_jl
//	A lots of small changes and cleanups
//	
//	Revision 1.3  2001/07/31 17:16:30  dj_jl
//	Just moved Log to the end of file
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************

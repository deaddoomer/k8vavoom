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

#include "d_local.h"

// MACROS ------------------------------------------------------------------

#define MAX_SURFS			2000
#define MAX_EDGES			8000
#define MAX_SPANS			3000

#define SURF_SKY			1
#define SURF_SKY_BOX		2
#define SURF_BACKGROUND		4
#define SURF_HORIZON		8

//	Theoretically cliping can give only 4 new vertexes. In practice due to
// roundof errors we can get more extra vertexes
#define NUM_EXTRA_VERTS		16
#define MAX_STACK_VERTS		64

// TYPES -------------------------------------------------------------------

struct edge_t
{
	fixed_t		u;
	fixed_t		u_step;
	edge_t		*prev;
	edge_t		*next;
	word		surfs[2];
	edge_t		*nextremove;
};

struct surf_t
{
	surf_t		*next;			// active surface stack in r_edge.c
	surf_t		*prev;			// used in r_edge.c for active surf stack
	espan_t		*spans;			// pointer to linked list of spans to draw
	int			key;				// sorting key (BSP order)
	int			last_u;				// set during tracing
	int			spanstate;			// 0 = not in span
									// 1 = in span
									// -1 = in inverted span (end before
									//  start)
	int			flags;				// currentface flags
	float		nearzi;				// nearest 1/z on surface, for mipmapping

	surface_t	*surf;

	VTexture*	Texture1;
	VTexture*	Texture2;
	float		offs1;
	float		offs2;

	int			ColourMap;

	// Make size 64 bytes to simplify asm
	int			reserved1;
	int			reserved2;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

extern "C"
{
void D_ClipEdge(const TVec&, const TVec&, TClipPlane*, int);
void D_GenerateSpans();
}

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float	scale_for_mip;
extern float	d_scalemip[3];
extern int		d_minmip;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

extern "C" {

#if !USE_ASM_I386
float			d_zistepu;
float			d_zistepv;
float			d_ziorigin;
float			d_sdivzstepu;
float			d_tdivzstepu;
float			d_sdivzstepv;
float			d_tdivzstepv;
float			d_sdivzorigin;
float			d_tdivzorigin;
fixed_t			sadjust;
fixed_t			tadjust;
fixed_t			bbextents;
fixed_t			bbextentt;

void*			cacheblock;
int				cachewidth;
byte*			d_transluc;// For translucent spans
word			*d_srctranstab;
word			*d_dsttranstab;
#endif

float			r_nearzi;
int				r_emited;
float			d_u1, d_v1;
int				d_ceilv1;
int				d_lastvertvalid;
TVec			firstvert;

edge_t			r_edges[MAX_EDGES];
edge_t			*edge_p, *edge_max;

surf_t			surfaces[MAX_SURFS];
surf_t			*surface_p, *surf_max;

// surfaces are generated in back to front order by the bsp, so if a surf
// pointer is greater than another one, it should be drawn in front
// surfaces[1] is the background, and is used as the active surface stack

edge_t			*newedges[MAXSCREENHEIGHT];
edge_t			*removeedges[MAXSCREENHEIGHT];

edge_t			edge_head;
edge_t			edge_tail;

espan_t			*span_p, *max_span_p;

int				current_iv;

int				d_HorizonLight;

}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int		r_currentkey;

static int		outofsurfs;
static int		outofedges;

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	D_BeginEdgeFrame
//
//==========================================================================

void VSoftwareDrawer::BeginEdgeFrame()
{
	guard(D_BeginEdgeFrame);
	edge_p = r_edges;
	edge_max = &r_edges[MAX_EDGES];

	surface_p = &surfaces[2];	// background is surface 1,
								//  surface 0 is a dummy
	surf_max = &surfaces[MAX_SURFS];
	surfaces[1].spans = NULL;	// no background spans yet
	surfaces[1].flags =	SURF_BACKGROUND;

	// put the background behind everything in the world
	surfaces[1].key = 0x7FFFFFFF;
	r_currentkey = 0;

	memset(newedges, 0, sizeof(newedges));
	memset(removeedges, 0, sizeof(removeedges));

	outofsurfs = 0;
	outofedges = 0;
	unguard;
}

#if !USE_ASM_I386

//==========================================================================
//
//	D_EmitEdge
//
//==========================================================================

static void D_EmitEdge(const TVec &pv0, const TVec &pv1)
{
	edge_t		*edge, *pcheck;
	int			u_check;
	float		z1, u, u_step;
	int			v, v2, ceilv0, ceilv1;
	float		u0, v0;
	float		u1, v1;
	TVec		tr;

	if (d_lastvertvalid)
	{
		u0 = d_u1;
		v0 = d_v1;
		ceilv0 = d_ceilv1;
	}
	else
	{
		firstvert = pv0;
		TransformVector(pv0 - vieworg, tr);
		if (tr.z < 0.01)
			tr.z = 0.01;

		z1 = 1.0f / tr.z;
		if (z1 > r_nearzi)
		{
			r_nearzi = z1;
		}
		u0 = tr.x * z1 * xprojection + centrexfrac;
		v0 = tr.y * z1 * yprojection + centreyfrac;

		if (u0 < vrectx_adj)
			u0 = vrectx_adj;
		if (u0 > vrectw_adj)
			u0 = vrectw_adj;
		if (v0 < vrecty_adj)
			v0 = vrecty_adj;
		if (v0 > vrecth_adj)
			v0 = vrecth_adj;

		ceilv0 = (int)ceil(v0);
	}

	TransformVector(pv1 - vieworg, tr);
	if (tr.z < 0.01)
		tr.z = 0.01;

	z1 = 1.0f / tr.z;
	if (z1 > r_nearzi)
	{
		r_nearzi = z1;
	}
	u1 = tr.x * z1 * xprojection + centrexfrac;
	v1 = tr.y * z1 * yprojection + centreyfrac;

	if (u1 < vrectx_adj)
		u1 = vrectx_adj;
	if (u1 > vrectw_adj)
		u1 = vrectw_adj;
	if (v1 < vrecty_adj)
		v1 = vrecty_adj;
	if (v1 > vrecth_adj)
		v1 = vrecth_adj;

	ceilv1 = (int)ceil(v1);

	d_u1 = u1;
	d_v1 = v1;
	d_ceilv1 = ceilv1;
	d_lastvertvalid = true;

	// create the edge
	if (ceilv0 == ceilv1)
	{
		return;		// horizontal edge
	}

	r_emited = true;

	edge = edge_p++;

	if (ceilv0 > ceilv1)
	{
		// trailing edge (go from p2 to p1)
		v = ceilv1;
		v2 = ceilv0 - 1;

		edge->surfs[0] = surface_p - surfaces;
		edge->surfs[1] = 0;

		u_step = (u0 - u1) / (v0 - v1);
		u = u1 + ((float)ceilv1 - v1) * u_step;
	}
	else
	{
		// leading edge (go from p1 to p2)
		v = ceilv0;
		v2 = ceilv1 - 1;

		edge->surfs[0] = 0;
		edge->surfs[1] = surface_p - surfaces;

		u_step = (u1 - u0) / (v1 - v0);
		u = u0 + ((float)ceilv0 - v0) * u_step;
	}

	edge->u = (fixed_t)(u * 0x100000) + 0xfffff;
	edge->u_step = (fixed_t)(u_step * 0x100000);

	//
	// sort the edge in normally
	//
	u_check = edge->u;
	if (edge->surfs[0])
		u_check++;	// sort trailers after leaders

	if (!newedges[v] || newedges[v]->u >= u_check)
	{
		edge->next = newedges[v];
		newedges[v] = edge;
	}
	else
	{
		pcheck = newedges[v];
		while (pcheck->next && pcheck->next->u < u_check)
			pcheck = pcheck->next;
		edge->next = pcheck->next;
		pcheck->next = edge;
	}

	edge->nextremove = removeedges[v2];
	removeedges[v2] = edge;
}

//==========================================================================
//
//	D_ClipEdge
//
//==========================================================================

extern "C" void D_ClipEdge(const TVec &v0, const TVec &v1,
	TClipPlane *clip, int clipflags)
{
	if (clip)
	{
		do
		{
			if (!(clipflags & clip->clipflag))
			{
				continue;
			}

			float d0 = DotProduct(v0, clip->normal) - clip->dist;
			float d1 = DotProduct(v1, clip->normal) - clip->dist;
			if (d0 >= 0)
			{
				// point 0 is unclipped
				if (d1 >= 0)
				{
					// both points are unclipped
					continue;
				}

				// only point 1 is clipped

				TVec &clipvert = clip->enter;
				clip->entered = true;

				float f = d0 / (d0 - d1);
				clipvert.x = v0.x + f * (v1.x - v0.x);
				clipvert.y = v0.y + f * (v1.y - v0.y);
				clipvert.z = v0.z + f * (v1.z - v0.z);

				D_ClipEdge(v0, clipvert, clip->next, clipflags);
				if (clip->exited)
				{
					clip->entered = false;
					clip->exited = false;
					D_ClipEdge(clipvert, clip->exit, view_clipplanes, clipflags ^ clip->clipflag);
				}
				return;
			}
			else
			{
				// point 0 is clipped

				if (d1 < 0)
				{
					// both points are clipped
					return;
				}

				// only point 0 is clipped

				TVec &clipvert = clip->exit;
				clip->exited = true;

				float f = d0 / (d0 - d1);
				clipvert.x = v0.x + f * (v1.x - v0.x);
				clipvert.y = v0.y + f * (v1.y - v0.y);
				clipvert.z = v0.z + f * (v1.z - v0.z);

				if (clip->entered)
				{
					clip->entered = false;
					clip->exited = false;
					D_ClipEdge(clip->enter, clipvert, view_clipplanes, clipflags ^ clip->clipflag);
				}
				D_ClipEdge(clipvert, v1, clip->next, clipflags);
				return;
			}
		} while ((clip = clip->next) != NULL);
	}

	//	add the vertex
	D_EmitEdge(v0, v1);
}

#endif

//==========================================================================
//
//	D_ClipPolygon
//
//==========================================================================

static bool D_ClipPolygon(surface_t* surf, int clipflags, bool AddViewOrg,
	int Flags)
{
	guard(D_ClipPolygon);
	if (surface_p == surf_max)
	{
		outofsurfs++;
		return false;
	}
	if (edge_p + surf->count + 4 >= edge_max)
	{
		outofedges += surf->count;
		return false;
	}

	for (int i = 0; i < 4; i++)
	{
		view_clipplanes[i].entered = false;
		view_clipplanes[i].exited = false;
	}

	r_emited = 0;
	r_nearzi = 0;
	d_lastvertvalid = false;

	if (!AddViewOrg)
	{
		for (int i = 0; i < surf->count; i++)
		{
			D_ClipEdge(surf->verts[i ? i - 1 : surf->count - 1],
				surf->verts[i], view_clipplanes, clipflags);
		}
	}
	else
	{
		for (int i = 0; i < surf->count; i++)
		{
			D_ClipEdge(surf->verts[i ? i - 1 : surf->count - 1] + vieworg,
				surf->verts[i] + vieworg, view_clipplanes, clipflags);
		}
	}
	if (d_lastvertvalid)
	{
		D_ClipEdge(firstvert, firstvert, NULL, 0);
	}

	if (!r_emited)
	{
		return false;
	}

	surface_p->surf = surf;
	surface_p->spans = NULL;

	surface_p->next = NULL;
	surface_p->prev = NULL;
	surface_p->key = r_currentkey++;
	surface_p->last_u = 0;
	surface_p->spanstate = 0;
	surface_p->flags = Flags;
	surface_p->nearzi = r_nearzi;

	return true;
	unguard;
}

//==========================================================================
//
//	VSoftwareDrawer::DrawPolygon
//
//==========================================================================

void VSoftwareDrawer::DrawPolygon(surface_t* surf, int clipflags)
{
	guard(VSoftwareDrawer::DrawPolygon);
	if (!D_ClipPolygon(surf, clipflags, false, 0))
	{
		return;
	}
	surface_p++;
	unguard;
}

//==========================================================================
//
//	VSoftwareDrawer::DrawSkyPortal
//
//==========================================================================

void VSoftwareDrawer::DrawSkyPortal(surface_t*, int)
{
}

//==========================================================================
//
//	VSoftwareDrawer::BeginSky
//
//==========================================================================

void VSoftwareDrawer::BeginSky()
{
}

//==========================================================================
//
//	VSoftwareDrawer::DrawSkyPolygon
//
//==========================================================================

void VSoftwareDrawer::DrawSkyPolygon(surface_t* surf, bool bIsSkyBox,
	VTexture* Texture1, float offs1, VTexture* Texture2, float offs2,
	int CMap)
{
	guard(VSoftwareDrawer::DrawSkyPolygon);
	if (!D_ClipPolygon(surf, 15, true, bIsSkyBox ? SURF_SKY_BOX : SURF_SKY))
	{
		return;
	}
	surface_p->Texture1 = Texture1;
	surface_p->Texture2 = Texture2;
	surface_p->offs1 = offs1;
	surface_p->offs2 = offs2;
	surface_p->ColourMap = CMap;

	surface_p++;
	unguard;
}

//==========================================================================
//
//	VSoftwareDrawer::EndSky
//
//==========================================================================

void VSoftwareDrawer::EndSky()
{
}

//==========================================================================
//
//	VSoftwareDrawer::DrawHorizonPolygon
//
//==========================================================================

void VSoftwareDrawer::DrawHorizonPolygon(surface_t* surf, int clipflags)
{
	guard(VSoftwareDrawer::DrawHorizonPolygon);
	if (!D_ClipPolygon(surf, clipflags, false, SURF_HORIZON))
	{
		return;
	}
	surface_p++;
	unguard;
}

//==========================================================================
//
//	D_InsertNewEdges
//
//	Adds the edges in the linked list edgestoadd, adding them to the edges
// in the linked list edgelist.  edgestoadd is assumed to be sorted on u,
// and non-empty (this is actually newedges[v]).  edgelist is assumed to be
// sorted on u, with a sentinel at the end (actually, this is the active
// edge table starting at edge_head.next).
//
//==========================================================================

static void D_InsertNewEdges(edge_t *edgestoadd, edge_t *edgelist)
{
	edge_t	*next_edge;

	do
	{
		next_edge = edgestoadd->next;
edgesearch:
		if (edgelist->u >= edgestoadd->u)
			goto addedge;
		edgelist=edgelist->next;
		if (edgelist->u >= edgestoadd->u)
			goto addedge;
		edgelist=edgelist->next;
		if (edgelist->u >= edgestoadd->u)
			goto addedge;
		edgelist=edgelist->next;
		if (edgelist->u >= edgestoadd->u)
			goto addedge;
		edgelist=edgelist->next;
		goto edgesearch;

		// insert edgestoadd before edgelist
addedge:
		edgestoadd->next = edgelist;
		edgestoadd->prev = edgelist->prev;
		edgelist->prev->next = edgestoadd;
		edgelist->prev = edgestoadd;
	} while ((edgestoadd = next_edge) != NULL);
}

//==========================================================================
//
//	D_RemoveEdges
//
//==========================================================================

static void D_RemoveEdges(edge_t *pedge)
{
	do
	{
		pedge->next->prev = pedge->prev;
		pedge->prev->next = pedge->next;
	} while ((pedge = pedge->nextremove) != NULL);
}

//==========================================================================
//
//	D_StepActiveU
//
//==========================================================================

static void D_StepActiveU(edge_t *pedge, edge_t *tail)
{
	edge_t		*pnext_edge, *pwedge;

	while (pedge != tail)
	{
		pedge->u += pedge->u_step;
		if (pedge->u < pedge->prev->u)
		{
			// push it back to keep it sorted
			pnext_edge = pedge->next;

			// pull the edge out of the edge list
			pedge->next->prev = pedge->prev;
			pedge->prev->next = pedge->next;

			// find out where the edge goes in the edge list
			pwedge = pedge->prev->prev;

			while (pwedge->u > pedge->u)
			{
				pwedge = pwedge->prev;
			}

			// put the edge back into the edge list
			pedge->next = pwedge->next;
			pedge->prev = pwedge;
			pedge->next->prev = pedge;
			pwedge->next = pedge;

			pedge = pnext_edge;
		}
		else
		{
			pedge = pedge->next;
		}
	}
}

#if !USE_ASM_I386

//==========================================================================
//
//	D_LeadingEdge
//
//==========================================================================

static void D_LeadingEdge(edge_t *edge)
{
	espan_t			*span;
	surf_t			*surf, *surf2;
	int				iu;

	if (edge->surfs[1])
	{
		// it's adding a new surface in, so find the correct place
		surf = &surfaces[edge->surfs[1]];

		// don't start a span if this is an inverted span, with the end
		// edge preceding the start edge (that is, we've already seen the
		// end edge)
		if (++surf->spanstate == 1)
		{
			surf2 = surfaces[1].next;

			if (surf->key < surf2->key)
				goto newtop;

			do
			{
				surf2 = surf2->next;
			} while (surf->key >= surf2->key);

			goto gotposition;

newtop:
			// emit a span (obscures current top)
			iu = edge->u >> 20;

			if (iu > surf2->last_u)
			{
				span = span_p++;
				span->u = surf2->last_u;
				span->count = iu - span->u;
				span->v = current_iv;
				span->pnext = surf2->spans;
				surf2->spans = span;
			}

			// set last_u on the new span
			surf->last_u = iu;
				
gotposition:
			// insert before surf2
			surf->next = surf2;
			surf->prev = surf2->prev;
			surf2->prev->next = surf;
			surf2->prev = surf;
		}
	}
}

//==========================================================================
//
//	D_TrailingEdge
//
//==========================================================================

static void D_TrailingEdge(surf_t *surf, edge_t *edge)
{
	espan_t			*span;
	int				iu;

	//	Don't generate a span if this is an inverted span, with the end
	// edge preceding the start edge (that is, we haven't seen the
	// start edge yet)
	if (--surf->spanstate == 0)
	{
		if (surf == surfaces[1].next)
		{
			// emit a span (current top going away)
			iu = edge->u >> 20;
			if (iu > surf->last_u)
			{
				span = span_p++;
				span->u = surf->last_u;
				span->count = iu - span->u;
				span->v = current_iv;
				span->pnext = surf->spans;
				surf->spans = span;
			}

			// set last_u on the surface below
			surf->next->last_u = iu;
		}

		surf->prev->next = surf->next;
		surf->next->prev = surf->prev;
	}
}

//==========================================================================
//
//	D_CleanupSpan
//
//==========================================================================

static void D_CleanupSpan()
{
	surf_t	*surf;
	int		iu;
	espan_t	*span;

	//	Now that we've reached the right edge of the screen, we're done with
	// any unfinished surfaces, so emit a span for whatever's on top
	surf = surfaces[1].next;
	iu = viewwidth;//edge_tail_u_shift20;
	if (iu > surf->last_u)
	{
		span = span_p++;
		span->u = surf->last_u;
		span->count = iu - span->u;
		span->v = current_iv;
		span->pnext = surf->spans;
		surf->spans = span;
	}

	// reset spanstate for all surfaces in the surface stack
	do
	{
		surf->spanstate = 0;
		surf = surf->next;
	} while (surf != &surfaces[1]);
}

//==========================================================================
//
//	D_GenerateSpans
//
//==========================================================================

extern "C" void D_GenerateSpans()
{
	edge_t			*edge;
	surf_t			*surf;

	// clear active surfaces to just the background surface
	surfaces[1].next = surfaces[1].prev = &surfaces[1];
	surfaces[1].last_u = 0;

	// generate spans
	for (edge = edge_head.next; edge != &edge_tail; edge=edge->next)
	{			
		if (edge->surfs[0])
		{
			// it has a left surface, so a surface is going away for this span
			surf = &surfaces[edge->surfs[0]];

			D_TrailingEdge(surf, edge);

			if (!edge->surfs[1])
				continue;
		}

		D_LeadingEdge(edge);
	}

	D_CleanupSpan();
}

#endif

//==========================================================================
//
//	D_MipLevelForScale
//
//==========================================================================

int D_MipLevelForScale (float scale)
{
	int		lmiplevel;

	if (scale >= d_scalemip[0])
		lmiplevel = 0;
	else if (scale >= d_scalemip[1])
		lmiplevel = 1;
	else if (scale >= d_scalemip[2])
		lmiplevel = 2;
	else
		lmiplevel = 3;

	if (lmiplevel < d_minmip)
		lmiplevel = d_minmip;

	return lmiplevel;
}

//==========================================================================
//
//	D_CalcGradients
//
//==========================================================================

static void D_CalcGradients(surface_t *pface, int miplevel, const TVec &modelorg)
{
	TVec		p_normal;
	float		mipscale;
	float		t;
	texinfo_t	*tex;

	tex = pface->texinfo;

	mipscale = 1.0 / (float)(1 << miplevel);

	TransformVector(pface->plane->normal, p_normal);
	float distinv = 1.0 / (pface->plane->dist - DotProduct(modelorg, pface->plane->normal));

	d_zistepu = p_normal.x * distinv / xprojection;
	d_zistepv = p_normal.y * distinv / yprojection;
	d_ziorigin = p_normal.z * distinv -
			centrexfrac * d_zistepu -
			centreyfrac * d_zistepv;

	TVec tr_saxis;
	TVec tr_taxis;

	TransformVector(tex->saxis, tr_saxis);
	TransformVector(tex->taxis, tr_taxis);

	t = mipscale / xprojection;
	d_sdivzstepu = tr_saxis.x * t;
	d_tdivzstepu = tr_taxis.x * t;

	t = mipscale / yprojection;
	d_sdivzstepv = tr_saxis.y * t;
	d_tdivzstepv = tr_taxis.y * t;

	d_sdivzorigin = tr_saxis.z * mipscale -
		centrexfrac * d_sdivzstepu - centreyfrac * d_sdivzstepv;
	d_tdivzorigin = tr_taxis.z * mipscale -
		centrexfrac * d_tdivzstepu - centreyfrac * d_tdivzstepv;

	t = 0x10000 * mipscale;
	sadjust = ((fixed_t)(DotProduct(modelorg, tex->saxis) * t + 0.5)) -
			((pface->texturemins[0] << 16) >> miplevel)
			+ ((fixed_t)(tex->soffs * t));
	tadjust = ((fixed_t)(DotProduct(modelorg, tex->taxis) * t + 0.5)) -
			((pface->texturemins[1] << 16) >> miplevel)
			+ ((fixed_t)(tex->toffs * t));

	//
	// -1 (-epsilon) so we never wander off the edge of the texture
	//
	bbextents = ((pface->extents[0] << 16) >> miplevel) - 1;
	bbextentt = ((pface->extents[1] << 16) >> miplevel) - 1;
}

//==========================================================================
//
//	D_CalcZGradients
//
//==========================================================================

static void D_CalcZGradients(surface_t *pface)
{
	TVec		p_normal;

	TransformVector(pface->plane->normal, p_normal);
	float distinv = 1.0 / (pface->plane->dist - DotProduct(vieworg,
		pface->plane->normal));

	d_zistepu = p_normal.x * distinv / xprojection;
	d_zistepv = p_normal.y * distinv / yprojection;
	d_ziorigin = p_normal.z * distinv -
			centrexfrac * d_zistepu -
			centreyfrac * d_zistepv;
}

//==========================================================================
//
//	D_CalcHorizonGradients
//
//==========================================================================

static void D_CalcHorizonGradients(sec_plane_t* Plane, texinfo_t* tex)
{
	TVec		p_normal;
	float		t;

	TransformVector(Plane->normal, p_normal);
	float distinv = 1.0 / (Plane->dist - DotProduct(vieworg, Plane->normal));

	d_zistepu = p_normal.x * distinv / xprojection;
	d_zistepv = p_normal.y * distinv / yprojection;
	d_ziorigin = p_normal.z * distinv -
			centrexfrac * d_zistepu -
			centreyfrac * d_zistepv;

	TVec tr_saxis;
	TVec tr_taxis;

	TransformVector(tex->saxis, tr_saxis);
	TransformVector(tex->taxis, tr_taxis);

	t = 1.0 / xprojection;
	d_sdivzstepu = tr_saxis.x * t;
	d_tdivzstepu = tr_taxis.x * t;

	t = 1.0 / yprojection;
	d_sdivzstepv = tr_saxis.y * t;
	d_tdivzstepv = tr_taxis.y * t;

	d_sdivzorigin = tr_saxis.z -
		centrexfrac * d_sdivzstepu - centreyfrac * d_sdivzstepv;
	d_tdivzorigin = tr_taxis.z -
		centrexfrac * d_tdivzstepu - centreyfrac * d_tdivzstepv;

	t = 0x10000;
	sadjust = ((fixed_t)(DotProduct(vieworg, tex->saxis) * t + 0.5)) +
		((fixed_t)(tex->soffs * t));
	tadjust = ((fixed_t)(DotProduct(vieworg, tex->taxis) * t + 0.5)) +
		((fixed_t)(tex->toffs * t));
}

//==========================================================================
//
//	VSoftwareDrawer::DrawSurfaces
//
//==========================================================================

void VSoftwareDrawer::DrawSurfaces()
{
	surfcache_t		*cache;
	int				miplevel;

	for (surf_t *surf = &surfaces[1]; surf != surface_p; surf++)
	{
		if (!surf->spans)
		{
			continue;
		}

		if (!surf->flags)
		{
			miplevel = D_MipLevelForScale(surf->nearzi * scale_for_mip);
			D_CalcGradients(surf->surf, miplevel, vieworg);
			cache = CacheSurface(surf->surf, miplevel);
			cachewidth = cache->width;
			cacheblock = cache->data;
			D_DrawSpans(surf->spans);
			D_DrawZSpans(surf->spans);
		}
		else if (surf->flags & SURF_SKY)
		{
			D_CalcGradients(surf->surf, 0, TVec(0, 0, 0));
			cache = CacheSkySurface(surf->surf, surf->Texture1,
				surf->Texture2, surf->offs1, surf->offs2, surf->ColourMap);
			cachewidth = cache->width;
			cacheblock = cache->data;
			D_DrawSpans(surf->spans);
			D_CalcZGradients(surf->surf);
			D_DrawZSpans(surf->spans);
		}
		else if (surf->flags & SURF_SKY_BOX)
		{
			D_CalcGradients(surf->surf, 0, TVec(0, 0, 0));
			SetTexture(surf->Texture1, surf->ColourMap);
			D_DrawSpans(surf->spans);
			D_CalcZGradients(surf->surf);
			D_DrawZSpans(surf->spans);
		}
		else if (surf->flags & SURF_HORIZON)
		{
			D_CalcHorizonGradients(surf->surf->HorizonPlane,
				surf->surf->texinfo);
			SetTexture(surf->surf->texinfo->Tex,
				surf->surf->texinfo->ColourMap);
			SetFade(surf->surf->Fade);
			d_HorizonLight = 31 - (surf->surf->Light >> 27);
			D_DrawHorizonSpans(surf->spans);
			D_CalcZGradients(surf->surf);
			D_DrawZSpans(surf->spans);
		}
		else if (surf->flags & SURF_BACKGROUND)
		{
			//FIXME
		}
	}
}

//==========================================================================
//
//	VSoftwareDrawer::WorldDrawing
//
//	Input:
//	newedges[] array
//		this has links to edges, which have links to surfaces
//
//	Output:
//		Each surface has a linked list of its visible spans
//
//==========================================================================

void VSoftwareDrawer::WorldDrawing()
{
	guard(VSoftwareDrawer::WorldDrawing);
	int			iv, bottom;
	byte		basespans[MAX_SPANS * sizeof(espan_t) + CACHE_SIZE];
	espan_t		*basespan_p;
	surf_t		*s;

	basespan_p = (espan_t *)
			((long)(basespans + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	max_span_p = &basespan_p[MAX_SPANS - viewwidth];

	span_p = basespan_p;

	// clear active edges to just the background edges around the whole screen
	// FIXME: most of this only needs to be set up once
	edge_head.u = 0;
	edge_head.u_step = 0;
	edge_head.prev = NULL;
	edge_head.next = &edge_tail;
	edge_head.surfs[0] = 0;
	edge_head.surfs[1] = 1;
	
	edge_tail.u = (viewwidth << 20) + 0xFFFFF;
	edge_tail.u_step = 0;
	edge_tail.prev = &edge_head;
	edge_tail.next = NULL;
	edge_tail.surfs[0] = 1;
	edge_tail.surfs[1] = 0;

	//
	// process all scan lines
	//
	bottom = viewheight - 1;

	for (iv = 0; iv < bottom; iv++)
	{
		current_iv = iv;

		// mark that the head (background start) span is pre-included
		surfaces[1].spanstate = 1;

		if (newedges[iv])
		{
			D_InsertNewEdges(newedges[iv], edge_head.next);
		}

		D_GenerateSpans();

		//	Flush the span list if we can't be sure we have enough spans
		// left for the next scan
		if (span_p >= max_span_p)
		{
			DrawSurfaces();

			// clear the surface span pointers
			for (s = &surfaces[1]; s < surface_p; s++)
				s->spans = NULL;

			span_p = basespan_p;
		}

		if (removeedges[iv])
			D_RemoveEdges(removeedges[iv]);

		if (edge_head.next != &edge_tail)
			D_StepActiveU(edge_head.next, &edge_tail);
	}

	// do the last scan (no need to step or sort or remove on the last scan)

	current_iv = iv;

	// mark that the head (background start) span is pre-included
	surfaces[1].spanstate = 1;

	if (newedges[iv])
		D_InsertNewEdges(newedges[iv], edge_head.next);

	D_GenerateSpans();

	// draw whatever's left in the span list
	DrawSurfaces();

	if (outofsurfs)
		GCon->Logf(NAME_Dev, "Out of %d surfs", outofsurfs);
	if (outofedges)
		GCon->Logf(NAME_Dev, "Out of %d edges", outofedges);
	unguard;
}

//==========================================================================
//
//	VSoftwareDrawer::StartPortal
//
//==========================================================================

bool VSoftwareDrawer::StartPortal(VPortal*)
{
	return false;
}

//==========================================================================
//
//	VSoftwareDrawer::EndPortal
//
//==========================================================================

void VSoftwareDrawer::EndPortal(VPortal*)
{
}

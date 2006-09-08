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
#include "r_local.h"

// MACROS ------------------------------------------------------------------

#define MAX_STATIC_LIGHTS	256

// TYPES -------------------------------------------------------------------

struct light_t
{
	TVec		origin;
	float		radius;
	vuint32		color;
	int			leafnum;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int					r_dlightframecount;
bool				r_light_add;

vuint32				blocklights[18 * 18];
vuint32				blocklightsr[18 * 18];
vuint32				blocklightsg[18 * 18];
vuint32				blocklightsb[18 * 18];
vuint32				blockaddlightsr[18 * 18];
vuint32				blockaddlightsg[18 * 18];
vuint32				blockaddlightsb[18 * 18];

byte				light_remap[256];
VCvarI				r_darken("r_darken", "0", CVAR_Archive);
VCvarI				r_ambient("r_ambient", "0");
int					light_mem;
VCvarI				r_extrasamples("r_extrasamples", "0", CVAR_Archive);
VCvarI				r_dynamic("r_dynamic", "1", CVAR_Archive);
VCvarI				r_dynamic_clip("r_dynamic_clip", "0", CVAR_Archive);
VCvarI				r_static_lights("r_static_lights", "1", CVAR_Archive);
VCvarI				r_static_add("r_static_add", "0", CVAR_Archive);
VCvarF				r_specular("r_specular", "0.1", CVAR_Archive);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int			num_lights;
static light_t		lights[MAX_STATIC_LIGHTS];

static TVec			smins, smaxs;
static TVec			worldtotex[2];
static TVec			textoworld[2];
static TVec			texorg;
static TVec			surfpt[18 * 18 * 4];
static int			numsurfpt;
static bool			points_calculated;
static float		lightmap[18 * 18 * 4];
static float		lightmapr[18 * 18 * 4];
static float		lightmapg[18 * 18 * 4];
static float		lightmapb[18 * 18 * 4];
static bool			light_hit;
static byte			*facevis;
static bool			is_colored;

static int			c_bad;

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	R_ClearLights
//
//==========================================================================

void R_ClearLights(void)
{
	guard(R_ClearLights);
	num_lights = 0;

	for (int i = 0; i < 256; i++)
	{
		light_remap[i] = byte(i * i / 255);
	}
	unguard;
}

//==========================================================================
//
//	R_AddStaticLight
//
//==========================================================================

void R_AddStaticLight(const TVec &origin, float radius, vuint32 color)
{
	guard(R_AddStaticLight);
	if (num_lights == MAX_STATIC_LIGHTS)
	{
		GCon->Log("Too many static lights");
		return;
	}
	lights[num_lights].origin = origin;
	lights[num_lights].radius = radius;
	lights[num_lights].color = color;
	lights[num_lights].leafnum = CL_PointInSubsector(origin.x, origin.y) -
		GClLevel->Subsectors;
	num_lights++;
	unguard;
}

//==========================================================================
//
//	CalcMinMaxs
//
//==========================================================================

static void CalcMinMaxs(surface_t *surf)
{
	guard(CalcMinMaxs);
	smins = TVec(99999.0, 99999.0, 99999.0);
	smaxs = TVec(-999999.0, -999999.0, -999999.0);

	for (int i = 0; i < surf->count; i++)
	{
		TVec &v = surf->verts[i];
		if (smins.x > v.x)
			smins.x = v.x;
		if (smins.y > v.y)
			smins.y = v.y;
		if (smins.z > v.z)
			smins.z = v.z;
		if (smaxs.x < v.x)
			smaxs.x = v.x;
		if (smaxs.y < v.y)
			smaxs.y = v.y;
		if (smaxs.z < v.z)
			smaxs.z = v.z;
	}
	unguard;
}

//==========================================================================
//
//	CastRay
//
//	Returns the distance between the points, or -1 if blocked
//
//==========================================================================

static float CastRay(const TVec &p1, const TVec &p2, float squaredist)
{
	guard(CastRay);
	float	t;
	bool	trace;
	TVec	delta;

	delta =	p2 - p1;
	t = DotProduct(delta, delta);
	if (t >	squaredist)
		return -1;		// too far away
		
	trace = CL_TraceLine(p1, p2);
	if (!trace)
		return -1;		// ray was blocked
		
	if (t == 0)
		t = 1;			// don't blow up...
	return sqrt(t);
	unguard;
}

//==========================================================================
//
//	CalcFaceVectors
//
//	Fills in texorg, worldtotex. and textoworld
//
//==========================================================================

static void CalcFaceVectors(surface_t *surf)
{
	guard(CalcFaceVectors);
	texinfo_t	*tex;
	int			i;
	TVec		texnormal;
	float		distscale;
	float		dist, len;

	tex = surf->texinfo;
	
	// convert from float to vec_t
	worldtotex[0] = tex->saxis;
	worldtotex[1] = tex->taxis;

	//	Calculate a normal to the texture axis. Points can be moved along
	// this without changing their S/T
	texnormal.x = tex->taxis.y * tex->saxis.z
		- tex->taxis.z * tex->saxis.y;
	texnormal.y = tex->taxis.z * tex->saxis.x
		- tex->taxis.x * tex->saxis.z;
	texnormal.z = tex->taxis.x * tex->saxis.y
		- tex->taxis.y * tex->saxis.x;
	texnormal = Normalise(texnormal);

	// flip it towards plane normal
	distscale = DotProduct(texnormal, surf->plane->normal);
	if (!distscale)
		Host_Error("Texture axis perpendicular to face");
	if (distscale < 0)
	{
		distscale = -distscale;
		texnormal = -texnormal;
	}	

	// distscale is the ratio of the distance along the texture normal to
	// the distance along the plane normal
	distscale = 1 / distscale;

	for (i = 0; i < 2; i++)
	{
		len = Length(worldtotex[i]);
		dist = DotProduct(worldtotex[i], surf->plane->normal);
		dist *= distscale;
		textoworld[i] = worldtotex[i] - dist * texnormal;
		textoworld[i] = textoworld[i] * (1 / len) * (1 / len);
	}

	// calculate texorg on the texture plane
	for (i = 0; i < 3; i++)
		texorg[i] = -tex->soffs * textoworld[0][i] - tex->toffs * textoworld[1][i];

	// project back to the face plane
	dist = DotProduct(texorg, surf->plane->normal) - surf->plane->dist - 1;
	dist *= distscale;
	texorg = texorg - dist * texnormal;
	unguard;
}

//==========================================================================
//
//	CalcPoints
//
//	For each texture aligned grid point, back project onto the plane
// to get the world xyz value of the sample point
//
//==========================================================================

static void CalcPoints(surface_t *surf)
{
	guard(CalcPoints);
	int		i;
	int		s, t;
	int		w, h;
	int		step;
	float	starts, startt, us, ut;
	float	mids, midt;
	TVec	*spt;
	TVec	facemid;

	//
	// fill in surforg
	// the points are biased towards the center of the surface
	// to help avoid edge cases just inside walls
	//
	spt = surfpt;
	mids = surf->texturemins[0] + surf->extents[0] / 2;
	midt = surf->texturemins[1] + surf->extents[1] / 2;

	facemid = texorg + textoworld[0] * mids + textoworld[1] * midt;

	if (r_extrasamples)
	{
		// extra filtering
		w = ((surf->extents[0] >> 4) + 1) * 2;
		h = ((surf->extents[1] >> 4) + 1) * 2;
		starts = surf->texturemins[0] - 8;
		startt = surf->texturemins[1] - 8;
		step = 8;
	}
	else
	{
		w = (surf->extents[0] >> 4) + 1;
		h = (surf->extents[1] >> 4) + 1;
		starts = surf->texturemins[0];
		startt = surf->texturemins[1];
		step = 16;
	}

	numsurfpt = w * h;
	for (t = 0; t < h; t++)
	{
		for (s = 0; s < w; s++, spt++)
		{
			us = starts + s * step;
			ut = startt + t * step;

			// if a line can be traced from surf to facemid, the point is good
			for (i = 0; i < 6; i++)
			{
				// calculate texture point
				*spt = texorg + textoworld[0] * us + textoworld[1] * ut;
				if (CL_TraceLine(facemid, *spt))
				{
					break;	// got it
				}
				if (i & 1)
				{
					if (us > mids)
					{
						us -= 8;
						if (us < mids)
							us = mids;
					}
					else
					{
						us += 8;
						if (us > mids)
							us = mids;
					}
				}
				else
				{
					if (ut > midt)
					{
						ut -= 8;
						if (ut < midt)
							ut = midt;
					}
					else
					{
						ut += 8;
						if (ut > midt)
							ut = midt;
					}
				}
				
				// move surf 8 pixels towards the center
				*spt += 8 * Normalise(facemid - *spt);
			}
			if (i == 2)
				c_bad++;
		}
	}
	unguard;
}

//==========================================================================
//
//	SingleLightFace
//
//==========================================================================

static void SingleLightFace(light_t *light, surface_t *surf)
{
	guard(SingleLightFace);
	float	dist;
	TVec	incoming;
	float	angle;
	float	add;
	TVec	*spt;
	int		c;
	float	squaredist;
	float	rmul, gmul, bmul;

	// Check potential visibility
	if (!(facevis[light->leafnum >> 3] & (1 << (light->leafnum & 7))))
		return;

	// Check bounding box
	if (light->origin.x + light->radius < smins.x ||
		light->origin.x - light->radius > smaxs.x ||
		light->origin.y + light->radius < smins.y ||
		light->origin.y - light->radius > smaxs.y ||
		light->origin.z + light->radius < smins.z ||
		light->origin.z - light->radius > smaxs.z)
	{
		return;
	}

	dist = DotProduct(light->origin, surf->plane->normal) - surf->plane->dist;
	
	// don't bother with lights behind the surface
	if (dist <= -0.1)
		return;
		
	// don't bother with light too far away
	if (dist > light->radius)
	{
		return;
	}

	//	Calc points only when surface may be lit by a light
	if (!points_calculated)
	{
		CalcFaceVectors(surf);
		CalcPoints(surf);

		memset(lightmap, 0, numsurfpt * 4);
		memset(lightmapr, 0, numsurfpt * 4);
		memset(lightmapg, 0, numsurfpt * 4);
		memset(lightmapb, 0, numsurfpt * 4);
		points_calculated = true;
	}

	//
	// check it for real
	//
	spt = surfpt;
	squaredist = light->radius * light->radius;
	rmul = ((light->color >> 16) & 0xff) / 255.0;
	gmul = ((light->color >> 8) & 0xff) / 255.0;
	bmul = (light->color & 0xff) / 255.0;
	for (c = 0; c < numsurfpt; c++, spt++)
	{
		dist = CastRay(light->origin, *spt, squaredist);
		if (dist < 0)
			continue;	// light doesn't reach

		incoming = Normalise(light->origin - *spt);
		angle = DotProduct(incoming, surf->plane->normal);

		angle = 0.5 + 0.5 * angle;
		add = light->radius - dist;
		add *= angle;
		if (add < 0)
			continue;
		lightmap[c] += add;
		lightmapr[c] += add * rmul;
		lightmapg[c] += add * gmul;
		lightmapb[c] += add * bmul;
		if (lightmap[c] > 1)		// ignore real tiny lights
		{
			light_hit = true;
			if (light->color != 0xffffffff)
				is_colored = true;
		}
	}
	unguard;
}

//==========================================================================
//
//	R_LightFace
//
//==========================================================================

void R_LightFace(surface_t *surf, subsector_t *leaf)
{
	guard(R_LightFace);
	int			i, s, t, w, h;
	float		total;

	facevis = GClLevel->LeafPVS(leaf);
	points_calculated = false;
	light_hit = false;
	is_colored = false;

	//
	// cast all lights
	//
	CalcMinMaxs(surf);
	if (r_static_lights)
	{
		for (i = 0; i < num_lights; i++)
		{
			SingleLightFace(&lights[i], surf);
		}
	}

	if (!light_hit)
	{
		// no light hitting it
		if (surf->lightmap)
		{
			Z_Free(surf->lightmap);
			surf->lightmap = NULL;
		}
		if (surf->lightmap_rgb)
		{
			Z_Free(surf->lightmap_rgb);
			surf->lightmap_rgb = NULL;
		}
		return;
	}

	w = (surf->extents[0] >> 4) + 1;
	h = (surf->extents[1] >> 4) + 1;

	//	If the surface already has a lightmap, we will reuse it, otherwiese
	// we must allocate a new block
	if (is_colored)
	{
		if (surf->lightmap_rgb)
		{
			Z_Free(surf->lightmap_rgb);
		}
		surf->lightmap_rgb = (rgb_t*)Z_Malloc(w * h * 3);
		light_mem += w * h * 3;

		i = 0;
		for (t = 0; t < h; t++)
		{
			for (s = 0; s < w; s++, i++)
			{
				if (r_extrasamples)
				{
					// filtered sample
					total = lightmapr[t*w*4+s*2] +
							lightmapr[t*2*w*2+s*2+1] +
							lightmapr[(t*2+1)*w*2+s*2] +
							lightmapr[(t*2+1)*w*2+s*2+1];
					total *= 0.25;
				}
				else
					total = lightmapr[i];
				if (total > 255)
					total = 255;
				if (total < 0)
					Sys_Error("light < 0");
				surf->lightmap_rgb[i].r = byte(total);

				if (r_extrasamples)
				{
					// filtered sample
					total = lightmapg[t*w*4+s*2] +
							lightmapg[t*2*w*2+s*2+1] +
							lightmapg[(t*2+1)*w*2+s*2] +
							lightmapg[(t*2+1)*w*2+s*2+1];
					total *= 0.25;
				}
				else
					total = lightmapg[i];
				if (total > 255)
					total = 255;
				if (total < 0)
					Sys_Error("light < 0");
				surf->lightmap_rgb[i].g = byte(total);

				if (r_extrasamples)
				{
					// filtered sample
					total = lightmapb[t*w*4+s*2] +
							lightmapb[t*2*w*2+s*2+1] +
							lightmapb[(t*2+1)*w*2+s*2] +
							lightmapb[(t*2+1)*w*2+s*2+1];
					total *= 0.25;
				}
				else
					total = lightmapb[i];
				if (total > 255)
					total = 255;
				if (total < 0)
					Sys_Error("light < 0");
				surf->lightmap_rgb[i].b = byte(total);
			}
		}
	}
	else
	{
		if (surf->lightmap_rgb)
		{
			Z_Free(surf->lightmap_rgb);
			surf->lightmap_rgb = NULL;
		}
	}

	if (surf->lightmap)
	{
		Z_Free(surf->lightmap);
	}
	surf->lightmap = (byte*)Z_Malloc(w * h);
	light_mem += w * h;

	i = 0;
	for (t = 0; t < h; t++)
	{
		for (s = 0; s < w; s++, i++)
		{
			if (r_extrasamples)
			{
				// filtered sample
				total = lightmap[t*w*4+s*2] +
						lightmap[t*2*w*2+s*2+1] +
						lightmap[(t*2+1)*w*2+s*2] +
						lightmap[(t*2+1)*w*2+s*2+1];
				total *= 0.25;
			}
			else
				total = lightmap[i];
			if (total > 255)
				total = 255;
			if (total < 0)
				Sys_Error("light < 0");
			surf->lightmap[i] = byte(total);
		}
	}
	unguard;
}

//**************************************************************************
//**
//**	DYNAMIC LIGHTS
//**
//**************************************************************************

//==========================================================================
//
//	R_MarkLights
//
//==========================================================================

void R_MarkLights(dlight_t *light, int bit, int bspnum)
{
	guard(R_MarkLights);
	static byte	*dyn_facevis;
	int leafnum;

    if (bspnum & NF_SUBSECTOR)
    {
		int num;

		if (bspnum == -1)
		    num = 0;
		else
		    num = bspnum & (~NF_SUBSECTOR);
		subsector_t *ss = &GClLevel->Subsectors[num];

		if (r_dynamic_clip)
		{
			dyn_facevis = GClLevel->LeafPVS(ss);
			leafnum = CL_PointInSubsector(light->origin.x, light->origin.y)
							- GClLevel->Subsectors;

			// Check potential visibility
			if (!(dyn_facevis[leafnum >> 3] & (1 << (leafnum & 7))))
				return;
		}

		if (ss->dlightframe != r_dlightframecount)
		{
			ss->dlightbits = 0;
			ss->dlightframe = r_dlightframecount;
		}
		ss->dlightbits |= bit;
	}
	else
	{
		node_t* node = &GClLevel->Nodes[bspnum];
		float dist = DotProduct(light->origin, node->normal) - node->dist;

		if (dist > -light->radius + light->minlight)
		{
			R_MarkLights(light, bit, node->children[0]);
		}
		if (dist < light->radius - light->minlight)
		{
			R_MarkLights(light, bit, node->children[1]);
		}
	}
	unguard;
}

//==========================================================================
//
//	R_PushDlights
//
//==========================================================================

void R_PushDlights(void)
{
	guard(R_PushDlights);
	int			i;
	dlight_t	*l;

	r_dlightframecount++;

	if (!r_dynamic)
	{
		return;
	}

	l = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, l++)
	{
		if (l->die < cl->time || !l->radius)
			continue;
		R_MarkLights(l, 1 << i, GClLevel->NumNodes - 1);
	}
	unguard;
}

//==========================================================================
//
//	R_LightPoint
//
//==========================================================================

vuint32 R_LightPoint(const TVec &p)
{
	guard(R_LightPoint);
	subsector_t		*sub;
	subregion_t		*reg;
	float			l, lr, lg, lb, d, add;
	int				i, s, t, ds, dt;
	surface_t		*surf;
	int				ltmp;
	rgb_t			*rgbtmp;
	static byte		*dyn_facevis;
	int				leafnum;


	if (fixedlight)
	{
		return fixedlight | (fixedlight << 8) | (fixedlight << 16) | (fixedlight << 24);
	}

	sub = CL_PointInSubsector(p.x, p.y);
	reg = sub->regions;
	while (reg->next)
	{
		d = DotProduct(p, reg->floor->secplane->normal) - reg->floor->secplane->dist;

		if (d >= 0.0)
		{
			break;
		}

		reg = reg->next;
	}

	//	Region's base light
	l = reg->secregion->params->lightlevel;
	if (r_darken)
	{
		l = light_remap[MIN(255, (int)l)];
	}
	int SecLightColor = reg->secregion->params->LightColor;
	lr = ((SecLightColor >> 16) & 255) * l / 255.0;
	lg = ((SecLightColor >> 8) & 255) * l / 255.0;
	lb = (SecLightColor & 255) * l / 255.0;

	//	Light from floor's lightmap
	s = (int)(DotProduct(p, reg->floor->texinfo.saxis) + reg->floor->texinfo.soffs);
	t = (int)(DotProduct(p, reg->floor->texinfo.taxis) + reg->floor->texinfo.toffs);
	for (surf = reg->floor->surfs; surf; surf = surf->next)
	{
		if (!surf->lightmap)
		{
			continue;
		}
		if (s < surf->texturemins[0] ||	t < surf->texturemins[1])
		{
			continue;
		}

		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];

		if (ds > surf->extents[0] || dt > surf->extents[1])
		{
			continue;
		}

		if (surf->lightmap_rgb)
		{
			l += surf->lightmap[(ds >> 4) + (dt >> 4) * ((surf->extents[0] >> 4) + 1)];
			rgbtmp = &surf->lightmap_rgb[(ds >> 4) + (dt >> 4) * ((surf->extents[0] >> 4) + 1)];
			lr += rgbtmp->r;
			lg += rgbtmp->g;
			lb += rgbtmp->b;
		}
		else
		{
			ltmp = surf->lightmap[(ds >> 4) + (dt >> 4) * ((surf->extents[0] >> 4) + 1)];
			l += ltmp;
			lr += ltmp;
			lg += ltmp;
			lb += ltmp;
		}
		break;
	}

	//	Add dynamic lights
	if (sub->dlightframe == r_dlightframecount)
	{
		for (i = 0; i < MAX_DLIGHTS; i++)
		{
			if (r_dynamic_clip)
			{
				dyn_facevis = GClLevel->LeafPVS(sub);
				leafnum = CL_PointInSubsector(cl_dlights[i].origin.x, cl_dlights[i].origin.y)
								- GClLevel->Subsectors;

				// Check potential visibility
				if (!(dyn_facevis[leafnum >> 3] & (1 << (leafnum & 7))))
						continue;
			}
			if (!(sub->dlightbits & (1 << i)))
				continue;

			add = (cl_dlights[i].radius - cl_dlights[i].minlight) - Length(p - cl_dlights[i].origin);
	
			if (add > 0)
			{
				l += add;
				lr += add * ((cl_dlights[i].color >> 16) & 0xff) / 255.0;
				lg += add * ((cl_dlights[i].color >> 8) & 0xff) / 255.0;
				lb += add * (cl_dlights[i].color & 0xff) / 255.0;
			}
		}
	}

	if (l > 255)
		l = 255;
	if (lr > 255)
		lr = 255;
	if (lg > 255)
		lg = 255;
	if (lb > 255)
		lb = 255;

	return ((int)l << 24) | ((int)lr << 16) | ((int)lg << 8) | ((int)lb);
	unguard;
}

//==========================================================================
//
//	R_AddDynamicLights
//
//==========================================================================

void R_AddDynamicLights(surface_t *surf)
{
	guard(R_AddDynamicLights);
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight, rmul, gmul, bmul;
	TVec		impact, local;
	int			s, t, i;
	int			smax, tmax;
	texinfo_t	*tex;
	subsector_t *sub;
	static byte	*dyn_facevis;
	int			leafnum;


	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;

	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++)
	{
		if (!(surf->dlightbits & (1<<lnum)))
			continue;		// not lit by this light

		rad = cl_dlights[lnum].radius;
		dist = DotProduct(cl_dlights[lnum].origin, surf->plane->normal) -
				surf->plane->dist;
		if (r_dynamic_clip)
		{
			if (dist <= -0.1)
				continue;
		}
		
		rad -= fabs(dist);
		minlight = cl_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		impact = cl_dlights[lnum].origin - surf->plane->normal * dist;

		if (r_dynamic_clip)
		{
			sub = CL_PointInSubsector(surf->verts->x, surf->verts->y);
			dyn_facevis = GClLevel->LeafPVS(sub);
			leafnum = CL_PointInSubsector(cl_dlights[lnum].origin.x, cl_dlights[lnum].origin.y)
							- GClLevel->Subsectors;

			// Check potential visibility
			if (!(dyn_facevis[leafnum >> 3] & (1 << (leafnum & 7))))
				continue;
		}

		rmul = (cl_dlights[lnum].color >> 16) & 0xff;
		gmul = (cl_dlights[lnum].color >> 8) & 0xff;
		bmul = cl_dlights[lnum].color & 0xff;

		local.x = DotProduct(impact, tex->saxis) + tex->soffs;
		local.y = DotProduct(impact, tex->taxis) + tex->toffs;

		local.x -= surf->texturemins[0];
		local.y -= surf->texturemins[1];
		
		for (t = 0; t < tmax; t++)
		{
			td = (int)local.y - t * 16;
			if (td < 0)
				td = -td;
			for (s = 0; s < smax; s++)
			{
				sd = (int)local.x - s * 16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td >> 1);
				else
					dist = td + (sd >> 1);

				if (dist < minlight)
				{
					i = t * smax + s;
					blocklights[i] += (vuint32)((rad - dist) * 256);
					blocklightsr[i] += (vuint32)((rad - dist) * rmul);
					blocklightsg[i] += (vuint32)((rad - dist) * gmul);
					blocklightsb[i] += (vuint32)((rad - dist) * bmul);
					if (cl_dlights[lnum].color != 0xffffffff)
						is_colored = true;
				}
			}
		}
	}
	unguard;
}

//==========================================================================
//
//	R_BuildLightMap
//
//	Combine and scale multiple lightmaps into the 8.8 format in blocklights
//
//==========================================================================

bool R_BuildLightMap(surface_t *surf, int shift)
{
	guard(R_BuildLightMap);
	int			smax, tmax;
	int			t;
	int			i, size;
	byte		*lightmap;
	rgb_t		*lightmap_rgb;

	is_colored = false;
	r_light_add = false;
	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax*tmax;
	lightmap = surf->lightmap;
	lightmap_rgb = surf->lightmap_rgb;

	// clear to ambient
	t = surf->Light >> 24;
	t =	MAX(t, r_ambient);
 	t <<= 8;
	int tR = ((surf->Light >> 16) & 255) * t / 255;
	int tG = ((surf->Light >> 8) & 255) * t / 255;
	int tB = (surf->Light & 255) * t / 255;
	if (tR != tG || tR != tB)
		is_colored = true;
	for (i = 0; i < size; i++)
	{
		blocklights[i] = t;
		blocklightsr[i] = tR;
		blocklightsg[i] = tG;
		blocklightsb[i] = tB;
		blockaddlightsr[i] = 0;
		blockaddlightsg[i] = 0;
		blockaddlightsb[i] = 0;
	}

	// add lightmap
	if (lightmap_rgb)
	{
		if (!lightmap)
		{
			Sys_Error("RGB lightmap without uncolored lightmap");
		}
		is_colored = true;
		for (i = 0; i < size; i++)
		{
			blocklights[i] += lightmap[i] << 8;
			blocklightsr[i] += lightmap_rgb[i].r << 8;
			blocklightsg[i] += lightmap_rgb[i].g << 8;
			blocklightsb[i] += lightmap_rgb[i].b << 8;
			if (!r_static_add)
			{
				if (blocklightsr[i] > 0xffff)
					blocklightsr[i] = 0xffff;
				if (blocklightsg[i] > 0xffff)
					blocklightsg[i] = 0xffff;
				if (blocklightsb[i] > 0xffff)
					blocklightsb[i] = 0xffff;
			}
		}
	}
	else if (lightmap)
	{
		for (i = 0; i < size; i++)
		{
			t = lightmap[i] << 8;
			blocklights[i] += t;
			blocklightsr[i] += t;
			blocklightsg[i] += t;
			blocklightsb[i] += t;
			if (!r_static_add)
			{
				if (blocklightsr[i] > 0xffff)
					blocklightsr[i] = 0xffff;
				if (blocklightsg[i] > 0xffff)
					blocklightsg[i] = 0xffff;
				if (blocklightsb[i] > 0xffff)
					blocklightsb[i] = 0xffff;
			}
		}
	}

	// add all the dynamic lights
	if (surf->dlightframe == r_dlightframecount)
		R_AddDynamicLights(surf);

	//  Calc additive light. This must be done before lightmap procesing
	// because it will clamp all lights
	if (!shift)
	{
		for (i = 0; i < size; i++)
		{
			t = blocklightsr[i] - 0x10000;
			if (t > 0)
			{
				t = int(r_specular * t);
				if (t > 0xffff)
					t = 0xffff;
				blockaddlightsr[i] = t;
				r_light_add = true;
			}

			t = blocklightsg[i] - 0x10000;
			if (t > 0)
			{
				t = int(r_specular * t);
				if (t > 0xffff)
					t = 0xffff;
				blockaddlightsg[i] = t;
				r_light_add = true;
			}

			t = blocklightsb[i] - 0x10000;
			if (t > 0)
			{
				t = int(r_specular * t);
				if (t > 0xffff)
					t = 0xffff;
				blockaddlightsb[i] = t;
				r_light_add = true;
			}
		}
	}

	// bound, invert, and shift
	int minlight = 1 << (8 - shift);
	for (i = 0; i < size; i++)
	{
		t = (255 * 256 - (int)blocklights[i]) >> shift;
		if (t < minlight)
			t = minlight;
		blocklights[i] = t;

		t = (255 * 256 - (int)blocklightsr[i]) >> shift;
		if (t < minlight)
			t = minlight;
		blocklightsr[i] = t;

		t = (255 * 256 - (int)blocklightsg[i]) >> shift;
		if (t < minlight)
			t = minlight;
		blocklightsg[i] = t;

		t = (255 * 256 - (int)blocklightsb[i]) >> shift;
		if (t < minlight)
			t = minlight;
		blocklightsb[i] = t;
	}

	return is_colored;
	unguard;
}

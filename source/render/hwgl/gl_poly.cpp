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

#include "gl_local.h"

#include "render/r_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static VCvarB glsw_report_verts("glsw_report_verts", false, "Report number of shadow volume vertices?", 0);
static VCvarB gl_decal_debug_nostencil("gl_decal_debug_nostencil", false, "Don't touch this!", 0);
static VCvarB gl_decal_debug_noalpha("gl_decal_debug_noalpha", false, "Don't touch this!", 0);
static VCvarB gl_decal_dump_max("gl_decal_dump_max", false, "Don't touch this!", 0);
static VCvarB gl_decal_reset_max("gl_decal_reset_max", false, "Don't touch this!", 0);

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	glVertex
//
//==========================================================================

inline void glVertex(const TVec &v)
{
	glVertex3f(v.x, v.y, v.z);
}

//==========================================================================
//
//	VOpenGLDrawer::WorldDrawing
//
//==========================================================================

void VOpenGLDrawer::WorldDrawing()
{
	guard(VOpenGLDrawer::WorldDrawing);
	if (HaveShaders)
	{
		WorldDrawingShaders();
		return;
	}

	//int			lb, i;
	surfcache_t	*cache;
	GLfloat		s, t;
	GLfloat		lights, lightt;
	surface_t	*surf;
	texinfo_t	*tex;

	//	First draw horizons.
	if (RendLev->HorizonPortalsHead)
	{
		for (surf = RendLev->HorizonPortalsHead; surf; surf = surf->DrawNext)
		{
			if (surf->plane->PointOnSide(vieworg))
			{
				//	Viewer is in back side or on plane
				continue;
			}

			DoHorizonPolygon(surf);
		}
	}

	//	For sky areas we just write to the depth buffer to prevent drawing
	// polygons behind the sky.
	if (RendLev->SkyPortalsHead)
	{
		glDisable(GL_TEXTURE_2D);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		for (surf = RendLev->SkyPortalsHead; surf; surf = surf->DrawNext)
		{
			if (surf->plane->PointOnSide(vieworg))
			{
				//	Viewer is in back side or on plane
				continue;
			}

			glBegin(GL_POLYGON);
			for (int i = 0; i < surf->count; ++i) glVertex(surf->verts[i]);
			glEnd();
		}
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glEnable(GL_TEXTURE_2D);
	}

	//	Draw surfaces.
	if (RendLev->SimpleSurfsHead)
	{
		for (surf = RendLev->SimpleSurfsHead; surf; surf = surf->DrawNext)
		{
			if (surf->plane->PointOnSide(vieworg))
			{
				//	Viewer is in back side or on plane
				continue;
			}

			texinfo_t* tex = surf->texinfo;
			SetTexture(tex->Tex, tex->ColourMap);

			if (surf->lightmap != NULL || surf->dlightframe == r_dlightframecount)
			{
				glColor4f(1, 1, 1, 1);
			}
			else
			{
				float lev = float(surf->Light >> 24) / 255.0;
				glColor4f(((surf->Light >> 16) & 255) * lev / 255.0,
					((surf->Light >> 8) & 255) * lev / 255.0,
					(surf->Light & 255) * lev / 255.0, 1.0);
			}
			SetFade(surf->Fade);

			glBegin(GL_POLYGON);
			for (int i = 0; i < surf->count; ++i)
			{
				glTexCoord2f((DotProduct(surf->verts[i], tex->saxis) + tex->soffs) * tex_iw,
					(DotProduct(surf->verts[i], tex->taxis) + tex->toffs) * tex_ih);
				glVertex(surf->verts[i]);
			}
			glEnd();
		}
	}

	if (HaveMultiTexture)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		SelectTexture(1);
		glEnable(GL_TEXTURE_2D);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		SelectTexture(0);
		glColor4f(1, 1, 1, 1);

		for (int lb = 0; lb < NUM_BLOCK_SURFS; ++lb)
		{
			if (!RendLev->light_chain[lb])
			{
				continue;
			}

			SelectTexture(1);
			glBindTexture(GL_TEXTURE_2D, lmap_id[lb]);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			if (RendLev->block_changed[lb])
			{
				RendLev->block_changed[lb] = false;
				glTexImage2D(GL_TEXTURE_2D, 0, 4, BLOCK_WIDTH, BLOCK_HEIGHT,
					0, GL_RGBA, GL_UNSIGNED_BYTE, RendLev->light_block[lb]);
			}
			SelectTexture(0);

			for (cache = RendLev->light_chain[lb]; cache; cache = cache->chain)
			{
				surf = cache->surf;
				if (surf->plane->PointOnSide(vieworg))
				{
					//	Viewer is in back side or on plane
					continue;
				}
				tex = surf->texinfo;

				SetTexture(tex->Tex, tex->ColourMap);
				SetFade(surf->Fade);
				glBegin(GL_POLYGON);
				for (int i = 0; i < surf->count; ++i)
				{
					s = DotProduct(surf->verts[i], tex->saxis) + tex->soffs;
					t = DotProduct(surf->verts[i], tex->taxis) + tex->toffs;
					lights = (s - surf->texturemins[0] + cache->s * 16 + 8) / (BLOCK_WIDTH * 16);
					lightt = (t - surf->texturemins[1] + cache->t * 16 + 8) / (BLOCK_HEIGHT * 16);
					MultiTexCoord(0, s * tex_iw, t * tex_ih);
					MultiTexCoord(1, lights, lightt);
					glVertex(surf->verts[i]);
				}
				glEnd();
			}
		}

		SelectTexture(1);
		glDisable(GL_TEXTURE_2D);
		SelectTexture(0);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
	else
	{
		glDepthMask(0);		// don't bother writing Z
		glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
		glEnable(GL_BLEND);
		glColor4f(1, 1, 1, 1);

		for (int lb = 0; lb < NUM_BLOCK_SURFS; ++lb)
		{
			if (!RendLev->light_chain[lb])
			{
				continue;
			}

			glBindTexture(GL_TEXTURE_2D, lmap_id[lb]);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

			if (RendLev->block_changed[lb])
			{
				RendLev->block_changed[lb] = false;
				glTexImage2D(GL_TEXTURE_2D, 0, 4, BLOCK_WIDTH, BLOCK_HEIGHT,
					0, GL_RGBA, GL_UNSIGNED_BYTE, RendLev->light_block[lb]);
			}

			for (cache = RendLev->light_chain[lb]; cache; cache = cache->chain)
			{
				surf = cache->surf;
				if (surf->plane->PointOnSide(vieworg))
				{
					//	Viewer is in back side or on plane
					continue;
				}
				tex = surf->texinfo;

				SetFade(surf->Fade);
				glBegin(GL_POLYGON);
				for (int i = 0; i < surf->count; ++i)
				{
					s = (DotProduct(surf->verts[i], tex->saxis) + tex->soffs - surf->texturemins[0] + cache->s * 16 + 8) / (BLOCK_WIDTH * 16);
					t = (DotProduct(surf->verts[i], tex->taxis) + tex->toffs - surf->texturemins[1] + cache->t * 16 + 8) / (BLOCK_HEIGHT * 16);
					glTexCoord2f(s, t);
					glVertex(surf->verts[i]);
				}
				glEnd();
			}
		}

		glDisable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(1);		// back to normal Z buffering
	}

	//
	//	Add specular lights
	//
	if (specular_highlights)
	{
		glDepthMask(0);		// don't bother writing Z
		glBlendFunc(GL_ONE, GL_ONE);
		glEnable(GL_BLEND);
		glColor4f(1, 1, 1, 1);
		SetFade(0);

		for (int lb = 0; lb < NUM_BLOCK_SURFS; ++lb)
		{
			if (!RendLev->add_chain[lb])
			{
				continue;
			}

			glBindTexture(GL_TEXTURE_2D, addmap_id[lb]);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

			if (RendLev->add_changed[lb])
			{
				RendLev->add_changed[lb] = false;
				glTexImage2D(GL_TEXTURE_2D, 0, 4, BLOCK_WIDTH, BLOCK_HEIGHT,
					0, GL_RGBA, GL_UNSIGNED_BYTE, RendLev->add_block[lb]);
			}

			for (cache = RendLev->add_chain[lb]; cache; cache = cache->addchain)
			{
				surf = cache->surf;
				if (surf->plane->PointOnSide(vieworg))
				{
					//	Viewer is in back side or on plane
					continue;
				}
				tex = surf->texinfo;

				glBegin(GL_POLYGON);
				for (int i = 0; i < surf->count; ++i)
				{
					s = (DotProduct(surf->verts[i], tex->saxis) + tex->soffs - surf->texturemins[0] + cache->s * 16 + 8) / (BLOCK_WIDTH * 16);
					t = (DotProduct(surf->verts[i], tex->taxis) + tex->toffs - surf->texturemins[1] + cache->t * 16 + 8) / (BLOCK_HEIGHT * 16);
					glTexCoord2f(s, t);
					glVertex(surf->verts[i]);
				}
				glEnd();
			}
		}

		glDisable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(1);		// back to normal Z buffering
	}
	unguard;
}

//==========================================================================
//
// VOpenGLDrawer::RenderShaderDecalsStart
//
//==========================================================================

void VOpenGLDrawer::RenderShaderDecalsStart () {
  decalStcVal = 255; // next value for stencil buffer (clear on the first use, and clear on each wrap)
  decalUsedStencil = false;
}

//==========================================================================
//
// VOpenGLDrawer::RenderShaderDecalsEnd
//
//==========================================================================

void VOpenGLDrawer::RenderShaderDecalsEnd () {
  if (decalUsedStencil) {
    // clear stencil buffer, as other stages may expect it cleared
    // this is not strictly necessary, but 'cmon, be polite!
    glClear(GL_STENCIL_BUFFER_BIT);
  }
}

//==========================================================================
//
// VOpenGLDrawer::RenderPrepareShaderDecals
//
//==========================================================================

void VOpenGLDrawer::RenderPrepareShaderDecals (surface_t *surf, bool lmap) {
  if (!surf->dcseg || !surf->dcseg->decals) return; // nothing to do

  if (gl_decal_debug_nostencil) return; // debug

  if (++decalStcVal == 0) { glClear(GL_STENCIL_BUFFER_BIT); decalStcVal = 1; } // it wrapped, so clear stencil buffer
  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_ALWAYS, decalStcVal, 0xff);
  glStencilOp(GL_KEEP, GL_KEEP, /*GL_INCR*/GL_REPLACE);
  decalUsedStencil = true;
}

//==========================================================================
//
// VOpenGLDrawer::RenderFinishShaderDecals
//
//==========================================================================

bool VOpenGLDrawer::RenderFinishShaderDecals (surface_t *surf, bool lmap) {
  if (!surf->dcseg || !surf->dcseg->decals) return false; // nothing to do

  if (lmap) {
    SelectTexture(1);
    glBindTexture(GL_TEXTURE_2D, 0);
    SelectTexture(2);
    glBindTexture(GL_TEXTURE_2D, 0);
    SelectTexture(0);
  }

  p_glUseProgramObjectARB(SurfDecalProgram);
  p_glUniform1iARB(SurfDecalIsLightmap, (lmap ? GL_TRUE : GL_FALSE));
  p_glUniform1iARB(SurfDecalTextureLoc, 0);
  p_glUniform1iARB(SurfDecalFogTypeLoc, r_fog&3);
  texinfo_t *tex = surf->texinfo;

  //glEnable(GL_POLYGON_OFFSET_FILL);
  //glPolygonOffset(-4.0f, -1.0f);

  glDepthMask(GL_FALSE);
  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_EQUAL, decalStcVal, 0xff);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  glEnable(GL_BLEND);
  //glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  //glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_ALPHA_TEST); // just in case

  //glEnable(GL_DEPTH_TEST);

  //glDisable(GL_BLEND);

  if (gl_decal_debug_nostencil) {
    glDisable(GL_STENCIL_TEST);
  }

  if (gl_decal_debug_noalpha) {
    glDisable(GL_BLEND);
  }

  // also, clear dead decals here, 'cause why not?
  decal_t* prev = nullptr;
  decal_t* dc = surf->dcseg->decals;

  int rdcount = 0;
  static int maxrdcount = 0;
  if (gl_decal_reset_max) { maxrdcount = 0; gl_decal_reset_max = false; }

  while (dc) {
    if (dc->texture < 0 || dc->alpha <= 0 || dc->scaleX <= 0 || dc->scaleY <= 0) {
      // remove it
      decal_t* n = dc->next;
      if (!dc->animator) {
        if (prev) prev->next = n; else surf->dcseg->decals = n;
        delete dc;
      } else {
        prev = dc;
      }
      dc = n;
      continue;
    }

    auto dtex = GTextureManager[dc->texture];
    if (!dtex || dtex->Width < 1 || dtex->Height < 1) {
      // remove it
      decal_t* n = dc->next;
      if (!dc->animator) {
        if (prev) prev->next = n; else surf->dcseg->decals = n;
        delete dc;
      } else {
        prev = dc;
      }
      dc = n;
      continue;
    }

    // use origScale to get the original starting point
    float txw2 = dtex->GetWidth()*dc->origScaleX*0.5f;
    float txh2 = dtex->GetHeight()*dc->origScaleY*0.5f;

    float txw = dtex->GetWidth()*dc->scaleX;
    float txh = dtex->GetHeight()*dc->scaleY;

    if (txw < 1 || txh < 1) {
      // remove it
      decal_t* n = dc->next;
      if (!dc->animator) {
        if (prev) prev->next = n; else surf->dcseg->decals = n;
        delete dc;
      } else {
        prev = dc;
      }
      dc = n;
      continue;
    }

    float lev = (dc->flags&decal_t::Fullbright ? 1.0f : (float)(surf->Light>>24)/255.0f);
    p_glUniform4fARB(SurfDecalLightLoc, ((surf->Light>>16)&255)/255.0f, ((surf->Light>>8)&255)/255.0f, (surf->Light&255)/255.0f, lev);
    if (surf->Fade && (dc->flags&decal_t::Fullbright) == 0) {
      p_glUniform1iARB(SurfDecalFogEnabledLoc, GL_TRUE);
      p_glUniform4fARB(SurfDecalFogColourLoc, ((surf->Fade>>16)&255)/255.0f, ((surf->Fade>>8)&255)/255.0f, (surf->Fade&255)/255.0f, 1.0f);
      p_glUniform1fARB(SurfDecalFogDensityLoc, (surf->Fade == FADE_LIGHT ? 0.3 : r_fog_density));
      p_glUniform1fARB(SurfDecalFogStartLoc, (surf->Fade == FADE_LIGHT ? 1.0 : r_fog_start));
      p_glUniform1fARB(SurfDecalFogEndLoc, (surf->Fade == FADE_LIGHT ? 1024.0 * r_fade_factor : r_fog_end));
    } else {
      p_glUniform1iARB(SurfDecalFogEnabledLoc, GL_FALSE);
    }

    //glColor3f(1, 0, 0); // just in case
    p_glUniform4fARB(SurfDecalSplatColourLoc, dc->shade[0], dc->shade[1], dc->shade[2], dc->shade[3]);
    p_glUniform1fARB(SurfDecalSplatAlphaLoc, dc->alpha);

    SetTexture(dtex, tex->ColourMap); // this sets `tex_iw` and `tex_ih`

    TVec lv1, lv2;
    lv1 = *(dc->seg->side ? dc->seg->linedef->v2 : dc->seg->linedef->v1);
    lv2 = *(dc->seg->side ? dc->seg->linedef->v1 : dc->seg->linedef->v2);

    //TODO:FIXME: offsets
    //if (dtex->SOffset || dtex->TOffset) fprintf(stderr, "tx#%d ofs: %d, %d\n", dc->texture, dtex->SOffset, dtex->TOffset);

    TVec dir = (lv2-lv1)/dc->linelen;
    float xstofs = dc->xdist-txw2+dc->ofsX;
    TVec v0 = lv1+dir*xstofs;
    TVec v2 = lv1+dir*(xstofs+txw);

    float dcz = dc->curz+txh2-dc->ofsY;
    // fix Z, if necessary
    if (dc->flags&decal_t::SlideFloor) {
      // should slide with back floor
      dcz += dc->bsec->floor.TexZ;
    } else if (dc->flags&decal_t::SlideCeil) {
      // should slide with back ceiling
      dcz += dc->bsec->ceiling.TexZ;
    }

    float texx0 = (dc->flags&decal_t::FlipX ? 1.0f : 0.0f);
    float texx1 = (dc->flags&decal_t::FlipX ? 0.0f : 1.0f);
    float texy0 = (dc->flags&decal_t::FlipY ? 1.0f : 0.0f);
    float texy1 = (dc->flags&decal_t::FlipY ? 0.0f : 1.0f);

    glBegin(GL_QUADS);
      glTexCoord2f(texx0, texy0); glVertex3f(v0.x, v0.y, dcz);
      glTexCoord2f(texx0, texy1); glVertex3f(v0.x, v0.y, dcz-txh);
      glTexCoord2f(texx1, texy1); glVertex3f(v2.x, v2.y, dcz-txh);
      glTexCoord2f(texx1, texy0); glVertex3f(v2.x, v2.y, dcz);
    glEnd();

    prev = dc;
    dc = dc->next;

    ++rdcount;
  }

  if (rdcount > maxrdcount) {
    maxrdcount = rdcount;
    if (gl_decal_dump_max) GCon->Logf("*** max decals on seg: %d", maxrdcount);
  }

  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_STENCIL_TEST);
  glDepthMask(GL_TRUE);

  //glPolygonOffset(0.0f, 0.0f);
  //glDisable(GL_POLYGON_OFFSET_FILL);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  if (lmap) {
    p_glUseProgramObjectARB(SurfLightmapProgram);
  } else {
    p_glUseProgramObjectARB(SurfSimpleProgram);
  }

  //glColor4f(1, 1, 1, 1); // just in case

  return true;
}

//==========================================================================
//
//	VOpenGLDrawer::WorldDrawingShaders
//
//==========================================================================

void VOpenGLDrawer::WorldDrawingShaders()
{
	guard(VOpenGLDrawer::WorldDrawingShaders);
	surfcache_t	*cache;
	surface_t	*surf;
	texinfo_t	*tex;

	// first draw horizons
	if (RendLev->HorizonPortalsHead) {
		for (surf = RendLev->HorizonPortalsHead; surf; surf = surf->DrawNext) {
			if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
			DoHorizonPolygon(surf);
		}
	}

	// for sky areas we just write to the depth buffer to prevent drawing polygons behind the sky
	if (RendLev->SkyPortalsHead) {
		p_glUseProgramObjectARB(SurfZBufProgram);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		for (surf = RendLev->SkyPortalsHead; surf; surf = surf->DrawNext) {
			if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane
			glBegin(GL_POLYGON);
			for (int i = 0; i < surf->count; ++i) glVertex(surf->verts[i]);
			glEnd();
		}
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}

	RenderShaderDecalsStart();

	// draw surfaces without lightmaps
	if (RendLev->SimpleSurfsHead) {
		p_glUseProgramObjectARB(SurfSimpleProgram);
		p_glUniform1iARB(SurfSimpleTextureLoc, 0);
		p_glUniform1iARB(SurfSimpleFogTypeLoc, r_fog & 3);

		for (surf = RendLev->SimpleSurfsHead; surf; surf = surf->DrawNext) {
			if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane

			texinfo_t* tex = surf->texinfo;
			SetTexture(tex->Tex, tex->ColourMap);

			p_glUniform3fvARB(SurfSimpleSAxisLoc, 1, &tex->saxis.x);
			p_glUniform1fARB(SurfSimpleSOffsLoc, tex->soffs);
			p_glUniform1fARB(SurfSimpleTexIWLoc, tex_iw);
			p_glUniform3fvARB(SurfSimpleTAxisLoc, 1, &tex->taxis.x);
			p_glUniform1fARB(SurfSimpleTOffsLoc, tex->toffs);
			p_glUniform1fARB(SurfSimpleTexIHLoc, tex_ih);

			float lev = (float)(surf->Light>>24)/255.0f;
			p_glUniform4fARB(SurfSimpleLightLoc, ((surf->Light>>16)&255)*lev/255.0f, ((surf->Light>>8)&255)*lev/255.0f, (surf->Light&255)*lev/255.0f, 1.0f);
			if (surf->Fade) {
				p_glUniform1iARB(SurfSimpleFogEnabledLoc, GL_TRUE);
				p_glUniform4fARB(SurfSimpleFogColourLoc, ((surf->Fade>>16)&255)/255.0f, ((surf->Fade>>8)&255)/255.0f, (surf->Fade&255)/255.0f, 1.0f);
				p_glUniform1fARB(SurfSimpleFogDensityLoc, (surf->Fade == FADE_LIGHT ? 0.3 : r_fog_density));
				p_glUniform1fARB(SurfSimpleFogStartLoc, (surf->Fade == FADE_LIGHT ? 1.0 : r_fog_start));
				p_glUniform1fARB(SurfSimpleFogEndLoc, (surf->Fade == FADE_LIGHT ? 1024.0 * r_fade_factor : r_fog_end));
			} else {
				p_glUniform1iARB(SurfSimpleFogEnabledLoc, GL_FALSE);
			}

			// fill stencil buffer for decals
			if (surf->dcseg && surf->dcseg->decals) RenderPrepareShaderDecals(surf, false);

			glBegin(GL_POLYGON);
			for (int i = 0; i < surf->count; ++i) glVertex(surf->verts[i]);
			glEnd();

			// draw decals
			if (surf->dcseg && surf->dcseg->decals) RenderFinishShaderDecals(surf, false);
		}
	}

	p_glUseProgramObjectARB(SurfLightmapProgram);
	p_glUniform1iARB(SurfLightmapTextureLoc, 0);
	p_glUniform1iARB(SurfLightmapLightMapLoc, 1);
	p_glUniform1iARB(SurfLightmapSpecularMapLoc, 2);
	p_glUniform1iARB(SurfLightmapFogTypeLoc, r_fog & 3);

	// draw surfaces with lightmaps
	for (int lb = 0; lb < NUM_BLOCK_SURFS; ++lb) {
		if (!RendLev->light_chain[lb]) continue;

		SelectTexture(1);
		glBindTexture(GL_TEXTURE_2D, lmap_id[lb]);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (RendLev->block_changed[lb]) {
			RendLev->block_changed[lb] = false;
			glTexImage2D(GL_TEXTURE_2D, 0, 4, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, RendLev->light_block[lb]);
			RendLev->add_changed[lb] = true;
		}

		SelectTexture(2);
		glBindTexture(GL_TEXTURE_2D, addmap_id[lb]);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (RendLev->add_changed[lb]) {
			RendLev->add_changed[lb] = false;
			glTexImage2D(GL_TEXTURE_2D, 0, 4, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, RendLev->add_block[lb]);
		}

		SelectTexture(0);

		for (cache = RendLev->light_chain[lb]; cache; cache = cache->chain) {
			surf = cache->surf;
			if (surf->plane->PointOnSide(vieworg)) continue; // viewer is in back side or on plane

			tex = surf->texinfo;
			SetTexture(tex->Tex, tex->ColourMap);

			p_glUniform3fvARB(SurfLightmapSAxisLoc, 1, &tex->saxis.x);
			p_glUniform1fARB(SurfLightmapSOffsLoc, tex->soffs);
			p_glUniform1fARB(SurfLightmapTexIWLoc, tex_iw);
			p_glUniform3fvARB(SurfLightmapTAxisLoc, 1, &tex->taxis.x);
			p_glUniform1fARB(SurfLightmapTOffsLoc, tex->toffs);
			p_glUniform1fARB(SurfLightmapTexIHLoc, tex_ih);
			p_glUniform1fARB(SurfLightmapTexMinSLoc, surf->texturemins[0]);
			p_glUniform1fARB(SurfLightmapTexMinTLoc, surf->texturemins[1]);
			p_glUniform1fARB(SurfLightmapCacheSLoc, cache->s);
			p_glUniform1fARB(SurfLightmapCacheTLoc, cache->t);

			if (surf->Fade) {
				p_glUniform1iARB(SurfLightmapFogEnabledLoc, GL_TRUE);
				p_glUniform4fARB(SurfLightmapFogColourLoc, ((surf->Fade>>16)&255)/255.0f, ((surf->Fade>>8)&255)/255.0f, (surf->Fade&255)/255.0f, 1.0f);
				p_glUniform1fARB(SurfLightmapFogDensityLoc, (surf->Fade == FADE_LIGHT ? 0.3 : r_fog_density));
				p_glUniform1fARB(SurfLightmapFogStartLoc, (surf->Fade == FADE_LIGHT ? 1.0 : r_fog_start));
				p_glUniform1fARB(SurfLightmapFogEndLoc, (surf->Fade == FADE_LIGHT ? 1024.0 * r_fade_factor : r_fog_end));
			} else {
				p_glUniform1iARB(SurfLightmapFogEnabledLoc, GL_FALSE);
			}

			// fill stencil buffer for decals
			if (surf->dcseg && surf->dcseg->decals) RenderPrepareShaderDecals(surf, true);

			glBegin(GL_POLYGON);
			for (int i = 0; i < surf->count; ++i) glVertex(surf->verts[i]);
			glEnd();

			// draw decals
			if (surf->dcseg && surf->dcseg->decals) {
				if (RenderFinishShaderDecals(surf, true)) {
					SelectTexture(1);
					glBindTexture(GL_TEXTURE_2D, lmap_id[lb]);
					SelectTexture(2);
					glBindTexture(GL_TEXTURE_2D, addmap_id[lb]);
					SelectTexture(0);
				}
			}
		}
	}

	RenderShaderDecalsEnd();

	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::DrawWorldAmbientPass
//
//==========================================================================

void VOpenGLDrawer::DrawWorldAmbientPass()
{
	guard(VOpenGLDrawer::DrawWorldAmbientPass);
	//	First draw horizons.
	if (RendLev->HorizonPortalsHead)
	{
		for (surface_t* surf = RendLev->HorizonPortalsHead; surf; surf = surf->DrawNext)
		{
			if (surf->plane->PointOnSide(vieworg))
			{
				//	Viewer is in back side or on plane
				continue;
			}

			DoHorizonPolygon(surf);
		}
	}

	if (RendLev->SkyPortalsHead)
	{
		p_glUseProgramObjectARB(SurfZBufProgram);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		for (surface_t* surf = RendLev->SkyPortalsHead; surf; surf = surf->DrawNext)
		{
			if (surf->plane->PointOnSide(vieworg))
			{
				//	Viewer is in back side or on plane
				continue;
			}

			glBegin(GL_POLYGON);
			for (int i = 0; i < surf->count; i++)
			{
				glVertex(surf->verts[i]);
			}
			glEnd();
		}
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}

	RenderShaderDecalsStart();

	p_glUseProgramObjectARB(ShadowsAmbientProgram);
	p_glUniform1iARB(ShadowsAmbientTextureLoc, 0);
	for (surface_t* surf = RendLev->SimpleSurfsHead; surf; surf = surf->DrawNext)
	{
		if (surf->plane->PointOnSide(vieworg))
		{
			//	Viewer is in back side or on plane
			continue;
		}

		texinfo_t* tex = surf->texinfo;
		SetTexture(tex->Tex, tex->ColourMap);
		p_glUniform3fvARB(ShadowsAmbientSAxisLoc, 1, &tex->saxis.x);
		p_glUniform1fARB(ShadowsAmbientSOffsLoc, tex->soffs);
		p_glUniform1fARB(ShadowsAmbientTexIWLoc, tex_iw);
		p_glUniform3fvARB(ShadowsAmbientTAxisLoc, 1, &tex->taxis.x);
		p_glUniform1fARB(ShadowsAmbientTOffsLoc, tex->toffs);
		p_glUniform1fARB(ShadowsAmbientTexIHLoc, tex_ih);

		float lev = float(surf->Light >> 24) / 255.0;
		p_glUniform4fARB(ShadowsAmbientLightLoc,
			((surf->Light >> 16) & 255) * lev / 255.0,
			((surf->Light >> 8) & 255) * lev / 255.0,
			(surf->Light & 255) * lev / 255.0, 1.0);

		// fill stencil buffer for decals
		if (surf->dcseg && surf->dcseg->decals) RenderPrepareShaderDecals(surf, true);

		glBegin(GL_POLYGON);
		for (int i = 0; i < surf->count; ++i) glVertex(surf->verts[i]);
		glEnd();

		// draw decals
		if (surf->dcseg && surf->dcseg->decals) {
			if (RenderFinishShaderDecals(surf, false)) {
				p_glUseProgramObjectARB(ShadowsAmbientProgram);
			}
		}
	}

	RenderShaderDecalsEnd();

	unguard;
}


//==========================================================================
//
//	VOpenGLDrawer::BeginShadowVolumesPass
//
//==========================================================================

void VOpenGLDrawer::BeginShadowVolumesPass()
{
	guard(VOpenGLDrawer::BeginShadowVolumesPass);
	//	Set up for shadow volume rendering.
	glEnable(GL_STENCIL_TEST);
	glDepthMask(GL_FALSE);
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::BeginLightShadowVolumes
//
//==========================================================================

static int swcount = 0;

void VOpenGLDrawer::BeginLightShadowVolumes()
{
	guard(VOpenGLDrawer::BeginLightShadowVolumes);
	//	Set up for shadow volume rendering.
	glClear(GL_STENCIL_BUFFER_BIT);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.0f, 10.0f);
	glDepthFunc(GL_LESS);

	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glStencilFunc(GL_ALWAYS, 0x0, 0xff);
	p_glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP_EXT, GL_KEEP);
	p_glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP_EXT, GL_KEEP);

	p_glUseProgramObjectARB(SurfZBufProgram);
	swcount = 0;
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::EndLightShadowVolumes
//
//==========================================================================

void VOpenGLDrawer::EndLightShadowVolumes()
{
	guard(VOpenGLDrawer::EndLightShadowVolumes);
	if (glsw_report_verts) GCon->Logf("swcount=%d", swcount);
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::RenderSurfaceShadowVolume
//
//==========================================================================

void VOpenGLDrawer::RenderSurfaceShadowVolume(surface_t *surf, TVec& LightPos, float Radius, bool LightCanCross)
{
	guard(VOpenGLDrawer::RenderSurfaceShadowVolume);
	int i;
	TArray<TVec>    v;
	if (surf->plane->PointOnSide(vieworg) && LightCanCross)
	{
		//	Viewer is in back side or on plane
		return;
	}
	float dist = DotProduct(LightPos, surf->plane->normal) - surf->plane->dist;
	if ((dist <= 0.0 && !LightCanCross) || dist < -Radius || dist > Radius)
	{
		//	Light is too far away
		return;
	}
	v.SetNum(surf->count);

  swcount += surf->count*4;

	for (i = 0; i < surf->count; i++)
	{
		v[i] = Normalise(surf->verts[i] - LightPos);
		v[i] *= M_INFINITY;
		v[i] += LightPos;
	}

	glBegin(GL_POLYGON);
	for (i = surf->count - 1; i >= 0; i--)
	{
		glVertex(v[i]);
	}
	glEnd();

	glBegin(GL_POLYGON);
	for (i = 0; i < surf->count; i++)
	{
		glVertex(surf->verts[i]);
	}
	glEnd();

	glBegin(GL_TRIANGLE_STRIP);
	for (i = 0; i < surf->count; i++)
	{
		glVertex(surf->verts[i]);
		glVertex(v[i]);
	}
	glVertex(surf->verts[0]);
	glVertex(v[0]);
	glEnd();

	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::BeginLightPass
//
//==========================================================================

void VOpenGLDrawer::BeginLightPass(TVec& LightPos, float Radius, vuint32 Colour)
{
	guard(VOpenGLDrawer::BeginLightPass);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_POLYGON_OFFSET_FILL);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	glStencilFunc(GL_EQUAL, 0x0, 0xff);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glEnable(GL_BLEND);

	p_glUseProgramObjectARB(ShadowsLightProgram);
	p_glUniform3fARB(ShadowsLightLightPosLoc, LightPos.x, LightPos.y, LightPos.z);
	p_glUniform1fARB(ShadowsLightLightRadiusLoc, Radius);
	p_glUniform3fARB(ShadowsLightLightColourLoc,
		((Colour >> 16) & 255) / 255.0,
		((Colour >> 8) & 255) / 255.0,
		(Colour & 255) / 255.0);
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::DrawSurfaceLight
//
//==========================================================================

void VOpenGLDrawer::DrawSurfaceLight(surface_t* Surf, TVec& LightPos, float Radius, bool LightCanCross)
{
	guard(VOpenGLDrawer::DrawSurfaceLight);
	if (Surf->plane->PointOnSide(vieworg))
	{
		//	Viewer is in back side or on plane
		return;
	}
	float dist = DotProduct(LightPos, Surf->plane->normal) - Surf->plane->dist;
	if ((dist <= 0.0 && !LightCanCross) || dist < -Radius || dist > Radius)
	{
		//	Light is too far away
		return;
	}
	p_glUniform1iARB(ShadowsAmbientTextureLoc, 0);
	texinfo_t* tex = Surf->texinfo;
	SetTexture(tex->Tex, tex->ColourMap);
	p_glUniform3fvARB(ShadowsAmbientSAxisLoc, 1, &tex->saxis.x);
	p_glUniform1fARB(ShadowsAmbientSOffsLoc, tex->soffs);
	p_glUniform1fARB(ShadowsAmbientTexIWLoc, tex_iw);
	p_glUniform3fvARB(ShadowsAmbientTAxisLoc, 1, &tex->taxis.x);
	p_glUniform1fARB(ShadowsAmbientTOffsLoc, tex->toffs);
	p_glUniform1fARB(ShadowsAmbientTexIHLoc, tex_ih);
	p_glVertexAttrib3fvARB(ShadowsLightSurfNormalLoc, &Surf->plane->normal.x);
	p_glVertexAttrib1fvARB(ShadowsLightSurfDistLoc, &Surf->plane->dist);
	p_glUniform3fARB(ShadowsLightViewOrigin, vieworg.x, vieworg.y, vieworg.z);

	glBegin(GL_POLYGON);
	for (int i = 0; i < Surf->count; i++)
	{
		glVertex(Surf->verts[i]);
	}
	glEnd();
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::DrawWorldTexturesPass
//
//==========================================================================

void VOpenGLDrawer::DrawWorldTexturesPass()
{
	guard(VOpenGLDrawer::DrawWorldTexturesPass);
	//	Stop stenciling now.
	glDisable(GL_STENCIL_TEST);

	glBlendFunc(GL_DST_COLOR, GL_ZERO);
	glEnable(GL_BLEND);

	p_glUseProgramObjectARB(ShadowsTextureProgram);
	p_glUniform1iARB(ShadowsTextureTextureLoc, 0);

	for (surface_t* surf = RendLev->SimpleSurfsHead; surf; surf = surf->DrawNext)
	{
		if (surf->plane->PointOnSide(vieworg))
		{
			//	Viewer is in back side or on plane
			continue;
		}

		// this is for advanced renderer only

		texinfo_t* tex = surf->texinfo;
		SetTexture(tex->Tex, tex->ColourMap);
		glBegin(GL_POLYGON);
		for (int i = 0; i < surf->count; i++)
		{
			p_glVertexAttrib2fARB(ShadowsTextureTexCoordLoc,
				(DotProduct(surf->verts[i], tex->saxis) + tex->soffs) * tex_iw,
				(DotProduct(surf->verts[i], tex->taxis) + tex->toffs) * tex_ih);
			glVertex(surf->verts[i]);
		}
		glEnd();
	}
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::DrawWorldFogPass
//
//==========================================================================

void VOpenGLDrawer::DrawWorldFogPass()
{
	guard(VOpenGLDrawer::DrawWorldFogPass);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//	Draw surfaces.
	p_glUseProgramObjectARB(ShadowsFogProgram);
	p_glUniform1iARB(ShadowsFogFogTypeLoc, r_fog & 3);

	for (surface_t* surf = RendLev->SimpleSurfsHead; surf; surf = surf->DrawNext)
	{
		if (!surf->Fade)
		{
			continue;
		}

		if (surf->plane->PointOnSide(vieworg))
		{
			//	Viewer is in back side or on plane
			continue;
		}

		p_glUniform4fARB(ShadowsFogFogColourLoc,
			((surf->Fade >> 16) & 255) / 255.0,
			((surf->Fade >> 8) & 255) / 255.0,
			(surf->Fade & 255) / 255.0, 1.0);
		p_glUniform1fARB(ShadowsFogFogDensityLoc, surf->Fade == FADE_LIGHT ? 0.3 : r_fog_density);
		p_glUniform1fARB(ShadowsFogFogStartLoc, surf->Fade == FADE_LIGHT ? 1.0 : r_fog_start);
		p_glUniform1fARB(ShadowsFogFogEndLoc, surf->Fade == FADE_LIGHT ? 1024.0 * r_fade_factor : r_fog_end);

		glBegin(GL_POLYGON);
		for (int i = 0; i < surf->count; i++)
		{
			glVertex(surf->verts[i]);
		}
		glEnd();
	}
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::EndFogPass
//
//==========================================================================

void VOpenGLDrawer::EndFogPass()
{
	guard(VOpenGLDrawer::EndFogPass);
	glDisable(GL_BLEND);

	//	Back to normal z-buffering.
	glDepthMask(GL_TRUE);
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::DoHorizonPolygon
//
//==========================================================================

void VOpenGLDrawer::DoHorizonPolygon(surface_t* Surf)
{
	guard(VOpenGLDrawer::DoHorizonPolygon);
	float Dist = 4096.0;
	TVec v[4];
	if (Surf->HorizonPlane->normal.z > 0.0)
	{
		v[0] = Surf->verts[0];
		v[3] = Surf->verts[3];
		TVec HDir = -Surf->plane->normal;

		TVec Dir1 = Normalise(vieworg - Surf->verts[1]);
		TVec Dir2 = Normalise(vieworg - Surf->verts[2]);
		float Mul1 = 1.0 / DotProduct(HDir, Dir1);
		v[1] = Surf->verts[1] + Dir1 * Mul1 * Dist;
		float Mul2 = 1.0 / DotProduct(HDir, Dir2);
		v[2] = Surf->verts[2] + Dir2 * Mul2 * Dist;
		if (v[1].z < v[0].z)
		{
			v[1] = Surf->verts[1] + Dir1 * Mul1 * Dist * (Surf->verts[1].z -
				Surf->verts[0].z) / (Surf->verts[1].z - v[1].z);
			v[2] = Surf->verts[2] + Dir2 * Mul2 * Dist * (Surf->verts[2].z -
				Surf->verts[3].z) / (Surf->verts[2].z - v[2].z);
		}
	}
	else
	{
		v[1] = Surf->verts[1];
		v[2] = Surf->verts[2];
		TVec HDir = -Surf->plane->normal;

		TVec Dir1 = Normalise(vieworg - Surf->verts[0]);
		TVec Dir2 = Normalise(vieworg - Surf->verts[3]);
		float Mul1 = 1.0 / DotProduct(HDir, Dir1);
		v[0] = Surf->verts[0] + Dir1 * Mul1 * Dist;
		float Mul2 = 1.0 / DotProduct(HDir, Dir2);
		v[3] = Surf->verts[3] + Dir2 * Mul2 * Dist;
		if (v[1].z < v[0].z)
		{
			v[0] = Surf->verts[0] + Dir1 * Mul1 * Dist * (Surf->verts[1].z -
				Surf->verts[0].z) / (v[0].z - Surf->verts[0].z);
			v[3] = Surf->verts[3] + Dir2 * Mul2 * Dist * (Surf->verts[2].z -
				Surf->verts[3].z) / (v[3].z - Surf->verts[3].z);
		}
	}

	texinfo_t* Tex = Surf->texinfo;
	SetTexture(Tex->Tex, Tex->ColourMap);

	if (HaveShaders)
	{
		p_glUseProgramObjectARB(SurfSimpleProgram);
		p_glUniform1iARB(SurfSimpleTextureLoc, 0);
		p_glUniform1iARB(SurfSimpleFogTypeLoc, r_fog & 3);

		p_glUniform3fvARB(SurfSimpleSAxisLoc, 1, &Tex->saxis.x);
		p_glUniform1fARB(SurfSimpleSOffsLoc, Tex->soffs);
		p_glUniform1fARB(SurfSimpleTexIWLoc, tex_iw);
		p_glUniform3fvARB(SurfSimpleTAxisLoc, 1, &Tex->taxis.x);
		p_glUniform1fARB(SurfSimpleTOffsLoc, Tex->toffs);
		p_glUniform1fARB(SurfSimpleTexIHLoc, tex_ih);

		float lev = float(Surf->Light >> 24) / 255.0;
		p_glUniform4fARB(SurfSimpleLightLoc,
			((Surf->Light >> 16) & 255) * lev / 255.0,
			((Surf->Light >> 8) & 255) * lev / 255.0,
			(Surf->Light & 255) * lev / 255.0, 1.0);
		if (Surf->Fade)
		{
			p_glUniform1iARB(SurfSimpleFogEnabledLoc, GL_TRUE);
			p_glUniform4fARB(SurfSimpleFogColourLoc,
				((Surf->Fade >> 16) & 255) / 255.0,
				((Surf->Fade >> 8) & 255) / 255.0,
				(Surf->Fade & 255) / 255.0, 1.0);
			p_glUniform1fARB(SurfSimpleFogDensityLoc, Surf->Fade == FADE_LIGHT ? 0.3 : r_fog_density);
			p_glUniform1fARB(SurfSimpleFogStartLoc, Surf->Fade == FADE_LIGHT ? 1.0 : r_fog_start);
			p_glUniform1fARB(SurfSimpleFogEndLoc, Surf->Fade == FADE_LIGHT ? 1024.0 * r_fade_factor : r_fog_end);
		}
		else
		{
			p_glUniform1iARB(SurfSimpleFogEnabledLoc, GL_FALSE);
		}

		//	Draw it
		glDepthMask(GL_FALSE);
		glBegin(GL_POLYGON);
		for (int i = 0; i < 4; i++)
		{
			glVertex(v[i]);
		}
		glEnd();
		glDepthMask(GL_TRUE);

		//	Write to the depth buffer.
		p_glUseProgramObjectARB(SurfZBufProgram);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glBegin(GL_POLYGON);
		for (int i = 0; i < Surf->count; i++)
		{
			glVertex(Surf->verts[i]);
		}
		glEnd();
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	}
	else
	{
		float lev = float(Surf->Light >> 24) / 255.0;
		glColor4f(((Surf->Light >> 16) & 255) * lev / 255.0,
			((Surf->Light >> 8) & 255) * lev / 255.0,
			(Surf->Light & 255) * lev / 255.0, 1.0);
		SetFade(Surf->Fade);

		//	Draw it
		glDepthMask(GL_FALSE);
		glBegin(GL_POLYGON);
		for (int i = 0; i < 4; i++)
		{
			glTexCoord2f((DotProduct(v[i], Tex->saxis) + Tex->soffs) * tex_iw,
				(DotProduct(v[i], Tex->taxis) + Tex->toffs) * tex_ih);
			glVertex(v[i]);
		}
		glEnd();
		glDepthMask(GL_TRUE);

		//	Write to the depth buffer.
		glDisable(GL_TEXTURE_2D);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glBegin(GL_POLYGON);
		for (int i = 0; i < Surf->count; i++)
		{
			glVertex(Surf->verts[i]);
		}
		glEnd();
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glEnable(GL_TEXTURE_2D);
	}
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::DrawSkyPolygon
//
//==========================================================================

void VOpenGLDrawer::DrawSkyPolygon(surface_t* surf, bool bIsSkyBox,
	VTexture* Texture1, float offs1, VTexture* Texture2, float offs2,
	int CMap)
{
	guard(VOpenGLDrawer::DrawSkyPolygon);
	int		i;
	int		sidx[4];

	SetFade(surf->Fade);
	sidx[0] = 0;
	sidx[1] = 1;
	sidx[2] = 2;
	sidx[3] = 3;
	if (!bIsSkyBox)
	{
		if (surf->verts[1].z > 0)
		{
			sidx[1] = 0;
			sidx[2] = 3;
		}
		else
		{
			sidx[0] = 1;
			sidx[3] = 2;
		}
	}
	texinfo_t *tex = surf->texinfo;
	if (HaveShaders)
	{
		if (Texture2->Type != TEXTYPE_Null)
		{
			SetTexture(Texture1, CMap);
			SelectTexture(1);
			SetTexture(Texture2, CMap);
			SelectTexture(0);

			p_glUseProgramObjectARB(SurfDSkyProgram);
			p_glUniform1iARB(SurfDSkyTextureLoc, 0);
			p_glUniform1iARB(SurfDSkyTexture2Loc, 1);
			p_glUniform1fARB(SurfDSkyBrightnessLoc, r_sky_bright_factor);

			glBegin(GL_POLYGON);
			for (i = 0; i < surf->count; i++)
			{
				p_glVertexAttrib2fARB(SurfDSkyTexCoordLoc,
					(DotProduct(surf->verts[sidx[i]], tex->saxis) + tex->soffs - offs1) * tex_iw,
					(DotProduct(surf->verts[i], tex->taxis) + tex->toffs) * tex_ih);
				p_glVertexAttrib2fARB(SurfDSkyTexCoord2Loc,
					(DotProduct(surf->verts[sidx[i]], tex->saxis) + tex->soffs - offs2) * tex_iw,
					(DotProduct(surf->verts[i], tex->taxis) + tex->toffs) * tex_ih);
				glVertex(surf->verts[i]);
			}
			glEnd();
		}
		else
		{
			SetTexture(Texture1, CMap);

			p_glUseProgramObjectARB(SurfSkyProgram);
			p_glUniform1iARB(SurfSkyTextureLoc, 0);
			p_glUniform1fARB(SurfSkyBrightnessLoc, r_sky_bright_factor);

			glBegin(GL_POLYGON);
			for (i = 0; i < surf->count; i++)
			{
				p_glVertexAttrib2fARB(SurfSkyTexCoordLoc,
					(DotProduct(surf->verts[sidx[i]], tex->saxis) + tex->soffs - offs1) * tex_iw,
					(DotProduct(surf->verts[i], tex->taxis) + tex->toffs) * tex_ih);
				glVertex(surf->verts[i]);
			}
			glEnd();
		}
	}
	else
	{
		if (HaveMultiTexture && Texture2->Type != TEXTYPE_Null)
		{
			SetTexture(Texture1, CMap);
			SelectTexture(1);
			glEnable(GL_TEXTURE_2D);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
			SetTexture(Texture2, CMap);
			SelectTexture(0);

			glColor4f(r_sky_bright_factor * 1, r_sky_bright_factor * 1, r_sky_bright_factor * 1, 1);
			glBegin(GL_POLYGON);
			for (i = 0; i < surf->count; i++)
			{
				MultiTexCoord(0, 
					(DotProduct(surf->verts[sidx[i]], tex->saxis) + tex->soffs - offs1) * tex_iw,
					(DotProduct(surf->verts[i], tex->taxis) + tex->toffs) * tex_ih);
				MultiTexCoord(1, 
					(DotProduct(surf->verts[sidx[i]], tex->saxis) + tex->soffs - offs2) * tex_iw,
					(DotProduct(surf->verts[i], tex->taxis) + tex->toffs) * tex_ih);
				glVertex(surf->verts[i]);
			}
			glEnd();

			SelectTexture(1);
			glDisable(GL_TEXTURE_2D);
			SelectTexture(0);
		}
		else
		{
			SetTexture(Texture1, CMap);
			glBegin(GL_POLYGON);
			glColor4f(r_sky_bright_factor * 1, r_sky_bright_factor * 1, r_sky_bright_factor * 1, 1);
			for (i = 0; i < surf->count; i++)
			{
				glTexCoord2f(
					(DotProduct(surf->verts[sidx[i]], tex->saxis) + tex->soffs - offs1) * tex_iw,
					(DotProduct(surf->verts[i], tex->taxis) + tex->toffs) * tex_ih);
				glVertex(surf->verts[i]);
			}
			glEnd();

			if (Texture2->Type != TEXTYPE_Null)
			{
				SetTexture(Texture2, CMap);
				glEnable(GL_BLEND);
				glBegin(GL_POLYGON);
				glColor4f(1, 1, 1, 1);
				for (i = 0; i < surf->count; i++)
				{
					glTexCoord2f(
						(DotProduct(surf->verts[sidx[i]], tex->saxis) + tex->soffs - offs2) * tex_iw,
						(DotProduct(surf->verts[i], tex->taxis) + tex->toffs) * tex_ih);
					glVertex(surf->verts[i]);
				}
				glEnd();
				glDisable(GL_BLEND);
			}
		}
	}
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::DrawMaskedPolygon
//
//==========================================================================

void VOpenGLDrawer::DrawMaskedPolygon(surface_t* surf, float Alpha,
	bool Additive)
{
	guard(VOpenGLDrawer::DrawMaskedPolygon);
	if (surf->plane->PointOnSide(vieworg))
	{
		//	Viewer is in back side or on plane
		return;
	}

	texinfo_t* tex = surf->texinfo;
	SetTexture(tex->Tex, tex->ColourMap);

	if (HaveShaders)
	{
		p_glUseProgramObjectARB(SurfMaskedProgram);
		p_glUniform1iARB(SurfMaskedTextureLoc, 0);
		p_glUniform1iARB(SurfMaskedFogTypeLoc, r_fog & 3);

		if (surf->lightmap != NULL ||
			surf->dlightframe == r_dlightframecount)
		{
			RendLev->BuildLightMap(surf, 0);
			int w = (surf->extents[0] >> 4) + 1;
			int h = (surf->extents[1] >> 4) + 1;
			int size = w * h;
			int r = 0;
			int g = 0;
			int b = 0;
			for (int i = 0; i < size; i++)
			{
				r += 255 * 256 - blocklightsr[i];
				g += 255 * 256 - blocklightsg[i];
				b += 255 * 256 - blocklightsb[i];
			}
			double iscale = 1.0 / (size * 255 * 256);
			p_glUniform4fARB(SurfMaskedLightLoc, r * iscale, g * iscale, b * iscale, Alpha);
		}
		else
		{
			float lev = float(surf->Light >> 24) / 255.0;
			p_glUniform4fARB(SurfMaskedLightLoc,
				((surf->Light >> 16) & 255) * lev / 255.0,
				((surf->Light >> 8) & 255) * lev / 255.0,
				(surf->Light & 255) * lev / 255.0, Alpha);
		}
		if (surf->Fade)
		{
			p_glUniform1iARB(SurfMaskedFogEnabledLoc, GL_TRUE);
			p_glUniform4fARB(SurfMaskedFogColourLoc,
				((surf->Fade >> 16) & 255) / 255.0,
				((surf->Fade >> 8) & 255) / 255.0,
				(surf->Fade & 255) / 255.0, Alpha);
			p_glUniform1fARB(SurfMaskedFogDensityLoc, surf->Fade == FADE_LIGHT ? 0.3 : r_fog_density);
			p_glUniform1fARB(SurfMaskedFogStartLoc, surf->Fade == FADE_LIGHT ? 1.0 : r_fog_start);
			p_glUniform1fARB(SurfMaskedFogEndLoc, surf->Fade == FADE_LIGHT ? 1024.0 * r_fade_factor : r_fog_end);
		}
		else
		{
			p_glUniform1iARB(SurfMaskedFogEnabledLoc, GL_FALSE);
		}

		if (blend_sprites || Additive || Alpha < 1.0)
		{
			p_glUniform1fARB(SurfMaskedAlphaRefLoc, 0.333);
			glEnable(GL_BLEND);
		}
		else
		{
			p_glUniform1fARB(SurfMaskedAlphaRefLoc, 0.555);
		}
		if (Additive)
		{
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		}

		glBegin(GL_POLYGON);
		for (int i = 0; i < surf->count; i++)
		{
			p_glVertexAttrib2fARB(SurfMaskedTexCoordLoc,
				(DotProduct(surf->verts[i], tex->saxis) + tex->soffs) * tex_iw,
				(DotProduct(surf->verts[i], tex->taxis) + tex->toffs) * tex_ih);
			glVertex(surf->verts[i]);
		}
		glEnd();

		if (blend_sprites || Additive || Alpha < 1.0)
		{
			glDisable(GL_BLEND);
		}
		if (Additive)
		{
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
	}
	else
	{
		glEnable(GL_ALPHA_TEST);
		if (blend_sprites || Additive || Alpha < 1.0)
		{
			glAlphaFunc(GL_GREATER, 0.111);
			glEnable(GL_BLEND);
		}
		if (Additive)
		{
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		}

		if (surf->lightmap != NULL ||
			surf->dlightframe == r_dlightframecount)
		{
			RendLev->BuildLightMap(surf, 0);
			int w = (surf->extents[0] >> 4) + 1;
			int h = (surf->extents[1] >> 4) + 1;
			int size = w * h;
			int r = 0;
			int g = 0;
			int b = 0;
			for (int i = 0; i < size; i++)
			{
				r += 255 * 256 - blocklightsr[i];
				g += 255 * 256 - blocklightsg[i];
				b += 255 * 256 - blocklightsb[i];
			}
			double iscale = 1.0 / (size * 255 * 256);
			glColor4f(r * iscale, g * iscale, b * iscale, Alpha);
		}
		else
		{
			float lev = float(surf->Light >> 24) / 255.0;
			glColor4f(((surf->Light >> 16) & 255) * lev / 255.0,
				((surf->Light >> 8) & 255) * lev / 255.0,
				(surf->Light & 255) * lev / 255.0, Alpha);
		}
		SetFade(surf->Fade);

		glBegin(GL_POLYGON);
		for (int i = 0; i < surf->count; i++)
		{
			glTexCoord2f((DotProduct(surf->verts[i], tex->saxis) + tex->soffs) * tex_iw,
				(DotProduct(surf->verts[i], tex->taxis) + tex->toffs) * tex_ih);
			glVertex(surf->verts[i]);
		}
		glEnd();

		if (blend_sprites || Additive || Alpha < 1.0)
		{
			glAlphaFunc(GL_GREATER, 0.333);
			glDisable(GL_BLEND);
		}
		if (Additive)
		{
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		glDisable(GL_ALPHA_TEST);
	}
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::DrawSpritePolygon
//
//==========================================================================

void VOpenGLDrawer::DrawSpritePolygon(TVec *cv, VTexture* Tex, float Alpha,
	bool Additive, VTextureTranslation* Translation, int CMap, vuint32 light,
	vuint32 Fade, const TVec&, float, const TVec& saxis, const TVec& taxis,
	const TVec& texorg)
{
	guard(VOpenGLDrawer::DrawSpritePolygon);
	TVec	texpt;

	SetSpriteLump(Tex, Translation, CMap);

	switch (sprite_tex_linear)
	{
		case 1:
		{
			spr_maxfilter = GL_LINEAR;
			spr_minfilter = GL_LINEAR;
			spr_mipfilter = GL_NEAREST;
			break;
		}
		case 2:
		{
			spr_maxfilter = GL_LINEAR;
			spr_minfilter = GL_LINEAR;
			spr_mipfilter = GL_LINEAR_MIPMAP_NEAREST;
			break;
		}
		case 3:
		{
			spr_maxfilter = GL_LINEAR;
			spr_minfilter = GL_LINEAR;
			spr_mipfilter = GL_LINEAR_MIPMAP_LINEAR;
			break;
		}
		case 4: // BILINEAR
		{
			spr_maxfilter = GL_NEAREST;
			spr_minfilter = GL_LINEAR_MIPMAP_NEAREST;
			spr_mipfilter = GL_LINEAR_MIPMAP_NEAREST;
			break;
		}
		case 5: // TRILINEAR
		{
			spr_maxfilter = GL_NEAREST;
			spr_minfilter = GL_LINEAR_MIPMAP_LINEAR;
			spr_mipfilter = GL_LINEAR_MIPMAP_LINEAR;
			break;
		}
		default:
		{
			spr_maxfilter = GL_NEAREST;
			spr_minfilter = GL_NEAREST;
			spr_mipfilter = GL_NEAREST;
		}
	}
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, spr_mipfilter);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, spr_maxfilter);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texture_filter_anisotropic);
	if (max_anisotropy > 1.0) {
		glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), (GLfloat)(gl_texture_filter_anisotropic < 0 ? 0 : gl_texture_filter_anisotropic > max_anisotropy ? max_anisotropy : gl_texture_filter_anisotropic));
	}

	if (HaveShaders)
	{
		p_glUseProgramObjectARB(SurfMaskedProgram);
		p_glUniform1iARB(SurfMaskedTextureLoc, 0);
		p_glUniform1iARB(SurfMaskedFogTypeLoc, r_fog & 3);

		p_glUniform4fARB(SurfMaskedLightLoc,
			((light >> 16) & 255) / 255.0,
			((light >> 8) & 255) / 255.0,
			(light & 255) / 255.0, Alpha);
		if (Fade)
		{
			p_glUniform1iARB(SurfMaskedFogEnabledLoc, GL_TRUE);
			p_glUniform4fARB(SurfMaskedFogColourLoc,
				((Fade >> 16) & 255) / 255.0,
				((Fade >> 8) & 255) / 255.0,
				(Fade & 255) / 255.0, Alpha);
			p_glUniform1fARB(SurfMaskedFogDensityLoc, Fade == FADE_LIGHT ? 0.3 : r_fog_density);
			p_glUniform1fARB(SurfMaskedFogStartLoc, Fade == FADE_LIGHT ? 1.0 : r_fog_start);
			p_glUniform1fARB(SurfMaskedFogEndLoc, Fade == FADE_LIGHT ? 1024.0 * r_fade_factor : r_fog_end);
		}
		else
		{
			p_glUniform1iARB(SurfMaskedFogEnabledLoc, GL_FALSE);
		}

		if (blend_sprites || Additive || Alpha < 1.0)
		{
			p_glUniform1fARB(SurfMaskedAlphaRefLoc, 0.333);
			glEnable(GL_BLEND);
		}
		else
		{
			p_glUniform1fARB(SurfMaskedAlphaRefLoc, 0.555);
		}
		if (Additive)
		{
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		}

		glBegin(GL_QUADS);

		texpt = cv[0] - texorg;
		p_glVertexAttrib2fARB(SurfMaskedTexCoordLoc,
			DotProduct(texpt, saxis) * tex_iw,
			DotProduct(texpt, taxis) * tex_ih);
		glVertex(cv[0]);

		texpt = cv[1] - texorg;
		p_glVertexAttrib2fARB(SurfMaskedTexCoordLoc,
			DotProduct(texpt, saxis) * tex_iw,
			DotProduct(texpt, taxis) * tex_ih);
		glVertex(cv[1]);

		texpt = cv[2] - texorg;
		p_glVertexAttrib2fARB(SurfMaskedTexCoordLoc,
			DotProduct(texpt, saxis) * tex_iw,
			DotProduct(texpt, taxis) * tex_ih);
		glVertex(cv[2]);

		texpt = cv[3] - texorg;
		p_glVertexAttrib2fARB(SurfMaskedTexCoordLoc,
			DotProduct(texpt, saxis) * tex_iw,
			DotProduct(texpt, taxis) * tex_ih);
		glVertex(cv[3]);

		glEnd();

		if (blend_sprites || Additive || Alpha < 1.0)
		{
			glDisable(GL_BLEND);
		}
		if (Additive)
		{
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
	}
	else
	{
		glEnable(GL_ALPHA_TEST);
		if (blend_sprites || Additive || Alpha < 1.0)
		{
			glAlphaFunc(GL_GREATER, 0.111);
			glEnable(GL_BLEND);
		}
		if (Additive)
		{
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		}

		vuint32 alpha = (int)(255 * Alpha);
		SetColour((light & 0x00ffffff) | (alpha << 24));
		SetFade(Fade);

		glBegin(GL_QUADS);

		texpt = cv[0] - texorg;
		glTexCoord2f(DotProduct(texpt, saxis) * tex_iw,
			DotProduct(texpt, taxis) * tex_ih);
		glVertex(cv[0]);

		texpt = cv[1] - texorg;
		glTexCoord2f(DotProduct(texpt, saxis) * tex_iw,
			DotProduct(texpt, taxis) * tex_ih);
		glVertex(cv[1]);

		texpt = cv[2] - texorg;
		glTexCoord2f(DotProduct(texpt, saxis) * tex_iw,
			DotProduct(texpt, taxis) * tex_ih);
		glVertex(cv[2]);

		texpt = cv[3] - texorg;
		glTexCoord2f(DotProduct(texpt, saxis) * tex_iw,
			DotProduct(texpt, taxis) * tex_ih);
		glVertex(cv[3]);

		glEnd();

		if (blend_sprites || Additive || Alpha < 1.0)
		{
			glAlphaFunc(GL_GREATER, 0.333);
			glDisable(GL_BLEND);
		}
		if (Additive)
		{
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		glDisable(GL_ALPHA_TEST);
	}
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::StartParticles
//
//==========================================================================

void VOpenGLDrawer::StartParticles()
{
	guard(VOpenGLDrawer::StartParticles);
	glEnable(GL_BLEND);
	if (HaveShaders)
	{
		p_glUseProgramObjectARB(SurfPartProgram);
		p_glUniform1iARB(SurfPartSmoothParticleLoc, gl_smooth_particles);
		glBegin(GL_QUADS);
	}
	else
	{
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.111);
		if (pointparmsable)
		{
			GLfloat parms[3] = { 0.0, 1.0, 0.0 };
			p_glPointParameterfvEXT(GLenum(GL_DISTANCE_ATTENUATION_EXT), parms);
			p_glPointParameterfEXT(GLenum(GL_POINT_FADE_THRESHOLD_SIZE_EXT), 1.0);
			glDisable(GL_TEXTURE_2D);
			glEnable(GL_POINT_SMOOTH);
			glBegin(GL_POINTS);
		}
		else
		{
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, maxfilter);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipfilter);
			glBindTexture(GL_TEXTURE_2D, particle_texture);
			glBegin(GL_QUADS);
		}
	}
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::DrawParticle
//
//==========================================================================

void VOpenGLDrawer::DrawParticle(particle_t *p)
{
	guard(VOpenGLDrawer::DrawParticle);
	if (HaveShaders)
	{
		float r = ((p->colour >> 16) & 255) / 255.0;
		float g = ((p->colour >> 8) & 255) / 255.0;
		float b = (p->colour & 255) / 255.0;
		float a = ((p->colour >> 24) & 255) / 255.0;
		p_glVertexAttrib4fARB(SurfPartLightValLoc, r, g, b, a);
		p_glVertexAttrib2fARB(SurfPartTexCoordLoc, -1, -1);
		glVertex(p->org - viewright * p->Size + viewup * p->Size);
		p_glVertexAttrib4fARB(SurfPartLightValLoc, r, g, b, a);
		p_glVertexAttrib2fARB(SurfPartTexCoordLoc, 1, -1);
		glVertex(p->org + viewright * p->Size + viewup * p->Size);
		p_glVertexAttrib4fARB(SurfPartLightValLoc, r, g, b, a);
		p_glVertexAttrib2fARB(SurfPartTexCoordLoc, 1, 1);
		glVertex(p->org + viewright * p->Size - viewup * p->Size);
		p_glVertexAttrib4fARB(SurfPartLightValLoc, r, g, b, a);
		p_glVertexAttrib2fARB(SurfPartTexCoordLoc, -1, 1);
		glVertex(p->org - viewright * p->Size - viewup * p->Size);
	}
	else
	{
		SetColour(p->colour);
		if (pointparmsable)
		{
			glVertex(p->org);
		}
		else
		{
			glTexCoord2f(0, 0);
			glVertex(p->org - viewright * p->Size + viewup * p->Size);
			glTexCoord2f(1, 0);
			glVertex(p->org + viewright * p->Size + viewup * p->Size);
			glTexCoord2f(1, 1);
			glVertex(p->org + viewright * p->Size - viewup * p->Size);
			glTexCoord2f(0, 1);
			glVertex(p->org - viewright * p->Size - viewup * p->Size);
		}
	}
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::EndParticles
//
//==========================================================================

void VOpenGLDrawer::EndParticles()
{
	guard(VOpenGLDrawer::EndParticles);
	glEnd();
	if (!HaveShaders)
	{
		glDisable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.333);
		if (pointparmsable)
		{
			glDisable(GL_POINT_SMOOTH);
			glEnable(GL_TEXTURE_2D);
		}
	}
	glDisable(GL_BLEND);
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::StartPortal
//
//==========================================================================

bool VOpenGLDrawer::StartPortal(VPortal* Portal, bool UseStencil)
{
	guard(VOpenGLDrawer::StartPortal);
	if (UseStencil)
	{
		//	Doesn't work for now.
		if (RendLev->NeedsInfiniteFarClip)
		{
			return false;
		}

		//	Disable drawing
		if (HaveShaders)
		{
			p_glUseProgramObjectARB(SurfZBufProgram);
		}
		else
		{
			glDisable(GL_TEXTURE_2D);
		}
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glDepthMask(GL_FALSE);

		//	Set up stencil test.
		if (!RendLev->PortalDepth)
		{
			glEnable(GL_STENCIL_TEST);
		}
		glStencilFunc(GL_EQUAL, RendLev->PortalDepth, ~0);
		glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

		//	Mark the portal area.
		DrawPortalArea(Portal);

		//	Set up stencil test for portal
		glStencilFunc(GL_EQUAL, RendLev->PortalDepth + 1, ~0);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

		if (Portal->NeedsDepthBuffer())
		{
			glDepthMask(GL_TRUE);
			//	Clear depth buffer
			glDepthRange(1, 1);
			glDepthFunc(GL_ALWAYS);
			DrawPortalArea(Portal);
			glDepthFunc(GL_LEQUAL);
			glDepthRange(0, 1);
		}
		else
		{
			glDepthMask(GL_FALSE);
			glDisable(GL_DEPTH_TEST);
		}

		//	Enable drawing.
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		if (!HaveShaders)
		{
			glEnable(GL_TEXTURE_2D);
		}

		RendLev->PortalDepth++;
	}
	else
	{
		if (!Portal->NeedsDepthBuffer())
		{
			glDepthMask(GL_FALSE);
			glDisable(GL_DEPTH_TEST);
		}
	}
	return true;
	unguard;
}

//==========================================================================
//
//	VOpenGLDrawer::DrawPortalArea
//
//==========================================================================

void VOpenGLDrawer::DrawPortalArea(VPortal* Portal)
{
	guard(VOpenGLDrawer::DrawPortalArea);
	for (int i = 0; i < Portal->Surfs.Num(); i++)
	{
		const surface_t* Surf = Portal->Surfs[i];
		glBegin(GL_POLYGON);
		for (int j = 0; j < Surf->count; j++)
		{
			glVertex(Surf->verts[j]);
		}
		glEnd();
	}
	unguard;
}

//==========================================================================
//
//	VSoftwareDrawer::EndPortal
//
//==========================================================================

void VOpenGLDrawer::EndPortal(VPortal* Portal, bool UseStencil)
{
	guard(VOpenGLDrawer::EndPortal);
	if (HaveShaders)
	{
		p_glUseProgramObjectARB(SurfZBufProgram);
	}
	else
	{
		glDisable(GL_TEXTURE_2D);
	}
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	if (UseStencil)
	{
		if (Portal->NeedsDepthBuffer())
		{
			//	Clear depth buffer
			glDepthRange(1, 1);
			glDepthFunc(GL_ALWAYS);
			DrawPortalArea(Portal);
			glDepthFunc(GL_LEQUAL);
			glDepthRange(0, 1);
		}
		else
		{
			glDepthMask(GL_TRUE);
			glEnable(GL_DEPTH_TEST);
		}

		glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);

		//	Draw proper z-buffer for the portal area.
		glDepthFunc(GL_ALWAYS);
		DrawPortalArea(Portal);
		glDepthFunc(GL_LEQUAL);

		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

		RendLev->PortalDepth--;
		glStencilFunc(GL_EQUAL, RendLev->PortalDepth, ~0);
		if (!RendLev->PortalDepth)
		{
			glDisable(GL_STENCIL_TEST);
		}
	}
	else
	{
		if (Portal->NeedsDepthBuffer())
		{
			//	Clear depth buffer
			glClear(GL_DEPTH_BUFFER_BIT);
		}
		else
		{
			glDepthMask(GL_TRUE);
			glEnable(GL_DEPTH_TEST);
		}

		//	Draw proper z-buffer for the portal area.
		DrawPortalArea(Portal);
	}

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	if (!HaveShaders)
	{
		glEnable(GL_TEXTURE_2D);
	}
	unguard;
}

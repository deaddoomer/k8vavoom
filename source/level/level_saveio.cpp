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
//**  Copyright (C) 2018-2023 Ketmar Dark
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
#include "../server/sv_local.h"
#include "../psim/p_decal.h"
#include "../psim/p_entity.h"
#ifdef CLIENT
# include "../automap.h"
#endif
#include "../utils/ntvalueioex.h"


//==========================================================================
//
//  DecalIO
//
//==========================================================================
static void DecalIO (VStream &Strm, decal_t *dc, VLevel *level, bool mustBeFlatDecal=false) {
  if (!dc) return;
  {
    VNTValueIOEx vio(&Strm);
    //if (!vio.IsLoading()) GCon->Logf("SAVE: texture: id=%d; name=<%s>", dc->texture.id, *GTextureManager.GetTextureName(dc->texture));
    vio.io(VName("texture"), dc->texture);
    // decal prototyps
    VName decname = (dc->proto ? dc->proto->name : NAME_None);
    vio.iodef(VName("proto"), decname, NAME_None);
    if (Strm.IsLoading()) dc->proto = VDecalDef::find(decname);
    //vio.io(VName("dectype"), dc->dectype);
    vio.io(VName("translation"), dc->translation);
    if (Strm.IsLoading()) {
      vint32 hsc = 0;
      vio.iodef(VName("hasShadeClr"), hsc, 0); // special value
      if (hsc == 1) {
        vio.io(VName("shadeclr"), dc->shadeclr); // special value
        vio.iodef(VName("origshadeclr"), dc->origshadeclr, -3); // special value
        if (dc->origshadeclr == -3) dc->origshadeclr = dc->shadeclr;
      } else {
        dc->shadeclr = dc->origshadeclr = (dc->proto ? dc->proto->shadeclr : -1);
      }
    } else {
      // saving
      vint32 hsc = 1;
      vio.io(VName("hasShadeClr"), hsc); // special value
      vio.io(VName("shadeclr"), dc->shadeclr); // special value
      vio.io(VName("origshadeclr"), dc->shadeclr); // special value
    }
    if (vio.IsError()) Host_Error("error reading decals");
    //if (vio.IsLoading()) GCon->Logf("LOAD: texture: id=%d; name=<%s>", dc->texture.id, *GTextureManager.GetTextureName(dc->texture));
    if (dc->texture <= 0) {
      GCon->Logf(NAME_Warning, "LOAD: decal of type '%s' has missing texture", *decname);
      dc->texture = 0;
    }
    vio.io(VName("flags"), dc->flags);
    vio.iodef(VName("angle"), dc->angle, 0.0f);
    vio.io(VName("orgz"), dc->orgz);
    vio.io(VName("curz"), dc->curz);
    vio.io(VName("xdist"), dc->xdist);
    vio.io(VName("ofsX"), dc->ofsX);
    vio.io(VName("ofsY"), dc->ofsY);
    vio.io(VName("origScaleX"), dc->origScaleX);
    vio.io(VName("origScaleY"), dc->origScaleY);
    vio.io(VName("scaleX"), dc->scaleX);
    vio.io(VName("scaleY"), dc->scaleY);
    vio.io(VName("origAlpha"), dc->origAlpha);
    vio.io(VName("alpha"), dc->alpha);
    //vuint32 additive = (dc->additive ? 1 : 0);
    //vio.iodef(VName("additive"), additive, 0u);
    //if (Strm.IsLoading()) dc->additive = (additive != 0);
    vio.iodef(VName("dcsurface"), dc->dcsurf, 0u);

    // boot params
    vuint32 hasBootParams = (dc->boottime > 0.0f ? 1u : 0u);
    vio.iodef(VName("hasBootParams"), hasBootParams, 0u);
    if (hasBootParams) {
      vio.io(VName("boottime"), dc->boottime);
      vio.iodef(VName("bootanimator"), dc->bootanimator, NAME_None);
      vio.iodef(VName("bootshade"), dc->bootshade, -2);
      vio.iodef(VName("boottranslation"), dc->boottranslation, -2);
      vio.iodef(VName("bootalpha"), dc->bootalpha, -1.0f);
    }

    // different code for wall/flat decals
    if (dc->dcsurf == 0) {
      if (mustBeFlatDecal) Host_Error("error in decal i/o");
      // wall decal
      vint32 slsec = (Strm.IsLoading() ? -666 : (dc->slidesec ? (int)(ptrdiff_t)(dc->slidesec-&level->Sectors[0]) : -1));
      vio.iodef(VName("slidesec"), slsec, -666);
      if (Strm.IsLoading()) {
        if (slsec == -666) {
          // fix backsector
          if ((dc->flags&(decal_t::SlideFloor|decal_t::SlideCeil)) && !dc->slidesec) {
            line_t *lin = dc->seg->linedef;
            if (!lin) Sys_Error("Save loader: invalid seg linedef (0)!");
            int bsidenum = (dc->flags&decal_t::SideDefOne ? 1 : 0);
            dc->slidesec = (bsidenum ? dc->seg->backsector : dc->seg->frontsector);
            GCon->Logf("Save loader: fixed backsector for decal");
          }
        } else {
          dc->slidesec = (slsec < 0 || slsec >= level->NumSectors ? nullptr : &level->Sectors[slsec]);
        }
      }
      if (vio.IsError()) Host_Error("error in decal i/o");
    } else {
      // floor/ceiling decal
      if (!mustBeFlatDecal) Host_Error("error in decal i/o");
      vio.iodef(VName("worldx"), dc->worldx, 0.0f);
      vio.iodef(VName("worldy"), dc->worldy, 0.0f);
      //vio.iodef(VName("height"), dc->height, 2.0f);
      if (!Strm.IsLoading() && !dc->sub) Host_Error("error in decal i/o");
      vint32 slsec = (Strm.IsLoading() ? -666 : (int)(ptrdiff_t)(dc->sub-&level->Subsectors[0]));
      vio.iodef(VName("ownersubsec"), slsec, -666);
      if (Strm.IsLoading()) {
        if (slsec < 0 || slsec >= level->NumSubsectors) Host_Error("error in decal i/o");
        dc->sub = &level->Subsectors[slsec];
      }
      vint32 eridx = (Strm.IsLoading() ? -666 : dc->eregindex);
      vio.iodef(VName("secregindex"), eridx, -666);
      if (eridx < 0) Host_Error("error in decal i/o");
      if (Strm.IsLoading()) {
        dc->eregindex = eridx;
      }
    }
  }
  VDecalAnim::Serialise(Strm, dc->animator);
}


//==========================================================================
//
//  writeOrCheckUInt
//
//==========================================================================
static bool writeOrCheckUInt (VStream &Strm, vuint32 value, const char *errmsg=nullptr) {
  if (Strm.IsLoading()) {
    vuint32 v;
    Strm << v;
    if (v != value || Strm.IsError()) {
      if (errmsg) Host_Error("Save loader: invalid value for %s; got 0x%08x, but expected 0x%08x", errmsg, v, value);
      return false;
    }
  } else {
    Strm << value;
  }
  return !Strm.IsError();
}


#define EXTSAVE_NUMSEC_MAGIC0 (-0x7fefefea)
#define EXTSAVE_NUMSEC_MAGIC1 (-0x7fefefeb)


//==========================================================================
//
//  VLevel::SerialiseOther
//
//  FIXME: add error checking for sections!
//
//==========================================================================
void VLevel::SerialiseOther (VStream &Strm) {
  int i;
  sector_t *sec;
  line_t *li;
  side_t *si;

  // do not save stale map marks
  #ifdef CLIENT
  AM_ClearMarksIfMapChanged(this);
  #endif

  if (Strm.IsLoading()) {
    KillAllMapDecals();
    // reset subsector update frame and "seen" flag
    for (auto &&sub : allSubsectors()) {
      sub.updateWorldFrame = 0;
      sub.miscFlags &= ~subsector_t::SSMF_Rendered;
    }
    // clear seen segs on loading
    for (auto &&seg : allSegs()) seg.flags &= ~SF_MAPPED;
  }

  // write/check various numbers, so we won't load invalid save accidentally
  // this is not the best or most reliable way to check it, but it is better
  // than nothing...

  writeOrCheckUInt(Strm, LSSHash, "geometry hash");
  bool segsHashOK = writeOrCheckUInt(Strm, SegHash);

  bool doDecals = segsHashOK;
  if (doDecals && Strm.IsExtendedFormat()) {
    if (Strm.IsLoading()) {
      doDecals = Strm.HasExtendedSection("vlevel/decals.dat");
    } else {
      // if we don't have decals, don't write them
      doDecals = false;
      for (int f = 0; !doDecals && f < (int)NumSegs; ++f) {
        doDecals = !!Segs[f].decalhead;
      }
    }
  }

  // decals
  if (doDecals) {
    Strm.OpenExtendedSection("vlevel/decals.dat", VStream::NonSeekable);

    vuint32 dctotal = 0;
    if (Strm.IsLoading()) {
      // load decals
      vint32 dcSize = 0;
      Strm << dcSize;
      int stpos = Strm.Tell();
      // load decals
      for (int f = 0; f < (int)NumSegs; ++f) {
        vuint32 dcount = 0;
        //Segs[f].killAllDecals(); // no need to
        vassert(!Segs[f].decalhead);
        // load decal count for this seg
        Strm << dcount;
        // hack to not break old saves
        if (dcount == 0xffffffffu) {
          Strm << dcount;
          //decal_t *decal = nullptr; // previous
          while (dcount-- > 0) {
            decal_t *dc = new decal_t;
            memset((void *)dc, 0, sizeof(decal_t));
            dc->seg = &Segs[f];
            DecalIO(Strm, dc, this);
            if (dc->texture <= 0) {
              delete dc->animator;
              delete dc;
            } else {
              // add to decal list
              dc->seg = nullptr;
              AddAnimatedDecal(dc);
              dc->calculateBBox();
              Segs[f].appendDecal(dc);
            }
            ++dctotal;
          }
        } else {
          // oops, non-zero count, old decal data, skip it
          GCon->Logf("skipping old decal data, cannot load it (this is harmless)");
          if (!Strm.IsExtendedFormat()) {
            Strm.Seek(stpos+dcSize);
          }
        }
      }
      GCon->Logf("%u decals loaded", dctotal);
    } else {
      // save decals
      vint32 dcSize = -1; // will be fixed later
      int dcStartPos = Strm.Tell();
      Strm << dcSize; // will be fixed later
      for (int f = 0; f < (int)NumSegs; ++f) {
        // count decals
        vuint32 dcount = 0;
        for (decal_t *decal = Segs[f].decalhead; decal; decal = decal->next) ++dcount;
        vuint32 newmark = 0xffffffffu;
        Strm << newmark;
        Strm << dcount;
        for (decal_t *decal = Segs[f].decalhead; decal; decal = decal->next) {
          DecalIO(Strm, decal, this);
          ++dctotal;
        }
      }
      if (!Strm.IsExtendedFormat()) {
        auto currPos = Strm.Tell();
        Strm.Seek(dcStartPos);
        dcSize = currPos-(dcStartPos+4);
        Strm << dcSize;
        Strm.Seek(currPos);
      }
      GCon->Logf("%u decals saved", dctotal);
    }

    Strm.CloseExtendedSection();
  } else {
    // skip decals
    if (!Strm.IsExtendedFormat()) {
      vint32 dcSize = 0;
      Strm << dcSize;
      if (Strm.IsLoading()) {
        if (dcSize < 0) Host_Error("decals section is broken");
        Strm.Seek(Strm.Tell()+dcSize);
      }
      if (dcSize) {
        GCon->Logf("seg hash doesn't match (this is harmless, but you lost decals)");
      } else {
        GCon->Logf("seg hash doesn't match (this is harmless)");
      }
    }
  }

  // hack, to keep compatibility
  bool extSaveVer = !Strm.IsLoading();
  bool hasSectorDecals = !Strm.IsLoading();

  // write "extended save" flag
  if (Strm.IsExtendedFormat()) {
    // extended format
    if (!Strm.IsLoading()) {
      hasSectorDecals = !!subdecalhead;
    } else {
      hasSectorDecals = Strm.HasExtendedSection("vlevel/flat_decals.dat");
    }
    extSaveVer = true;
  } else {
    // write flag
    if (!Strm.IsLoading()) {
      vint32 scflag = EXTSAVE_NUMSEC_MAGIC1;
      Strm << STRM_INDEX(scflag);
    }
  }

  // sectors
  {
    Strm.OpenExtendedSection("vlevel/sectors.dat", VStream::NonSeekable);

    vint32 cnt = NumSectors;
    Strm << STRM_INDEX(cnt);

    // check "extended save" magic
    if (Strm.IsLoading() && !Strm.IsExtendedFormat()) {
      if (cnt == EXTSAVE_NUMSEC_MAGIC0 || cnt == EXTSAVE_NUMSEC_MAGIC1) {
        hasSectorDecals = (cnt == EXTSAVE_NUMSEC_MAGIC1);
        extSaveVer = true;
        Strm << STRM_INDEX(cnt);
      }
    } else {
      extSaveVer = true;
    }
    if (cnt != NumSectors) Host_Error("invalid number of sectors (%d vs %d)", cnt, NumSectors);

    for (i = 0, sec = Sectors; i < NumSectors; ++i, ++sec) {
      VNTValueIOEx vio(&Strm);
      vio.io(VName("floor.dist"), sec->floor.dist);
      vio.io(VName("floor.TexZ"), sec->floor.TexZ);
      vio.io(VName("floor.pic"), sec->floor.pic);
      vio.io(VName("floor.xoffs"), sec->floor.xoffs);
      vio.io(VName("floor.yoffs"), sec->floor.yoffs);
      vio.io(VName("floor.XScale"), sec->floor.XScale);
      vio.io(VName("floor.YScale"), sec->floor.YScale);
      vio.io(VName("floor.Angle"), sec->floor.Angle);
      vio.io(VName("floor.BaseAngle"), sec->floor.BaseAngle);
      vio.iodef(VName("floor.BaseXOffs"), sec->floor.BaseXOffs, 0.0f);
      vio.io(VName("floor.BaseYOffs"), sec->floor.BaseYOffs);
      vio.iodef(VName("floor.PObjCX"), sec->floor.PObjCX, 0.0f);
      vio.iodef(VName("floor.PObjCY"), sec->floor.PObjCY, 0.0f);
      vio.io(VName("floor.flags"), sec->floor.flags);
      vio.iodef(VName("floor.flipFlags"), sec->floor.flipFlags, 0u);
      vio.io(VName("floor.Alpha"), sec->floor.Alpha);
      vio.io(VName("floor.MirrorAlpha"), sec->floor.MirrorAlpha);
      vio.io(VName("floor.LightSourceSector"), sec->floor.LightSourceSector);
      vio.io(VName("floor.SkyBox"), sec->floor.SkyBox);
      vio.io(VName("ceiling.dist"), sec->ceiling.dist);
      vio.io(VName("ceiling.TexZ"), sec->ceiling.TexZ);
      vio.io(VName("ceiling.pic"), sec->ceiling.pic);
      vio.io(VName("ceiling.xoffs"), sec->ceiling.xoffs);
      vio.io(VName("ceiling.yoffs"), sec->ceiling.yoffs);
      vio.io(VName("ceiling.XScale"), sec->ceiling.XScale);
      vio.io(VName("ceiling.YScale"), sec->ceiling.YScale);
      vio.io(VName("ceiling.Angle"), sec->ceiling.Angle);
      vio.io(VName("ceiling.BaseAngle"), sec->ceiling.BaseAngle);
      vio.iodef(VName("ceiling.BaseXOffs"), sec->ceiling.BaseXOffs, 0.0f);
      vio.io(VName("ceiling.BaseYOffs"), sec->ceiling.BaseYOffs);
      vio.iodef(VName("ceiling.PObjCX"), sec->ceiling.PObjCX, 0.0f);
      vio.iodef(VName("ceiling.PObjCY"), sec->ceiling.PObjCY, 0.0f);
      vio.io(VName("ceiling.flags"), sec->ceiling.flags);
      vio.iodef(VName("ceiling.flipFlags"), sec->ceiling.flipFlags, 0u);
      vio.io(VName("ceiling.Alpha"), sec->ceiling.Alpha);
      vio.io(VName("ceiling.MirrorAlpha"), sec->ceiling.MirrorAlpha);
      vio.io(VName("ceiling.LightSourceSector"), sec->ceiling.LightSourceSector);
      vio.io(VName("ceiling.SkyBox"), sec->ceiling.SkyBox);
      vio.io(VName("params.lightlevel"), sec->params.lightlevel);
      vio.io(VName("params.LightColor"), sec->params.LightColor);
      vio.io(VName("params.Fade"), sec->params.Fade);
      vio.io(VName("params.contents"), sec->params.contents);
      vio.io(VName("special"), sec->special);
      vio.io(VName("tag"), sec->sectorTag);
      vio.io(VName("seqType"), sec->seqType);
      const vuint32 origSectorFlags = sec->SectorFlags;
      vio.io(VName("SectorFlags"), sec->SectorFlags);
      vio.io(VName("SoundTarget"), sec->SoundTarget);
      vio.io(VName("FloorData"), sec->FloorData);
      vio.io(VName("CeilingData"), sec->CeilingData);
      vio.io(VName("LightingData"), sec->LightingData);
      vio.io(VName("AffectorData"), sec->AffectorData);
      vio.io(VName("ActionList"), sec->ActionList);
      vio.io(VName("Damage"), sec->Damage);
      vio.io(VName("DamageType"), sec->DamageType);
      vio.io(VName("DamageInterval"), sec->DamageInterval);
      vio.io(VName("DamageLeaky"), sec->DamageLeaky);
      vio.io(VName("Friction"), sec->Friction);
      vio.io(VName("MoveFactor"), sec->MoveFactor);
      vio.io(VName("Gravity"), sec->Gravity);
      vio.io(VName("Sky"), sec->Sky);
      if (Strm.IsLoading()) {
        // fix flags
        const vuint32 xmask = sector_t::SF_IsTransDoor|sector_t::SF_Hidden|sector_t::SF_IsTransDoorTop|sector_t::SF_IsTransDoorBot|sector_t::SF_IsTransDoorTrack;
        sec->SectorFlags = (sec->SectorFlags&~xmask)|(origSectorFlags&xmask);
        // load additional tags
        sec->moreTags.clear();
        int moreTagCount = 0;
        vio.iodef(VName("moreTagCount"), moreTagCount, 0);
        if (moreTagCount < 0 || moreTagCount > 32767) Host_Error("invalid lindef");
        char tmpbuf[64];
        for (int mtf = 0; mtf < moreTagCount; ++mtf) {
          snprintf(tmpbuf, sizeof(tmpbuf), "moreTag%d", mtf);
          int mtag = 0;
          vio.io(VName(tmpbuf), mtag);
          if (!mtag || mtag == -1) continue;
          bool found = false;
          for (int cc = 0; cc < sec->moreTags.length(); ++cc) if (sec->moreTags[cc] == mtag) { found = true; break; }
          if (found) continue;
          sec->moreTags.append(mtag);
        }
        // setup sector bounds
        CalcSecMinMaxs(sec);
        sec->ThingList = nullptr;
      } else {
        // save more tags
        int moreTagCount = sec->moreTags.length();
        vio.io(VName("moreTagCount"), moreTagCount);
        char tmpbuf[64];
        for (int mtf = 0; mtf < moreTagCount; ++mtf) {
          snprintf(tmpbuf, sizeof(tmpbuf), "moreTag%d", mtf);
          int mtag = sec->moreTags[mtf];
          vio.io(VName(tmpbuf), mtag);
        }
      }
    }
    if (Strm.IsLoading()) HashSectors();

    Strm.CloseExtendedSection();
  }

  // extended info section
  vint32 hasSegVisibility = 1;
  vint32 hasAutomapMarks =
    #ifdef CLIENT
      1;
    #else
      0;
    #endif

  if (!Strm.IsExtendedFormat()) {
    if (extSaveVer) {
      VNTValueIOEx vio(&Strm);
      vio.io(VName("extflags.hassegvis"), hasSegVisibility);
      vio.io(VName("extflags.hasmapmarks"), hasAutomapMarks);
    } else {
      hasSegVisibility = 0;
      hasAutomapMarks = 0;
    }
  } else {
    if (Strm.IsLoading()) {
      hasSegVisibility = Strm.HasExtendedSection("vlevel/seg_visibility.dat");
      #ifdef CLIENT
      hasAutomapMarks = Strm.HasExtendedSection("vlevel/automap_marks.dat");
      #endif
    } else {
      // do not save automap marks if there are none
      #ifdef CLIENT
      const int mmcount = AM_GetMaxMarks();
      hasAutomapMarks = 0;
      for (int markidx = 0; !hasAutomapMarks && markidx < mmcount; ++markidx) {
        hasAutomapMarks = (AM_IsMarkActive(markidx) ? 1 : 0);
      }
      #endif
    }
  }

  // seg visibility
  bool segvisLoaded = false;
  if (hasSegVisibility && (segsHashOK || !Strm.IsExtendedFormat())) {
    Strm.OpenExtendedSection("vlevel/seg_visibility.dat", VStream::NonSeekable);

    //if (Strm.IsLoading()) GCon->Log("loading seg mapping");
    vint32 dcSize = -1;
    int dcStartPos = Strm.Tell();
    if (segsHashOK) {
      Strm << dcSize; // will be fixed later for writer
      vint32 segCount = NumSegs;
      Strm << segCount;
      if (segCount == NumSegs && segCount > 0) {
        seg_t *seg = &Segs[0];
        for (i = NumSegs; i--; ++seg) {
          VNTValueIOEx vio(&Strm);
          vio.io(VName("seg.flags"), seg->flags);
          // recheck linedef if we have some mapped segs on it
          if (seg->linedef && (seg->flags&SF_MAPPED)) seg->linedef->exFlags |= (ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED);
        }
        segvisLoaded = !Strm.IsError();
        // fix size, if necessary
        if (!Strm.IsExtendedFormat() && !Strm.IsLoading()) {
          auto currPos = Strm.Tell();
          Strm.Seek(dcStartPos);
          dcSize = currPos-(dcStartPos+4);
          Strm << dcSize;
          Strm.Seek(currPos);
        }
      } else {
        vassert(Strm.IsLoading());
        if (!Strm.IsExtendedFormat()) {
          if (dcSize < 0) Host_Error("invalid segmap size");
          GCon->Logf("segcount doesn't match for seg mapping (this is harmless)");
          Strm.Seek(dcStartPos+4+dcSize);
        }
      }
    } else {
      if (!Strm.IsExtendedFormat()) {
        vassert(Strm.IsLoading());
        Strm << dcSize;
        if (dcSize < 0) Host_Error("invalid segmap size");
        GCon->Logf("seg hash doesn't match for seg mapping (this is harmless)");
        Strm.Seek(dcStartPos+4+dcSize);
      }
    }

    Strm.CloseExtendedSection();
  }

  // automap marks
  if (hasAutomapMarks) {
    int number =
    #ifdef CLIENT
      AM_GetMaxMarks();
    #else
      0;
    #endif
    Strm.OpenExtendedSection("vlevel/automap_marks.dat", VStream::NonSeekable);

    Strm << STRM_INDEX(number);
    if (number < 0 || number > 1024) Host_Error("invalid automap marks data");
    //GCon->Logf(NAME_Debug, "marks: %d", number);
    if (Strm.IsLoading()) {
      // load automap marks
      #ifdef CLIENT
      AM_ClearMarks();
      #endif
      for (int markidx = 0; markidx < number; ++markidx) {
        VNTValueIOEx vio(&Strm);
        float x = 0, y = 0;
        vint32 active = 0;
        vio.io(VName("mark.active"), active);
        vio.io(VName("mark.x"), x);
        vio.io(VName("mark.y"), y);
        // do not replace user marks
        #ifdef CLIENT
        //GCon->Logf(NAME_Debug, "  idx=%d; pos=(%g,%g); active=%d", markidx, x, y, active);
        if (active && !AM_IsMarkActive(markidx) && isFiniteF(x)) {
          //GCon->Logf(NAME_Debug, "   set!");
          AM_SetMarkXY(markidx, x, y);
        }
        #endif
      }
    } else {
      // save automap marks
      #ifdef CLIENT
      for (int markidx = 0; markidx < number; ++markidx) {
        VNTValueIOEx vio(&Strm);
        float x = AM_GetMarkX(markidx), y = AM_GetMarkY(markidx);
        vint32 active = (AM_IsMarkActive(markidx) ? 1 : 0);
        vio.io(VName("mark.active"), active);
        vio.io(VName("mark.x"), x);
        vio.io(VName("mark.y"), y);
      }
      #else
      vassert(number == 0);
      #endif
    }

    Strm.CloseExtendedSection();
  } else {
    #ifdef CLIENT
    if (Strm.IsLoading()) AM_ClearMarks();
    #endif
  }

  // subsector decals
  if (hasSectorDecals) {
    Strm.OpenExtendedSection("vlevel/flat_decals.dat", VStream::NonSeekable);

    vint32 sscount = (Strm.IsLoading() ? 0 : NumSubsectors);
    Strm << STRM_INDEX(sscount);
    vint32 sdctotal = 0;
    if (Strm.IsLoading()) {
      vint32 ssdatasize = -1;
      Strm << ssdatasize;
      auto stpos = Strm.Tell();
      if (sscount != NumSubsectors) {
        // skip it
        GCon->Log("skipping old subsector decal data, cannot load it (this is harmless)");
      } else {
        // load number of decals
        Strm << STRM_INDEX(sdctotal);
        // load sector decals
        for (int f = sdctotal; f > 0; f--) {
          decal_t *dc = new decal_t;
          memset((void *)dc, 0, sizeof(decal_t));
          DecalIO(Strm, dc, this, true);
          if (dc->texture <= 0) {
            delete dc->animator;
            delete dc;
          } else {
            // add to decal list
            AppendDecalToSubsectorList(dc);
          }
        }
        GCon->Logf("%d subsector decals loaded", sdctotal);
      }
      // just in case
      if (!Strm.IsExtendedFormat()) {
        Strm.Seek(stpos+ssdatasize);
      }
    } else {
      // save decals
      vint32 ssdatasize = -1;
      int ssdpos = Strm.Tell();
      Strm << ssdatasize; // will be fixed later
      // count number of decals
      for (const decal_t *dc = subdecalhead; dc; dc = dc->next) ++sdctotal;
      // save number of decals
      Strm << STRM_INDEX(sdctotal);
      // save sector decals
      for (decal_t *dc = subdecalhead; dc; dc = dc->next) {
        DecalIO(Strm, dc, this, true);
      }
      if (!Strm.IsExtendedFormat()) {
        auto currPos = Strm.Tell();
        Strm.Seek(ssdpos);
        ssdatasize = currPos-(ssdpos+4);
        Strm << ssdatasize;
        Strm.Seek(currPos);
      }
      GCon->Logf("%d subsector decals saved", sdctotal);
    }

    Strm.CloseExtendedSection();
  }

  // lines
  {
    Strm.OpenExtendedSection("vlevel/lines.dat", VStream::NonSeekable);

    vint32 cnt = NumLines;
    Strm << STRM_INDEX(cnt);
    if (Strm.IsLoading()) {
      if (cnt != NumLines) Host_Error("invalid number of linedefs");
    }

    for (i = 0, li = Lines; i < NumLines; ++i, ++li) {
      {
        VNTValueIOEx vio(&Strm);
        vio.io(VName("flags"), li->flags);
        vio.io(VName("SpacFlags"), li->SpacFlags);
        //vio.iodef(VName("exFlags"), li->exFlags, 0);
        vio.io(VName("exFlags"), li->exFlags);
        vio.io(VName("special"), li->special);
        vio.io(VName("arg1"), li->arg1);
        // UDMF arg0str hack
        if (UDMFNamespace != NAME_None && ((li->special >= 80 && li->special <= 85) || li->special == 226)) {
          if (Strm.IsLoading()) {
            // loading
            if (li->arg1 < 0) {
              VName n = NAME_None;
              vio.io(VName("arg1str"), n);
              //GCon->Logf(NAME_Debug, "*** translated arg1str from %d to %d (%s)", li->arg1, -n.GetIndex(), *n);
              li->arg1 = -n.GetIndex();
            }
          } else {
            // saving
            if (li->arg1 < 0) {
              // negative means "string" (name)
              VName n = VName::CreateWithIndex(-li->arg1);
              if (n.isValid()) vio.io(VName("arg1str"), n);
            }
          }
        }
        vio.io(VName("arg2"), li->arg2);
        vio.io(VName("arg3"), li->arg3);
        vio.io(VName("arg4"), li->arg4);
        vio.io(VName("arg5"), li->arg5);
        vio.io(VName("LineTag"), li->lineTag);
        vio.io(VName("alpha"), li->alpha);
        vio.iodef(VName("locknumber"), li->locknumber, 0);
        if (Strm.IsLoading()) {
          // load additional tags
          li->moreTags.clear();
          int moreTagCount = 0;
          vio.iodef(VName("moreTagCount"), moreTagCount, 0);
          if (moreTagCount < 0 || moreTagCount > 32767) Host_Error("invalid lindef");
          char tmpbuf[64];
          for (int mtf = 0; mtf < moreTagCount; ++mtf) {
            snprintf(tmpbuf, sizeof(tmpbuf), "moreTag%d", mtf);
            int mtag = 0;
            vio.io(VName(tmpbuf), mtag);
            if (!mtag || mtag == -1) continue;
            bool found = false;
            for (int cc = 0; cc < li->moreTags.length(); ++cc) if (li->moreTags[cc] == mtag) { found = true; break; }
            if (found) continue;
            li->moreTags.append(mtag);
          }
          // mark partially mapped lines as fully mapped if segvis map is not loaded
          if (!segvisLoaded) {
            if (li->exFlags&(ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED)) {
              li->flags |= ML_MAPPED;
            }
            li->exFlags &= ~(ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED);
          }
        } else {
          // save more tags
          int moreTagCount = li->moreTags.length();
          vio.io(VName("moreTagCount"), moreTagCount);
          char tmpbuf[64];
          for (int mtf = 0; mtf < moreTagCount; ++mtf) {
            snprintf(tmpbuf, sizeof(tmpbuf), "moreTag%d", mtf);
            int mtag = li->moreTags[mtf];
            vio.io(VName(tmpbuf), mtag);
          }
        }
      }

      for (int j = 0; j < 2; ++j) {
        VNTValueIOEx vio(&Strm);
        if (li->sidenum[j] == -1) {
          // do nothing
        } else {
          si = &Sides[li->sidenum[j]];
          vint32 lnum = si->LineNum;
          vio.io(VName("LineNum"), lnum);
          if (lnum != si->LineNum) Host_Error("invalid sidedef");
          vio.io(VName("TopTexture"), si->TopTexture);
          vio.io(VName("BottomTexture"), si->BottomTexture);
          vio.io(VName("MidTexture"), si->MidTexture);
          vio.io(VName("TopTextureOffset"), si->Top.TextureOffset);
          vio.io(VName("BotTextureOffset"), si->Bot.TextureOffset);
          vio.io(VName("MidTextureOffset"), si->Mid.TextureOffset);
          vio.io(VName("TopRowOffset"), si->Top.RowOffset);
          vio.io(VName("BotRowOffset"), si->Bot.RowOffset);
          vio.io(VName("MidRowOffset"), si->Mid.RowOffset);
          vio.io(VName("Flags"), si->Flags);
          vio.io(VName("Light"), si->Light);
          /*k8: no need to save scaling, as it cannot be changed by ACS/decorate.
                note that VC code can change it.
                do this with flags to not break old saves. */
          vint32 scales = 0;
          if (!Strm.IsLoading()) {
            if (si->Top.ScaleX != 1.0f) scales |= 0x01;
            if (si->Top.ScaleY != 1.0f) scales |= 0x02;
            if (si->Bot.ScaleX != 1.0f) scales |= 0x04;
            if (si->Bot.ScaleY != 1.0f) scales |= 0x08;
            if (si->Mid.ScaleX != 1.0f) scales |= 0x10;
            if (si->Mid.ScaleY != 1.0f) scales |= 0x20;
          }
          vio.iodef(VName("Scales"), scales, 0);
          if (scales&0x01) vio.io(VName("TopScaleX"), si->Top.ScaleX);
          if (scales&0x02) vio.io(VName("TopScaleY"), si->Top.ScaleY);
          if (scales&0x04) vio.io(VName("BotScaleX"), si->Bot.ScaleX);
          if (scales&0x08) vio.io(VName("BotScaleY"), si->Bot.ScaleY);
          if (scales&0x10) vio.io(VName("MidScaleX"), si->Mid.ScaleX);
          if (scales&0x20) vio.io(VName("MidScaleY"), si->Mid.ScaleY);
          // flags
          vio.io(VName("TopFlags"), si->Top.Flags);
          vio.io(VName("MidFlags"), si->Mid.Flags);
          vio.io(VName("BotFlags"), si->Bot.Flags);
          // rotations
          vio.io(VName("TopBaseAngle"), si->Top.BaseAngle);
          vio.io(VName("TopAngle"), si->Top.Angle);
          vio.io(VName("MidBaseAngle"), si->Mid.BaseAngle);
          vio.io(VName("MidAngle"), si->Mid.Angle);
          vio.io(VName("BotBaseAngle"), si->Bot.BaseAngle);
          vio.io(VName("BotAngle"), si->Bot.Angle);
        }
      }
    }
    if (Strm.IsLoading()) HashLines();

    Strm.CloseExtendedSection();
  }

  // restore subsector "rendered" flag
  if (Strm.IsLoading()) {
    if (segvisLoaded) {
      // segment visibility info present
      for (i = 0; i < NumSegs; ++i) {
        if (Segs[i].frontsub && (Segs[i].flags&SF_MAPPED)) {
          Segs[i].frontsub->miscFlags |= subsector_t::SSMF_Rendered;
        }
      }
    } else {
      // no segment visibility info, do nothing
    }
  }

  // polyobjs
  bool doPolyObjs = true;
  if (Strm.IsExtendedFormat()) {
    if (Strm.IsLoading()) {
      doPolyObjs = Strm.HasExtendedSection("vlevel/polyobjs.dat");
    } else {
      // if we don't have polyobjects, don't write them
      doPolyObjs = (NumPolyObjs != 0);
    }
  }

  if (doPolyObjs) {
    Strm.OpenExtendedSection("vlevel/polyobjs.dat", VStream::NonSeekable);

    vint32 cnt = NumPolyObjs;
    Strm << STRM_INDEX(cnt);
    if (Strm.IsLoading()) {
      if (cnt != NumPolyObjs) Host_Error("invalid number of polyobjects");
    }

    for (i = 0; i < NumPolyObjs; ++i) {
      VNTValueIOEx vio(&Strm);
      polyobj_t *po = PolyObjs[i];
      float angle = po->angle;
      float polyX = po->startSpot.x;
      float polyY = po->startSpot.y;
      float polyZ = po->startSpot.z;
      vuint32 polyFlags = po->PolyFlags;
      vio.io(VName("angle"), angle);
      vio.io(VName("startSpot.x"), polyX);
      vio.io(VName("startSpot.y"), polyY);
      vio.iodef(VName("startSpot.z"), polyZ, 0.0f);
      vio.iodef(VName("flags"), polyFlags, 0xffffffffu);
      if (Strm.IsLoading()) {
        if (polyFlags != 0xffffffffu) po->PolyFlags = polyFlags;
        FixPolyobjCachedFlags(po);
        MovePolyobj(po->tag, polyX-po->startSpot.x, polyY-po->startSpot.y, polyZ-po->startSpot.z, POFLAG_FORCED|POFLAG_NOLINK);
        if (angle) RotatePolyobj(po->tag, angle, POFLAG_FORCED|POFLAG_NOLINK|POFLAG_INDROT);
      }
      vio.io(VName("SpecialData"), PolyObjs[i]->SpecialData);
    }

    Strm.CloseExtendedSection();
  }

  // static lights
  bool doStaticLights = true;
  if (Strm.IsExtendedFormat()) {
    if (Strm.IsLoading()) {
      doStaticLights = Strm.HasExtendedSection("vlevel/static_lights.dat");
    } else {
      // if we don't have static lights, don't write them
      doStaticLights = (StaticLights.length() != 0);
    }
  }

  if (doStaticLights) {
    Strm.OpenExtendedSection("vlevel/static_lights.dat", VStream::NonSeekable);

    TMapNC<vuint32, VEntity *> suidmap;
    vint32 slcount = StaticLights.length();

    Strm << STRM_INDEX(slcount);
    if (Strm.IsLoading()) {
      StaticLights.setLength(slcount);
      if (StaticLightsMap) StaticLightsMap->clear();
    } else {
      // build uid map
      for (TThinkerIterator<VEntity> ent(this); ent; ++ent) {
        if (!ent->ServerUId) GCon->Logf(NAME_Error, "entity '%s:%u' has no suid!", ent->GetClass()->GetName(), ent->GetUniqueId());
        suidmap.put(ent->ServerUId, *ent);
      }
    }

    int lidx = -1;
    for (auto &&sl : StaticLights) {
      ++lidx;
      VNTValueIOEx vio(&Strm);
      vio.io(VName("Origin"), sl.Origin);
      vio.io(VName("Radius"), sl.Radius);
      vio.io(VName("Color"), sl.Color);
      vint32 inactive = (Strm.IsLoading() ? 0 : (sl.Flags&rep_light_t::LightActive ? 0 : 1));
      vio.iodef(VName("Inactive"), inactive, 0);
      // owner
      if (Strm.IsLoading()) {
        VEntity *owner = nullptr;
        vio.io(VName("Owner"), owner);
        sl.OwnerUId = (owner ? owner->ServerUId : 0);
        if (sl.OwnerUId) {
          if (!StaticLightsMap) StaticLightsMap = new TMapNC<vuint32, int>();
          auto oidp = StaticLightsMap->get(sl.OwnerUId);
          if (oidp) StaticLights[*oidp].OwnerUId = 0; //FIXME
          StaticLightsMap->put(sl.OwnerUId, lidx);
        }
      } else {
        auto opp = suidmap.get(sl.OwnerUId);
        VEntity *owner = (opp ? *opp : nullptr);
        vio.io(VName("Owner"), owner);
      }
      vio.io(VName("ConeDir"), sl.ConeDir);
      vio.io(VName("ConeAngle"), sl.ConeAngle);
      vio.io(VName("Flags"), sl.Flags);
      if (Strm.IsLoading()) {
        if (inactive) {
          sl.Flags &= ~rep_light_t::LightActive;
        } else {
          sl.Flags |= rep_light_t::LightActive;
        }
        sl.Flags |= rep_light_t::LightChanged;
      }
    }

    Strm.CloseExtendedSection();
  } else {
    if (Strm.IsLoading()) {
      StaticLights.setLength(0);
      if (StaticLightsMap) StaticLightsMap->clear();
    }
  }

  // ACS: script thinkers must be serialized first
  // script thinkers
  bool doAcsThinkers = true;
  if (Strm.IsExtendedFormat()) {
    if (Strm.IsLoading()) {
      doAcsThinkers = Strm.HasExtendedSection("vlevel/acs_thinkers.dat");
    } else {
      // if we don't have ACS thinkers, don't write them
      doAcsThinkers = (scriptThinkers.length() != 0);
    }
  }

  if (doAcsThinkers) {
    Strm.OpenExtendedSection("vlevel/acs_thinkers.dat", VStream::NonSeekable);

    vuint8 xver = 1;
    Strm << xver;
    if (xver != 1) Host_Error("Save is broken (invalid scripts version %u)", (unsigned)xver);
    vint32 sthcount = scriptThinkers.length();
    Strm << STRM_INDEX(sthcount);
    if (sthcount < 0) Host_Error("Save is broken (invalid number of scripts)");
    if (Strm.IsLoading()) scriptThinkers.setLength(sthcount);
    //GCon->Logf("VLSR(%p): %d scripts", (void *)this, sthcount);
    for (int f = 0; f < sthcount; ++f) {
      VSerialisable *obj = scriptThinkers[f];
      Strm << obj;
      if (obj && obj->GetClassName() != "VAcs") {
        Host_Error("Save is broken (loaded `%s` instead of `VAcs`)", *obj->GetClassName());
      }
      //GCon->Logf("VLSR: script #%d: %p", f, (void *)obj);
      scriptThinkers[f] = (VLevelScriptThinker *)obj;
    }

    Strm.CloseExtendedSection();
  } else {
    if (Strm.IsLoading()) scriptThinkers.setLength(0);
  }

  // script manager
  {
    Strm.OpenExtendedSection("vlevel/acs_manager.dat", VStream::NonSeekable);

    vuint8 xver = 0;
    Strm << xver;
    if (xver != 0) Host_Error("Save is broken (invalid acs manager version %u)", (unsigned)xver);
    Acs->Serialise(Strm);

    Strm.CloseExtendedSection();
  }

  // camera textures
  bool doCameraTex = true;
  if (Strm.IsExtendedFormat()) {
    if (Strm.IsLoading()) {
      doCameraTex = Strm.HasExtendedSection("vlevel/camera_textures.dat");
    } else {
      // if we don't have camera textures, don't write them
      doCameraTex = (CameraTextures.length() != 0);
    }
  }

  if (doCameraTex) {
    Strm.OpenExtendedSection("vlevel/camera_textures.dat", VStream::NonSeekable);

    int NumCamTex = CameraTextures.length();
    Strm << STRM_INDEX(NumCamTex);
    if (Strm.IsLoading()) CameraTextures.setLength(NumCamTex);
    for (i = 0; i < NumCamTex; ++i) {
      VNTValueIOEx vio(&Strm);
      vio.io(VName("Camera"), CameraTextures[i].Camera);
      vio.io(VName("TexNum"), CameraTextures[i].TexNum);
      vio.io(VName("FOV"), CameraTextures[i].FOV);
    }

    Strm.CloseExtendedSection();
  } else {
    if (Strm.IsLoading()) CameraTextures.setLength(0);
  }

  // translation tables
  bool doTransTlbs = true;
  if (Strm.IsExtendedFormat()) {
    if (Strm.IsLoading()) {
      doTransTlbs = Strm.HasExtendedSection("vlevel/trans_tables.dat");
    } else {
      // if we don't have translation tables, don't write them
      doTransTlbs = (Translations.length() != 0);
    }
  }

  if (doTransTlbs) {
    Strm.OpenExtendedSection("vlevel/trans_tables.dat", VStream::NonSeekable);

    int NumTrans = Translations.length();
    Strm << STRM_INDEX(NumTrans);
    if (Strm.IsLoading()) Translations.setLength(NumTrans);
    for (i = 0; i < NumTrans; ++i) {
      vuint8 Present = !!Translations[i];
      {
        VNTValueIOEx vio(&Strm);
        vio.io(VName("Present"), Present);
      }
      if (Strm.IsLoading()) {
        if (Present) {
          Translations[i] = new VTextureTranslation;
        } else {
          Translations[i] = nullptr;
        }
      }
      if (Present) Translations[i]->Serialise(Strm);
    }

    Strm.CloseExtendedSection();
  } else {
    if (Strm.IsLoading()) Translations.setLength(0);
  }

  // body queue translation tables
  bool doBodyQTransTlbs = true;
  if (Strm.IsExtendedFormat()) {
    if (Strm.IsLoading()) {
      doBodyQTransTlbs = Strm.HasExtendedSection("vlevel/trans_tables_bodyq.dat");
    } else {
      // if we don't have body queue translation tables, don't write them
      doBodyQTransTlbs = (BodyQueueTrans.length() != 0);
    }
  }

  if (doBodyQTransTlbs) {
    Strm.OpenExtendedSection("vlevel/trans_tables_bodyq.dat", VStream::NonSeekable);

    int NumTrans = BodyQueueTrans.length();
    Strm << STRM_INDEX(NumTrans);
    if (Strm.IsLoading()) BodyQueueTrans.setLength(NumTrans);
    for (i = 0; i < NumTrans; ++i) {
      vuint8 Present = !!BodyQueueTrans[i];
      {
        VNTValueIOEx vio(&Strm);
        vio.io(VName("Present"), Present);
      }
      if (Strm.IsLoading()) {
        if (Present) {
          BodyQueueTrans[i] = new VTextureTranslation;
        } else {
          BodyQueueTrans[i] = nullptr;
        }
      }
      if (Present) BodyQueueTrans[i]->Serialise(Strm);
    }

    Strm.CloseExtendedSection();
  } else {
    if (Strm.IsLoading()) BodyQueueTrans.setLength(0);
  }

  // zones
  bool doZones = true;
  if (Strm.IsExtendedFormat()) {
    if (Strm.IsLoading()) {
      doZones = Strm.HasExtendedSection("vlevel/zones.dat");
    } else {
      // if we don't have zones, don't write them
      doZones = (NumZones != 0);
    }
  }

  if (doZones) {
    Strm.OpenExtendedSection("vlevel/zones.dat", VStream::NonSeekable);

    vint32 cnt = NumZones;
    Strm << STRM_INDEX(cnt);
    if (Strm.IsLoading()) {
      if (cnt != NumZones) Host_Error("invalid number of zones");
    }

    for (i = 0; i < NumZones; ++i) {
      VNTValueIOEx vio(&Strm);
      vio.io(VName("zoneid"), Zones[i]);
    }

    Strm.CloseExtendedSection();
  }
}

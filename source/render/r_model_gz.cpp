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
//**  Copyright (C) 2018-2022 Ketmar Dark
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
#include "r_local.h"
#include "r_local_gz.h"


//==========================================================================
//
//  GZModelDef::GZModelDef
//
//==========================================================================
GZModelDef::GZModelDef ()
  : className()
  , models()
  //, scale(1, 1, 1)
  //, offset(0, 0, 0)
  //, zoffset(0)
  //, mat(VMatrix4::Identity)
  , rotationSpeed(0)
  , usePitch(0)
  , usePitchInverted(false)
  , useRoll(0)
  , angleOffset(0, 0, 0)
  , frames()
{
}


//==========================================================================
//
//  GZModelDef::~GZModelDef
//
//==========================================================================
GZModelDef::~GZModelDef () {
  clear();
}


//==========================================================================
//
//  GZModelDef::clear
//
//==========================================================================
void GZModelDef::clear () {
  className.clear();
  models.clear();
  //scale = TVec(1, 1, 1);
  //offset = TVec(0, 0, 0);
  //zoffset = 0;
  //mat.SetIdentity();
  rotationSpeed = 0;
  usePitch = 0;
  usePitchInverted = false;
  useRoll = false;
  angleOffset = TAVec(0.0f, 0.0f, 0.0f);
  frames.clear();
}


//==========================================================================
//
//  GZModelDef::ParseMD2Frames
//
//==========================================================================
bool GZModelDef::ParseMD2Frames (VStr /*mdpath*/, TArray<VStr> &/*names*/) {
  return false;
}


//==========================================================================
//
//  GZModelDef::IsModelFileExists
//
//==========================================================================
bool GZModelDef::IsModelFileExists (VStr /*mdpath*/) {
  return true;
}


//==========================================================================
//
//  GZModelDef::buildPath
//
//==========================================================================
VStr GZModelDef::buildPath (VScriptParser *sc, VStr path) {
  // normalize path
  if (path.length()) {
    TArray<VStr> parr;
    path.fixSlashes().SplitPath(parr);
    if (VStr::IsSplittedPathAbsolute(parr)) parr.removeAt(0);
    int pidx = 0;
    while (pidx < parr.length()) {
      if (parr[pidx] == ".") {
        parr.removeAt(pidx);
        continue;
      }
      if (parr[pidx] == "..") {
        parr.removeAt(pidx);
        if (pidx > 0) {
          --pidx;
          parr.removeAt(pidx);
        }
        continue;
      }
      bool allDots = true;
      for (const char *s = parr[pidx].getCStr(); *s; ++s) if (*s != '.') { allDots = false; break; }
      if (allDots) sc->Error(va("invalid model path '%s' in model '%s'", *parr[pidx], *className));
      ++pidx;
    }
    path.clear();
    for (auto &&s : parr) {
      path += s.toLowerCase();
      path += '/';
    }
    //GLog.WriteLine("<%s>", *path);
  }
  return path;
}


//==========================================================================
//
//  sanitiseScale
//
//==========================================================================
static TVec sanitiseScale (const TVec &scale) {
  TVec res = scale;
  if (!isFiniteF(res.x) || !res.x) res.x = 1.0f;
  if (!isFiniteF(res.y) || !res.y) res.y = 1.0f;
  if (!isFiniteF(res.z) || !res.z) res.z = 1.0f;
  // usually, scale "-1" is for GZDoom HUD weapons. wtf?!
  // it seems, that this is *required* in GZDoom for some reason
  res.x = fabsf(res.x);
  res.y = fabsf(res.y);
  res.z = fabsf(res.z);
  return res;
}


//==========================================================================
//
//  buildMat
//
//==========================================================================
static void buildMat (VMatrix4 &mat, TVec scale, TVec offset, TVec rotCenter, bool hud) {
  (void)rotCenter;
  mat.SetIdentity();
  // hack for QStuffUltra
  if (!hud) {
    if (scale.z > 1.0f && offset.z >= 24.0f) offset.z += 4.0f*scale.z;
  }
  offset.x /= scale.x;
  offset.y /= scale.y;
  offset.z /= scale.z;
  mat *= VMatrix4::BuildScale(scale);
  mat *= VMatrix4::BuildOffset(offset);
}


//==========================================================================
//
//  GZModelDef::parse
//
//==========================================================================
void GZModelDef::parse (VScriptParser *sc) {
  clear();
  VStr path;
  // get class name
  sc->ExpectString();
  className = sc->String;
  sc->Expect("{");
  bool rotating = false;
  bool hudscale = false;
  TVec scale = TVec(1.0f, 1.0f, 1.0f);
  TVec offset = TVec::ZeroVector;
  TVec rotCenter = TVec::ZeroVector;
  while (!sc->Check("}")) {
    // skip flags
    if (sc->Check("IGNORETRANSLATION") ||
        sc->Check("INTERPOLATEDOUBLEDFRAMES") ||
        sc->Check("NOINTERPOLATION") ||
        sc->Check("DONTCULLBACKFACES") ||
        sc->Check("USEROTATIONCENTER"))
    {
      sc->Message(va("modeldef flag '%s' is not supported yet in model '%s'", *sc->String, *className));
      continue;
    }
    // "rotating"
    if (sc->Check("rotating")) {
      //if (rotationSpeed == 0) rotationSpeed = 8; // arbitrary value, ignored for now
      rotating = true;
      continue;
    }
    // "rotation-speed"
    if (sc->Check("rotation-speed")) {
      sc->ExpectFloatWithSign();
      rotationSpeed = sc->Float;
      continue;
    }
    // "rotation-vector"
    if (sc->Check("rotation-vector")) {
      sc->Message(va("modeldef command '%s' is not supported yet in model '%s'", *sc->String, *className));
      sc->ExpectFloatWithSign();
      sc->ExpectFloatWithSign();
      sc->ExpectFloatWithSign();
      continue;
    }
    // "path"
    if (sc->Check("path")) {
      sc->ExpectString();
      path = buildPath(sc, sc->String);
      continue;
    }
    // "skin"
    if (sc->Check("skin")) {
      sc->ExpectNumber();
      int skidx = sc->Number;
      if (skidx < 0 || skidx > 1024) sc->Error(va("invalid skin number (%d) in model '%s'", skidx, *className));
      sc->ExpectString();
      while (models.length() <= skidx) models.alloc();
      VStr xpath = (!sc->String.isEmpty() ? path+sc->String.toLowerCase() : VStr::EmptyString);
      if (!models[skidx].skinFile.isEmpty()) sc->Message(va("model #%d for class '%s' redefined skin", skidx, *className));
      models[skidx].skinFile = xpath;
      continue;
    }
    // "SurfaceSkin"
    if (sc->Check("SurfaceSkin")) {
      // SurfaceSkin model-index surface-index skin-file
      /*
      sc->Message(va("modeldef command '%s' is not supported yet in model '%s'", *sc->String, *className));
      sc->ExpectNumber();
      sc->ExpectNumber();
      sc->ExpectString();
      */
      // model index
      sc->ExpectNumber();
      int skidx = sc->Number;
      if (skidx < 0 || skidx > 1024) sc->Error(va("invalid skin number (%d) in model '%s'", skidx, *className));
      // submodel index
      sc->ExpectNumber();
      int smidx = sc->Number;
      if (skidx < 0 || skidx > 1024) sc->Error(va("invalid skin submodel number (%d) in model '%s'", smidx, *className));
      // skin file name
      sc->ExpectString();
      while (models.length() <= skidx) models.alloc();
      VStr xpath = (!sc->String.isEmpty() ? path+sc->String.toLowerCase() : VStr::EmptyString);
      while (models[skidx].subskinFiles.length() <= smidx) models[skidx].subskinFiles.append(VStr::EmptyString);
      models[skidx].subskinFiles[smidx] = xpath;
      continue;
    }
    // "model"
    if (sc->Check("model")) {
      sc->ExpectNumber();
      int mdidx = sc->Number;
      if (mdidx < 0 || mdidx > 1024) sc->Error(va("invalid model number (%d) in model '%s'", mdidx, *className));
      sc->ExpectString();
      VStr mname = sc->String.toLowerCase();
      if (!mname.isEmpty()) {
        VStr ext = mname.extractFileExtension();
        if (ext.isEmpty()) {
          sc->Message(va("gz alias model '%s' is in unknown format, defaulted to md3", *className));
          //mname += ".md3"; // no need to do it, we must preserve file name
        } else if (!ext.strEquCI(".md2") && !ext.strEquCI(".md3")) {
          sc->Message(va("gz alias model '%s' is in unknown format '%s', defaulted to md3", *className, *ext+1));
          //mname.clear(); // ok, allow it to load, the loader will take care of throwing it away
        }
        mname = path+mname;
      }
      while (models.length() <= mdidx) models.alloc();
      if (!models[mdidx].modelFile.isEmpty()) sc->Message(va("redefined model #%d for class '%s'", mdidx, *className));
      models[mdidx].modelFile = mname.fixSlashes();
      continue;
    }
    // "AngleOffset"
    if (sc->Check("AngleOffset")) {
      sc->ExpectFloatWithSign();
      angleOffset.yaw = AngleMod(sc->Float);
      continue;
    }
    // "PitchOffset"
    if (sc->Check("PitchOffset")) {
      sc->ExpectFloatWithSign();
      angleOffset.pitch = AngleMod(sc->Float);
      continue;
    }
    // "RollOffset"
    if (sc->Check("RollOffset")) {
      sc->ExpectFloatWithSign();
      angleOffset.roll = AngleMod(sc->Float);
      continue;
    }
    // "rotation-center"
    if (sc->Check("rotation-center")) {
      //sc->Message(va("modeldef command '%s' is not supported yet in model '%s'", *sc->String, *className));
      sc->ExpectFloatWithSign();
      rotCenter.x = sc->Float;
      sc->ExpectFloatWithSign();
      rotCenter.y = sc->Float;
      sc->ExpectFloatWithSign();
      rotCenter.z = sc->Float;
      continue;
    }
    // "scale"
    if (sc->Check("scale")) {
      // x
      sc->ExpectFloatWithSign();
      if (sc->Float == 0) sc->Message(va("invalid x scale in model '%s'", *className));
      scale.x = sc->Float;
      hudscale = (scale.x < 0);
      // y
      sc->ExpectFloatWithSign();
      if (sc->Float == 0) sc->Message(va("invalid y scale in model '%s'", *className));
      scale.y = sc->Float;
      // z
      sc->ExpectFloatWithSign();
      if (sc->Float == 0) sc->Message(va("invalid z scale in model '%s'", *className));
      scale.z = sc->Float;
      // normalize
      scale = sanitiseScale(scale);
      continue;
    }
    // "Offset"
    if (sc->Check("Offset")) {
      sc->ExpectFloatWithSign();
      offset.x = sc->Float;
      sc->ExpectFloatWithSign();
      offset.y = sc->Float;
      sc->ExpectFloatWithSign();
      offset.z = sc->Float;
      continue;
    }
    // "ZOffset"
    if (sc->Check("ZOffset")) {
      sc->ExpectFloatWithSign();
      offset.z = sc->Float;
      continue;
    }
    // "InheritActorPitch"
    if (sc->Check("InheritActorPitch")) {
      usePitch = -1;
      usePitchInverted = true;
      continue;
    }
    // "InheritActorRoll"
    if (sc->Check("InheritActorRoll")) {
      useRoll = -1;
      continue;
    }
    // "UseActorPitch"
    if (sc->Check("UseActorPitch")) {
      usePitch = -1;
      usePitchInverted = false;
      continue;
    }
    // "UseActorRoll"
    if (sc->Check("UseActorRoll")) {
      useRoll = -1;
      continue;
    }
    // "PitchFromMomentum"
    if (sc->Check("PitchFromMomentum")) {
      usePitch = 1;
      usePitchInverted = false;
      continue;
    }
    // "frameindex"
    if (sc->Check("frameindex")) {
      // FrameIndex sprbase sprframe modelindex frameindex
      Frame frm;
      buildMat(frm.mat, scale, offset, rotCenter, hudscale);
      // sprite name
      sc->ExpectString();
      frm.sprbase = sc->String.toLowerCase();
      if (frm.sprbase.length() != 4) sc->Error(va("invalid sprite name '%s' in model '%s'", *frm.sprbase, *className));
      // sprite frame
      sc->ExpectString();
      if (sc->String.length() == 0) sc->Error(va("empty sprite frame in model '%s'", *className));
      if (sc->String.length() != 1) {
        // gozzo wiki says that there can be only one frame, so fuck it
        sc->Message(va("invalid sprite frame '%s' in model '%s'; FIX YOUR BROKEN CODE!", *sc->String, *className));
      }
      char fc = sc->String[0];
      if (fc >= 'a' && fc <= 'z') fc = fc-'a'+'A';
      frm.sprframe = fc-'A';
      if (frm.sprframe < 0 || frm.sprframe > 31) sc->Error(va("invalid sprite frame '%s' in model '%s'", *sc->String, *className));
      // model index
      sc->ExpectNumber();
      // model "-1" means "hidden"
      if (sc->Number < 0 || sc->Number > 1024) sc->Error(va("invalid model index %d in model '%s'", sc->Number, *className));
      frm.mdindex = frm.origmdindex = sc->Number;
      // frame index
      sc->ExpectNumber();
      if (sc->Number < 0 || sc->Number > 1024) sc->Error(va("invalid model frame %d in model '%s'", sc->Number, *className));
      frm.frindex = frm.origmdlframe = sc->Number;
      // check if we already have equal frame, there is no need to keep duplicates
      bool replaced = false;
      for (auto &&ofr : frames) {
        if (frm.sprframe == ofr.sprframe &&
            frm.mdindex == ofr.mdindex &&
            frm.sprbase == ofr.sprbase &&
            frm.mat == ofr.mat)
        {
          // i found her!
          ofr.frindex = frm.frindex;
          replaced = true;
        }
      }
      // store it, if it wasn't a replacement
      if (!replaced) frames.append(frm);
      continue;
    }
    // "frame"
    if (sc->Check("frame")) {
      // Frame sprbase sprframe modelindex framename
      Frame frm;
      buildMat(frm.mat, scale, offset, rotCenter, hudscale);
      // sprite name
      sc->ExpectString();
      frm.sprbase = sc->String.toLowerCase();
      if (frm.sprbase.length() != 4) sc->Error(va("invalid sprite name '%s' in model '%s'", *frm.sprbase, *className));
      // sprite frame
      sc->ExpectString();
      if (sc->String.length() != 1) sc->Error(va("invalid sprite frame '%s' in model '%s'", *sc->String, *className));
      char fc = sc->String[0];
      if (fc >= 'a' && fc <= 'z') fc = fc-'a'+'A';
      frm.sprframe = fc-'A';
      if (frm.sprframe < 0 || frm.sprframe > 31) sc->Error(va("invalid sprite frame '%s' in model '%s'", *sc->String, *className));
      // model index
      sc->ExpectNumber();
      // model "-1" means "hidden"
      if (sc->Number < 0 || sc->Number > 1024) sc->Error(va("invalid model index %d in model '%s'", sc->Number, *className));
      frm.mdindex = frm.origmdindex = sc->Number;
      // frame name
      sc->ExpectString();
      //if (sc->String.isEmpty()) sc->Error(va("empty model frame name model '%s'", *className));
      frm.frindex = frm.origmdlframe = -1;
      frm.frname = sc->String;
      // check if we already have equal frame, there is no need to keep duplicates
      bool replaced = false;
      for (auto &&ofr : frames) {
        if (frm.sprframe == ofr.sprframe &&
            frm.mdindex == ofr.mdindex &&
            frm.sprbase == ofr.sprbase &&
            frm.mat == ofr.mat)
        {
          // i found her!
          ofr.frindex = -1;
          ofr.frname = frm.frname;
          replaced = true;
        }
      }
      // store it, if it wasn't a replacement
      if (!replaced) frames.append(frm);
      continue;
    }
    // unknown shit, try to ignore it
    if (!sc->GetString()) sc->Error(va("unexpected EOF in model '%s'", *className));
    sc->Message(va("unknown MODELDEF command '%s' in model '%s'", *sc->String, *className));
    for (;;) {
      if (!sc->GetString()) sc->Error(va("unexpected EOF in model '%s'", *className));
      if (sc->String.strEqu("{")) sc->Error(va("unexpected '{' in model '%s'", *className));
      if (sc->String.strEqu("}")) { sc->UnGet(); break; }
      if (sc->Crossed) { sc->UnGet(); break; }
    }
  }
  if (rotating && rotationSpeed == 0) rotationSpeed = 8; // arbitrary value
  if (!rotating) rotationSpeed = 0; // reset rotation flag

  /*
  if (zoffset != 0.0f) {
    TVec offset = TVec(0.0f, 0.0f, zoffset);
    mat *= VMatrix4::BuildOffset(offset);
    //mat = VMatrix4::BuildOffset(offset)*mat;
  }
  */
  VMatrix4 mat;
  buildMat(mat, scale, offset, rotCenter, hudscale);
  checkModelSanity(mat);
}


//==========================================================================
//
//  GZModelDef::findModelFrame
//
//  -1: not found
//
//==========================================================================
int GZModelDef::findModelFrame (int mdlindex, int mdlframe, bool allowAppend, const VMatrix4 &mat) {
  if (mdlindex < 0 || mdlindex >= models.length() || models[mdlindex].modelFile.isEmpty()) return -1;
  //k8: dunno if i have to check it here
  VStr mn = models[mdlindex].modelFile.extractFileExtension().toLowerCase();
  if (!mn.isEmpty() && mn != ".md2" && mn != ".md3") return -1;
  for (auto &&it : models[mdlindex].frameMap.itemsIdx()) {
    const MdlFrameInfo &fi = it.value();
    vassert(fi.mdlindex == mdlindex);
    if (fi.mdlframe == mdlframe) return it.index();
  }
  if (!allowAppend) return -1;
  // append it
  //!!!GLog.Logf(NAME_Warning, "alias model '%s' has no frame %d, appending", *models[mdlindex].modelFile, mdlframe);
  MdlFrameInfo &fi = models[mdlindex].frameMap.alloc();
  fi.mdlindex = mdlindex;
  fi.mdlframe = mdlframe;
  fi.vvframe = models[mdlindex].frameMap.length()-1;
  //fi.scale = scale;
  //fi.offset = offset;
  //fi.zoffset = zoffset;
  fi.mat = mat;
  fi.used = true; // why not?
  return fi.vvframe;
}


//==========================================================================
//
//  GZModelDef::checkModelSanity
//
//==========================================================================
void GZModelDef::checkModelSanity (const VMatrix4 &mat) {
  // build frame map
  bool hasValidFrames = false;
  bool hasInvalidFrames = false;

  // clear existing frame maps, just in case
  int validModelCount = 0;
  for (auto &&mdl : models) {
    mdl.frameMap.clear();
    mdl.reported = false;
    mdl.frlist.clear();
    mdl.frlistLoaded = false;
    // remove non-existant model
    if (mdl.modelFile.isEmpty()) continue;
    if (!IsModelFileExists(mdl.modelFile)) { mdl.modelFile.clear(); continue; }
    ++validModelCount;
  }

  TMap<VStr, int> frameMap; // key: frame base; value; first in list

  for (auto &&it : frames.itemsIdx()) {
    Frame &frm = it.value();
    frm.linkSprBase = -1;
    // check for MD2 named frames
    if (frm.frindex == -1) {
      int mdlindex = frm.mdindex;
      if (mdlindex < 0 || mdlindex >= models.length() || models[mdlindex].modelFile.isEmpty() || models[mdlindex].reported) {
        frm.vvindex = -1;
      } else {
        if (!models[mdlindex].frlistLoaded) {
          VStr mfn = models[mdlindex].modelFile;
          GLog.WriteLine(NAME_Init, "loading alias model (md2) frames from '%s' (for class '%s')", *mfn, *className);
          if (!ParseMD2Frames(mfn, models[mdlindex].frlist)) {
            VStream *strm = FL_OpenFileRead(mfn);
            if (!strm) {
              GLog.WriteLine(NAME_Warning, "alias model '%s' not found for class '%s'", *mfn, *className);
            } else {
              VStream::Destroy(strm);
              GLog.WriteLine(NAME_Warning, "cannot parse alias model '%s' not found for class '%s'", *mfn, *className);
            }
            models[mdlindex].reported = true;
          }
          models[mdlindex].frlistLoaded = true;
        }
        frm.vvindex = -1;
        for (auto &&nit : models[mdlindex].frlist.itemsIdx()) {
          if (nit.value().strEquCI(frm.frname)) {
            frm.vvindex = nit.index();
            break;
          }
        }
        if (frm.vvindex < 0 && !models[mdlindex].reported) {
          GLog.WriteLine(NAME_Warning, "alias model '%s' has no frame '%s' for class '%s'", *models[mdlindex].modelFile, *frm.frname, *className);
          models[mdlindex].reported = true;
          /*
          if (models[mdlindex].frlist.length()) {
            GLog.Logf(NAME_Debug, "=== %d frame%s ===", models[mdlindex].frlist.length(), (models[mdlindex].frlist.length() != 1 ? "s" : ""));
            for (auto &&nit : models[mdlindex].frlist.itemsIdx()) GLog.Logf(NAME_Debug, "  %s", *nit.value().quote());
          }
          */
        }
      }
      if (frm.vvindex >= 0) {
        frm.vvindex = findModelFrame(frm.mdindex, frm.vvindex, true, mat); // allow appending
      }
    } else {
      frm.vvindex = findModelFrame(frm.mdindex, frm.frindex, true, mat); // allow appending
    }

    if (frm.vvindex < 0) {
      if (frm.frindex >= 0) {
        GLog.WriteLine(NAME_Warning, "alias model '%s' has invalid model index (%d) in frame %d", *className, frm.mdindex, it.index());
      } else {
        GLog.WriteLine(NAME_Warning, "alias model '%s' has invalid model frame '%s' in frame %d", *className, *frm.frname, it.index());
      }
      hasInvalidFrames = true;
      continue;
    }

    hasValidFrames = true;
    frm.angleOffset = angleOffset; // copy it here
    frm.rotationSpeed = rotationSpeed;
    frm.usePitch = usePitch;
    frm.usePitchInverted = usePitchInverted;
    frm.useRoll = useRoll;

    // add to frame map; order doesn't matter
    {
      auto fsp = frameMap.get(frm.sprbase);
      frm.linkSprBase = (fsp ? *fsp : -1);
      frameMap.put(frm.sprbase, it.index());
    }
  }

  // is it empty?
  if (!hasValidFrames && !hasInvalidFrames) { clear(); return; }

  if (!hasValidFrames) {
    GLog.WriteLine(NAME_Warning, "gz alias model '%s' nas no valid frames!", *className);
    clear();
    return;
  }

  {
    // check if we have several models, but only one model defined for sprite frame
    // if it is so, attach all models to this frame (it seems that GZDoom does this)
    // note that invalid frames weren't added to frame map
    const int origFrLen = frames.length();
    for (int fridx = 0; fridx < origFrLen; ++fridx) {
      Frame &frm = frames[fridx];
      if (frm.vvindex < 0) continue; // ignore invalid frames
      auto fsp = frameMap.get(frm.sprbase);
      if (!fsp) continue; // the thing that should not be
      // remove duplicate frames, count models
      int mdcount = 0;
      for (int idx = *fsp; idx >= 0; idx = frames[idx].linkSprBase) {
        if (idx == fridx) { ++mdcount; continue; }
        Frame &cfr = frames[idx];
        if (cfr.sprframe != frm.sprframe) continue; // different sprite animation frame
        // different model?
        if (cfr.mdindex != frm.mdindex) { ++mdcount; continue; }
        // check if it is a duplicate model frame
        if (cfr.frindex == frm.frindex) cfr.vvindex = -1; // don't render this, it is excessive
      }
      vassert(frm.vvindex >= 0);
      // if only one model used, but we have more, attach all models
      if (mdcount < 2 && validModelCount > 1) {
        GCon->Logf(NAME_Warning, "force-attaching all models to gz alias model '%s', frame %s %c", *className, *frm.sprbase.toUpperCase(), 'A'+frm.sprframe);
        for (int mnum = 0; mnum < models.length(); ++mnum) {
          if (mnum == frm.mdindex) continue;
          if (models[mnum].modelFile.isEmpty()) continue;
          Frame newfrm = frm;
          newfrm.mdindex = mnum;
          if (newfrm.frindex == -1) {
            //md2 named
            newfrm.vvindex = findModelFrame(newfrm.mdindex, newfrm.vvindex, true, mat); // allow appending
          } else {
            //indexed
            newfrm.vvindex = findModelFrame(newfrm.mdindex, newfrm.frindex, true, mat); // allow appending
          }
          frames.append(newfrm);
        }
      }
    }
  }

  // remove invalid frames
  if (hasInvalidFrames) {
    int fidx = 0;
    while (fidx < frames.length()) {
      if (frames[fidx].vvindex < 0) {
        //GLog.WriteLine(NAME_Warning, "alias model '%s': removing frame #%d: %s", *className, fidx, *frames[fidx].toString());
        frames.removeAt(fidx);
      } else {
        ++fidx;
      }
    }
    if (frames.length() == 0) { hasValidFrames = false; hasInvalidFrames = false; clear(); return; }
  }

  // clear unused model names
  for (auto &&mdl : models) {
    if (mdl.frameMap.length() == 0) {
      mdl.modelFile.clear();
      mdl.skinFile.clear();
      mdl.subskinFiles.clear();
    }
  }
}


//==========================================================================
//
//  GZModelDef::merge
//
//  merge this model frames with another model frames
//  GZDoom seems to do this, so we have too
//
//==========================================================================
void GZModelDef::merge (GZModelDef &other) {
  if (&other == this) return; // nothing to do
  if (other.isEmpty()) return; // nothing to do

  //GCon->Logf(NAME_Debug, "MERGE: <%s> and <%s>", *className, *other.className);

  // this is brute-force approach, which can be made faster, but meh...
  // just go through other model def frames, and try to find the correspondence
  // in this model, or append new data.
  bool compactFrameMaps = false;
  for (auto &&ofrm : other.frames) {
    if (ofrm.vvindex < 0) continue; // this frame is invalid
    // ok, we have a frame, try to find a model for it
    VStr omdf = other.models[ofrm.mdindex].modelFile;
    VStr osdf = other.models[ofrm.mdindex].skinFile;
    TArray<VStr> subosdf = other.models[ofrm.mdindex].subskinFiles;
    int mdlEmpty = -1; // first empty model in model array, to avoid looping twice
    int mdlindex = -1;
    for (auto &&mdlit : models.itemsIdx()) {
      if (omdf.strEquCI(mdlit.value().modelFile) && osdf.strEquCI(mdlit.value().skinFile)) {
        bool equal = (subosdf.length() == mdlit.value().subskinFiles.length());
        if (equal) {
          for (int f = 0; f < subosdf.length(); ++f) {
            if (!subosdf[f].strEquCI(mdlit.value().subskinFiles[f])) {
              equal = false;
              break;
            }
          }
        }
        if (equal) {
          mdlindex = mdlit.index();
          break;
        }
      }
      if (mdlEmpty < 0 && mdlit.value().frameMap.length() == 0) mdlEmpty = mdlit.index();
    }

    // if we didn't found a suitable model, append a new one
    bool newModel = false;
    if (mdlindex < 0) {
      newModel = true;
      if (mdlEmpty < 0) {
        mdlEmpty = models.length();
        models.alloc();
      }
      mdlindex = mdlEmpty;
      MSDef &nmdl = models[mdlindex];
      nmdl.modelFile = omdf;
      nmdl.skinFile = osdf;
      nmdl.subskinFiles = subosdf;
      //GCon->Logf(NAME_Debug, "  new model: <%s> <%s>", *omdf, *osdf);
    }

    // try to find a model frame to reuse
    MSDef &rmdl = models[mdlindex];
    const MdlFrameInfo &omfrm = other.models[ofrm.mdindex].frameMap[ofrm.vvindex];
    int frmapindex = -1;
    if (!newModel) {
      for (auto &&mfrm : rmdl.frameMap) {
        if (mfrm.mdlindex == mdlindex &&
            mfrm.mdlframe == omfrm.mdlframe &&
            mfrm.mat == omfrm.mat)
        {
          // yay, i found her!
          // reuse this frame
          frmapindex = mfrm.vvframe;
          /*
          GCon->Logf(NAME_Debug, "  merge frames: %d -> %d (frmapindex=%d)", ofrm.vvindex, mfrm.vvframe, frmapindex);
          GCon->Logf(NAME_Debug, "    mfrm=%s", *mfrm.toString());
          GCon->Logf(NAME_Debug, "    ofrm=%s", *ofrm.toString());
          */
          break;
        }
      }
    }

    if (frmapindex < 0) {
      // ok, we have no suitable model frame, append a new one
      //GCon->Logf(NAME_Debug, "  new frame (mdlindex=%d; frindex=%d); ofrm=%s", mdlindex, ofrm.frindex, *ofrm.toString());
      frmapindex = rmdl.frameMap.length();
      MdlFrameInfo &nfi = rmdl.frameMap.alloc();
      nfi.mdlindex = mdlindex;
      nfi.mdlframe = ofrm.frindex;
      nfi.vvframe = rmdl.frameMap.length()-1;
      nfi.mat = omfrm.mat;
    }

    // find sprite frame to replace
    // HACK: same model indicies will be replaced; this is how GZDoom does it
    int spfindex = -1;
    for (auto &&sit : frames.itemsIdx()) {
      Frame &ff = sit.value();
      if (ff.sprframe == ofrm.sprframe &&
          ff.origmdindex == ofrm.origmdindex &&
          ff.sprbase == ofrm.sprbase &&
          ff.mat == ofrm.mat)
      {
        GLog.WriteLine(NAME_Warning, "class '%s' (%s%c) attaches alias models several times!", *className, *ff.sprbase.toUpperCase(), 'A'+ff.sprframe);
        spfindex = sit.index();
        break;
      }
    }

    if (spfindex < 0) {
      // append new sprite frame
      spfindex = frames.length();
      (void)frames.alloc();
    } else {
      if (!compactFrameMaps && frames[spfindex].vvindex != frmapindex) compactFrameMaps = true;
    }

    // replace sprite frame
    Frame &newfrm = frames[spfindex];
    newfrm.sprbase = ofrm.sprbase;
    newfrm.sprframe = ofrm.sprframe;
    newfrm.mdindex = mdlindex;
    newfrm.origmdindex = ofrm.origmdindex;
    newfrm.frindex = ofrm.frindex;
    newfrm.angleOffset = ofrm.angleOffset;
    newfrm.rotationSpeed = ofrm.rotationSpeed;
    newfrm.usePitch = ofrm.usePitch;
    newfrm.usePitchInverted = ofrm.usePitchInverted;
    newfrm.useRoll = ofrm.useRoll;
    newfrm.vvindex = frmapindex;
    newfrm.mat = ofrm.mat;
  }

  // remove unused model frames
  if (compactFrameMaps) {
    // actually, simply rebuild frame maps for each model
    // i may rewrite it in the future, but for now it is ok
    int unusedFramesCount = 0;
    for (auto &&mdl : models) {
      for (auto &&frm : mdl.frameMap) {
        frm.used = false;
        ++unusedFramesCount;
      }
    }
    // mark all used frames
    for (auto &&frm : frames) {
      if (frm.vvindex < 0) continue; // just in case
      if (!models[frm.mdindex].frameMap[frm.vvindex].used) {
        models[frm.mdindex].frameMap[frm.vvindex].used = true;
        --unusedFramesCount;
      }
    }
    vassert(unusedFramesCount >= 0);
    if (unusedFramesCount == 0) return; // nothing to do

    // rebuild frame maps
    for (auto &&mit : models.itemsIdx()) {
      MSDef &mdl = mit.value();
      TArray<MdlFrameInfo> newmap;
      TArray<int> newvvindex;
      newvvindex.setLength(mdl.frameMap.length());
      for (auto &&frm : mdl.frameMap) {
        if (!frm.used) continue;
        newvvindex[frm.vvframe] = newmap.length();
        newmap.append(frm);
      }
      if (newmap.length() == mdl.frameMap.length()) continue; // nothing to do
      //FIXME: make this faster!
      for (auto &&frm : frames) {
        if (frm.vvindex < 0) continue; // just in case
        if (frm.mdindex != mit.index()) continue;
        vassert(newvvindex[frm.vvindex] >= 0);
        frm.vvindex = newvvindex[frm.vvindex];
      }
      mdl.frameMap = newmap;
      // fix indicies
      for (auto &&xit : mdl.frameMap.itemsIdx()) xit.value().vvframe = xit.index();
    }
  }
}


//==========================================================================
//
//  GZModelDef::createXml
//
//==========================================================================
VStr GZModelDef::createXml () {
  VStr res = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n";
  res += "<!-- ";
  res += className;
  res += " -->\n";
  res += "<vavoom_model_definition>\n";

  // write models
  for (auto &&it : models.itemsIdx()) {
    const MSDef &mdl = it.value();
    if (mdl.frameMap.length() == 0) continue; // this model is unused
    vassert(!mdl.modelFile.isEmpty());
    const char *mdtag = (mdl.modelFile.extractFileExtension().strEquCI(".md2") ? "md2" : "md3");
    res += va("  <model name=\"%s_%d\">\n", *className.toLowerCase().xmlEscape(), it.index());
    res += va("    <%s file=\"%s\" noshadow=\"false\">\n", mdtag, *mdl.modelFile.xmlEscape());
    bool thesame = true;
    for (int f = 1; f < mdl.frameMap.length(); ++f) {
      if (mdl.frameMap[f-1].mat != mdl.frameMap[f].mat) {
        thesame = false;
        break;
      }
    }
    //thesame = false;
    if (thesame && mdl.frameMap.length()) {
      VMatrix4 mat = mdl.frameMap[0].mat;
      res += "      <transform>\n";
      res += "        <matrix absolute=\"true\">\n";
      for (int y = 0; y < 4; ++y) {
        res += "         ";
        for (int x = 0; x < 4; ++x) {
          res += va(" %g", mat.m[y][x]);
        }
        res += "\n";
      }
      res += "        </matrix>\n";
      res += "      </transform>\n";
    }
    if (!mdl.skinFile.isEmpty()) res += va("      <skin file=\"%s\" />\n", *mdl.skinFile.xmlEscape());
    if (mdl.subskinFiles.length() != 0) {
      for (int f = 0; f < mdl.subskinFiles.length(); ++f) {
        VStr sf = mdl.subskinFiles[f];
        if (sf.isEmpty()) continue;
        res += va("      <subskin submodel_index=\"%d\" file=\"%s\" />\n", f, *sf.xmlEscape());
      }
    }
    // write frame list
    int fidx = 0;
    while (fidx < mdl.frameMap.length()) {
      const MdlFrameInfo &fi = mdl.frameMap[fidx];
      int fend = fidx+1;
      while (fend < mdl.frameMap.length()) {
        const MdlFrameInfo &xf = mdl.frameMap[fend];
        if (xf.mdlframe != mdl.frameMap[fend-1].mdlframe+1 || xf.mat != fi.mat) {
          break;
        }
        ++fend;
      }
      //vassert(it.index() == fi.mdlindex);
      //vassert(fit.index() == fi.vvframe);

      res += va("      <frame index=\"%d\"", fi.mdlframe);
      if (fend != fidx+1) {
        res += va(" end_index=\"%d\"", mdl.frameMap[fend-1].mdlframe);
      }
      if (thesame || fi.mat == VMatrix4::Identity) {
        res += " />  <!-- ";
        if (fend == fidx+1) res += va("%d", fidx); else res += va("%d-%d", fidx, fend-1);
        res += " -->\n";
      } else {
        res += ">  <!-- ";
        if (fend == fidx+1) res += va("%d", fidx); else res += va("%d-%d", fidx, fend-1);
        res += " -->\n";
        res += "        <transform>\n";
        res += "          <matrix absolute=\"true\">\n";
        for (int y = 0; y < 4; ++y) {
          res += "           ";
          for (int x = 0; x < 4; ++x) {
            res += va(" %g", fi.mat.m[y][x]);
          }
          res += "\n";
        }
        res += "          </matrix>\n";
        res += "        </transform>\n";
        res += "      </frame>\n";
      }

      fidx = fend;
    }
    res += va("    </%s>\n", mdtag);
    res += "  </model>\n";
  }

  // write class definition
  res += va("  <class name=\"%s\" noselfshadow=\"true\">\n", *className.xmlEscape());
  for (auto &&frm : frames) {
    if (frm.vvindex < 0) continue;
    res += va("    <state sprite=\"%s\" sprite_frame=\"%s\" model=\"%s_%d\" frame_index=\"%d\" gzdoom=\"true\"",
      *frm.sprbase.toUpperCase().xmlEscape(),
      *VStr((char)(frm.sprframe+'A')).xmlEscape(),
      *className.toLowerCase().xmlEscape(), frm.mdindex,
      frm.vvindex);
    if (frm.rotationSpeed) res += " rotation=\"true\"";
    if (frm.angleOffset.yaw) res += va(" rotate_yaw=\"%g\"", frm.angleOffset.yaw);
    if (frm.angleOffset.pitch) res += va(" rotate_pitch=\"%g\"", frm.angleOffset.pitch);
    if (frm.angleOffset.roll) res += va(" rotate_roll=\"%g\"", frm.angleOffset.roll);
    if (frm.usePitch < 0) {
      res += va(" usepitch=\"actor%s\"", (frm.usePitchInverted ? "-inverted" : ""));
    } else if (frm.usePitch > 0) {
      res += va(" usepitch=\"momentum%s\"", (frm.usePitchInverted ? "-inverted" : ""));
    }
         if (frm.useRoll < 0) res += va(" useroll=\"actor\"");
    else if (frm.useRoll > 0) res += va(" useroll=\"momentum\"");
    res += " />\n";
  }
  res += "  </class>\n";
  res += "</vavoom_model_definition>\n";
  return res;
}

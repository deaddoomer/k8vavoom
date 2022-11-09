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
#include "../psim/p_entity.h"
#include "../dehacked/vc_dehacked.h"
#include "r_local.h"
#include "r_local_gz.h"


extern VCvarB vox_cache_enabled;


static VCvarI mdl_verbose_loading("mdl_verbose_loading", "0", "Verbose alias model loading?", CVAR_NoShadow/*|CVAR_Archive*/);
static VCvarB r_preload_alias_models("r_preload_alias_models", true, "Preload all alias models and their skins?", CVAR_Archive|CVAR_PreInit|CVAR_NoShadow);
static VCvarI dbg_dump_gzmodels("dbg_dump_gzmodels", "0", "Dump xml files for gz modeldefs (1:final;2:all)?", /*CVAR_Archive|*/CVAR_PreInit|CVAR_NoShadow);
static VCvarB mdl_debug_md3("mdl_debug_md3", false, "Show 'automatic submodels' for MD3?", CVAR_PreInit|CVAR_NoShadow|CVAR_Archive);


static int cli_DisableModeldef = 0;
static int cli_IgnoreReplaced = 0;
static int cli_IgnoreRestrictions = 0;
static TMap<VStrCI, bool> cli_IgnoreModelClass;

/*static*/ bool cliRegister_rmodel_args =
  VParsedArgs::RegisterFlagSet("-no-modeldef", "disable GZDoom MODELDEF lump parsing", &cli_DisableModeldef) &&
  VParsedArgs::RegisterFlagSet("-model-ignore-replaced", "do not disable models for replaced sprites", &cli_IgnoreReplaced) &&
  VParsedArgs::RegisterFlagSet("-model-ignore-restrictions", "ignore model restrictions", &cli_IgnoreRestrictions) &&
  VParsedArgs::RegisterCallback("-model-ignore-classes", "!do not use model for the following class names", [] (VArgs &args, int idx) -> int {
    for (++idx; !VParsedArgs::IsArgBreaker(args, idx); ++idx) {
      VStr mn = args[idx];
      if (!mn.isEmpty()) cli_IgnoreModelClass.put(mn, true);
    }
    return idx;
  });



// ////////////////////////////////////////////////////////////////////////// //
// RR GG BB or -1
static int parseHexRGB (VStr str) {
  vuint32 ppc = M_ParseColor(*str);
  return (ppc&0xffffff);
}


// ////////////////////////////////////////////////////////////////////////// //
// in "r_local.h"
bool ClassModelMapNeedRebuild = true;
TMapNC<VName, VClassModelScript *> ClassModelMap;
// in "r_public.h"
TArray<int> AllModelTextures;

static TArray<VModel *> mod_known;
static TArray<VStr> mod_gznames;
static TArray<VMeshModel *> GMeshModels;
static TArray<VClassModelScript *> ClassModels;
static TMapNC<int, bool> AllModelTexturesSeen;


// ////////////////////////////////////////////////////////////////////////// //
static void ParseGZModelDefs ();


class GZModelDefEx : public GZModelDef {
public:
  virtual bool ParseMD2Frames (VStr mdpath, TArray<VStr> &names) override;
  virtual bool IsModelFileExists (VStr mdpath) override;
};


//==========================================================================
//
//  GZModelDefEx::ParseMD2Frames
//
//  return `true` if model was succesfully found and parsed, or
//  false if model wasn't found or in invalid format
//  WARNING: don't clear `names` array!
//
//==========================================================================
bool GZModelDefEx::ParseMD2Frames (VStr mdpath, TArray<VStr> &names) {
  return VMeshModel::LoadMD2Frames(mdpath, names);
}


//==========================================================================
//
//  GZModelDefEx::IsModelFileExists
//
//==========================================================================
bool GZModelDefEx::IsModelFileExists (VStr mdpath) {
  if (mdpath.length() == 0) return false;
  VStream *strm = FL_OpenFileRead(mdpath);
  if (!strm) return false;
  bool okfmt = VMeshModel::IsKnownModelFormat(strm);
  VStream::Destroy(strm);
  return okfmt;
}


//==========================================================================
//
//  RM_RebuildClassModelMap
//
//==========================================================================
void RM_RebuildClassModelMap () {
  // build map
  ClassModelMapNeedRebuild = false;
  ClassModelMap.reset();
  for (auto &&mdl : ClassModels) {
    if (mdl->Name == NAME_None || !mdl->Model || mdl->Frames.length() == 0) continue;
    ClassModelMap.put(mdl->Name, mdl);
  }
}


//==========================================================================
//
//  R_HaveClassModelByName
//
//==========================================================================
bool R_HaveClassModelByName (VName clsName) {
  return !!RM_FindClassModelByName(clsName);
}


//==========================================================================
//
//  R_EntModelNoSelfShadow
//
//==========================================================================
bool R_EntModelNoSelfShadow (VEntity *mobj) {
  VClassModelScript *cs = RM_FindClassModelByName(VRenderLevelShared::GetClassNameForModel(mobj));
  return (cs ? cs->noSelfShadow : true);
}


//==========================================================================
//
//  R_InitModels
//
//==========================================================================
void R_InitModels () {
  GCon->Log(NAME_Init, "loading model scripts");
  C_FlushSplash();
  for (int Lump = W_IterateFile(-1, "models/models.xml"); Lump != -1; Lump = W_IterateFile(Lump, "models/models.xml")) {
    VStream *lumpstream = W_CreateLumpReaderNum(Lump);
    VCheckedStream Strm(lumpstream, true); // load to memory
    if (mdl_verbose_loading) {
      GCon->Logf(NAME_Init, "parsing model definition '%s'", *W_FullLumpName(Lump));
    }
    // parse the file
    VXmlDocument *Doc = new VXmlDocument();
    Doc->Parse(Strm, "models/models.xml");
    //for (VXmlNode *N = Doc->Root.FindChild("include"); N; N = N->FindNext()) Mod_FindName(N->GetAttribute("file"));
    for (VXmlNode *N : Doc->Root.allChildren()) {
      if (!N->Name.strEqu("include")) Sys_Error("%s: invalid model script node '%s'", *N->Loc.toStringNoCol(), *N->Name);
      if (N->HasChildren()) Sys_Error("%s: model script include node should not have children", *N->Loc.toStringNoCol());
      // check attrs
      {
        auto bad = N->FindBadAttribute("file", nullptr);
        if (bad) Sys_Error("%s: invalid model script attribute '%s'", *bad->Loc.toStringNoCol(), *bad->Name);
      }
      if (!N->HasAttribute("file")) Sys_Error("%s: model script include node doesn't have \"file\" attribute", *N->Loc.toStringNoCol());
      Mod_FindName(N->GetAttribute("file"));
    }
    delete Doc;
  }
  GCon->Log(NAME_Init, "all model scripts loaded");
  C_FlushSplash();

  if (!cli_DisableModeldef) ParseGZModelDefs();
}


//==========================================================================
//
//  R_FreeModels
//
//==========================================================================
void R_FreeModels () {
  for (auto &&it : mod_known) {
    delete it;
    it = nullptr;
  }
  mod_known.clear();

  for (auto &&it : GMeshModels) {
    delete it;
    it = nullptr;
  }
  GMeshModels.clear();

  for (auto &&it : ClassModels) {
    delete it;
    it = nullptr;
  }
  ClassModels.clear();

  RM_FreeModelRenderer();
  ClassModelMap.clear();
  ClassModelMapNeedRebuild = true;
}


//==========================================================================
//
//  Mod_FindMeshModel
//
//==========================================================================
static VMeshModel *Mod_FindMeshModel (VStr filename, VStr name, int meshIndex,
                                      bool isVoxel, bool useVoxelPivotZ)
{
  if (name.IsEmpty()) Sys_Error("Mod_ForName: nullptr name");

  if (name.indexOf('/') < 0) {
    filename = filename.ExtractFilePath().toLowerCase();
    if (filename.length()) name = filename+name;
  }

  // search the currently loaded models
  for (auto &&it : GMeshModels) {
    if (it->MeshIndex == meshIndex && it->Name == name) return it;
  }

  VMeshModel *mod = new VMeshModel();
  mod->Name = name;
  mod->MeshIndex = meshIndex;
  mod->loaded = false;
  mod->isVoxel = isVoxel;
  mod->useVoxelPivotZ = useVoxelPivotZ;
  mod->voxOptLevel = -1;
  mod->voxFixTJunk = false;
  mod->voxHollowFill = true;
  mod->voxSkinTextureId = -1;
  mod->GlMode = VMeshModel::GlNone;
  GMeshModels.Append(mod);

  return mod;
}


//==========================================================================
//
//  xatof
//
//==========================================================================
static float xatof (VXmlNode *N, VStr str) {
  str = str.xstrip();
  float res = 0.0f;
  if (str.convertFloat(&res)) {
    if (isfinite(res)) return res;
  }
  Sys_Error("invalid float value '%s' at %s", *str, *N->Loc.toStringNoCol());
}


//==========================================================================
//
//  ParseIntWithDefault
//
//==========================================================================
static int ParseIntWithDefault (VXmlNode *SN, const char *fieldname, int defval) {
  vassert(SN);
  vassert(fieldname && fieldname[0]);
  if (!SN->HasAttribute(fieldname)) return defval;
  int val = defval;
  if (!SN->GetAttribute(fieldname).trimAll().convertInt(&val)) Sys_Error("model node '%s' should have integer value, but '%s' found", fieldname, *SN->GetAttribute(fieldname));
  return val;
}


//==========================================================================
//
//  ParseFloatWithDefault
//
//==========================================================================
static float ParseFloatWithDefault (VXmlNode *SN, const char *fieldname, float defval) {
  vassert(SN);
  vassert(fieldname && fieldname[0]);
  if (!SN->HasAttribute(fieldname)) return defval;
  float val = defval;
  if (!SN->GetAttribute(fieldname).trimAll().convertFloat(&val)) Sys_Error("model node '%s' should have floating value, but '%s' found", fieldname, *SN->GetAttribute(fieldname));
  return val;
}


//==========================================================================
//
//  ParseAngle
//
//==========================================================================
static void ParseAngle (VXmlNode *N, const char *name, ModelAngle &angle) {
  angle.SetRelative(0.0f);
  VStr aname = VStr("angle_")+name;
  if (N->HasAttribute(aname)) {
    VStr val = N->GetAttribute(aname);
    if (val.ICmp("random") == 0) angle.SetAbsoluteRandom(); else angle.SetAbsolute(xatof(N, val));
  } else {
    aname = VStr("rotate_")+name;
    if (N->HasAttribute(aname)) {
      VStr val = N->GetAttribute(aname);
      if (val.ICmp("random") == 0) angle.SetRelativeRandom(); else angle.SetRelative(xatof(N, val));
    }
  }
}


//==========================================================================
//
//  ParseRndZOffset
//
//==========================================================================
static void ParseRndZOffset (VXmlNode *N, ModelZOffset &zoffset) {
  zoffset.vmin = ParseFloatWithDefault(N, "random_zoffset_min", zoffset.vmin);
  zoffset.vmax = ParseFloatWithDefault(N, "random_zoffset_max", zoffset.vmax);
}


//==========================================================================
//
//  isTrueValue
//
//==========================================================================
static inline bool isTrueValue (VStr val) {
  return
    val == "yes" ||
    val == "tan" ||
    val == "true" ;
}


//==========================================================================
//
//  isFalseValue
//
//==========================================================================
static inline bool isFalseValue (VStr val) {
  return
    val == "no" ||
    val == "ona" ||
    val == "false" ;
}


//==========================================================================
//
//  ParseBool
//
//==========================================================================
static bool ParseBool (VXmlNode *N, const char *name, bool defval) {
  if (!N->HasAttribute(name)) return defval;
  VStr val = N->GetAttribute(name);
  if (isTrueValue(val)) return true;
  if (isFalseValue(val)) return false;
  Sys_Error("invalid boolean value '%s' for attribute '%s' at %s", *val, name, *N->Loc.toStringNoCol());
}


//==========================================================================
//
//  ParseVector
//
//  `vec` must be initialised
//
//==========================================================================
static void ParseVector (VXmlNode *SN, TVec &vec, const char *basename, bool propagate) {
  vassert(SN);
  vassert(basename);
  if (SN->HasAttribute(basename)) {
    vec.x = xatof(SN, SN->GetAttribute(basename));
    vec.y = (propagate ? vec.x : 0.0f);
    vec.z = (propagate ? vec.x : 0.0f);
  } else {
    VStr xname;
    xname = VStr(basename)+"_x"; if (SN->HasAttribute(xname)) vec.x = xatof(SN, SN->GetAttribute(xname));
    xname = VStr(basename)+"_y"; if (SN->HasAttribute(xname)) vec.y = xatof(SN, SN->GetAttribute(xname));
    xname = VStr(basename)+"_z"; if (SN->HasAttribute(xname)) vec.z = xatof(SN, SN->GetAttribute(xname));
  }
}


//==========================================================================
//
//  parseGZScaleOfs
//
//==========================================================================
static TVec parseGZScaleOfs (VXmlNode *SN, TVec vec) {
  if (SN->FirstChild) {
    Sys_Error("%s: gztransform node '%s' can't have children", *SN->Loc.toStringNoCol(), *SN->Name);
  }
  for (int f = 0; f < SN->Attributes.length(); ++f) {
    int idx;
         if (SN->Attributes[f].Name == "x") idx = 0;
    else if (SN->Attributes[f].Name == "y") idx = 1;
    else if (SN->Attributes[f].Name == "z") idx = 2;
    else Sys_Error("%s: gztransform node '%s' has invalid attribute '%s'",
                   *SN->Attributes[f].Loc.toStringNoCol(), *SN->Name, *SN->Attributes[f].Name);
    VStr s = SN->Attributes[f].Value.xstrip();
    float res = 0.0f;
    if (!s.convertFloat(&res) || !isfinite(res)) {
      Sys_Error("%s: invalid node '%s' attribute '%s' value '%s'",
                *SN->Attributes[f].Loc.toStringNoCol(), *SN->Name, *SN->Attributes[f].Name, *s);
    }
    vec[idx] = res;
  }
  return vec;
}


//==========================================================================
//
//  performTransOperation
//
//==========================================================================
static bool performTransOperation (VXmlNode *SN, VMatrix4 &trans) {
  enum {
    OpScale,
    OpOffset,
    OpRotate,
  };

  int op;
       if (SN->Name == "scale") op = OpScale;
  else if (SN->Name == "offset" || SN->Name == "shift" || SN->Name == "move") op = OpOffset;
  else if (SN->Name == "rotate") op = OpRotate;
  else return false;

  if (SN->FirstChild && SN->Attributes.length()) {
    Sys_Error("%s: transform node '%s' can have either children, or attributes",
              *SN->Loc.toStringNoCol(), *SN->Name);
  }

  if (!SN->FirstChild && !SN->Attributes.length()) {
    Sys_Error("%s: transform node '%s' must have either children, or attributes",
              *SN->Loc.toStringNoCol(), *SN->Name);
  }

  bool wasNN[3] = {false, false, false};
  TVec vec = (op == OpScale ? TVec(1.0f, 1.0f, 1.0f) : TVec::ZeroVector);

  VXmlNode *n = SN->FirstChild;
  int aidx = 0;

  for (;;) {
    VStr type, value;
    VTextLocation loc;
    if (SN->FirstChild) {
      if (!n) break;
      if (n->FirstChild) Sys_Error("%s: node '%s' cannot have children", *n->Loc.toStringNoCol(), *n->Name);
      if (n->Attributes.length()) Sys_Error("%s: node '%s' cannot have attributes", *n->Loc.toStringNoCol(), *n->Name);
      type = n->Name;
      value = n->Value.xstrip();
      loc = n->Loc;
      n = n->NextSibling;
    } else {
      if (aidx >= SN->Attributes.length()) break;
      type = SN->Attributes[aidx].Name;
      value = SN->Attributes[aidx].Value.xstrip();
      loc = SN->Attributes[aidx].Loc;
      ++aidx;
    }

    int idx = -1;
    bool gzrot = false;
    if (type == "value") {
      if (op == OpRotate) {
        Sys_Error("%s: `value` has no sence for rotation", *loc.toStringNoCol());
      }
      if (wasNN[0] || wasNN[1] || wasNN[2]) {
        Sys_Error("%s: arg '%s' conflicts with previous args", *loc.toStringNoCol(), *type);
      }
      wasNN[0] = wasNN[1] = wasNN[2] = true;
    } else {
           if ((op != OpRotate && type == "x") || (op == OpRotate && type == "pitch")) idx = 0; // RotateX is pitch
      else if ((op != OpRotate && type == "y") || (op == OpRotate && type == "roll")) idx = 1; // RotateY is roll
      else if ((op != OpRotate && type == "z") || (op == OpRotate && type == "yaw")) idx = 2; // RotateZ is yaw
      else if (op == OpRotate && type == "gz_voxel_yaw") { idx = 2; gzrot = true; } // gz voxel rotation
      else Sys_Error("%s: unknown arg '%s'", *loc.toStringNoCol(), *type);
      if (wasNN[idx]) Sys_Error("%s: duplicate arg '%s'", *loc.toStringNoCol(), *type);
      wasNN[idx] = true;
    }

    float flt = 0.0f;
    if (!value.convertFloat(&flt) || !isfinite(flt)) {
      Sys_Error("%s: arg '%s' is not a valid float (%s)", *loc.toStringNoCol(), *type, *value);
    }
    // gz voxel rotation angle?
    if (gzrot) vec[idx] = 90-flt;
    if (op == OpRotate) flt = AngleMod360(flt);
    if (idx < 0) vec.x = vec.y = vec.z = flt; else vec[idx] = flt;
  }

  VMatrix4 mat;
  switch (op) {
    case OpScale:
      if (vec == TVec(1.0f, 1.0f, 1.0f)) return true;
      mat = VMatrix4::BuildScale(vec);
      break;
    case OpOffset:
      if (vec == TVec(0.0f, 0.0f, 0.0f)) return true;
      mat = VMatrix4::BuildOffset(vec);
      break;
    case OpRotate:
      if (vec == TVec(0.0f, 0.0f, 0.0f)) return true;
      mat = VMatrix4::Identity;
      if (vec.y != 0.0f) mat *= VMatrix4::RotateY(vec.y); // roll
      if (vec.z != 0.0f) mat *= VMatrix4::RotateZ(vec.z); // yaw
      if (vec.x != 0.0f) mat *= VMatrix4::RotateX(vec.x); // pitch
      break;
    default: __builtin_trap(); // the thing that should not be
  }

  trans *= mat;
  return true;
}


//==========================================================================
//
//  parseTransNode
//
//==========================================================================
static bool parseTransNode (VXmlNode *SN, VMatrix4 &trans, const VMatrix4 &orig) {
  if (!SN) return false;
  if (SN->Name != "transform") return false;

  for (VXmlNode *XN = SN->FirstChild; XN; XN = XN->NextSibling) {
    if (XN->Name == "reset") {
      if (XN->FirstChild) {
        Sys_Error("node '%s' cannot have children at %s", *XN->Name, *XN->Loc.toStringNoCol());
      }
      if (XN->Attributes.length()) {
        Sys_Error("node '%s' cannot have attributes at %s", *XN->Name, *XN->Loc.toStringNoCol());
      }
      trans = orig;
      continue;
    }

    if (XN->Name == "matrix") {
      auto bad = XN->FindBadAttribute("absolute", nullptr);
      if (bad) Sys_Error("%s: matrix definition has invalid attribute '%s'", *bad->Loc.toStringNoCol(), *bad->Name);
      VMatrix4 mat = VMatrix4::Identity;
      VStr s = XN->Value.xstrip();
      if (s != "identity") {
        for (int y = 0; y < 4; ++y) {
          for (int x = 0; x < 4; ++x) {
            s = s.xstripleft();
            int pos = 0;
            while (pos < s.length() && (uint8_t)s[pos] > 32) ++pos;
            if (pos == 0) Sys_Error("out of matrix values at %s", *XN->Loc.toStringNoCol());
            float res = 0.0f;
            if (!s.left(pos).convertFloat(&res) || !isfinite(res)) {
              Sys_Error("invalid matrix float at %s", *XN->Loc.toStringNoCol());
            }
            mat.m[y][x] = res;
            s.chopLeft(pos);
          }
        }
        s = s.xstrip();
        if (s.length() != 0) Sys_Error("extra matrix data at %s", *XN->Loc.toStringNoCol());
      }
      if (ParseBool(XN, "absolute", false)) trans = mat; else trans = trans*mat;
      continue;
    }

    //if (parseTransNodeChild(XN, trans)) continue;
    if (performTransOperation(XN, trans)) continue;
    Sys_Error("unknown transform node '%s' at %s", *XN->Name, *XN->Loc.toStringNoCol());
  }

  //trans.MatTrans.decompose(trans.DecTrans);
  return true;
}


//==========================================================================
//
//  parseRotCenterNode
//
//==========================================================================
static bool parseRotCenterNode (VXmlNode *SN, TVec &rotCenter) {
  if (!SN) return false;
  if (SN->Name != "rotcenter") return false;

  if (SN->FirstChild) {
    Sys_Error("node '%s' cannot have children at %s", *SN->Name, *SN->Loc.toStringNoCol());
  }

  rotCenter = TVec::ZeroVector;

  // no attrs means "reset rotation center"
  for (int f = 0; f < SN->Attributes.length(); ++f) {
    int idx;
         if (SN->Attributes[f].Name == "x") idx = 0;
    else if (SN->Attributes[f].Name == "y") idx = 1;
    else if (SN->Attributes[f].Name == "z") idx = 2;
    else Sys_Error("%s: invalid node '%s' attribute '%s'", *SN->Loc.toStringNoCol(), *SN->Name, *SN->Attributes[f].Name);
    VStr s = SN->Attributes[f].Value.xstrip();
    float res = 0.0f;
    if (!s.convertFloat(&res) || !isfinite(res)) {
      Sys_Error("%s: invalid node '%s' attribute '%s' value '%s'", *SN->Loc.toStringNoCol(), *SN->Name, *SN->Attributes[f].Name, *s);
    }
    rotCenter[idx] = res;
  }

  return true;
}


//==========================================================================
//
//  dumpMatrix
//
//==========================================================================
static void __attribute__((used)) dumpMatrix (const char *pfx, const VMatrix4 &mt) {
  GCon->Logf("%s(%g, %g, %g, %g)", pfx, mt.m[0][0], mt.m[0][1], mt.m[0][2], mt.m[0][3]);
  GCon->Logf("%s(%g, %g, %g, %g)", pfx, mt.m[1][0], mt.m[1][1], mt.m[1][2], mt.m[1][3]);
  GCon->Logf("%s(%g, %g, %g, %g)", pfx, mt.m[2][0], mt.m[2][1], mt.m[2][2], mt.m[2][3]);
  GCon->Logf("%s(%g, %g, %g, %g)", pfx, mt.m[3][0], mt.m[3][1], mt.m[3][2], mt.m[3][3]);
}


//==========================================================================
//
//  ParseTransform
//
//  `trans` must be initialised
//
//==========================================================================
static void ParseTransform (VStr mfile, int mindex, VXmlNode *SN, VMatrix4 &trans) {
  TVec shift = TVec::ZeroVector;
  TVec offset = TVec::ZeroVector;
  TVec scale = TVec(1.0f, 1.0f, 1.0f);

  ParseVector(SN, shift, "shift", false);
  ParseVector(SN, offset, "offset", false);
  ParseVector(SN, scale, "scale", true);

  VMatrix4 shiftmat = VMatrix4::BuildOffset(shift);
  VMatrix4 scalemat = VMatrix4::BuildScale(scale);
  VMatrix4 ofsmat = VMatrix4::BuildOffset(offset);

  VMatrix4 finmt = trans;
  finmt *= shiftmat;
  finmt *= scalemat;
  finmt *= ofsmat;

  trans = finmt;
}


//==========================================================================
//
//  FindClassStateByIndex
//
//==========================================================================
static VState *FindClassStateByIndex (VClass *cls, int idx) {
  if (!cls) return nullptr;
  for (VState *s = cls->States; s; s = s->Next) {
    if (s->InClassIndex == idx && (s->Frame&VState::FF_SKIPMODEL) == 0) return s;
  }
  return nullptr;
}


//==========================================================================
//
//  IsValidSpriteFrame
//
//  inexisting sprites are valid!
//
//==========================================================================
static bool IsValidSpriteFrame (int lump, VName sprname, int sprframe, bool iwadonly, bool thiswadonly, bool prevvalid=true) {
  if (lump < 0) return true;
  int sprindex = VClass::FindSprite(sprname); /* don't append */
  if (sprindex <= 1) return prevvalid; // <0: not found; 0: tnt1; 1: ----
  if (!cli_IgnoreReplaced && IsDehReplacedSprite(sprname)) {
    //GCon->Logf(NAME_Debug, "model: ignore replaced sprite '%s'", *sprname);
    return false;
  }
  SpriteTexInfo txnfo;
  if (!R_GetSpriteTextureInfo(&txnfo, sprindex, sprframe)) return prevvalid;
  VTexture *basetex = GTextureManager[txnfo.texid];
  if (!basetex) return prevvalid;
  if (!cli_IgnoreRestrictions) {
    if (iwadonly && !W_IsIWADLump(basetex->SourceLump)) return false;
    if (thiswadonly && W_LumpFile(basetex->SourceLump) != W_LumpFile(lump)) return false;
  }
  return true;
}


//==========================================================================
//
//  IsValidSpriteState
//
//  inexisting states/sprites are valid!
//
//==========================================================================
static bool IsValidSpriteState (int lump, VState *state, bool iwadonly, bool thiswadonly, bool prevvalid=true) {
  if (lump < 0) return prevvalid;
  if (!state) return prevvalid;
  return IsValidSpriteFrame(lump, state->SpriteName, (state->Frame&VState::FF_FRAMEMASK), iwadonly, thiswadonly, prevvalid);
}


//==========================================================================
//
//  ParseModelXml
//
//==========================================================================
static void ParseModelXml (int lump, VModel *Mdl, VXmlDocument *Doc, bool isGZDoom=false) {
  // verify that it's a model definition file
  if (Doc->Root.Name != "vavoom_model_definition") Sys_Error("%s: %s is not a valid model definition file", *Doc->Root.Loc.toStringNoCol(), *Mdl->Name);

  // check top-level nodes
  {
    auto bad = Doc->Root.FindBadChild("model", "class", nullptr);
    if (bad) Sys_Error("%s: model file has invalid node '%s'", *bad->Loc.toStringNoCol(), *bad->Name);
  }

  //if (Doc->Root.HasAttributes()) Sys_Error("%s: model file node should not have attributes", *Doc->Root.Loc.toStringNoCol());
  // check attrs
  {
    auto bad = Doc->Root.FindBadAttribute("noselfshadow", "noshadow",
                                          "fullbright", "usedepth",
                                          "iwadonly", "thiswadonly",
                                          "rotation", "bobbing",
                                          nullptr);
    if (bad) Sys_Error("%s: model file has invalid attribute '%s'", *bad->Loc.toStringNoCol(), *bad->Name);
  }

  Mdl->DefaultClass = nullptr;
  // "noselfshadow" means "don't cast shadow from self light source"
  // this is generally the case for light decorations, and doesn't hurt for others
  const bool globNoSelfShadow = ParseBool(&Doc->Root, "noselfshadow", true);
  const bool globNoShadow = ParseBool(&Doc->Root, "noshadow", false);
  const bool globFullBright = ParseBool(&Doc->Root, "fullbright", false);
  const bool globUseDepth = ParseBool(&Doc->Root, "usedepth", false);
  const bool globIWadOnly = ParseBool(&Doc->Root, "iwadonly", false);
  const bool globThisWadOnly = ParseBool(&Doc->Root, "thiswadonly", false);
  const bool globRotation = ParseBool(&Doc->Root, "rotation", false);
  const bool globBobbing = ParseBool(&Doc->Root, "bobbing", false);

  // process model definitions
  for (VXmlNode *ModelNode : Doc->Root.childrenWithName("model")) {
    // check attrs
    {
      auto bad = ModelNode->FindBadAttribute("name", "noselfshadow", nullptr);
      if (bad) Sys_Error("%s: model declaration has invalid attribute '%s'", *bad->Loc.toStringNoCol(), *bad->Name);
    }

    VScriptModel &SMdl = Mdl->Models.Alloc();
    SMdl.Name = *ModelNode->GetAttribute("name");
    SMdl.HasAlphaMul = false;
    //SMdl.NextDefFrame = 0;
    if (SMdl.Name == NAME_None) Sys_Error("%s: model declaration has empty name", *ModelNode->Loc.toStringNoCol());

    // check nodes
    {
      auto bad = ModelNode->FindBadChild("md2", "md3", "kvx", nullptr);
      if (bad) Sys_Error("%s: model '%s' definition has invalid node '%s'", *bad->Loc.toStringNoCol(), *Mdl->Name, *bad->Name);
    }

    // process model parts
    for (VXmlNode *ModelDefNode : ModelNode->allChildren()) {
      if (!ModelDefNode->Name.strEqu("md2") &&
          !ModelDefNode->Name.strEqu("md3") &&
          !ModelDefNode->Name.strEqu("kvx"))
      {
        continue; //Sys_Error("%s: invalid model '%s' definition node '%s'", *ModelDefNode->Loc.toStringNoCol(), *Mdl->Name, mdx);
      }
      const bool isMD3 = ModelDefNode->Name.strEqu("md3");
      const bool isVoxel = ModelDefNode->Name.strEqu("kvx");
      bool useVoxelPivotZ = false;
      if (isVoxel && ModelDefNode->HasAttribute("pivotz")) {
        useVoxelPivotZ = ParseBool(ModelDefNode, "pivotz", false);
      }

      // check attrs
      {
        auto bad = ModelDefNode->FindBadAttribute("file", "mesh_index", "version", "position_file",
                                                  "skin_anim_speed", "skin_anim_range",
                                                  "fullbright", "noshadow", "usedepth", "allowtransparency",
                                                  "shift", "shift_x", "shift_y", "shift_z",
                                                  "offset", "offset_x", "offset_y", "offset_z",
                                                  "scale", "scale_x", "scale_y", "scale_z",
                                                  "pivotz", "alpha_mul",
                                                  nullptr);
        if (bad) Sys_Error("%s: model '%s' definition has invalid attribute '%s'", *bad->Loc.toStringNoCol(), *Mdl->Name, *bad->Name);
      }

      // check nodes
      {
        auto bad = ModelDefNode->FindBadChild("frame", "skin", "subskin", "transform", nullptr);
        if (bad) Sys_Error("%s: model '%s' definition has invalid node '%s'", *bad->Loc.toStringNoCol(), *Mdl->Name, *bad->Name);
      }

      VScriptSubModel *Md2 = &SMdl.SubModels.Alloc();
      const int Md2Index = SMdl.SubModels.length()-1;

      bool hasMeshIndex = false;
      Md2->MeshIndex = 0;
      if (ModelDefNode->HasAttribute("mesh_index")) {
        if (isVoxel) Sys_Error("%s: voxel model '%s' definition cannot have \"mesh_index\"", *ModelDefNode->Loc.toStringNoCol(), *Mdl->Name);
        hasMeshIndex = true;
        Md2->MeshIndex = VStr::atoi(*ModelDefNode->GetAttribute("mesh_index"));
      }

      if (!ModelDefNode->HasAttribute("file")) Sys_Error("%s: model '%s' has no file", *ModelDefNode->Loc.toStringNoCol(), *Mdl->Name);
      VStr mfile = ModelDefNode->GetAttribute("file").ToLower().FixFileSlashes();
      Md2->Model = Mod_FindMeshModel(Mdl->Name, mfile, Md2->MeshIndex, isVoxel, useVoxelPivotZ);

      // version
      Md2->Version = ParseIntWithDefault(ModelDefNode, "version", -1);

      // position model
      Md2->PositionModel = nullptr;
      if (ModelDefNode->HasAttribute("position_file")) {
        if (isVoxel) Sys_Error("%s: voxel model '%s' definition cannot have \"position_file\"", *ModelDefNode->Loc.toStringNoCol(), *Mdl->Name);
        Md2->PositionModel = Mod_FindMeshModel(Mdl->Name, ModelDefNode->GetAttribute("position_file").ToLower().FixFileSlashes(), Md2->MeshIndex, isVoxel, useVoxelPivotZ);
      }

      // skin animation
      Md2->SkinAnimSpeed = 0;
      Md2->SkinAnimRange = 0;
      if (ModelDefNode->HasAttribute("skin_anim_speed")) {
        if (isVoxel) Sys_Error("%s: voxel model '%s' definition cannot have \"skin_anim_speed\"", *ModelDefNode->Loc.toStringNoCol(), *Mdl->Name);
        if (!ModelDefNode->HasAttribute("skin_anim_range")) Sys_Error("'skin_anim_speed' requires 'skin_anim_range'");
        Md2->SkinAnimSpeed = ParseIntWithDefault(ModelDefNode, "skin_anim_speed", 1);
        Md2->SkinAnimRange = ParseIntWithDefault(ModelDefNode, "skin_anim_range", 1);
      }

      AliasModelTrans BaseTrans;
      ParseTransform(mfile, Md2->MeshIndex, ModelDefNode, BaseTrans.MatTrans);

      // fullbright flag
      Md2->FullBright = ParseBool(ModelDefNode, "fullbright", globFullBright);
      // no shadow flag
      Md2->NoShadow = ParseBool(ModelDefNode, "noshadow", globNoShadow);
      // force depth test flag (for things like monsters with alpha transaparency)
      Md2->UseDepth = ParseBool(ModelDefNode, "usedepth", globUseDepth);

      // allow transparency in skin files
      // for skins that are transparent in solid models (Alpha = 1.0f)
      Md2->AllowTransparency = ParseBool(ModelDefNode, "allowtransparency", false);

      Md2->AlphaMul = ParseFloatWithDefault(ModelDefNode, "alpha_mul", 1.0f);
           if (Md2->AlphaMul <= 0.0f) Md2->AlphaMul = 0.0f;
      else if (Md2->AlphaMul >= 1.0f) Md2->AlphaMul = 1.0f;
      else SMdl.HasAlphaMul = true;

      // process frames
      int curframeindex = 0;
      AliasModelTrans savedTrans = BaseTrans;
      for (VXmlNode *FrameDefNode = ModelDefNode->FirstChild; FrameDefNode; FrameDefNode = FrameDefNode->NextSibling) {
        // global matrix
        if (parseTransNode(FrameDefNode, BaseTrans.MatTrans, savedTrans.MatTrans)) {
          savedTrans = BaseTrans;
          continue;
        }
        // global rotcenter
        if (parseRotCenterNode(FrameDefNode, BaseTrans.RotCenter)) {
          savedTrans = BaseTrans;
          continue;
        }
        if (FrameDefNode->Name == "gzscale") {
          BaseTrans.gzScale = parseGZScaleOfs(FrameDefNode, TVec(1.0f, 1.0f, 1.0f));
          savedTrans = BaseTrans;
          continue;
        }
        if (FrameDefNode->Name == "gzoffset") {
          BaseTrans.gzPreScaleOfs = parseGZScaleOfs(FrameDefNode, TVec::ZeroVector);
          savedTrans = BaseTrans;
          continue;
        }

        if (FrameDefNode->Name != "frame") continue;
        //if (FrameDefNode->HasChildren()) Sys_Error("%s: model '%s' frame definition should have no children", *FrameDefNode->Loc.toStringNoCol(), *Mdl->Name);

        {
          auto bad = FrameDefNode->FindBadAttribute("index", "end_index", "count",
                                                    "position_index", "alpha_start", "alpha_end", "skin_index",
                                                    "shift", "shift_x", "shift_y", "shift_z",
                                                    "offset", "offset_x", "offset_y", "offset_z",
                                                    "scale", "scale_x", "scale_y", "scale_z", nullptr);
          if (bad) Sys_Error("%s: model '%s' frame definition has invalid attribute '%s'", *bad->Loc.toStringNoCol(), *Mdl->Name, *bad->Name);
        }

        // check children
        {
          auto bad = FrameDefNode->FindBadChild("transform", "gzscale", "gzoffset", nullptr);
          if (bad) Sys_Error("%s: model '%s' frame definition has invalid node '%s'", *bad->Loc.toStringNoCol(), *Mdl->Name, *bad->Name);
        }

        int sidx = curframeindex, eidx = curframeindex;

        if (FrameDefNode->HasAttribute("index")) {
          sidx = ParseIntWithDefault(FrameDefNode, "index", curframeindex);
          if (sidx < 0 || sidx > 65535) Sys_Error("%s: model '%s' frame definition has invalid index %d", *FrameDefNode->Loc.toStringNoCol(), *Mdl->Name, sidx);
          eidx = sidx;
        }

        if (FrameDefNode->HasAttribute("count")) {
          auto bad = FrameDefNode->FindFirstAttributeOf("end_index", nullptr);
          if (bad) Sys_Error("%s: model '%s' frame definition has invalid attribute '%s' (count/endindex conflict)", *bad->Loc.toStringNoCol(), *Mdl->Name, *bad->Name);
          const int count = ParseIntWithDefault(FrameDefNode, "count", 0);
          if (count <= 0 || count > 65535) Sys_Error("%s: model '%s' frame definition has invalid count %d", *FrameDefNode->Loc.toStringNoCol(), *Mdl->Name, count);
          eidx = sidx+count-1;
        } else if (FrameDefNode->HasAttribute("end_index")) {
          auto bad = FrameDefNode->FindFirstAttributeOf("count", nullptr);
          if (bad) Sys_Error("%s: model '%s' frame definition has invalid attribute '%s' (endindex/count conflict)", *bad->Loc.toStringNoCol(), *Mdl->Name, *bad->Name);
          eidx = ParseIntWithDefault(FrameDefNode, "end_index", sidx);
          if (eidx < sidx || eidx > 65535) Sys_Error("%s: model '%s' frame definition has invalid end index %d", *FrameDefNode->Loc.toStringNoCol(), *Mdl->Name, eidx);
        }
        //GCon->Logf(NAME_Debug, "%s: MDL '%s': sidx=%d; eidx=%d", *FrameDefNode->Loc.toStringNoCol(), *Mdl->Name, sidx, eidx);

        // frame transformations
        AliasModelTrans XFTrans = BaseTrans;
        ParseTransform(mfile, Md2->MeshIndex, FrameDefNode, XFTrans.MatTrans);

        for (VXmlNode *xt = FrameDefNode->FirstChild; xt; xt = xt->NextSibling) {
          if (parseTransNode(xt, XFTrans.MatTrans, BaseTrans.MatTrans)) continue;
          if (parseRotCenterNode(xt, XFTrans.RotCenter)) continue;
          if (xt->Name == "gzscale") {
            XFTrans.gzScale = parseGZScaleOfs(xt, TVec(1.0f, 1.0f, 1.0f));
            continue;
          }
          if (xt->Name == "gzoffset") {
            XFTrans.gzPreScaleOfs = parseGZScaleOfs(xt, TVec::ZeroVector);
            continue;
          }
          Sys_Error("unknown frame node '%s' at %s", *xt->Name, *xt->Loc.toStringNoCol());
        }

        XFTrans.decompose();

        const int posidx = ParseIntWithDefault(FrameDefNode, "position_index", 0);
        const float alphaStart = ParseFloatWithDefault(FrameDefNode, "alpha_start", 1.0f);
        const float alphaEnd = ParseFloatWithDefault(FrameDefNode, "alpha_end", 1.0f);
        const int fsknidx = ParseIntWithDefault(FrameDefNode, "skin_index", -1);

        for (int fnum = sidx; fnum <= eidx; ++fnum) {
          VScriptSubModel::VFrame &F = Md2->Frames.Alloc();
          // frame index
          F.Index = fnum;
          // position model frame index
          F.PositionIndex = posidx;
          // frame transformation
          F.Transform = XFTrans;
          // alpha
          F.AlphaStart = alphaStart;
          F.AlphaEnd = alphaEnd;
          // skin index
          F.SkinIndex = fsknidx;
        }

        curframeindex = eidx+1;
      }

      // no frames? add one
      if (Md2->Frames.length() == 0) {
        VScriptSubModel::VFrame &F = Md2->Frames.Alloc();
        // frame index
        F.Index = 0;
        // position model frame index
        F.PositionIndex = 0;
        // frame transformation
        F.Transform = BaseTrans;
        F.Transform.decompose();
        // alpha
        F.AlphaStart = 1.0f;
        F.AlphaEnd = 1.0f;
        // skin index
        F.SkinIndex = -1;
      }

      // process skins
      for (VXmlNode *SkN : ModelDefNode->childrenWithName("skin")) {
        if (isVoxel) Sys_Error("%s: voxel model '%s' definition cannot have skins", *SkN->Loc.toStringNoCol(), *Mdl->Name);
        if (SkN->HasChildren()) Sys_Error("%s: model '%s' skin definition should have no children", *SkN->Loc.toStringNoCol(), *Mdl->Name);
        {
          auto bad = SkN->FindBadAttribute("file", "shade", nullptr);
          if (bad) Sys_Error("%s: model '%s' skin definition has invalid attribute '%s'", *bad->Loc.toStringNoCol(), *Mdl->Name, *bad->Name);
        }
        VStr sfl = SkN->GetAttribute("file").ToLower().FixFileSlashes();
        if (sfl.length()) {
          if (sfl.indexOf('/') < 0) sfl = Md2->Model->Name.ExtractFilePath()+sfl;
          if (mdl_verbose_loading > 2) {
            GCon->Logf("%s: model '%s': skin file '%s'", *SkN->Loc.toStringNoCol(), *SMdl.Name, *sfl);
          }
          VMeshModel::SkinInfo &si = Md2->Skins.alloc();
          si.fileName = *sfl;
          si.textureId = -1;
          si.shade = -1;
          if (SkN->HasAttribute("shade")) {
            sfl = SkN->GetAttribute("shade");
            si.shade = parseHexRGB(sfl);
          }
        }
      }

      // if this is MD3 without mesh index, create additional models for all meshes
      if (!hasMeshIndex && isMD3) {
        // load model and get number of meshes
        VStream *md3strm = FL_OpenFileRead(Md2->Model->Name);
        // allow missing models
        if (md3strm) {
          int fsidx = SMdl.SubModels.length()-1;
          char sign[4] = {0};
          md3strm->Serialise(sign, 4);
          if (memcmp(sign, "IDP3", 4) == 0) {
            // skip uninteresting data
            md3strm->Seek(4+4+64+4+4+4);
            vuint32 n = 0;
            md3strm->Serialise(&n, 4);
            n = LittleLong(n);
            if (n > 1 && n < 64) {
              if (mdl_debug_md3.asBool()) {
                GCon->Logf(NAME_Init, "%s: model '%s' got automatic submodel%s for %u more mesh%s",
                           *ModelDefNode->Loc.toStringNoCol(), *Md2->Model->Name,
                           (n > 2 ? "s" : ""), n-1, (n > 2 ? "es" : ""));
              }
              for (unsigned f = 1; f < n; ++f) {
                VScriptSubModel &newmdl = SMdl.SubModels.Alloc();
                Md2 = &SMdl.SubModels[Md2Index]; // this pointer may change, so refresh it
                newmdl.copyFrom(*Md2);
                newmdl.MeshIndex = f;
                newmdl.Model = Mod_FindMeshModel(Mdl->Name, newmdl.Model->Name, newmdl.MeshIndex, isVoxel, useVoxelPivotZ);
                if (newmdl.PositionModel) {
                  newmdl.PositionModel = Mod_FindMeshModel(Mdl->Name, newmdl.PositionModel->Name, newmdl.MeshIndex, isVoxel, useVoxelPivotZ);
                }
              }
            } else {
              if (n != 1) {
                GCon->Logf(NAME_Warning, "%s: model '%s' has invalid number of meshes (%u)",
                           *ModelDefNode->Loc.toStringNoCol(), *Md2->Model->Name, n);
              }
            }
          }
          delete md3strm;
          // load subskins
          TArray<VMeshModel::SkinInfo> SubSkins;
          for (VXmlNode *SkN : ModelDefNode->childrenWithName("subskin")) {
            if (SkN->HasChildren()) Sys_Error("%s: model '%s' subskin definition should have no children", *SkN->Loc.toStringNoCol(), *Mdl->Name);
            {
              auto bad = SkN->FindBadAttribute("file", "shade", "submodel_index", nullptr);
              if (bad) Sys_Error("%s: model '%s' skin definition has invalid attribute '%s'", *bad->Loc.toStringNoCol(), *Mdl->Name, *bad->Name);
            }
            int subidx = ParseIntWithDefault(SkN, "submodel_index", -1);
            VStr sfl = SkN->GetAttribute("file").ToLower().FixFileSlashes();
            if (sfl.length() && subidx >= 0 && subidx < 1024) {
              if (sfl.indexOf('/') < 0) sfl = Md2->Model->Name.ExtractFilePath()+sfl;
              if (mdl_verbose_loading > 2) {
                GCon->Logf("%s: model '%s': skin file '%s'", *SkN->Loc.toStringNoCol(), *SMdl.Name, *sfl);
              }
              while (SubSkins.length() <= subidx) SubSkins.alloc();
              VMeshModel::SkinInfo &si = SubSkins[subidx];
              si.fileName = *sfl;
              si.textureId = -1;
              si.shade = -1;
              if (SkN->HasAttribute("shade")) {
                sfl = SkN->GetAttribute("shade");
                si.shade = parseHexRGB(sfl);
              }
            }
          }
          //if (SubSkins.length() != 0) Sys_Error("found subskins for model '%s'", *SMdl.Name);
          // process subskins
          if (SubSkins.length()) {
            for (int f = fsidx; f < SMdl.SubModels.length(); ++f) {
              int skidx = f-fsidx;
              if (skidx >= 0 && skidx < SubSkins.length() && SubSkins[skidx].fileName != NAME_None) {
                VScriptSubModel &smdl = SMdl.SubModels[f];
                smdl.Skins.append(SubSkins[skidx]);
                for (auto &&frm : smdl.Frames) frm.SkinIndex = smdl.Skins.length()-1;
              }
            }
            //GCon->Logf(NAME_Init, "processed %d subskins for model '%s'", SubSkins.length(), *SMdl.Name);
          }
        }
      }
    }
  }

  bool ClassDefined = false;
  for (VXmlNode *ClassDefNode : Doc->Root.childrenWithName("class")) {
    // check attrs
    {
      auto bad = ClassDefNode->FindBadAttribute("name", "noselfshadow",
                                                "iwadonly", "thiswadonly",
                                                "rotation", "bobbing",
                                                "usepitch", "useroll",
                                                "random_zoffset_min", "random_zoffset_max",
                                                "gzdoom", "spriteshadow", nullptr);
      if (bad) Sys_Error("%s: model '%s' class definition has invalid attribute '%s'", *bad->Loc.toStringNoCol(), *Mdl->Name, *bad->Name);
    }

    // check nodes
    {
      auto bad = ClassDefNode->FindBadChild("state", nullptr);
      if (bad) Sys_Error("%s: model '%s' class definition has invalid node '%s'", *bad->Loc.toStringNoCol(), *Mdl->Name, *bad->Name);
    }

    const bool hasClassUsePitch = ClassDefNode->HasAttribute("usepitch");
    VStr classUsePitch = (hasClassUsePitch ? ClassDefNode->GetAttribute("usepitch") : VStr::EmptyString);

    const bool hasClassUseRoll = ClassDefNode->HasAttribute("useroll");
    VStr classUseRoll = (hasClassUseRoll ? ClassDefNode->GetAttribute("useroll") : VStr::EmptyString);

    VStr vcClassName = ClassDefNode->GetAttribute("name");
    VClass *xcls = VClass::FindClassNoCase(*vcClassName);
    if (xcls && !xcls->IsChildOf(VEntity::StaticClass())) xcls = nullptr;
    if (xcls) {
      if (developer) {
        GCon->Logf(NAME_Dev, "%s: found 3d model for class `%s`", *ClassDefNode->Loc.toStringNoCol(), xcls->GetName());
      }
      //GCon->Logf(NAME_Debug, "%s: found 3d model for class `%s`", *ClassDefNode->Loc.toStringNoCol(), xcls->GetName());
    } else {
      GCon->Logf(NAME_Init, "%s: found 3d model for unknown class `%s`",
                 *ClassDefNode->Loc.toStringNoCol(), *vcClassName);
    }

    VClassModelScript *Cls = new VClassModelScript();
    const bool classGZDoom = ParseBool(ClassDefNode, "gzdoom", false);
    Cls->Model = Mdl;
    Cls->Name = (xcls ? xcls->GetName() : *vcClassName);
    Cls->noSelfShadow = ParseBool(ClassDefNode, "noselfshadow", globNoSelfShadow);
    Cls->oneForAll = false;
    Cls->frameCacheBuilt = false;
    Cls->isGZDoom = isGZDoom;
    Cls->asTranslucent = false;
    Cls->spriteShadow = ParseBool(ClassDefNode, "spriteshadow", false);
    Cls->iwadonly = ParseBool(ClassDefNode, "iwadonly", globIWadOnly);
    Cls->thiswadonly = ParseBool(ClassDefNode, "thiswadonly", globThisWadOnly);
    ModelZOffset defzoffset;
    ParseRndZOffset(ClassDefNode, defzoffset);

    bool deleteIt = !xcls;
    if (!deleteIt && cli_IgnoreModelClass.has(*Cls->Name)) {
      GCon->Logf(NAME_Init, "%s: model '%s' ignored by user request",
                 *ClassDefNode->Loc.toStringNoCol(), *Cls->Name);
      deleteIt = true;
    }
    if (!deleteIt && xcls) {
      if (!Mdl->DefaultClass) Mdl->DefaultClass = Cls;
      ClassModels.Append(Cls);
      ClassModelMapNeedRebuild = true;
    }
    ClassDefined = true;
    //GCon->Logf("found model for class '%s'", *Cls->Name);

    bool hasOneAll = false;
    bool hasOthers = false;
    bool prevValid = true;
    bool hasDisabled = false;
    int frmFirstIdx = Cls->Frames.length();

    const bool defrot = ParseBool(ClassDefNode, "rotation", globRotation);
    const bool defbob = ParseBool(ClassDefNode, "bobbing", globBobbing);

    // process frames
    for (VXmlNode *StateDefNode : ClassDefNode->childrenWithName("state")) {
      if (StateDefNode->HasChildren()) Sys_Error("%s: model '%s' class definition should have no children", *StateDefNode->Loc.toStringNoCol(), *Mdl->Name);
      // check attrs
      {
        auto bad = StateDefNode->FindBadAttribute("angle_yaw", "angle_pitch", "angle_roll",
                                                  "rotate_yaw", "rotate_pitch", "rotate_roll",
                                                  "rotation", "bobbing",
                                                  "rotation_speed", "bobbing_speed",
                                                  "gzdoom", "usepitch", "useroll",
                                                  "index", "last_index", "sprite", "sprite_frame",
                                                  "model", "frame_index", "submodel_index", "hidden",
                                                  "inter", "angle_start", "angle_end", "alpha_start", "alpha_end",
                                                  "random_zoffset_min", "random_zoffset_max",
                                                  nullptr);
        if (bad) Sys_Error("%s: model '%s' class state definition has invalid attribute '%s'", *bad->Loc.toStringNoCol(), *Mdl->Name, *bad->Name);
      }

      VScriptedModelFrame &F = Cls->Frames.Alloc();

      ParseAngle(StateDefNode, "yaw", F.angleYaw);
      ParseAngle(StateDefNode, "pitch", F.anglePitch);
      ParseAngle(StateDefNode, "roll", F.angleRoll);

      F.zoffset = defzoffset;
      ParseRndZOffset(StateDefNode, F.zoffset);

      F.rotateSpeed = ParseFloatWithDefault(StateDefNode, "rotation_speed", -1.0f);
      F.bobSpeed = ParseFloatWithDefault(StateDefNode, "bobbing_speed", -1.0f);

      if (!ParseBool(StateDefNode, "rotation", defrot)) F.rotateSpeed = 0.0f;
      if (!ParseBool(StateDefNode, "bobbing", defbob)) F.bobSpeed = 0.0f;
      if (F.rotateSpeed < 0.0f) F.rotateSpeed = 100.0f;
      if (F.bobSpeed < 0.0f) F.bobSpeed = 180.0f;

      // some special things
      F.gzdoom = ParseBool(StateDefNode, "gzdoom", classGZDoom);
      if (F.gzdoom) {
        F.gzActorPitchInverted = false;
        F.gzActorPitch = MdlAndle_DontUse; // don't use actor pitch
        F.gzActorRoll = MdlAndle_DontUse; // don't use actor roll
      } else {
        F.gzActorPitchInverted = false;
        F.gzActorPitch = MdlAndle_FromActor; // default
        //F.gzActorPitch = DontUse; // default
        F.gzActorRoll = MdlAndle_DontUse; // don't use actor roll
      }

      if (hasClassUsePitch || StateDefNode->HasAttribute("usepitch")) {
        VStr pv = (StateDefNode->HasAttribute("usepitch") ? StateDefNode->GetAttribute("usepitch") : classUsePitch);
             if (pv == "actor") F.gzActorPitch = MdlAndle_FromActor;
        else if (pv == "momentum") F.gzActorPitch = MdlAndle_FromMomentum;
        else if (pv == "actor-inverted") { F.gzActorPitch = MdlAndle_FromActor; F.gzActorPitchInverted = true; }
        else if (pv == "momentum-inverted") { F.gzActorPitch = MdlAndle_FromMomentum; F.gzActorPitchInverted = true; }
        else if (isFalseValue(pv)) F.gzActorPitch = MdlAndle_DontUse;
        else if (pv == "default") {}
        else Sys_Error("%s: invalid \"usepitch\" attribute value '%s'", *StateDefNode->Loc.toStringNoCol(), *pv);
      }

      if (hasClassUseRoll || StateDefNode->HasAttribute("useroll")) {
        VStr pv = (StateDefNode->HasAttribute("useroll") ? StateDefNode->GetAttribute("useroll") : classUseRoll);
             if (pv == "actor") F.gzActorRoll = MdlAndle_FromActor;
        else if (pv == "momentum") F.gzActorRoll = MdlAndle_FromMomentum;
        else if (isFalseValue(pv)) F.gzActorRoll = MdlAndle_DontUse;
        else if (pv == "default") {}
        else Sys_Error("%s: invalid \"useroll\" attribute value '%s'", *StateDefNode->Loc.toStringNoCol(), *pv);
      }

      int lastIndex = -666;
      if (StateDefNode->HasAttribute("index")) {
        auto bad = StateDefNode->FindFirstAttributeOf("sprite", "sprite_frame", nullptr);
        if (bad) Sys_Error("%s: model '%s' class state definition has invalid attribute '%s' (index/sprite conflict)", *bad->Loc.toStringNoCol(), *Mdl->Name, *bad->Name);

        F.Number = ParseIntWithDefault(StateDefNode, "index", 0);
        if (F.Number < 0) F.Number = -1;
        if (F.Number >= 0) {
          lastIndex = ParseIntWithDefault(StateDefNode, "last_index", lastIndex);
        } else {
          if (StateDefNode->HasAttribute("last_index")) Sys_Error("Model '%s' has state range for \"all frames\"", *Mdl->Name);
        }
        F.sprite = NAME_None;
        F.frame = -1;
        /*
        {
          VState *dbgst = FindClassStateByIndex(xcls, F.Number);
          if (dbgst) GCon->Logf(NAME_Warning, "%s:%s:%d: %s (%s %c)", *Mdl->Name, *Cls->Name, F.Number, *dbgst->Loc.toStringNoCol(), *dbgst->SpriteName, (dbgst->Frame&VState::FF_FRAMEMASK)+'A');
        }
        */
        if (Cls->iwadonly || Cls->thiswadonly) {
          if (!xcls || !IsValidSpriteState(lump, FindClassStateByIndex(xcls, F.Number), Cls->iwadonly, Cls->thiswadonly, prevValid)) {
            prevValid = false;
            F.disabled = true;
            hasDisabled = true;
            if (!deleteIt) {
              GCon->Logf(NAME_Warning, "%s: skipped model '%s' class '%s' state #%d due to iwadonly restriction",
                         *StateDefNode->Loc.toStringNoCol(), *Mdl->Name, *Cls->Name, F.Number);
            }
          }
        } else if (!IsValidSpriteState(lump, FindClassStateByIndex(xcls, F.Number), false, false, true)) {
          F.disabled = true;
          hasDisabled = true;
          if (!deleteIt) {
            GCon->Logf(NAME_Warning, "%s: skipped model '%s' class '%s' state #%d due to replaced sprite",
                       *StateDefNode->Loc.toStringNoCol(), *Mdl->Name, *Cls->Name, F.Number);
          }
        } else {
          prevValid = true;
        }
      } else if (StateDefNode->HasAttribute("sprite") && StateDefNode->HasAttribute("sprite_frame")) {
        auto bad = StateDefNode->FindFirstAttributeOf("index", "last_index", nullptr);
        if (bad) Sys_Error("%s: model '%s' class state definition has invalid attribute '%s' (sprite/index conflict)", *bad->Loc.toStringNoCol(), *Mdl->Name, *bad->Name);

        VStr sprnamestr = StateDefNode->GetAttribute("sprite");
        VName sprname = (deleteIt ? NAME_None : VName(*sprnamestr, VName::FindLower));
        if (sprname == NAME_None) {
          if (sprnamestr.length() != 4) {
            Sys_Error("%s: Model '%s' has invalid state (empty sprite name '%s')", *StateDefNode->Loc.toStringNoCol(), *Mdl->Name, *sprnamestr);
          }
          if (!deleteIt) {
            GCon->Logf(NAME_Warning, "%s: Model '%s' has unknown sprite '%s', state removed",
                       *StateDefNode->Loc.toStringNoCol(), *Mdl->Name, *sprnamestr);
          }
          //sprname = VName("....", VName::Add);
        }
        VStr sprframe = StateDefNode->GetAttribute("sprite_frame");
        if (sprframe.length() != 1) Sys_Error("%s: Model '%s' has invalid state (invalid sprite frame '%s')", *StateDefNode->Loc.toStringNoCol(), *Mdl->Name, *sprframe);
        int sfr = sprframe[0];
        if (sfr >= 'a' && sfr <= 'z') sfr = sfr-'a'+'A';
        sfr -= 'A';
        if (sfr < 0 || sfr > 31) Sys_Error("%s: Model '%s' has invalid state (invalid sprite frame '%s')", *StateDefNode->Loc.toStringNoCol(), *Mdl->Name, *sprframe);
        F.Number = -1;
        F.sprite = sprname;
        F.frame = sfr;
        // check sprite frame validity
        if (sprname == NAME_None) {
          F.disabled = true;
          // don't set "hasDisabled", we don't need to disable everything
        } else if (Cls->iwadonly || Cls->thiswadonly) {
          if (!IsValidSpriteFrame(lump, sprname, F.frame, Cls->iwadonly, Cls->thiswadonly, prevValid)) {
            prevValid = false;
            F.disabled = true;
            hasDisabled = true;
            if (!deleteIt) {
              GCon->Logf(NAME_Warning, "%s: skipped model '%s' class '%s' frame '%s%c' due to iwadonly restriction",
                         *StateDefNode->Loc.toStringNoCol(), *Mdl->Name, *Cls->Name, *sprname, sprframe[0]);
            }
          }
        } else if (!IsValidSpriteFrame(lump, sprname, F.frame, false, false, true)) {
          F.disabled = true;
          hasDisabled = true;
          if (!deleteIt) {
            GCon->Logf(NAME_Warning, "%s: skipped model '%s' class '%s' frame '%s%c' due to replaced sprite",
                       *StateDefNode->Loc.toStringNoCol(), *Mdl->Name, *Cls->Name, *sprname, sprframe[0]);
          }
        } else {
          prevValid = true;
        }
      } else {
        Sys_Error("%s: Model '%s' has invalid state", *StateDefNode->Loc.toStringNoCol(), *Mdl->Name);
      }

      F.SubModelIndex = ParseIntWithDefault(StateDefNode, "submodel_index", -1);
      if (F.SubModelIndex < 0) F.SubModelIndex = -1;

      if (!StateDefNode->HasAttribute("model")) {
        if (Mdl->Models.length() != 1) {
          Sys_Error("%s: model '%s' has no submodel name", *StateDefNode->Loc.toStringNoCol(),
                    *Mdl->Name);
        }
        F.ModelIndex = 0;
      } else {
        F.ModelIndex = -1;
        VStr MdlName = StateDefNode->GetAttribute("model");
        for (int i = 0; i < Mdl->Models.length(); ++i) {
          if (Mdl->Models[i].Name == *MdlName) {
            F.ModelIndex = i;
            break;
          }
        }
        if (F.ModelIndex == -1) {
          Sys_Error("%s: model '%s' has no submodel '%s'", *StateDefNode->Loc.toStringNoCol(),
                    *Mdl->Name, *MdlName);
        }
      }

      if (!StateDefNode->HasAttribute("frame_index")) {
        //F.FrameIndex = Mdl->Models[F.ModelIndex].NextDefFrame;
        int okfrm = true;
        const VScriptModel &sms = Mdl->Models[F.ModelIndex];
        for (int sid = 0; sid < sms.SubModels.length(); ++sid) {
          if (sms.SubModels[sid].Frames.length() != 1) {
            okfrm = false;
            break;
          }
        }
        if (!okfrm || sms.SubModels.length() == 0) {
          Sys_Error("%s: model '%s' has state without frame index", *StateDefNode->Loc.toStringNoCol(), *Mdl->Name);
        }
        F.FrameIndex = 0;
      } else {
        F.FrameIndex = ParseIntWithDefault(StateDefNode, "frame_index", 0);
      }
      //Mdl->Models[F.ModelIndex].NextDefFrame = F.FrameIndex+1;

      if (ParseBool(StateDefNode, "hidden", false)) F.SubModelIndex = -2; // hidden

      if (!Cls->asTranslucent && Mdl->Models[F.ModelIndex].HasAlphaMul) Cls->asTranslucent = true;

      F.Inter = ParseFloatWithDefault(StateDefNode, "inter", 0.0f);

      F.AngleStart = ParseFloatWithDefault(StateDefNode, "angle_start", 0.0f);
      F.AngleEnd = ParseFloatWithDefault(StateDefNode, "angle_end", 0.0f);

      F.AlphaStart = ParseFloatWithDefault(StateDefNode, "alpha_start", 1.0f);
      F.AlphaEnd = ParseFloatWithDefault(StateDefNode, "alpha_end", 1.0f);

      if (F.Number >= 0 && lastIndex > 0) {
        for (int cfidx = F.Number+1; cfidx <= lastIndex; ++cfidx) {
          VScriptedModelFrame &ffr = Cls->Frames.Alloc();
          ffr.copyFrom(F);
          ffr.Number = cfidx;
          if (Cls->iwadonly || Cls->thiswadonly) {
            if (!xcls || !IsValidSpriteState(lump, FindClassStateByIndex(xcls, ffr.Number), Cls->iwadonly, Cls->thiswadonly, prevValid)) {
              prevValid = false;
              ffr.disabled = true;
              hasDisabled = true;
              GCon->Logf(NAME_Warning, "%s: skipped model '%s' class '%s' state #%d due to iwadonly restriction",
                         *StateDefNode->Loc.toStringNoCol(), *Mdl->Name, *Cls->Name, ffr.Number);
            } else {
              prevValid = true;
            }
          }
        }
        hasOthers = true;
      } else {
        if (F.Number < 0 && F.sprite == NAME_None) {
          F.Number = -666;
          hasOneAll = true;
        } else {
          hasOthers = true;
        }
      }
    }

    if (hasDisabled) {
      if (frmFirstIdx == 0) {
        Cls->Frames.clear();
      } else {
        for (int f = frmFirstIdx; f < Cls->Frames.length(); ++f) Cls->Frames[f].disabled = true;
      }
    }

    if (Cls->Frames.length() == 0) {
      if (!deleteIt) {
        GCon->Logf(NAME_Warning, "%s: model '%s' class '%s' has no states defined",
        *ClassDefNode->Loc.toStringNoCol(), *Mdl->Name, *Cls->Name);
      }
      ClassModels.Remove(Cls);
      ClassModelMapNeedRebuild = true;
      deleteIt = true;
    }

    if (deleteIt) { delete Cls; continue; }
    if (hasOneAll && !hasOthers) {
      Cls->oneForAll = true;
      //GCon->Logf("model '%s' for class '%s' is \"one-for-all\"", *Mdl->Name, *Cls->Name);
    }
  }
  if (!ClassDefined) Sys_Error("%s: model '%s' defined no classes",
                               *Doc->Root.Loc.toStringNoCol(), *Mdl->Name);

  // we don't need the xml file anymore
  delete Doc;
}


//==========================================================================
//
//  ParseModelScript
//
//==========================================================================
static void ParseModelScript (int lump, VModel *Mdl, VStream &Strm, bool isGZDoom=false) {
  // parse xml file
  VXmlDocument *Doc = new VXmlDocument();
  Doc->Parse(Strm, Mdl->Name);
  ParseModelXml(lump, Mdl, Doc, isGZDoom);
}


//==========================================================================
//
//  Mod_FindName
//
//  used in VC `InstallModel()` and in model.xml loader
//
//==========================================================================
VModel *Mod_FindName (VStr name) {
  if (name.IsEmpty()) Sys_Error("Mod_ForName: nullptr name");

  // search the currently loaded models
  for (auto &&it : mod_known) if (it->Name.ICmp(name) == 0) return it;

  VModel *mod = new VModel();
  mod->Name = name;
  mod_known.Append(mod);

  // load the file
  int lump = -1;
  VStream *Strm = FL_OpenFileRead(mod->Name, &lump);
  if (!Strm) Sys_Error("Couldn't load `%s` (Mod_FindName)", *mod->Name);
  if (mdl_verbose_loading > 1) GCon->Logf(NAME_Init, "parsing model script '%s'", *mod->Name);
  ParseModelScript(lump, mod, *Strm);
  VStream::Destroy(Strm);

  return mod;
}


//==========================================================================
//
//  FindGZModelDefInclude
//
//==========================================================================
static int FindGZModelDefInclude (int baselump, VStr incname) {
  int bestlump = -1;
  for (auto &&it : WadFileIterator(incname)) {
    //GCon->Logf(NAME_Debug, "BASE: %d; lump=%d; <%s>", baselump, it.lump, *it.fname);
    if (it.getFile() > W_LumpFile(baselump)) break;
    bestlump = it.lump;
  }
  //if (bestlump >= 0) GCon->Logf(NAME_Debug, "BASE: '%s'; INCLUDE: '%s'", *W_FullLumpName(baselump), *W_FullLumpName(bestlump));
  return bestlump;
}


//==========================================================================
//
//  ParseGZModelDefs
//
//==========================================================================
static void ParseGZModelDefs () {
  // build model list, so we can append them backwards
  TArray<GZModelDefEx *> gzmdlist;
  TMap<VStr, int> gzmdmap;

  struct IncStackItem {
    VScriptParser *sc;
    int lump;
  };

  VName mdfname = VName("modeldef", VName::FindLower);
  if (mdfname == NAME_None) return; // no such chunk
  // parse all modeldefs
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) != mdfname) continue;
    GCon->Logf(NAME_Init, "parsing GZDoom ModelDef script \"%s\"", *W_FullLumpName(Lump));
    // parse modeldef
    TArray<IncStackItem> includeStack;
    VScriptParser *sc = VScriptParser::NewWithLump(Lump);
    sc->SetEscape(false);
    int currentLump = Lump;
    for (;;) {
      while (sc->GetString()) {
        if (sc->String.strEquCI("model")) {
          auto mdl = new GZModelDefEx();
          mdl->parse(sc);
          // find model
          if (!mdl->isEmpty() && !mdl->className.isEmpty()) {
            // search the currently loaded models
            VClass *xcls = VClass::FindClassNoCase(*mdl->className);
            if (xcls && !xcls->IsChildOf(VEntity::StaticClass())) xcls = nullptr;
            if (!xcls) {
              GCon->Logf(NAME_Init, "  found 3d GZDoom model for unknown class `%s`", *mdl->className);
              delete mdl;
              continue;
            }
            // check if we already have k8vavoom model for this class
            if (RM_FindClassModelByName(xcls->GetName())) {
              for (auto &&cm : ClassModels) {
                if (cm->Name == NAME_None || !cm->Model || cm->Frames.length() == 0) continue;
                if (!cm->isGZDoom && cm->Name == xcls->GetName()) {
                  GCon->Logf(NAME_Init, "  skipped GZDoom model for '%s' (found native k8vavoom definition)", xcls->GetName());
                  delete mdl;
                  mdl = nullptr;
                  break;
                }
              }
              if (!mdl) continue;
            }
            // merge with already existing model, if there is any
            VStr locname = mdl->className.toLowerCase();
            auto omp = gzmdmap.get(locname);
            if (omp) {
              if (dbg_dump_gzmodels.asInt() > 1) {
                GCon->Log(NAME_Debug, "*** MERGE ***");
                auto xml = mdl->createXml();
                GCon->Logf(NAME_Debug, "==== NEW: \n%s\n====", *xml);
                xml = gzmdlist[*omp]->createXml();
                GCon->Logf(NAME_Debug, "==== OLD: \n%s\n====", *xml);
              }
              gzmdlist[*omp]->merge(*mdl);
              delete mdl;
              if (dbg_dump_gzmodels.asInt() > 1) {
                auto xml = gzmdlist[*omp]->createXml();
                GCon->Logf(NAME_Debug, "==== MERGED: \n%s\n====", *xml);
              }
            } else {
              // new model
              gzmdmap.put(locname, gzmdlist.length());
              gzmdlist.append(mdl);
            }
          }
          continue;
        } else if (sc->String.strEquCI("#include")) {
          if (!sc->GetString()) sc->Error("invalid MODELDEF directive 'include' expects argument");
          int ilmp = FindGZModelDefInclude(Lump, sc->String);
          if (ilmp < 0) sc->Error(va("invalid MODELDEF include file '%s' not found!", *sc->String));
          bool err = (ilmp == Lump || ilmp == currentLump);
          if (!err) for (auto &&it : includeStack) if (it.lump == ilmp) { err = true; break; }
          if (err) sc->Error(va("MODELDEF file '%s' got into endless include loop with include file '%s'!", *W_FullLumpName(Lump), *W_FullLumpName(ilmp)));
          IncStackItem si;
          si.sc = sc;
          si.lump = ilmp;
          includeStack.append(si);
          sc = VScriptParser::NewWithLump(ilmp);
          sc->SetEscape(false);
          currentLump = ilmp;
          continue;
        }
        sc->Error(va("invalid MODELDEF directive '%s'", *sc->String));
        //GLog.WriteLine("%s: <%s>", *sc->GetLoc().toStringNoCol(), *sc->String);
      }
      delete sc;
      if (includeStack.length() == 0) break;
      {
        IncStackItem &si = includeStack[includeStack.length()-1];
        currentLump = si.lump;
        sc = si.sc;
        si.sc = nullptr;
      }
      includeStack.removeAt(includeStack.length()-1);
    }
  }

  // insert GZDoom alias models
  int cnt = 0;
  for (auto &&mdl : gzmdlist) {
    VClass *xcls = VClass::FindClassNoCase(*mdl->className);
    vassert(xcls);
    // get xml here, because we're going to modify the model
    auto xml = mdl->createXml();
    // create impossible name, because why not?
    mdl->className = va("/gzmodels/..%s/..gzmodel_%d.xml", xcls->GetName(), cnt++);
    //GCon->Logf("***<%s>", *mdl->className);
    GCon->Logf(NAME_Init, "  found GZDoom model for '%s'", xcls->GetName());
    if (dbg_dump_gzmodels.asInt()) {
      VStr gzxmlname;
      int gzcount = -1;
      for (;;) {
        if (gzcount >= 0) {
          gzxmlname = va("/models/gzdoom/%s_%d.xml", xcls->GetName(), gzcount);
        } else {
          gzxmlname = va("/models/gzdoom/%s.xml", xcls->GetName());
        }
        ++gzcount;
        bool found = false;
        for (VStr gn : mod_gznames) if (gn == gzxmlname) { found = true; break; }
        if (!found) {
          mod_gznames.append(gzxmlname);
          break;
        }
      }
      GCon->Logf(NAME_Debug, "==== GZ CONVERTED MODEL: BEGIN ====");
      GCon->Logf(NAME_Debug, "%s", *gzxmlname);
      GCon->Logf(NAME_Debug, "%s", *xml.xstripright());
      GCon->Logf(NAME_Debug, "==== GZ CONVERTED MODEL: END ====");
    }
    VModel *mod = new VModel();
    mod->Name = mdl->className;
    mod_known.Append(mod);
    // parse xml
    VStream *Strm = new VMemoryStreamRO(mdl->className, xml.getCStr(), xml.length());
    ParseModelScript(-1, mod, *Strm, true); // gzdoom flag
    VStream::Destroy(Strm);
    // this is not strictly safe, but meh
    delete mdl;
  }
}


//==========================================================================
//
//  LoadModelSkins
//
//==========================================================================
static double LoadModelSkins (VModel *mdl, const vuint32 stotal, vuint32 &scount, double stt, double sttime, double ivl) {
  if (!mdl) return stt;
  // load submodel skins
  for (auto &&ScMdl : mdl->Models) {
    for (auto &&SubMdl : ScMdl.SubModels) {
      //if (SubMdl.Version != -1 && SubMdl.Version != Version) continue;
      // locate the proper data
      SubMdl.Model->LoadFromWad();
      //FIXME: this should be done earilier
      //!if (SubMdl.Model->HadErrors) SubMdl.NoShadow = true;
      // load overriden submodel skins
      if (SubMdl.Skins.length()) {
        for (auto &&si : SubMdl.Skins) {
          if (si.textureId >= 0) continue;
          //GCon->Logf(NAME_Log, "loading model '%s' (submodel %d) subskin '%s'", *ScMdl.Name, (int)(ptrdiff_t)(&SubMdl-&ScMdl.SubModels[0]), *si.fileName);
          if (si.fileName == NAME_None) {
            si.textureId = GTextureManager.DefaultTexture;
          } else {
            si.textureId = GTextureManager.AddFileTextureShaded(si.fileName, TEXTYPE_Skin, si.shade);
            if (si.textureId < 0) si.textureId = GTextureManager.DefaultTexture;
          }
          if (si.textureId > 0 && !AllModelTexturesSeen.has(si.textureId)) {
            AllModelTexturesSeen.put(si.textureId, true);
            AllModelTextures.append(si.textureId);
          }
        }
      } else {
        // load base model skins
        for (auto &&si : SubMdl.Model->Skins) {
          if (si.textureId >= 0) continue;
          //GCon->Logf(NAME_Log, "loading model '%s' (submodel %d) base skin '%s'", *ScMdl.Name, (int)(ptrdiff_t)(&SubMdl-&ScMdl.SubModels[0]), *si.fileName);
          if (si.fileName == NAME_None) {
            si.textureId = GTextureManager.DefaultTexture;
          } else {
            si.textureId = GTextureManager.AddFileTextureShaded(si.fileName, TEXTYPE_Skin, si.shade);
            if (si.textureId < 0) si.textureId = GTextureManager.DefaultTexture;
          }
          if (si.textureId > 0 && !AllModelTexturesSeen.has(si.textureId)) {
            AllModelTexturesSeen.put(si.textureId, true);
            AllModelTextures.append(si.textureId);
          }
        }
      }
      // progress
      scount += 1;
    }
  }
  const double xtt = Sys_Time();
  if (xtt-stt >= ivl) {
    stt = xtt;
    GCon->Logf(NAME_Init, "precached %u of %u model skins (%d.%03d seconds)",
               scount, stotal, (int)(xtt-sttime), (int)((xtt-sttime)*1000)%1000);
    C_FlushSplash();
  }
  return stt;
}


//==========================================================================
//
//  R_LoadAllModelsSkins
//
//==========================================================================
void R_LoadAllModelsSkins () {
  if (r_preload_alias_models) {
    GCon->Log(NAME_Init, "preloading model skins");
    C_FlushSplash();
    AllModelTextures.reset();
    AllModelTexturesSeen.reset();
    AllModelTexturesSeen.put(GTextureManager.DefaultTexture, true);
    const double ivl = (vox_cache_enabled ? 6.6 : 2.2);
    // count total (used for progress)
    vuint32 stotal = 0;
    vuint32 scount = 0;
    for (auto &&mdl : ClassModels) {
      for (auto &&ScMdl : mdl->Model->Models) stotal += ScMdl.SubModels.length();
    }
    const double sttime = Sys_Time();
    double stt = sttime;
    for (auto &&mdl : ClassModels) {
      if (mdl->Name == NAME_None || !mdl->Model || mdl->Frames.length() == 0) continue;
      stt = LoadModelSkins(mdl->Model, stotal, scount, stt, sttime, ivl);
      // automatically turn on model cache
      if (!vox_cache_enabled && Sys_Time()-sttime >= 3.0) {
        GCon->Log(NAME_Warning, "automatically turned on model cache");
        vox_cache_enabled = true;
        C_FlushSplash();
      }
    }
    AllModelTexturesSeen.clear();
    const double xtt = Sys_Time();
    GCon->Logf(NAME_Init, "all model skins preloaded (%d.%03d seconds)",
               (int)(xtt-sttime), (int)((xtt-sttime)*1000)%1000);
    C_FlushSplash();
  }
}

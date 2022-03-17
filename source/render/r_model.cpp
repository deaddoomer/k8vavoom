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
#include "../psim/p_player.h"
#include "../client/client.h"
#include "../dehacked/vc_dehacked.h"
#include "r_local.h"
#include "r_local_gz.h"


#define SMOOTHSTEP(x) ((x)*(x)*(3.0f-2.0f*(x)))


extern VCvarF gl_alpha_threshold;
static inline float getAlphaThreshold () { float res = gl_alpha_threshold; if (res < 0) res = 0; else if (res > 1) res = 1; return res; }

static VCvarI mdl_verbose_loading("mdl_verbose_loading", "0", "Verbose alias model loading?", CVAR_NoShadow/*|CVAR_Archive*/);

static VCvarB gl_dbg_log_model_rendering("gl_dbg_log_model_rendering", false, "Some debug log.", CVAR_PreInit|CVAR_NoShadow);

static VCvarB r_model_autorotating("r_model_autorotating", true, "Allow model autorotation?", CVAR_Archive);
static VCvarB r_model_autobobbing("r_model_autobobbing", true, "Allow model autobobbing?", CVAR_Archive);
static VCvarB r_model_ignore_missing_textures("r_model_ignore_missing_textures", false, "Do not render models with missing textures (if `false`, renders with checkerboards)?", CVAR_Archive|CVAR_NoShadow);

static VCvarB r_preload_alias_models("r_preload_alias_models", true, "Preload all alias models and their skins?", CVAR_Archive|CVAR_PreInit|CVAR_NoShadow);

static VCvarB dbg_dump_gzmodels("dbg_dump_gzmodels", false, "Dump xml files for gz modeldefs?", /*CVAR_Archive|*/CVAR_PreInit|CVAR_NoShadow);


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
struct VScriptSubModel {
  struct VFrame {
    int Index;
    int PositionIndex;
    float AlphaStart;
    float AlphaEnd;
    AliasModelTrans Transform;
    int SkinIndex;

    void copyFrom (const VFrame &src) {
      Index = src.Index;
      PositionIndex = src.PositionIndex;
      AlphaStart = src.AlphaStart;
      AlphaEnd = src.AlphaEnd;
      Transform = src.Transform;
      SkinIndex = src.SkinIndex;
    }
  };

  VMeshModel *Model;
  VMeshModel *PositionModel;
  int SkinAnimSpeed;
  int SkinAnimRange;
  int Version;
  int MeshIndex; // for md3
  TArray<VFrame> Frames;
  TArray<VMeshModel::SkinInfo> Skins;
  bool FullBright;
  bool NoShadow;
  bool UseDepth;
  bool AllowTransparency;
  float AlphaMul;

  void copyFrom (VScriptSubModel &src) {
    Model = src.Model;
    PositionModel = src.PositionModel;
    SkinAnimSpeed = src.SkinAnimSpeed;
    SkinAnimRange = src.SkinAnimRange;
    Version = src.Version;
    MeshIndex = src.MeshIndex; // for md3
    FullBright = src.FullBright;
    NoShadow = src.NoShadow;
    UseDepth = src.UseDepth;
    AllowTransparency = src.AllowTransparency;
    AlphaMul = src.AlphaMul;
    // copy skin names
    Skins.setLength(src.Skins.length());
    for (int f = 0; f < src.Skins.length(); ++f) Skins[f] = src.Skins[f];
    // copy skin shades
    //SkinShades.setLength(src.SkinShades.length());
    //for (int f = 0; f < src.SkinShades.length(); ++f) SkinShades[f] = src.SkinShades[f];
    // copy frames
    Frames.setLength(src.Frames.length());
    for (int f = 0; f < src.Frames.length(); ++f) Frames[f].copyFrom(src.Frames[f]);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct VScriptModel {
  VName Name;
  bool HasAlphaMul;
  TArray<VScriptSubModel> SubModels;
};


// ////////////////////////////////////////////////////////////////////////// //
struct ModelAngle {
public:
  enum Mode { Relative, RelativeRandom, Absolute, AbsoluteRandom };
public:
  float angle;
  Mode mode;

  inline ModelAngle () noexcept : angle(0.0f), mode(Relative) {}

  inline void SetRelative (float aangle) noexcept { angle = AngleMod(aangle); mode = Relative; }
  inline void SetAbsolute (float aangle) noexcept { angle = AngleMod(aangle); mode = Absolute; }
  inline void SetAbsoluteRandom () noexcept { angle = AngleMod(360.0f*FRandomFull()); mode = AbsoluteRandom; }
  inline void SetRelativeRandom () noexcept { angle = AngleMod(360.0f*FRandomFull()); mode = RelativeRandom; }

  inline bool IsRelative () const noexcept { return (mode <= RelativeRandom); }
  inline bool IsRandom () const noexcept { return (mode == RelativeRandom || mode == AbsoluteRandom); }

  inline bool IsEmpty () const noexcept { return (mode == Relative && angle == 0.0f); }

  inline float GetAngle (float baseangle, vuint8 rndVal) const noexcept {
    const float ang = (!IsRandom() ? angle : AngleMod((float)rndVal*360.0f/255.0f));
    return (IsRelative() ? AngleMod(baseangle+ang) : ang);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
enum {
  DontUse = 0,
  FromActor = -1,
  FromMomentum = 1,
};

struct VScriptedModelFrame {
  int Number;
  float Inter;
  int ModelIndex;
  int SubModelIndex; // you can select submodel from any model if you wish to; use -1 to render all submodels; -2 to render none
  int FrameIndex;
  float AngleStart;
  float AngleEnd;
  float AlphaStart;
  float AlphaEnd;
  ModelAngle angleYaw;
  ModelAngle angleRoll;
  ModelAngle anglePitch;
  float rotateSpeed; // yaw rotation speed
  float bobSpeed; // bobbing speed
  int gzActorPitch; // 0: don't use; <0: actor; >0: momentum
  bool gzActorPitchInverted;
  int gzActorRoll; // 0: don't use; <0: actor; >0: momentum
  bool gzdoom;
  //
  VName sprite;
  int frame; // sprite frame
  // index for next frame with the same sprite and frame
  int nextSpriteIdx;
  // index for next frame with the same number
  int nextNumberIdx;
  bool disabled;

  void copyFrom (const VScriptedModelFrame &src) {
    Number = src.Number;
    Inter = src.Inter;
    ModelIndex = src.ModelIndex;
    SubModelIndex = src.SubModelIndex;
    FrameIndex = src.FrameIndex;
    AngleStart = src.AngleStart;
    AngleEnd = src.AngleEnd;
    AlphaStart = src.AlphaStart;
    AlphaEnd = src.AlphaEnd;
    angleYaw = src.angleYaw;
    angleRoll = src.angleRoll;
    anglePitch = src.anglePitch;
    rotateSpeed = src.rotateSpeed;
    bobSpeed = src.bobSpeed;
    gzActorPitch = src.gzActorPitch;
    gzActorPitchInverted = src.gzActorPitchInverted;
    gzActorRoll = src.gzActorRoll;
    gzdoom = src.gzdoom;
    sprite = src.sprite;
    frame = src.frame;
    nextSpriteIdx = src.nextSpriteIdx;
    nextNumberIdx = src.nextNumberIdx;
    disabled = src.disabled;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct VClassModelScript {
  VName Name;
  VModel *Model;
  bool NoSelfShadow;
  bool OneForAll; // no need to do anything, this is "one model for all frames"
  TArray<VScriptedModelFrame> Frames;
  bool CacheBuilt;
  bool isGZDoom;
  bool iwadonly;
  bool thiswadonly;
  bool asTranslucent; // always queue this as translucent model?
  TMapNC<vuint32, int> SprFrameMap; // sprite name -> frame index (first)
  TMapNC<int, int> NumFrameMap; // frame number -> frame index (first)
};


// ////////////////////////////////////////////////////////////////////////// //
struct VModel {
  VStr Name;
  TArray<VScriptModel> Models;
  VClassModelScript *DefaultClass;
};


// ////////////////////////////////////////////////////////////////////////// //
static TArray<VModel *> mod_known;
static TArray<VMeshModel *> GMeshModels;
static TArray<VClassModelScript *> ClassModels;
static TMapNC<VName, VClassModelScript *> ClassModelMap;
static bool ClassModelMapRebuild = true;
TArray<int> AllModelTextures;
static TMapNC<int, bool> AllModelTexturesSeen;
static TMap<VStr, VModel *> fixedModelMap;


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
//  SprNameFrameToInt
//
//==========================================================================
static inline vuint32 SprNameFrameToInt (VName name, int frame) {
  return (vuint32)name.GetIndex()|((vuint32)frame<<19);
}


//==========================================================================
//
//  FindClassModelByName
//
//==========================================================================
static VClassModelScript *FindClassModelByName (VName clsName) {
  if (ClassModelMapRebuild) {
    // build map
    ClassModelMapRebuild = false;
    ClassModelMap.reset();
    for (auto &&mdl : ClassModels) {
      if (mdl->Name == NAME_None || !mdl->Model || mdl->Frames.length() == 0) continue;
      ClassModelMap.put(mdl->Name, mdl);
    }
  }
  auto mp = ClassModelMap.get(clsName);
  return (mp ? *mp : nullptr);
}


//==========================================================================
//
//  R_HaveClassModelByName
//
//==========================================================================
bool R_HaveClassModelByName (VName clsName) {
  return !!FindClassModelByName(clsName);
}


//==========================================================================
//
//  R_EntModelNoSelfShadow
//
//==========================================================================
bool R_EntModelNoSelfShadow (VEntity *mobj) {
  VClassModelScript *cs = FindClassModelByName(VRenderLevelShared::GetClassNameForModel(mobj));
  return (cs ? cs->NoSelfShadow : true);
}


//==========================================================================
//
//  R_InitModels
//
//==========================================================================
void R_InitModels () {
  GCon->Log(NAME_Init, "loading model scripts");
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

  fixedModelMap.clear();
  ClassModelMap.clear();
  ClassModelMapRebuild = true;
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
    if (type == "value") {
      if (wasNN[0] || wasNN[1] || wasNN[2]) {
        Sys_Error("%s: arg '%s' conflicts with previous args", *loc.toStringNoCol(), *type);
      }
      wasNN[0] = wasNN[1] = wasNN[2] = true;
    } else {
           if (type == "x" || (op == OpRotate && type == "pitch")) idx = 0; // RotateX is pitch
      else if (type == "y" || (op == OpRotate && type == "roll")) idx = 1; // RotateY is roll
      else if (type == "z" || (op == OpRotate && type == "yaw")) idx = 2; // RotateZ is yaw
      else Sys_Error("%s: unknown arg '%s'", *loc.toStringNoCol(), *type);
      if (wasNN[idx]) Sys_Error("%s: duplicate arg '%s'", *loc.toStringNoCol(), *type);
      wasNN[idx] = true;
    }

    float flt = 0.0f;
    if (!value.convertFloat(&flt) || !isfinite(flt)) {
      Sys_Error("%s: arg '%s' is not a valid float (%s)", *loc.toStringNoCol(), *type, *value);
    }
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
              GCon->Logf(NAME_Init, "%s: model '%s' got automatic submodel%s for %u more mesh%s",
                         *ModelDefNode->Loc.toStringNoCol(), *Md2->Model->Name,
                         (n > 2 ? "s" : ""), n-1, (n > 2 ? "es" : ""));
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
                                                nullptr);
      if (bad) Sys_Error("%s: model '%s' class definition has invalid attribute '%s'", *bad->Loc.toStringNoCol(), *Mdl->Name, *bad->Name);
    }

    // check nodes
    {
      auto bad = ClassDefNode->FindBadChild("state", nullptr);
      if (bad) Sys_Error("%s: model '%s' class definition has invalid node '%s'", *bad->Loc.toStringNoCol(), *Mdl->Name, *bad->Name);
    }

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
    Cls->Model = Mdl;
    Cls->Name = (xcls ? xcls->GetName() : *vcClassName);
    Cls->NoSelfShadow = ParseBool(ClassDefNode, "noselfshadow", globNoSelfShadow);
    Cls->OneForAll = false;
    Cls->CacheBuilt = false;
    Cls->isGZDoom = isGZDoom;
    Cls->asTranslucent = false;
    Cls->iwadonly = ParseBool(ClassDefNode, "iwadonly", globIWadOnly);
    Cls->thiswadonly = ParseBool(ClassDefNode, "thiswadonly", globThisWadOnly);

    bool deleteIt = !xcls;
    if (!deleteIt && cli_IgnoreModelClass.has(*Cls->Name)) {
      GCon->Logf(NAME_Init, "%s: model '%s' ignored by user request",
                 *ClassDefNode->Loc.toStringNoCol(), *Cls->Name);
      deleteIt = true;
    }
    if (!deleteIt && xcls) {
      if (!Mdl->DefaultClass) Mdl->DefaultClass = Cls;
      ClassModels.Append(Cls);
      ClassModelMapRebuild = true;
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
                                                  nullptr);
        if (bad) Sys_Error("%s: model '%s' class state definition has invalid attribute '%s'", *bad->Loc.toStringNoCol(), *Mdl->Name, *bad->Name);
      }

      VScriptedModelFrame &F = Cls->Frames.Alloc();

      ParseAngle(StateDefNode, "yaw", F.angleYaw);
      ParseAngle(StateDefNode, "pitch", F.anglePitch);
      ParseAngle(StateDefNode, "roll", F.angleRoll);

      F.rotateSpeed = ParseFloatWithDefault(StateDefNode, "rotation_speed", -1.0f);
      F.bobSpeed = ParseFloatWithDefault(StateDefNode, "bobbing_speed", -1.0f);

      if (!ParseBool(StateDefNode, "rotation", defrot)) F.rotateSpeed = 0.0f;
      if (!ParseBool(StateDefNode, "bobbing", defbob)) F.bobSpeed = 0.0f;
      if (F.rotateSpeed < 0.0f) F.rotateSpeed = 100.0f;
      if (F.bobSpeed < 0.0f) F.bobSpeed = 180.0f;

      // some special things
      F.gzdoom = ParseBool(StateDefNode, "gzdoom", false);
      if (F.gzdoom) {
        F.gzActorPitchInverted = false;
        F.gzActorPitch = DontUse; // don't use actor pitch
        F.gzActorRoll = DontUse; // don't use actor roll
      } else {
        F.gzActorPitchInverted = false;
        F.gzActorPitch = FromActor; // default
        //F.gzActorPitch = DontUse; // default
        F.gzActorRoll = DontUse; // don't use actor roll
      }

      if (StateDefNode->HasAttribute("usepitch")) {
        VStr pv = StateDefNode->GetAttribute("usepitch");
             if (pv == "actor") F.gzActorPitch = FromActor;
        else if (pv == "momentum") F.gzActorPitch = FromMomentum;
        else if (pv == "actor-inverted") { F.gzActorPitch = FromActor; F.gzActorPitchInverted = true; }
        else if (pv == "momentum-inverted") { F.gzActorPitch = FromMomentum; F.gzActorPitchInverted = true; }
        else if (isFalseValue(pv)) F.gzActorPitch = DontUse;
        else if (pv == "default") {}
        else Sys_Error("%s: invalid \"usepitch\" attribute value '%s'", *StateDefNode->Loc.toStringNoCol(), *pv);
      }

      if (StateDefNode->HasAttribute("useroll")) {
        VStr pv = StateDefNode->GetAttribute("useroll");
             if (pv == "actor") F.gzActorRoll = FromActor;
        else if (pv == "momentum") F.gzActorRoll = FromMomentum;
        else if (isFalseValue(pv)) F.gzActorRoll = DontUse;
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

      if (!StateDefNode->HasAttribute("frame_index")) {
        int okfrm = true;
        for (int sid = 0; sid < Mdl->Models.length(); ++sid) {
          if (Mdl->Models[sid].SubModels.length()) {
            if (Mdl->Models[sid].SubModels[0].Frames.length() > 1) {
              okfrm = false;
              break;
            }
          }
        }
        if (!okfrm) {
          Sys_Error("%s: model '%s' has state without frame index", *StateDefNode->Loc.toStringNoCol(), *Mdl->Name);
        }
        F.FrameIndex = 0;
      } else {
        F.FrameIndex = ParseIntWithDefault(StateDefNode, "frame_index", 0);
      }
      if (ParseBool(StateDefNode, "hidden", false)) F.SubModelIndex = -2; // hidden

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
      ClassModelMapRebuild = true;
      deleteIt = true;
    }

    if (deleteIt) { delete Cls; continue; }
    if (hasOneAll && !hasOthers) {
      Cls->OneForAll = true;
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
            if (FindClassModelByName(xcls->GetName())) {
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
              if (dbg_dump_gzmodels) {
                GCon->Log(NAME_Debug, "*** MERGE ***");
                auto xml = mdl->createXml();
                GCon->Logf(NAME_Debug, "==== NEW: \n%s\n====", *xml);
                xml = gzmdlist[*omp]->createXml();
                GCon->Logf(NAME_Debug, "==== OLD: \n%s\n====", *xml);
              }
              gzmdlist[*omp]->merge(*mdl);
              delete mdl;
              if (dbg_dump_gzmodels) {
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
    if (dbg_dump_gzmodels) GCon->Logf(NAME_Debug, "====\n%s\n====", *xml);
    mdl->className = va("/gzmodels/..%s/..gzmodel_%d.xml", xcls->GetName(), cnt++);
    //GCon->Logf("***<%s>", *mdl->className);
    GCon->Logf(NAME_Init, "  found GZDoom model for '%s'", xcls->GetName());
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
//  PositionModel
//
//==========================================================================
static void PositionModel (TVec &Origin, TAVec &Angles, VMeshModel *wpmodel, int InFrame) {
  wpmodel->LoadFromWad();
  unsigned frame = (unsigned)InFrame;
  if (frame >= (unsigned)wpmodel->Frames.length()) frame = 0;
  TVec p[3];
  /*k8: dunno
  mtriangle_t *ptris = (mtriangle_t *)((vuint8 *)pmdl+pmdl->ofstris);
  mframe_t *pframe = (mframe_t *)((vuint8 *)pmdl+pmdl->ofsframes+frame*pmdl->framesize);
  trivertx_t *pverts = (trivertx_t *)(pframe+1);
  for (int vi = 0; vi < 3; ++vi) {
    p[vi].x = pverts[ptris[0].vertindex[vi]].v[0]*pframe->scale[0]+pframe->scale_origin[0];
    p[vi].y = pverts[ptris[0].vertindex[vi]].v[1]*pframe->scale[1]+pframe->scale_origin[1];
    p[vi].z = pverts[ptris[0].vertindex[vi]].v[2]*pframe->scale[2]+pframe->scale_origin[2];
  }
  */
  const VMeshFrame &frm = wpmodel->Frames[(int)frame];
  for (int vi = 0; vi < 3; ++vi) {
    p[vi].x = frm.Verts[wpmodel->Tris[0].VertIndex[vi]].x;
    p[vi].y = frm.Verts[wpmodel->Tris[0].VertIndex[vi]].y;
    p[vi].z = frm.Verts[wpmodel->Tris[0].VertIndex[vi]].z;
  }
  TVec md_forward(0, 0, 0), md_left(0, 0, 0), md_up(0, 0, 0);
  AngleVectors(Angles, md_forward, md_left, md_up);
  md_left = -md_left;
  Origin += md_forward*p[0].x+md_left*p[0].y+md_up*p[0].z;
  TAVec wangles = VectorAngles(p[1]-p[0]);
  Angles.yaw = AngleMod(Angles.yaw+wangles.yaw);
  Angles.pitch = AngleMod(Angles.pitch+wangles.pitch);
}


//==========================================================================
//
//  UpdateClassFrameCache
//
//==========================================================================
static void UpdateClassFrameCache (VClassModelScript &Cls) {
  if (!Cls.CacheBuilt) {
    for (int i = 0; i < Cls.Frames.length(); ++i) {
      VScriptedModelFrame &frm = Cls.Frames[i];
      frm.nextSpriteIdx = frm.nextNumberIdx = -1;
      if (frm.disabled) continue;
      // sprite cache
      if (frm.sprite != NAME_None) {
        //FIXME: sanity checks
        vassert(frm.frame >= 0 && frm.frame < 4096);
        vassert(frm.sprite.GetIndex() > 0 && frm.sprite.GetIndex() < 524288);
        vuint32 nfi = SprNameFrameToInt(frm.sprite, frm.frame);
        if (!Cls.SprFrameMap.has(nfi)) {
          // new one
          Cls.SprFrameMap.put(nfi, i);
          //GCon->Logf(NAME_Debug, "*NEW sprite frame for '%s': %s %c (%d)", *Cls.Name, *frm.sprite, 'A'+frm.frame, i);
        } else {
          // add to list
          int idx = *Cls.SprFrameMap.get(nfi);
          while (Cls.Frames[idx].nextSpriteIdx != -1) idx = Cls.Frames[idx].nextSpriteIdx;
          Cls.Frames[idx].nextSpriteIdx = i;
          //GCon->Logf(NAME_Debug, "*  more sprite frames for '%s': %s %c (%d)", *Cls.Name, *frm.sprite, 'A'+frm.frame, i);
        }
      }
      // frame number cache
      if (frm.Number >= 0) {
        if (!Cls.NumFrameMap.has(frm.Number)) {
          // new one
          Cls.NumFrameMap.put(frm.Number, i);
        } else {
          // add to list
          int idx = *Cls.NumFrameMap.get(frm.Number);
          while (Cls.Frames[idx].nextNumberIdx != -1) idx = Cls.Frames[idx].nextNumberIdx;
          Cls.Frames[idx].nextNumberIdx = i;
        }
      }
    }
    Cls.CacheBuilt = true;
  }
}


#define FINDFRAME_CHECK_FRM  \
  const VScriptedModelFrame &frm = Cls.Frames[idx]; \
  if (res < 0 || (Cls.Frames[res].ModelIndex == frm.ModelIndex && Cls.Frames[res].SubModelIndex == frm.SubModelIndex)) { \
    if (frm.Inter <= Inter && frm.Inter > bestInter) { res = idx; bestInter = frm.Inter; } \
  }


//==========================================================================
//
//  FindFrame
//
//  returns first frame found
//  note that there can be several frames for one sprite!
//
//==========================================================================
static int FindFrame (VClassModelScript &Cls, const VAliasModelFrameInfo &Frame, float Inter) {
  UpdateClassFrameCache(Cls);

  // try cached frames
  if (Cls.OneForAll) return 0; // guaranteed

  // we have to check both index and frame here, because we don't know which was defined
  // i can preprocess this, but meh, i guess that hashtable+chain is fast enough

  int res = -1;
  float bestInter = -9999.9f;

  //FIXME: reduce pasta!
  if (Frame.sprite != NAME_None && Frame.frame >= 0 && Frame.frame < 4096) {
    // by sprite name
    int *idxp = Cls.SprFrameMap.get(SprNameFrameToInt(Frame.sprite, Frame.frame));
    if (idxp) {
      int idx = *idxp;
      while (idx >= 0) {
        FINDFRAME_CHECK_FRM
        idx = frm.nextSpriteIdx;
      }
      if (res >= 0) return res;
    }
  }

  if (Frame.index >= 0) {
    // by index
    int *idxp = Cls.NumFrameMap.get(Frame.index);
    if (idxp) {
      int idx = *idxp;
      while (idx >= 0) {
        FINDFRAME_CHECK_FRM
        idx = frm.nextNumberIdx;
      }
    }
  }

  return res;
}


#define FINDNEXTFRAME_CHECK_FRM  \
  const VScriptedModelFrame &nfrm = Cls.Frames[nidx]; \
  if (FDef.ModelIndex == nfrm.ModelIndex && FDef.SubModelIndex == nfrm.SubModelIndex && \
      nfrm.Inter >= FDef.Inter && nfrm.Inter < bestInter) \
  { \
    res = nidx; \
    bestInter = nfrm.Inter; \
  }


//==========================================================================
//
//  FindNextFrame
//
//==========================================================================
static int FindNextFrame (VClassModelScript &Cls, int FIdx, const VAliasModelFrameInfo &Frame, float Inter, float &InterpFrac) {
  if (FIdx < 0) { InterpFrac = 0.0f; return -1; } // just in case
  UpdateClassFrameCache(Cls);
  if (Inter < 0.0f) Inter = 0.0f; // just in case

  const VScriptedModelFrame &FDef = Cls.Frames[FIdx];

  // previous code was using `FIdx+1`, and it was wrong
  // just in case, check for valid `Inter`
  // walk the list
  if (FDef.Inter < 1.0f) {
    // doesn't finish time slice
    int res = -1;
    float bestInter = 9999.9f;

    if (FDef.sprite != NAME_None) {
      // by sprite name
      int nidx = FDef.nextSpriteIdx;
      while (nidx >= 0) {
        FINDNEXTFRAME_CHECK_FRM
        nidx = nfrm.nextSpriteIdx;
      }
    } else {
      // by frame index
      int nidx = FDef.nextNumberIdx;
      while (nidx >= 0) {
        FINDNEXTFRAME_CHECK_FRM
        nidx = nfrm.nextNumberIdx;
      }
    }

    // found interframe?
    if (res >= 0) {
      const VScriptedModelFrame &nfrm = Cls.Frames[res];
      if (nfrm.Inter <= FDef.Inter) {
        InterpFrac = 1.0f;
      } else {
        float frc = (Inter-FDef.Inter)/(nfrm.Inter-FDef.Inter);
        if (!isFiniteF(frc)) frc = 1.0f; // just in case
        InterpFrac = frc;
      }
      return res;
    }
  }

  // no interframe models found, get normal frame
  InterpFrac = (FDef.Inter >= 0.0f && FDef.Inter < 1.0f ? (Inter-FDef.Inter)/(1.0f-FDef.Inter) : 1.0f);
  return FindFrame(Cls, Frame, 0);
}


//==========================================================================
//
//  LoadModelSkins
//
//==========================================================================
static void LoadModelSkins (VModel *mdl) {
  if (!mdl) return;
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
    }
  }
}


//==========================================================================
//
//  R_LoadAllModelsSkins
//
//==========================================================================
void R_LoadAllModelsSkins () {
  if (r_preload_alias_models) {
    AllModelTextures.reset();
    AllModelTexturesSeen.reset();
    AllModelTexturesSeen.put(GTextureManager.DefaultTexture, true);
    for (auto &&mdl : ClassModels) {
      if (mdl->Name == NAME_None || !mdl->Model || mdl->Frames.length() == 0) continue;
      LoadModelSkins(mdl->Model);
    }
    AllModelTexturesSeen.clear();
  }
}


//==========================================================================
//
//  lerpvec
//
//==========================================================================
static inline void lerpvec (TVec &a, const TVec b, const float t) noexcept {
  a.x += (b.x-a.x)*t;
  a.y += (b.y-a.y)*t;
  a.z += (b.z-a.z)*t;
}


//==========================================================================
//
//  CheckModelEarlyRejects
//
//==========================================================================
static inline bool CheckModelEarlyRejects (const RenderStyleInfo &ri, ERenderPass Pass) {
  switch (Pass) {
    case RPASS_Normal: // lightmapped renderer is using this
      break;
    case RPASS_Ambient:
      if (ri.isAdditive()) return false;
      if (ri.isTranslucent() && ri.stencilColor) return false; // shaded too
      break;
    case RPASS_ShadowVolumes:
    case RPASS_ShadowMaps:
      if (ri.isTranslucent()) return false;
      break;
    case RPASS_Textures:
      if (ri.isAdditive() || ri.isShaded()) return false;
      break;
    case RPASS_Light:
      if (ri.isAdditive() || ri.isShaded()) return false;
      //if (ri.isTranslucent() && ri.stencilColor) return; // shaded too
      break;
    case RPASS_Fog:
      //FIXME
      if (ri.isAdditive() || ri.isShaded()) return false;
      //if (ri.stencilColor) return;
      break;
    case RPASS_NonShadow: // non-lightmapped renderers are using this for translucent models
      if (!ri.isAdditive() && !ri.isShaded()) return false;
      break;
    case RPASS_Glass:
      if (ri.isTranslucent()) return false;
      break;
  }
  return true;
}


//==========================================================================
//
//  DrawModel
//
//  FIXME: make this faster -- stop looping, cache data!
//
//==========================================================================
static void DrawModel (VLevel *Level, VEntity *mobj, const TVec &Org, const TAVec &Angles,
  float ScaleX, float ScaleY, VClassModelScript &Cls, int FIdx, int NFIdx,
  VTextureTranslation *Trans, int ColorMap, int Version,
  const RenderStyleInfo &ri,
  bool IsViewModel, float Inter,
  bool Interpolate, const TVec &LightPos, float LightRadius, ERenderPass Pass, bool isShadowVol)
{
  // some early rejects
  if (!CheckModelEarlyRejects(ri, Pass)) return;

  VScriptedModelFrame &FDef = Cls.Frames[FIdx];
  const int allowedsubmod = FDef.SubModelIndex;
  if (allowedsubmod == -2) return; // this frame is hidden
  VScriptedModelFrame &NFDef = Cls.Frames[NFIdx];
  VScriptModel &ScMdl = Cls.Model->Models[FDef.ModelIndex];

  int submodindex = -1;
  for (auto &&SubMdl : ScMdl.SubModels) {
    ++submodindex;
    if (allowedsubmod >= 0 && submodindex != allowedsubmod) continue; // only one submodel allowed
    if (SubMdl.Version != -1 && SubMdl.Version != Version) continue;
    if (SubMdl.AlphaMul <= 0.0f) continue;

    if (ScMdl.HasAlphaMul && SubMdl.AlphaMul < 1.0f) {
      if (Pass != RPASS_Glass && Pass != RPASS_Normal) continue;
    }

    if (FDef.FrameIndex >= SubMdl.Frames.length()) {
      if (FDef.FrameIndex != 0x7fffffff) {
        GCon->Logf("Bad sub-model frame index %d for model '%s' (class '%s')", FDef.FrameIndex, *ScMdl.Name, *Cls.Name);
        FDef.FrameIndex = 0x7fffffff;
      }
      continue;
    }

    // cannot interpolate between different models or submodels
    if (Interpolate) {
      if (FDef.ModelIndex != NFDef.ModelIndex ||
          FDef.SubModelIndex != NFDef.SubModelIndex ||
          NFDef.FrameIndex >= SubMdl.Frames.length())
      {
        Interpolate = false;
      }
    }

    VScriptSubModel::VFrame &F = SubMdl.Frames[FDef.FrameIndex];
    VScriptSubModel::VFrame &NF = SubMdl.Frames[Interpolate ? NFDef.FrameIndex : FDef.FrameIndex];

    // alpha
    float Md2Alpha = ri.alpha;
    if (ScMdl.HasAlphaMul) {
      if (Pass == RPASS_Glass || Pass == RPASS_Normal) Md2Alpha *= SubMdl.AlphaMul;
    }
    if (FDef.AlphaStart != 1.0f || FDef.AlphaEnd != 1.0f) Md2Alpha *= FDef.AlphaStart+(FDef.AlphaEnd-FDef.AlphaStart)*Inter;
    if (F.AlphaStart != 1.0f || F.AlphaEnd != 1.0f) Md2Alpha *= F.AlphaStart+(F.AlphaEnd-F.AlphaStart)*Inter;
    if (Md2Alpha <= 0.01f) continue;
    if (Md2Alpha > 1.0f) Md2Alpha = 1.0f;
    //Md2Alpha = 0.8f;

    // alpha-blended things should be rendered with "non-shadow" pass
    // `RPASS_Normal` means "lightmapped renderer", it always does it right
    if (Pass != RPASS_Normal) {
      // translucent?
      if (Pass != RPASS_NonShadow && Pass != RPASS_Glass && Md2Alpha < 1.0f) continue;
    }

    // locate the proper data
    SubMdl.Model->LoadFromWad();
    //FIXME: this should be done earilier
    //!if (SubMdl.Model->HadErrors) SubMdl.NoShadow = true;

    // skin animations
    int Md2SkinIdx = 0;
    if (F.SkinIndex >= 0) {
      Md2SkinIdx = F.SkinIndex;
    } else if (SubMdl.SkinAnimSpeed) {
      Md2SkinIdx = int((Level ? Level->Time : 0)*SubMdl.SkinAnimSpeed)%SubMdl.SkinAnimRange;
    }
    if (Md2SkinIdx < 0) Md2SkinIdx = 0; // just in case

    // get the proper skin texture ID
    int SkinID;
    if (SubMdl.Skins.length()) {
      // skins defined in definition file override all skins in MD2 file
      if (Md2SkinIdx < 0 || Md2SkinIdx >= SubMdl.Skins.length()) {
        if (SubMdl.Skins.length() == 0) Sys_Error("model '%s' has no skins", *SubMdl.Model->Name);
        //if (SubMdl.SkinShades.length() == 0) Sys_Error("model '%s' has no skin shades", *SubMdl.Model->Name);
        Md2SkinIdx = 0;
      }
      SkinID = SubMdl.Skins[Md2SkinIdx].textureId;
      if (SkinID < 0) {
        SkinID = GTextureManager.AddFileTextureShaded(SubMdl.Skins[Md2SkinIdx].fileName, TEXTYPE_Skin, SubMdl.Skins[Md2SkinIdx].shade);
        SubMdl.Skins[Md2SkinIdx].textureId = SkinID;
      }
    } else {
      if (SubMdl.Model->Skins.length() == 0) Sys_Error("model '%s' has no skins", *SubMdl.Model->Name);
      Md2SkinIdx = Md2SkinIdx%SubMdl.Model->Skins.length();
      if (Md2SkinIdx < 0) Md2SkinIdx = (Md2SkinIdx+SubMdl.Model->Skins.length())%SubMdl.Model->Skins.length();
      SkinID = SubMdl.Model->Skins[Md2SkinIdx].textureId;
      if (SkinID < 0) {
        //SkinID = GTextureManager.AddFileTexture(SubMdl.Model->Skins[Md2SkinIdx%SubMdl.Model->Skins.length()], TEXTYPE_Skin);
        SkinID = GTextureManager.AddFileTextureShaded(SubMdl.Model->Skins[Md2SkinIdx].fileName, TEXTYPE_Skin, SubMdl.Model->Skins[Md2SkinIdx].shade);
        SubMdl.Model->Skins[Md2SkinIdx].textureId = SkinID;
      }
    }
    if (SkinID < 0) SkinID = GTextureManager.DefaultTexture;

    // check for missing texture
    if (SkinID == GTextureManager.DefaultTexture) {
      if (r_model_ignore_missing_textures) return;
    }

    VTexture *SkinTex = GTextureManager(SkinID);
    if (!SkinTex || SkinTex->Type == TEXTYPE_Null) return; // do not render models without textures

    // get and verify frame number
    int Md2Frame = F.Index;
    if ((unsigned)Md2Frame >= (unsigned)SubMdl.Model->Frames.length()) {
      if (developer) GCon->Logf(NAME_Dev, "no such frame %d in model '%s'", Md2Frame, *SubMdl.Model->Name);
      Md2Frame = (Md2Frame <= 0 ? 0 : SubMdl.Model->Frames.length()-1);
      // stop further warnings
      F.Index = Md2Frame;
    }

    // get and verify next frame number
    int Md2NextFrame = Md2Frame;
    if (Interpolate) {
      Md2NextFrame = NF.Index;
      if ((unsigned)Md2NextFrame >= (unsigned)SubMdl.Model->Frames.length()) {
        if (developer) GCon->Logf(NAME_Dev, "no such next frame %d in model '%s'", Md2NextFrame, *SubMdl.Model->Name);
        Md2NextFrame = (Md2NextFrame <= 0 ? 0 : SubMdl.Model->Frames.length()-1);
        // stop further warnings
        NF.Index = Md2NextFrame;
      }
    }

    // position
    TVec Md2Org = Org;

    const char *passname = nullptr;
    if (gl_dbg_log_model_rendering) {
      switch (Pass) {
        case RPASS_Normal: passname = "normal"; break;
        case RPASS_Ambient: passname = "ambient"; break;
        case RPASS_ShadowVolumes: passname = "shadow"; break;
        case RPASS_ShadowMaps: passname = "shadowmaps"; break;
        case RPASS_Light: passname = "light"; break;
        case RPASS_Textures: passname = "texture"; break;
        case RPASS_Fog: passname = "fog"; break;
        case RPASS_NonShadow: passname = "nonshadow"; break;
        case RPASS_Glass: passname = "glass"; break;
        default: Sys_Error("WTF?!");
      }
      GCon->Logf("000: MODEL(%s): class='%s'; alpha=%f; noshadow=%d; usedepth=%d", passname, *Cls.Name, Md2Alpha, (int)SubMdl.NoShadow, (int)SubMdl.UseDepth);
    }

    switch (Pass) {
      case RPASS_Normal:
        break;
      case RPASS_Ambient:
        //if (ri.isTranslucent() && ri.stencilColor) continue; // already checked
        //if (ri.isAdditive()) continue; // already checked
        break;
      case RPASS_ShadowVolumes:
        if (Md2Alpha < 1.0f || SubMdl.NoShadow || SubMdl.Model->HadErrors) continue;
        break;
      case RPASS_ShadowMaps:
        if (Md2Alpha < 1.0f || SubMdl.NoShadow) continue;
        //if (ri.isTranslucent() && ri.stencilColor) continue; // already checked
        //if (ri.isAdditive()) continue; // already checked
        break;
      case RPASS_Textures:
        //if (Md2Alpha <= getAlphaThreshold() && !ri.isAdditive()) continue; // already checked
        //if (ri.isAdditive()) continue; // already checked
        break;
      case RPASS_Light:
        if (Md2Alpha <= getAlphaThreshold() /*|| SubMdl.NoShadow*/) continue;
        //if (ri.isTranslucent() && ri.stencilColor) continue; // no need to
        //if (ri.isAdditive()) continue; // already checked
        break;
      case RPASS_Fog:
        /*
        // noshadow model is rendered as "noshadow", so it doesn't need fog
        if (Md2Alpha <= getAlphaThreshold() || SubMdl.NoShadow) {
          //if (gl_dbg_log_model_rendering) GCon->Logf("  SKIP FOG FOR MODEL(%s): class='%s'; alpha=%f; noshadow=%d", passname, *Cls.Name, Md2Alpha, (int)SubMdl.NoShadow);
          continue;
        }
        */
        break;
      case RPASS_NonShadow:
        //if (Md2Alpha >= 1.0f && !Additive && !SubMdl.NoShadow) continue;
        //!if (Md2Alpha < 1.0f || ri.isAdditive() /*|| SubMdl.NoShadow*/) continue;
        //if (Md2Alpha >= 1.0f && !ri.isAdditive() /*|| SubMdl.NoShadow*/) continue;
        //if (ri.isTranslucent() && ri.stencilColor) continue;
        //
        //if (!ri.isAdditive()) continue; // already checked
        break;
      case RPASS_Glass:
        break;
    }

    if (gl_dbg_log_model_rendering) GCon->Logf("     MODEL(%s): class='%s'; alpha=%f; noshadow=%d", passname, *Cls.Name, Md2Alpha, (int)SubMdl.NoShadow);

    // angle
    TAVec Md2Angle = Angles;
    // position model
    if (SubMdl.PositionModel) PositionModel(Md2Org, Md2Angle, SubMdl.PositionModel, F.PositionIndex);

    const float smooth_inter = (Interpolate ? SMOOTHSTEP(Inter) : 0.0f);

    AliasModelTrans Transform = F.Transform;
    bool updateRot = false;

    if (Interpolate && smooth_inter > 0.0f && &F != &NF) {
      if (smooth_inter >= 1.0f) {
        Transform = NF.Transform;
      } else if (Transform.MatTrans != NF.Transform.MatTrans) {
        Transform.DecTrans = Transform.DecTrans.interpolate(NF.Transform.DecTrans, smooth_inter);
        Transform.MatTrans.recompose(Transform.DecTrans);
        //FIXME: what to do with RotCenter here?
        // gzdoom-style scale/offset?
        if (Transform.gzdoom || NF.Transform.gzdoom) {
          lerpvec(Transform.gzPreScaleOfs, NF.Transform.gzPreScaleOfs, smooth_inter);
          lerpvec(Transform.gzScale, NF.Transform.gzScale, smooth_inter);
          Transform.gzdoom = true;
        }
        //Transform.TransRot = Transform.MatTrans.getAngles();
        updateRot = true;
      }
    }

    if (!Transform.gzdoom) {
      Transform.MatTrans.scaleXY(ScaleX, ScaleY);
    } else {
      // done in renderer
      Transform.gzScale.x *= ScaleX;
      Transform.gzScale.y *= ScaleX;
      Transform.gzScale.z *= ScaleY;
    }

    if (!IsViewModel) {
      const vuint8 rndVal = (mobj ? (hashU32(mobj->ServerUId)>>4)&0xffu : 0);
      /* old code
        if (FDef.AngleStart || FDef.AngleEnd != 1.0f) {
          Md2Angle.yaw = AngleMod(Md2Angle.yaw+FDef.AngleStart+(FDef.AngleEnd-FDef.AngleStart)*Inter);
        }
      */

      Md2Angle.yaw = FDef.angleYaw.GetAngle(Md2Angle.yaw, rndVal);

      if (FDef.AngleStart || FDef.AngleEnd != FDef.AngleStart) {
        Md2Angle.yaw = AngleMod(Md2Angle.yaw+FDef.AngleStart+(FDef.AngleEnd-FDef.AngleStart)*Inter);
      }

      if (FDef.gzActorRoll == DontUse) {
        Md2Angle.roll = 0.0f;
      } else {
        Md2Angle.roll = FDef.angleRoll.GetAngle(Md2Angle.roll, rndVal);
      }

      if (!mobj || FDef.gzActorPitch == DontUse) {
        Md2Angle.pitch = 0.0f;
      } else {
        if (FDef.gzActorPitch == FromMomentum) Md2Angle.pitch = VectorAnglePitch(mobj->Velocity);
        if (FDef.gzActorPitchInverted) Md2Angle.pitch += 180.0f;
        Md2Angle.pitch = FDef.anglePitch.GetAngle(Md2Angle.pitch, rndVal);
      }

      if (Level && mobj) {
        if (r_model_autorotating && FDef.rotateSpeed) {
          Md2Angle.yaw = AngleMod(Md2Angle.yaw+Level->Time*FDef.rotateSpeed+rndVal*38.6f);
        }

        if (r_model_autobobbing && FDef.bobSpeed) {
          //GCon->Logf("UID: %3u (%s)", (hashU32(mobj->GetUniqueId())&0xff), *mobj->GetClass()->GetFullName());
          const float bobHeight = 4.0f;
          const float zdelta = msin(AngleMod(Level->Time*FDef.bobSpeed+rndVal*44.5f))*bobHeight;
          Md2Org.z += zdelta+bobHeight;
        }
      }

      // VMatrix4::RotateY: roll
      // VMatrix4::RotateZ: yaw
      // VMatrix4::RotateX: pitch
    }

    if (updateRot) Transform.TransRot = Transform.MatTrans.getAngles();

    // light
    vuint32 Md2Light = ri.light;
    if (SubMdl.FullBright) Md2Light = 0xffffffff;

    switch (Pass) {
      case RPASS_Normal:
      case RPASS_NonShadow:
      case RPASS_Glass:
        if (true /*IsViewModel || !isShadowVol*/) {
          RenderStyleInfo newri = ri;
          newri.light = Md2Light;
          newri.alpha = Md2Alpha;
          if (!newri.translucency && Md2Alpha < 1.0f) newri.translucency = RenderStyleInfo::Translucent;
          if (Pass == RPASS_Glass && !newri.translucency) {
            newri.translucency = RenderStyleInfo::Translucent;
          }
          Drawer->DrawAliasModel(Md2Org, Md2Angle, Transform,
            SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
            Trans, ColorMap,
            newri,
            IsViewModel, smooth_inter, Interpolate, SubMdl.UseDepth,
            SubMdl.AllowTransparency,
            !IsViewModel && isShadowVol); // for advanced renderer, we need to fill z-buffer, but not color buffer
        }
        break;
      case RPASS_Ambient:
        if (!SubMdl.AllowTransparency) {
          Drawer->DrawAliasModelAmbient(Md2Org, Md2Angle, Transform,
            SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
            Md2Light, Md2Alpha, smooth_inter, Interpolate, SubMdl.UseDepth,
            SubMdl.AllowTransparency);
        }
        break;
      case RPASS_ShadowVolumes:
        Drawer->DrawAliasModelShadow(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, smooth_inter, Interpolate,
          LightPos, LightRadius);
        break;
      case RPASS_ShadowMaps:
        Drawer->DrawAliasModelShadowMap(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
          Md2Alpha, smooth_inter, Interpolate, SubMdl.AllowTransparency);
        break;
      case RPASS_Light:
        Drawer->DrawAliasModelLight(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
          Md2Alpha, smooth_inter, Interpolate, SubMdl.AllowTransparency);
        break;
      case RPASS_Textures:
        Drawer->DrawAliasModelTextures(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
          Trans, ColorMap, ri, smooth_inter, Interpolate, SubMdl.UseDepth,
          SubMdl.AllowTransparency);
        break;
      case RPASS_Fog:
        Drawer->DrawAliasModelFog(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
          ri.fade, Md2Alpha, smooth_inter, Interpolate, SubMdl.AllowTransparency);
        break;
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::GetClassNameForModel
//
//==========================================================================
VName VRenderLevelShared::GetClassNameForModel (VEntity *mobj) noexcept {
  return
    mobj && mobj->State ?
      (r_models_strict ? mobj->GetClass()->Name : mobj->State->Outer->Name) :
      NAME_None;
}


//==========================================================================
//
//  VRenderLevelShared::IsAliasModelAllowedFor
//
//==========================================================================
bool VRenderLevelShared::IsAliasModelAllowedFor (VEntity *Ent) {
  if (!Ent || !Ent->State || Ent->IsGoingToDie() || !r_models) return false;
  switch (Ent->Classify()) {
    case VEntity::EType::ET_Unknown: return r_models_other;
    case VEntity::EType::ET_Player: return r_models_players;
    case VEntity::EType::ET_Missile: return r_models_missiles;
    case VEntity::EType::ET_Corpse: return r_models_corpses;
    case VEntity::EType::ET_Monster: return r_models_monsters;
    case VEntity::EType::ET_Decoration: return r_models_decorations;
    case VEntity::EType::ET_Pickup: return r_models_pickups;
    default: abort();
  }
  return true;
}


//==========================================================================
//
//  VRenderLevelShared::IsShadowAllowedFor
//
//==========================================================================
bool VRenderLevelShared::IsShadowAllowedFor (VEntity *Ent) {
  if (!Ent || !Ent->State || Ent->IsGoingToDie() || !r_models || !r_model_shadows) return false;
  if (cl && Ent == cl->Camera && !r_camera_player_shadows) return false;
  switch (Ent->Classify()) {
    case VEntity::EType::ET_Unknown: return r_shadows_other;
    case VEntity::EType::ET_Player: return r_shadows_players;
    case VEntity::EType::ET_Missile: return r_shadows_missiles;
    case VEntity::EType::ET_Corpse: return r_shadows_corpses;
    case VEntity::EType::ET_Monster: return r_shadows_monsters;
    case VEntity::EType::ET_Decoration: return r_shadows_decorations;
    case VEntity::EType::ET_Pickup: return r_shadows_pickups;
    default: abort();
  }
  return true;
}


//==========================================================================
//
//  VRenderLevelShared::HasEntityAliasModel
//
//==========================================================================
bool VRenderLevelShared::HasEntityAliasModel (VEntity *Ent) const {
  return (IsAliasModelAllowedFor(Ent) && FindClassModelByName(GetClassNameForModel(Ent)));
}


//==========================================================================
//
//  VRenderLevelShared::DrawAliasModel
//
//  this is used to draw so-called "fixed model"
//
//==========================================================================
bool VRenderLevelShared::DrawAliasModel (VEntity *mobj, const TVec &Org, const TAVec &Angles,
  float ScaleX, float ScaleY, VModel *Mdl,
  const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame,
  VTextureTranslation *Trans, int Version,
  const RenderStyleInfo &ri,
  bool IsViewModel, float Inter, bool Interpolate,
  ERenderPass Pass)
{
  //if (!IsViewModel && !IsAliasModelAllowedFor(mobj)) return false;
  int FIdx = FindFrame(*Mdl->DefaultClass, Frame, Inter);
  if (FIdx == -1) return false;
  float InterpFrac;
  int NFIdx = FindNextFrame(*Mdl->DefaultClass, FIdx, NextFrame, Inter, InterpFrac);
  if (NFIdx == -1) {
    NFIdx = FIdx;
    Interpolate = false;
  }
  DrawModel(Level, mobj, Org, Angles, ScaleX, ScaleY, *Mdl->DefaultClass, FIdx,
    NFIdx, Trans, ColorMap, Version, ri, /*Light, Fade, Alpha, Additive,*/
    IsViewModel, InterpFrac, Interpolate, CurrLightPos, CurrLightRadius,
    Pass, IsShadowVolumeRenderer() && !IsShadowMapRenderer());
  return true;
}


//==========================================================================
//
//  VRenderLevelShared::DrawAliasModel
//
//  this is used to draw entity models
//
//==========================================================================
bool VRenderLevelShared::DrawAliasModel (VEntity *mobj, VName clsName, const TVec &Org, const TAVec &Angles,
  float ScaleX, float ScaleY,
  const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame, //old:VState *State, VState *NextState,
  VTextureTranslation *Trans, int Version,
  const RenderStyleInfo &ri,
  bool IsViewModel, float Inter, bool Interpolate,
  ERenderPass Pass)
{
  if (clsName == NAME_None) return false;
  if (!IsViewModel && !IsAliasModelAllowedFor(mobj)) return false;

  VClassModelScript *Cls = FindClassModelByName(clsName);
  if (!Cls) {
    //GCon->Logf(NAME_Debug, "NO VIEW MODEL for class `%s`", *clsName);
    return false;
  }

  int FIdx = FindFrame(*Cls, Frame, Inter);
  if (FIdx == -1) {
    /*
    GCon->Logf(NAME_Debug, "NO VIEW MODEL for class `%s`: %s", *clsName, *Frame.toString());
    GCon->Logf(NAME_Debug, "  MFI: %s", *mobj->getMFI().toString());
    GCon->Logf(NAME_Debug, "  NEXT MFI: %s", *mobj->getNextMFI().toString());
    */
    //abort();
    return false;
  }

  /*
  GCon->Logf(NAME_Debug, "***FOUND view model for class `%s` (fidx=%d): %s", *clsName, FIdx, *Frame.toString());
  GCon->Logf(NAME_Debug, "  State: %s", *mobj->State->Loc.toStringNoCol());
  GCon->Logf(NAME_Debug, "  MFI: %s", *mobj->getMFI().toString());
  GCon->Logf(NAME_Debug, "  NEXT MFI: %s", *mobj->getNextMFI().toString());
  */

  // note that gzdoom-imported modeldef can have more than one model attached to one frame
  // process all attachments -- they should differ by model or submodel indicies

  const bool origInterp = Interpolate;
  while (FIdx >= 0) {
    float InterpFrac;
    int NFIdx = FindNextFrame(*Cls, FIdx, NextFrame, Inter, InterpFrac);
    if (NFIdx == -1) {
      NFIdx = FIdx;
      Interpolate = false;
    } else {
      Interpolate = origInterp;
    }

    DrawModel(Level, mobj, Org, Angles, ScaleX, ScaleY, *Cls, FIdx, NFIdx, Trans,
      ColorMap, Version, ri, IsViewModel,
      InterpFrac, Interpolate, CurrLightPos, CurrLightRadius, Pass,
      IsShadowVolumeRenderer() && !IsShadowMapRenderer());

    // try next one
    const VScriptedModelFrame &cfrm = Cls->Frames[FIdx];
    int res = -1;
    if (cfrm.sprite != NAME_None) {
      // by sprite name
      //if (IsViewModel) GCon->Logf(NAME_Debug, "000: %s: sprite=%s %c; midx=%d; smidx=%d; inter=%g (%g); nidx=%d", *Cls->Name, *cfrm.sprite, 'A'+cfrm.frame, cfrm.ModelIndex, cfrm.SubModelIndex, Inter, cfrm.Inter, FIdx);
      FIdx = cfrm.nextSpriteIdx;
      //GCon->Logf(NAME_Debug, "000: %s: sprite=%s %c; midx=%d; smidx=%d; inter=%g (%g); nidx=%d", *Cls->Name, *cfrm.sprite, 'A'+cfrm.frame, cfrm.ModelIndex, cfrm.SubModelIndex, Inter, cfrm.Inter, FIdx);
      while (FIdx >= 0) {
        const VScriptedModelFrame &nfrm = Cls->Frames[FIdx];
        //GCon->Logf(NAME_Debug, "  001: %s: sprite=%s %c; midx=%d; smidx=%d; inter=%g (%g)", *Cls->Name, *nfrm.sprite, 'A'+nfrm.frame, nfrm.ModelIndex, nfrm.SubModelIndex, Inter, nfrm.Inter);
        if (cfrm.ModelIndex != nfrm.ModelIndex || cfrm.SubModelIndex != nfrm.SubModelIndex) {
               if (nfrm.Inter <= Inter) res = FIdx;
          else if (nfrm.Inter > Inter) break; // the author shouldn't write incorrect defs
        }
        FIdx = nfrm.nextSpriteIdx;
      }
    } else {
      // by frame index
      FIdx = cfrm.nextNumberIdx;
      while (FIdx >= 0) {
        const VScriptedModelFrame &nfrm = Cls->Frames[FIdx];
        if (cfrm.ModelIndex != nfrm.ModelIndex || cfrm.SubModelIndex != nfrm.SubModelIndex) {
               if (nfrm.Inter <= Inter) res = FIdx;
          else if (nfrm.Inter > Inter) break; // the author shouldn't write incorrect defs
        }
        FIdx = nfrm.nextNumberIdx;
      }
    }
    FIdx = res;
  }

  return true;
}


//==========================================================================
//
//  FindFixedModelFor
//
//==========================================================================
static VModel *FindFixedModelFor (VEntity *Ent, bool verbose) {
  vassert(Ent);
  auto mpp = fixedModelMap.get(Ent->FixedModelName);
  if (mpp) return *mpp;
  // first time
  VStr fname = VStr("models/")+Ent->FixedModelName;
  if (!FL_FileExists(fname)) {
    if (verbose) GCon->Logf(NAME_Warning, "Can't find alias model '%s'", *Ent->FixedModelName);
    fixedModelMap.put(Ent->FixedModelName, nullptr);
    return nullptr;
  } else {
    VModel *mdl = Mod_FindName(fname);
    fixedModelMap.put(Ent->FixedModelName, mdl);
    return mdl;
  }
}


//==========================================================================
//
//  VRenderLevelShared::IsTranslucentEntityModel
//
//  returns `true` if this model should be queued as translucent
//
//==========================================================================
bool VRenderLevelShared::IsTranslucentEntityModel (VEntity *Ent, const RenderStyleInfo &ri,
                                                   float Inter)
{
  if (!Ent || (Ent->EntityFlags&VEntity::EF_FixedModel)) return false;

  VClassModelScript *Cls = FindClassModelByName(VRenderLevelShared::GetClassNameForModel(Ent));
  if (!Cls) return false;

  return Cls->asTranslucent;
}


//==========================================================================
//
//  VRenderLevelShared::DrawEntityModel
//
//==========================================================================
bool VRenderLevelShared::DrawEntityModel (VEntity *Ent, const RenderStyleInfo &ri, float Inter, ERenderPass Pass) {
  //VState *DispState = (Ent->EntityFlags&VEntity::EF_UseDispState ? Ent->DispState : Ent->State);
  //VState *DispState = Ent->State; //FIXME: skipframes

  // movement interpolation
  TVec sprorigin = Ent->GetDrawOrigin();

  // check if we want to interpolate model frames
  const bool Interpolate = r_interpolate_frames;
  if (Ent->EntityFlags&VEntity::EF_FixedModel) {
    VModel *Mdl = FindFixedModelFor(Ent, true); // verbose
    if (!Mdl) return false;
    return DrawAliasModel(Ent, sprorigin,
      Ent->/*Angles*/GetModelDrawAngles(), Ent->ScaleX, Ent->ScaleY, Mdl,
      Ent->getMFI(), Ent->getNextMFI(),
      GetTranslation(Ent->Translation),
      Ent->ModelVersion, ri, false, Inter,
      Interpolate, Pass);
  } else {
    return DrawAliasModel(Ent, GetClassNameForModel(Ent), sprorigin,
      Ent->/*Angles*/GetModelDrawAngles(), Ent->ScaleX, Ent->ScaleY,
      Ent->getMFI(), Ent->getNextMFI(),
      GetTranslation(Ent->Translation), Ent->ModelVersion,
      ri, false, Inter, Interpolate, Pass);
  }
}


//==========================================================================
//
//  VRenderLevelShared::CheckAliasModelFrame
//
//==========================================================================
bool VRenderLevelShared::CheckAliasModelFrame (VEntity *Ent, float Inter) {
  if (!Ent->State) return false;
  if (Ent->EntityFlags&VEntity::EF_FixedModel) {
    VModel *Mdl = FindFixedModelFor(Ent, false); // silent
    if (!Mdl) return false;
    return FindFrame(*Mdl->DefaultClass, Ent->getMFI(), Inter) != -1;
  } else {
    //VClassModelScript *Cls = FindClassModelByName(Ent->State->Outer->Name);
    VClassModelScript *Cls = FindClassModelByName(GetClassNameForModel(Ent));
    if (!Cls) return false;
    return (FindFrame(*Cls, Ent->getMFI(), Inter) != -1);
  }
}


//==========================================================================
//
//  R_DrawModelFrame
//
//  used only in UI, for model selector
//
//==========================================================================
void R_DrawModelFrame (const TVec &Origin, float Angle, VModel *Model,
  int Frame, int NextFrame,
  //const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame,
  const char *Skin, int TranslStart,
  int TranslEnd, int Color, float Inter)
{
  (void)Origin;
  (void)Angle;
  (void)Model;
  (void)Frame;
  (void)NextFrame;
  (void)Skin;
  (void)TranslStart;
  (void)TranslEnd;
  (void)Color;
  (void)Inter;

  //FIXME!
  /*
  bool Interpolate = true;
  int FIdx = FindFrame(*Model->DefaultClass, Frame, Inter);
  if (FIdx == -1) return;

  float InterpFrac;
  int NFIdx = FindNextFrame(*Model->DefaultClass, FIdx, NextFrame, Inter, InterpFrac);
  if (NFIdx == -1) {
    NFIdx = 0;
    Interpolate = false;
  }

  Drawer->viewangles.yaw = 180;
  Drawer->viewangles.pitch = 0;
  Drawer->viewangles.roll = 0;
  AngleVectors(Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup);
  Drawer->vieworg = TVec(0, 0, 0);

  refdef_t rd;

  rd.x = 0;
  rd.y = 0;
  rd.width = Drawer->getWidth();
  rd.height = Drawer->getHeight();
  rd.fovx = tan(DEG2RADF(90)*0.5f);
  rd.fovy = rd.fovx*3.0f/4.0f;
  rd.drawworld = false;
  rd.DrawCamera = false;

  Drawer->SetupView(nullptr, &rd);
  Drawer->SetupViewOrg();

  TAVec Angles;
  Angles.yaw = Angle;
  Angles.pitch = 0;
  Angles.roll = 0;

  DrawModel(nullptr, nullptr, Origin, Angles, 1.0f, 1.0f, *Model->DefaultClass, FIdx,
    NFIdx, R_GetCachedTranslation(R_SetMenuPlayerTrans(TranslStart,
    TranslEnd, Color), nullptr), 0, 0, 0xffffffff, 0, 1.0f, false, false,
    InterpFrac, Interpolate, TVec(), 0, RPASS_Normal, true); // force draw

  Drawer->EndView();
  */
}


//==========================================================================
//
//  R_DrawStateModelFrame
//
//  called from UI widget only
//
//==========================================================================
bool R_DrawStateModelFrame (VState *State, VState *NextState, float Inter,
                            const TVec &Origin, float Angle)
{
  bool Interpolate = true;
  if (!State) return false;
  VClassModelScript *Cls = FindClassModelByName(State->Outer->Name);
  if (!Cls) return false;
  int FIdx = FindFrame(*Cls, State->getMFI(), Inter);
  if (FIdx == -1) return false;
  float InterpFrac;
  int NFIdx = FindNextFrame(*Cls, FIdx, (NextState ? NextState->getMFI() : State->getMFI()), Inter, InterpFrac);
  if (NFIdx == -1) {
    NFIdx = 0;
    Interpolate = false;
  }

  Drawer->viewangles.yaw = 180;
  Drawer->viewangles.pitch = 0;
  Drawer->viewangles.roll = 0;
  AngleVectors(Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup);
  Drawer->vieworg = TVec(0, 0, 0);

  refdef_t rd;

  rd.x = 0;
  rd.y = 0;
  rd.width = Drawer->getWidth();
  rd.height = Drawer->getHeight();
  TClipBase::CalcFovXY(&rd.fovx, &rd.fovy, rd.width, rd.height, 90.0f, PixelAspect);
  rd.drawworld = false;
  rd.DrawCamera = false;

  Drawer->SetupView(nullptr, &rd);
  Drawer->SetupViewOrg();

  TAVec Angles;
  Angles.yaw = Angle;
  Angles.pitch = 0;
  Angles.roll = 0;

  RenderStyleInfo ri;
  ri.light = 0xffffffffu;
  ri.fade = 0;
  ri.alpha = 1.0f;
  ri.translucency = RenderStyleInfo::Normal;
  DrawModel(nullptr, nullptr, Origin, Angles, 1.0f, 1.0f, *Cls, FIdx, NFIdx, nullptr, 0, 0,
    ri, //0xffffffff, 0, 1.0f, false,
    false, InterpFrac, Interpolate, TVec(), 0, RPASS_Normal, true); // force draw

  Drawer->EndView();
  return true;
}


//static VClass *bulbClass = nullptr;


//==========================================================================
//
// R_DrawLightBulb
//
//==========================================================================
void R_DrawLightBulb (const TVec &Org, const TAVec &Angles, vuint32 rgbLight, ERenderPass Pass, bool isShadowVol, float ScaleX, float ScaleY) {
  /*
  if (!bulbClass) {
    bulbClass = VClass:FindClass("K8DebugLightBulb");
    if (!bulbClass) return;
  }
  */

  VClassModelScript *Cls = FindClassModelByName("K8DebugLightBulb");
  if (!Cls) return;

  VAliasModelFrameInfo Frame;
  Frame.sprite = NAME_None;
  Frame.frame = 0;
  Frame.index = 0;
  Frame.spriteIndex = 0;

  int FIdx = FindFrame(*Cls, Frame, 0.0f);
  if (FIdx == -1) return;

  rgbLight |= 0xff000000u;

  RenderStyleInfo ri;
  ri.light = rgbLight;
  ri.fade = 0;
  ri.alpha = 1.0f;
  ri.translucency = RenderStyleInfo::Normal;

  DrawModel(nullptr, nullptr, Org, Angles, ScaleX, ScaleY, *Cls, FIdx, FIdx, nullptr/*translation*/,
    0/*colormap*/, 0/*version*/, ri, false/*isviewmodel*/,
    0.0f/*interpfrac*/, false/*interpolate*/, TVec()/*currlightpos*/, 0/*currlightradius*/,
    Pass, isShadowVol);
}

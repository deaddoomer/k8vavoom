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
#ifndef VAVOOM_R_LOCAL_GZ_HEADER
#define VAVOOM_R_LOCAL_GZ_HEADER

#include "../gamedefs.h"
//#include "r_local.h"


// ////////////////////////////////////////////////////////////////////////// //
class GZModelDef {
public:
  struct Frame {
    VStr sprbase; // always lowercased
    int sprframe; // sprite frame index (i.e. 'A' is 0)
    int mdindex; // model index in `models`
    int origmdindex; // this is used to find replacements on merge
    int origmdlframe; // this is used to find replacements on merge
    int frindex; // frame index in model (will be used to build frame map)
    VStr frname; // for MD2 named frames
    // in k8vavoom, this is set per-frame
    // set in `checkModelSanity()`
    float rotationSpeed; // !0: rotating
    int usePitch; // 0: no; -1: inherit; 1: from momentum
    bool usePitchInverted;
    int useRoll; // 0: no; -1: inherit; 1: from momentum
    VMatrix4 matTrans; // local frame translation matrix
    int vvindex; // vavoom frame index in the given model (-1: invalid frame)
    // used only in sanity check method
    int linkSprBase; // <0: end of list

    VStr toString () const {
      VStr res = sprbase.toUpperCase();
      res += (char)(sprframe+'A');
      res += va(" mdi(%d) origmdi(%d) fri(%d) vvi(%d) rs(%g)",
        mdindex, origmdindex, frindex, vvindex,
        //angleOffset.yaw, angleOffset.pitch, angleOffset.roll,
        rotationSpeed);
      return res;
    }
  };

  struct MdlFrameInfo {
    int mdlindex; // model index in `models`
    int mdlframe; // model frame number
    int vvframe; // k8vavoom frame number
    // as we can merge models, different frames can have different translation matrices
    VMatrix4 matTrans; // local frame translation matrix
    // temporary working data
    bool used;

    VStr toString () const {
      return VStr(va("mdl(%d); frm(%d); vvfrm(%d)", mdlindex, mdlframe, vvframe));
    }
  };

  // model and skin definition
  struct MSDef {
    VStr modelFile; // with path
    VStr skinFile; // with path
    TArray<VStr> subskinFiles; // with path
    // set in `checkModelSanity()`
    TArray<MdlFrameInfo> frameMap;
    // used in sanity checks
    bool reported;
    TArray<VStr> frlist; // frame names for MD2 model
    bool frlistLoaded;
  };

protected:
  void checkModelSanity (const VMatrix4 &mat);

  // -1: not found
  int findModelFrame (int mdlindex, int mdlframe, bool allowAppend, const VMatrix4 &mat);

  VStr buildPath (VScriptParser *sc, VStr path);

public:
  VStr className;
  TArray<MSDef> models;
  float rotationSpeed; // !0: rotating
  bool usePitch;
  bool usePitchInverted;
  bool usePitchMomentum;
  bool useRoll;
  TVec rotationCenter;
  TArray<Frame> frames;

public:
  GZModelDef ();
  virtual ~GZModelDef ();

  void clear ();

  inline bool isEmpty () const { return (models.length() == 0 || frames.length() == 0); }

  // "model" keyword already eaten
  void parse (VScriptParser *sc);

  // merge this model frames with another model frames
  // GZDoom seems to do this, so we have too
  void merge (GZModelDef &other);

  VStr createXml ();

  // override this function to allow "Frame" modeldef parsing
  // return `true` if model was succesfully found and parsed, or
  // false if model wasn't found or in invalid format
  // WARNING: don't clear `names` array!
  virtual bool ParseMD2Frames (VStr mdpath, TArray<VStr> &names);

  virtual bool IsModelFileExists (VStr mdpath);
};


#endif

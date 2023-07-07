//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
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
// decal, decalgroup, decal animator
#ifndef VAVOOM_PSIM_DECAL_PRIVATE_HEADER
#define VAVOOM_PSIM_DECAL_PRIVATE_HEADER

#include "p_decal.h"


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimFader : public VDecalAnim {
public:
  enum { TypeId = TDecAnimFader };

public:
  // animator properties
  DecalFloatVal startTime, actionTime; // in seconds

protected:
  virtual void doIO (VStream &strm, VNTValueIOEx &vio) override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  inline VDecalAnimFader (ENoInit) noexcept : VDecalAnim(E_NoInit) {}
  inline VDecalAnimFader () noexcept : VDecalAnim(), startTime(0.0f), actionTime(0.0f) {}
  virtual ~VDecalAnimFader ();

  virtual vuint8 getTypeId () const noexcept override;
  virtual const char *getTypeName () const noexcept override;

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone (bool forced=false) override;

  virtual void genValues () noexcept override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  virtual bool calcWillDisappear () const noexcept override;

  virtual bool isFader () const noexcept override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimStretcher : public VDecalAnim {
public:
  enum { TypeId = TDecAnimStretcher };

public:
  // animator properties
  DecalFloatVal goalX, goalY;
  DecalFloatVal startTime, actionTime; // in seconds

protected:
  virtual void doIO (VStream &strm, VNTValueIOEx &vio) override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  inline VDecalAnimStretcher (ENoInit) noexcept : VDecalAnim(E_NoInit) {}
  inline VDecalAnimStretcher () noexcept : VDecalAnim(), goalX(), goalY(), startTime(0.0f), actionTime(0.0f) {}
  virtual ~VDecalAnimStretcher ();

  virtual vuint8 getTypeId () const noexcept override;
  virtual const char *getTypeName () const noexcept override;

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone (bool forced=false) override;

  virtual void genValues () noexcept override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  virtual bool calcWillDisappear () const noexcept override;
  virtual void calcMaxScales (float *sx, float *sy) const noexcept override;

  virtual bool isStretcher () const noexcept override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimSlider : public VDecalAnim {
public:
  enum { TypeId = TDecAnimSlider };

public:
  // animator properties
  DecalFloatVal distX, distY;
  DecalFloatVal startTime, actionTime; // in seconds
  bool k8reversey;

protected:
  virtual void doIO (VStream &strm, VNTValueIOEx &vio) override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  inline VDecalAnimSlider (ENoInit) noexcept : VDecalAnim(E_NoInit) {}
  inline VDecalAnimSlider () noexcept : VDecalAnim(), distX(), distY(), startTime(0.0f), actionTime(0.0f), k8reversey(false) {}
  virtual ~VDecalAnimSlider ();

  virtual vuint8 getTypeId () const noexcept override;
  virtual const char *getTypeName () const noexcept override;

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone (bool forced=false) override;

  virtual void genValues () noexcept override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  virtual bool isSlider () const noexcept override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimColorChanger : public VDecalAnim {
public:
  enum { TypeId = TDecAnimColorChanger };

public:
  // animator properties
  float dest[3];
  DecalFloatVal startTime, actionTime; // in seconds

protected:
  virtual void doIO (VStream &strm, VNTValueIOEx &vio) override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  inline VDecalAnimColorChanger (ENoInit) noexcept : VDecalAnim(E_NoInit) {}
  inline VDecalAnimColorChanger () noexcept : VDecalAnim(), startTime(0.0f), actionTime(0.0f) { dest[0] = dest[1] = dest[2] = 0.0f; }
  virtual ~VDecalAnimColorChanger ();

  virtual vuint8 getTypeId () const noexcept override;
  virtual const char *getTypeName () const noexcept override;

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone (bool forced=false) override;

  virtual void genValues () noexcept override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  virtual bool isColorChanger () const noexcept override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimCombiner : public VDecalAnim {
public:
  enum { TypeId = TDecAnimCombiner };

private:
  bool mIsCloned;

protected:
  virtual void doIO (VStream &strm, VNTValueIOEx &vio) override;

public:
  // animator properties
  TArray<VName> nameList; // can be empty in cloned/loaded object
  TArray<VDecalAnim *> list; // can contain less items than `nameList`

protected:
  virtual void fixup () override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  inline VDecalAnimCombiner (ENoInit) noexcept : VDecalAnim(E_NoInit) {}
  inline VDecalAnimCombiner () noexcept : VDecalAnim(), mIsCloned(false), nameList(), list() {}
  virtual ~VDecalAnimCombiner ();

  virtual vuint8 getTypeId () const noexcept override;
  virtual const char *getTypeName () const noexcept override;

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone (bool forced=false) override;

  virtual void genValues () noexcept override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  virtual bool calcWillDisappear () const noexcept override;
  virtual void calcMaxScales (float *sx, float *sy) const noexcept override;

  virtual bool isCombiner () const noexcept override;

  virtual int getCount () const noexcept override;
  virtual VDecalAnim *getAt (int idx) noexcept override;
  virtual bool hasTypeId (vuint8 tid, int depth=0) const noexcept override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


#endif

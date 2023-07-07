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

enum {
  CVAR_None       = 0,
  CVAR_Archive    = 0x0001u, // set to cause it to be saved to config.cfg
  CVAR_UserInfo   = 0x0002u, // added to userinfo when changed
  CVAR_ServerInfo = 0x0004u, // added to serverinfo when changed
  CVAR_Init       = 0x0008u, // don't allow change from console at all, but can be set from the command line
  CVAR_Latch      = 0x0010u, // save changes until server restart
  CVAR_Rom        = 0x0020u, // display only, cannot be set by user at all
  CVAR_Cheat      = 0x0040u, // can not be changed if cheats are disabled
  CVAR_Modified   = 0x0080u, // set each time the cvar is changed
  CVAR_FromMod    = 0x0100u, // this cvar came from cvarinfo
  CVAR_PreInit    = 0x4000u, // CLI change for this cvar should be processed before initializing the main game
  //
  CVAR_AlwaysArchive = 0x8000u, // always write to config
  //
  CVAR_ACS = 0x10000u, // created from ACS script
  //
  CVAR_User   = 0x20000u, // created with `set`
  CVAR_Hidden = 0x40000u,
  //
  CVAR_NoShadow = 0x80000u, // this cvar cannot be shadowed
};


// ////////////////////////////////////////////////////////////////////////// //
// console variable
class VCvar {
public:
  enum CVType {
    String,
    Int,
    Float,
    Bool,
  };

protected:
  const char *Name; // variable name
  const char *DefaultString; // default value
  const char *HelpString; // this *can* be owned, but as we never deleting cvar objects, it doesn't matter
  bool defstrOwned; // `true` if `DefaultString` is owned and should be deleted
  VStr StringValue; // current value
  unsigned Flags; // `CVAR_XXX` flags
  CVType Type;
  int IntValue; // atoi(string)
  float FloatValue; // atof(string)
  bool BoolValue; // interprets various "true" strings
  VStr LatchedString; // for `CVAR_Latch` variables
  VCvar *nextInBucket; // next cvar in this bucket
  vuint32 lnhash; // hash of lo-cased variable name
  VCvar *shadowVar; // if this cvar is shadowed, always return this value (but set the main one)

private:
  void CoerceToString ();
  void CoerceToFloat ();
  void CoerceToInt ();
  void CoerceToBool ();

  void EnsureShadow ();

public:
  // called on any change, before other callbacks
  void (*MeChangedCB) (VCvar *cvar, VStr oldValue);

public:
  VCvar (const char *AName, const char *ADefault, const char *AHelp, int AFlags=CVAR_None, CVType AType=CVType::String);
  VCvar (const char *AName, VStr ADefault, VStr AHelp, int AFlags=CVAR_None, CVType AType=CVType::String);

  static void AddAllVarsToAutocomplete (void (*addfn) (const char *name));

  inline bool IsACS () const noexcept { return (Flags&CVAR_ACS); }
  inline void SetACS () noexcept { Flags |= CVAR_ACS|CVAR_FromMod; }

  inline bool IsUser () const noexcept { return (Flags&CVAR_User); }
  inline void SetUser () noexcept { Flags |= CVAR_User; }

  inline bool IsHidden () const noexcept { return (Flags&CVAR_Hidden); }

  inline bool IsShadowed () const noexcept { return !!shadowVar; }

  inline CVType GetType () const noexcept { return Type; }
  // this will coerce values, if necessary
  void SetType (CVType atype);

  void SetInt (int value);
  void SetFloat (float value);
  void SetStr (VStr value);
  void SetBool (bool value);
  void SetStr (const char *value); // "null" means "default"

  void SetDefault (VStr value);

  inline void Set (int value) { SetInt(value); }
  inline void Set (float value) { SetFloat(value); }
  inline void Set (VStr value) { SetStr(value); }
  inline void Set (bool value) { SetBool(value); }
  inline void Set (const char *value) { SetStr(value); } // "null" means "default"

  // changes shadowvar if there is any
  inline void ForceSetInt (int value) { if (shadowVar) shadowVar->SetInt(value); else SetInt(value); }
  inline void ForceSetFloat (float value) { if (shadowVar) shadowVar->SetFloat(value); else SetFloat(value); }
  inline void ForceSetStr (VStr value) { if (shadowVar) shadowVar->SetStr(value); else SetStr(value); }
  inline void ForceSetBool (bool value) { if (shadowVar) shadowVar->SetBool(value); else SetBool(value); }
  inline void ForceSetStr (const char *value) { if (shadowVar) shadowVar->SetStr(value ? value : GetDefault()); else SetStr(value); }

  // changes shadowvar if there is any
  inline void ForceSet (int value) { ForceSetInt(value); }
  inline void ForceSet (float value) { ForceSetFloat(value); }
  inline void ForceSet (VStr value) { ForceSetStr(value); }
  inline void ForceSet (bool value) { ForceSetBool(value); }
  inline void ForceSet (const char *value) { ForceSetStr(value); }

  // WARNING! shadowed userinfo/serverinfo cvars won't work as expected!
  void SetShadowInt (int value);
  void SetShadowFloat (float value);
  void SetShadowStr (VStr value);
  void SetShadowBool (bool value);
  void SetShadowStr (const char *value);

  inline void SetShadow (int value) { SetShadowInt(value); }
  inline void SetShadow (float value) { SetShadowFloat(value); }
  inline void SetShadow (VStr value) { SetShadowStr(value); }
  inline void SetShadow (const char *value) { SetShadowStr(value); }

  inline bool IsReadOnly () const noexcept { return (Flags&CVAR_Rom); }
  inline void SetReadOnly (bool v) noexcept { if (v) Flags |= CVAR_Rom; else Flags &= ~CVAR_Rom; }

  // this clears modified flag on call
  inline bool IsModified () noexcept {
    bool ret = !!(Flags&CVAR_Modified);
    // clear modified flag
    Flags &= ~CVAR_Modified;
    return ret;
  }

  inline bool IsModifiedNoClear () const noexcept { return !!(Flags&CVAR_Modified); }

  inline bool IsPreInit () const noexcept { return !!(Flags&CVAR_PreInit); }
  inline bool IsModVar () const noexcept { return ((Flags&CVAR_FromMod) != 0); }

  inline bool asBool () const noexcept { return (shadowVar ? shadowVar->BoolValue : BoolValue); }
  inline int asInt () const noexcept { return (shadowVar ? shadowVar->IntValue : IntValue); }
  inline float asFloat () const noexcept { return (shadowVar ? shadowVar->FloatValue : FloatValue); }
  inline VStr asStr () const noexcept { return (shadowVar ? shadowVar->StringValue : StringValue); }

  inline int GetFlags () const noexcept { return Flags; }
  inline const char *GetName () const noexcept { return Name; }
  inline const char *GetHelp () const noexcept { return (HelpString ? HelpString : "no help yet: FIXME!!!"); }
  inline const char *GetDefault () const noexcept { return DefaultString; }

  bool CanBeModified (bool modonly=false, bool noserver=true) const noexcept;

  static void Init ();
  static void Shutdown ();

  static void SendAllUserInfos ();
  static void SendAllServerInfos ();

  static bool HasVar (const char *var_name);
  static bool HasModVar (const char *var_name);
  static bool HasModUserVar (const char *var_name); // non-server

  static bool CanBeModified (const char *var_name, bool modonly=false, bool noserver=true);

  static VCvar *CreateNew (VName var_name, VStr ADefault, VStr AHelp, int AFlags);
  static VCvar *CreateNewInt (VName var_name, int ADefault, VStr AHelp, int AFlags);
  static VCvar *CreateNewFloat (VName var_name, float ADefault, VStr AHelp, int AFlags);
  static VCvar *CreateNewBool (VName var_name, bool ADefault, VStr AHelp, int AFlags);
  static VCvar *CreateNewStr (VName var_name, VStr ADefault, VStr AHelp, int AFlags);

  static int GetInt (const char *var_name);
  static float GetFloat (const char *var_name);
  static bool GetBool (const char *var_name);
  //static const char *GetCharp (const char *var_name);
  static VStr GetString (const char *var_name);
  static VStr GetHelp (const char *var_name); // returns nullptr if there is no such cvar
  static int GetVarFlags (const char *var_name); // -1: not found

  static void Set (const char *var_name, int value);
  static void Set (const char *var_name, float value);
  static void Set (const char *var_name, VStr value);

  static bool Command (const TArray<VStr> &Args);
  static void WriteVariablesToStream (VStream *st, bool saveDefaultValues=false);

  static void Unlatch ();

  static inline void SetCheating (bool newState) { Cheating = newState; }
  static inline bool GetCheating () noexcept { return Cheating; }

  static VCvar *FindVariable (const char *name);

  static bool ParseBool (const char *s);

  // call this to disable setting `CVAR_Init` cvars
  static void HostInitComplete ();

  // unsorted; prefix is case-insensitive; doesn't clear list
  static void GetPrefixedList (TArray<VStr> &list, VStr pfx);

public:
  // required for various console commands
  static vuint32 countCVars ();
  static VCvar **getSortedList (); // contains `countCVars()` elements, must be `delete[]`d

  static void DumpHashStats ();
  static void DumpAllVars ();

private:
  VCvar (const VCvar &);
  void operator = (const VCvar &);

  void insertIntoList ();
  VCvar *insertIntoHash ();
  void DoSet (VStr value);

  static bool Initialised;
  static bool Cheating;

public:
  // called on any change, before other callbacks, but after cvar callback
  static void (*ChangedCB) (VCvar *cvar, VStr oldValue);
  // called when new cvar is created
  static void (*CreatedCB) (VCvar *cvar);

  // the following callbacks are called only after `Init() was called`
  // called when cvar with `CVAR_UserInfo` changed
  static void (*UserInfoSetCB) (VCvar *cvar);
  // called when cvar with `CVAR_ServerInfo` changed
  static void (*ServerInfoSetCB) (VCvar *cvar);
};


class VCvarI;
class VCvarF;
class VCvarS;
class VCvarB;


// ////////////////////////////////////////////////////////////////////////// //
// cvar that can be used as int variable
class VCvarI : public VCvar {
public:
  VCvarI (const char *AName, const char *ADefault, const char *AHelp, int AFlags=0) : VCvar(AName, ADefault, AHelp, AFlags, CVType::Int) {}
  VCvarI (const VCvar &);

  inline operator int () const { return IntValue; }
  //inline operator bool () const { return !!IntValue; }
  inline void operator = (int AValue) { Set(AValue); }
  void operator = (const VCvar &v);
  void operator = (const VCvarB &v);
  void operator = (const VCvarI &v);
};


// ////////////////////////////////////////////////////////////////////////// //
// cvar that can be used as float variable
class VCvarF : public VCvar {
public:
  VCvarF (const char *AName, const char *ADefault, const char *AHelp, int AFlags=0) : VCvar(AName, ADefault, AHelp, AFlags, CVType::Float) {}
  VCvarF (const VCvar &);

  inline operator float () const { return (shadowVar ? shadowVar->asFloat() : FloatValue); }
  //inline operator bool () const { return (isFiniteF(FloatValue) && FloatValue == 0); }
  inline void operator = (float AValue) { Set(AValue); }
  void operator = (const VCvar &v);
  void operator = (const VCvarB &v);
  void operator = (const VCvarI &v);
  void operator = (const VCvarF &v);
};


// ////////////////////////////////////////////////////////////////////////// //
// cvar that can be used as `char *` variable
class VCvarS : public VCvar {
public:
  VCvarS (const char *AName, const char *ADefault, const char *AHelp, int AFlags=0) : VCvar(AName, ADefault, AHelp, AFlags, CVType::String) {}
  VCvarS (const VCvar &);

  inline operator const char * () const { return (shadowVar ? *shadowVar->asStr() : *StringValue); }
  //inline operator bool () const { return ParseBool(*StringValue); }
  inline void operator = (const char *AValue) { Set(AValue); }
  void operator = (const VCvar &v);
};


// ////////////////////////////////////////////////////////////////////////// //
// cvar that can be used as bool variable
class VCvarB : public VCvar {
public:
  VCvarB (const char *AName, bool ADefault, const char *AHelp, int AFlags=0) : VCvar(AName, (ADefault ? "1" : "0"), AHelp, AFlags, CVType::Bool) {}

  inline operator bool () const { return (shadowVar ? shadowVar->asBool() : BoolValue); }
  void operator = (const VCvar &v);
  inline void operator = (bool v) { Set(v ? 1 : 0); }
  void operator = (const VCvarB &v);
  void operator = (const VCvarI &v);
  void operator = (const VCvarF &v);
};

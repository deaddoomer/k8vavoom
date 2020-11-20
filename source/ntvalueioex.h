//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2019-2020 Ketmar Dark
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
//**
//**  extended NTValue-based I/O
//**
//**************************************************************************


// ////////////////////////////////////////////////////////////////////////// //
class VNTValueIOEx : public VNTValueIO {
public:
  VStr prefix;

protected:
  VName transformName (VName vname) const;

public:
  VNTValueIOEx (VStream *astrm);

  // fuck you, shitplusplus!
  virtual void iodef (VName vname, vint32 &v, vint32 defval) override;

  virtual void io (VName vname, vint32 &v) override;
  virtual void io (VName vname, vuint32 &v) override;
  virtual void io (VName vname, float &v) override;
  virtual void io (VName vname, TVec &v) override;
  virtual void io (VName vname, VName &v) override;
  virtual void io (VName vname, VStr &v) override;
  virtual void io (VName vname, VClass *&v) override;
  virtual void io (VName vname, VObject *&v) override;
  virtual void io (VName vname, VSerialisable *&v) override;

  virtual void io (VName vname, VTextureID &v);

  virtual void io (VName vname, VThinker *&v);
  virtual void io (VName vname, VEntity *&v);

  virtual void io (VName vname, vuint8 &v);
};


// ////////////////////////////////////////////////////////////////////////// //
// owns srcstream
// will throw `Host_Error()` on any error
// should not be used with `new`
class VCheckedStream : public VStream {
private:
  mutable VStream *srcStream;
  bool useSysError; // default is false

private:
  void checkError () const;
  void checkValidityCond (bool mustBeTrue);
  inline void checkValidity () { checkValidityCond(true); }

  void openStreamAndCopy (VStream *st, bool doCopy);

public:
  VV_DISABLE_COPY(VCheckedStream)

  VCheckedStream (VStream *ASrcStream); // this should not be used with `new`
  // this seeks to 0
  VCheckedStream (VStream *ASrcStream, bool doCopy); // this should not be used with `new`
  VCheckedStream (int LumpNum, bool aUseSysError=false); // this should not be used with `new`; copies into memory
  virtual ~VCheckedStream () override;

  void SetSysErrorMode (); // use Sys_Error

  //void SetStream (VStream *ASrcStream);

  // stream interface
  virtual VStr GetName () const override;
  virtual bool IsError () const override; // this will call `Host_Error()`
  virtual void Serialise (void *Data, int Length) override;
  virtual void SerialiseBits (void *Data, int Length) override;
  virtual void SerialiseInt (vuint32 &Value/*, vuint32 Max*/) override;

  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual void Flush () override;
  virtual bool Close () override; // will free stream

  // interface functions for objects and classes streams
  virtual void io (VName &) override;
  virtual void io (VStr &) override;
  virtual void io (VObject *&) override;
  virtual void io (VMemberBase *&) override;
  virtual void io (VSerialisable *&) override;

  virtual void SerialiseStructPointer (void *&Ptr, VStruct *Struct) override;
};

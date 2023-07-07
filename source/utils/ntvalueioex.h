//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2019-2023 Ketmar Dark
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
#ifndef VAVOOM_NTVALUEEX_HEADER
#define VAVOOM_NTVALUEEX_HEADER


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
  virtual void iodef (VName vname, vuint32 &v, vuint32 defval) override;
  virtual void iodef (VName vname, float &v, float defval) override;
  virtual void iodef (VName vname, TVec &v, const TVec defval) override;
  virtual void iodef (VName vname, VName &v, VName defval) override;
  virtual void iodef (VName vname, VStr &v, VStr defval) override;

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


#endif

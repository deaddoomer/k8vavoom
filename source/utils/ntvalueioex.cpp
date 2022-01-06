//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2019-2022 Ketmar Dark
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
#include "../gamedefs.h"
#include "ntvalueioex.h"


//==========================================================================
//
//  VNTValueIOEx::VNTValueIOEx
//
//==========================================================================
VNTValueIOEx::VNTValueIOEx (VStream *astrm)
  : VNTValueIO(astrm)
  , prefix(VStr::EmptyString)
{
}


//==========================================================================
//
//  VNTValueIOEx::transformName
//
//==========================================================================
VName VNTValueIOEx::transformName (VName vname) const {
  if (prefix.length() == 0) return vname;
  return VName(*(prefix+(*vname)));
}


//==========================================================================
//
//  VNTValueIOEx::iodef
//
//==========================================================================
void VNTValueIOEx::iodef (VName vname, vint32 &v, vint32 defval) {
  VNTValueIO::iodef(transformName(vname), v, defval);
}


//==========================================================================
//
//  VNTValueIOEx::iodef
//
//==========================================================================
void VNTValueIOEx::iodef (VName vname, vuint32 &v, vuint32 defval) {
  VNTValueIO::iodef(transformName(vname), v, defval);
}


//==========================================================================
//
//  VNTValueIOEx::iodef
//
//==========================================================================
void VNTValueIOEx::iodef (VName vname, float &v, float defval) {
  VNTValueIO::iodef(transformName(vname), v, defval);
}


//==========================================================================
//
//  VNTValueIOEx::iodef
//
//==========================================================================
void VNTValueIOEx::iodef (VName vname, TVec &v, const TVec defval) {
  VNTValueIO::iodef(transformName(vname), v, defval);
}


//==========================================================================
//
//  VNTValueIOEx::iodef
//
//==========================================================================
void VNTValueIOEx::iodef (VName vname, VName &v, VName defval) {
  VNTValueIO::iodef(transformName(vname), v, defval);
}


//==========================================================================
//
//  VNTValueIOEx::iodef
//
//==========================================================================
void VNTValueIOEx::iodef (VName vname, VStr &v, VStr defval) {
  VNTValueIO::iodef(transformName(vname), v, defval);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, vint32 &v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, vuint32 &v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, float &v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, TVec &v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VName &v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VStr &v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VClass *&v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VObject *&v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VSerialisable *&v) {
  VNTValueIO::io(transformName(vname), v);
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VThinker *&v) {
  VObject *o = (VObject *)v;
  io(transformName(vname), o);
  if (IsLoading()) v = (VThinker *)o;
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VEntity *&v) {
  VObject *o = (VObject *)v;
  io(transformName(vname), o);
  if (IsLoading()) v = (VEntity *)o;
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, VTextureID &v) {
  if (IsError()) {
    if (IsLoading()) v.id = 0;
    return;
  }

  if (IsLoading()) {
    // reading
    VName tname;
    int ttype;
    io(transformName(vname), tname);
    io(VName(va("%s.ttype", *transformName(vname))), ttype);
    if (IsError()) {
      if (IsLoading()) v.id = 0;
      return;
    }
    if (ttype == -666 && tname == NAME_None) {
      // new version
      auto blob = readBlob(VName(va("%s.blobv0", *transformName(vname))));
      if (!blob.isValid() || !blob.isBlob()) {
        GCon->Log(NAME_Warning, "LOAD: save file is probably broken (no texture blob)");
        v.id = 0;
        return;
      }
      //GCon->Logf(NAME_Debug, "LDBLOB: size=%d", blob.getBlobSize());
      //VStr s = "   "; for (int f = 0; f < blob.getBlobSize(); ++f) s += va(" %02x", blob.getBlobPtr()[f]); GCon->Logf(NAME_Debug, "%s", *s);
      VMemoryStreamRO rst;
      rst.Setup("::texture", blob.getBlobPtr(), blob.getBlobSize());
      v.Serialise(rst);
    } else if (tname == NAME_None) {
      //GCon->Log(NAME_Warning, "LOAD: save file is probably broken (empty texture name)");
      v.id = 0;
    } else {
      auto lock = GTextureManager.LockMapLocalTextures();
      /*
      int texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Wall, true, false);
      if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Flat, true, false);
      if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Sprite, true, false);
      */
      int texid = GTextureManager.CheckNumForName(tname, ttype, true/*overload*/);
      if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Wall, true, false);
      if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Flat, true, false);
      if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Sprite, true, false);
      if (texid < 0) texid = GTextureManager.CheckNumForName(tname, TEXTYPE_Pic, true/*overload*/);
      //if (texid < 0) texid = GTextureManager.CheckNumForNameAndForce(tname, TEXTYPE_Pic, true, false);
      if (texid < 0) {
        GCon->Logf(NAME_Warning, "LOAD: save file is broken (texture '%s' not found)", *tname);
        texid = GTextureManager.DefaultTexture;
      }
      v.id = texid;
      if (GTextureManager.getIgnoreAnim(v.id)->Type != ttype) {
        GCon->Logf(NAME_Warning, "TXRD<%s>: %5d <%s> (%d)", *transformName(vname), v.id, *tname, GTextureManager.getIgnoreAnim(v.id)->Type);
      }
    }
    //GCon->Logf("txrd: <%s> : %d", *vname, v.id);
  } else {
    //GCon->Logf("txwr: <%s> : %d", *vname, v.id);
    // writing (new version)
    VName tname = NAME_None;
    int ttype = -666;
    io(transformName(vname), tname);
    io(VName(va("%s.ttype", *transformName(vname))), ttype);

    VMemoryStream mst;
    mst.BeginWrite();
    v.Serialise(mst);
    TArrayNC<vuint8> &arr = mst.GetArray();
    //GCon->Logf(NAME_Debug, "WRBLOB: size=%d", arr.length());
    writeBlob(VName(va("%s.blobv0", *transformName(vname))), arr.ptr(), arr.length());
    //VStr s = "   "; for (int f = 0; f < arr.length(); ++f) s += va(" %02x", arr[f]); GCon->Logf(NAME_Debug, "%s", *s);

    /* (old version)
    if (v.id > 0) {
      if (!GTextureManager.getIgnoreAnim(v.id)) {
        GCon->Logf(NAME_Warning, "SAVE: trying to save inexisting texture with id #%d", v.id);
        tname = VName(" ! ");
      } else {
        tname = GTextureManager.GetTextureName(v.id);
        ttype = GTextureManager.getIgnoreAnim(v.id)->Type;
        //GCon->Logf("TXWR<%s>: %5d <%s> (%d)", *vname, v.id, *tname, GTextureManager.getIgnoreAnim(v.id)->Type);
      }
    }
    io(transformName(vname), tname);
    io(VName(va("%s.ttype", *transformName(vname))), ttype);
    */
  }
}


//==========================================================================
//
//  VNTValueIOEx::io
//
//==========================================================================
void VNTValueIOEx::io (VName vname, vuint8 &v) {
  vuint32 n = v;
  io(transformName(vname), n);
  if (IsLoading()) v = clampToByte(n);
}

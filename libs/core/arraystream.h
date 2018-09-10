//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

//
//  VArrayStream
//
//  Similar to VMemoryStream, but uses reference to an external array.
//
class VArrayStream : public VStream
{
protected:
  TArray<vuint8>& Array;
  int       Pos;

public:
  VArrayStream(TArray<vuint8>&);

  virtual void Serialise(void*, int) override;
  virtual void Seek(int) override;
  virtual int Tell() override;
  virtual int TotalSize() override;

  void BeginRead()
  {
    bLoading = true;
  }
  void BeginWrite()
  {
    bLoading = false;
  }
  TArray<vuint8>& GetArray()
  {
    return Array;
  }
};

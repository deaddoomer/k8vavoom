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
#include "vc_public.h"


// ////////////////////////////////////////////////////////////////////////// //
static TArray<VStr> SourceFiles;
static TMap<VStr, vint32> SourceFilesMap;


//==========================================================================
//
//  TLocation::AddSourceFile
//
//==========================================================================
int TLocation::AddSourceFile (VStr SName) noexcept {
  if (SName.isEmpty()) SName = "<unknown>";
  if (SourceFiles.length() == 0) {
    // add dummy source file
    SourceFiles.Append("<err>");
    SourceFilesMap.put("<err>", 0);
  }
  // find it
  auto val = SourceFilesMap.get(SName);
  if (val) return *val;
  // not found, add it
  int idx = SourceFiles.length();
  SourceFiles.Append(SName);
  SourceFilesMap.put(SName, idx);
  return idx;
}


//==========================================================================
//
//  TLocation::ClearSourceFiles
//
//==========================================================================
void TLocation::ClearSourceFiles () noexcept {
  SourceFiles.Clear();
  SourceFilesMap.clear();
}


//==========================================================================
//
//  TLocation::GetSourceFileByIndex
//
//==========================================================================
VStr TLocation::GetSourceFileByIndex (int sidx) noexcept {
  if (sidx <= 0) return "(external)"; // confusing, yeah?
  if (sidx >= SourceFiles.length()) return "<wutafuck>";
  return SourceFiles[sidx];
}


//==========================================================================
//
//  TLocation::GetSourceFile
//
//==========================================================================
VStr TLocation::GetSourceFile () const noexcept {
  if (isInternal()) return "(external)"; // confusing, yeah?
  if (GetSrcIndex() < 0 || GetSrcIndex() >= SourceFiles.length()) return "<wutafuck>";
  return SourceFiles[GetSrcIndex()];
}


//==========================================================================
//
//  TLocation::toString
//
//==========================================================================
VStr TLocation::toString () const noexcept {
  if (GetLine()) {
    if (GetCol() > 0) {
      return GetSourceFile()+":"+VStr(GetLine())+":"+VStr(GetCol());
    } else {
      return GetSourceFile()+":"+VStr(GetLine())+":1";
    }
  } else {
    return VStr("(nowhere)");
  }
}


//==========================================================================
//
//  TLocation::toStringNoCol
//
//==========================================================================
VStr TLocation::toStringNoCol () const noexcept {
  if (GetLine()) {
    return GetSourceFile()+":"+VStr(GetLine());
  } else {
    return VStr("(nowhere)");
  }
}


//==========================================================================
//
//  TLocation::toStringLineCol
//
//==========================================================================
VStr TLocation::toStringLineCol () const noexcept {
  if (GetLine()) {
    if (GetCol() > 0) {
      return VStr(GetLine())+":"+VStr(GetCol());
    } else {
      return VStr(GetLine())+":1";
    }
  } else {
    return VStr("(nowhere)");
  }
}


//==========================================================================
//
//  TLocation::toStringShort
//
//  only file name and line number
//
//==========================================================================
VStr TLocation::toStringShort () const noexcept {
  VStr s = GetSourceFile();
  int pos = s.length();
  while (pos > 0 && s[pos-1] != ':' && s[pos-1] != '/' && s[pos-1] != '\\') --pos;
  s = s.mid(pos, s.length());
  return s+":"+VStr(GetLine());
}


#ifdef VAVOOMC_SMALL_TLOCATION
TLocation::TLocation (const TLocationLine &aloc) noexcept
  : LineCol(0)
  , SrcIdx16(0)
{
  SetSrcIdx(aloc.GetSrcIndex());
  SetLine(aloc.GetLine());
}
void TLocation::operator = (const TLocationLine &other) noexcept {
  LineCol = 0;
  SetLine(other.GetLine());
  SetSrcIdx(other.GetSrcIndex());
}
#else
TLocation::TLocation (const TLocationLine &aloc) noexcept
  : Line(0)
  , Col(0)
  , SrcIdx(0)
{
  SetSrcIdx(aloc.GetSrcIndex());
  SetLine(aloc.GetLine());
}
void TLocation::operator = (const TLocationLine &other) noexcept {
  Col = 0;
  Line = other.GetLine();
  SrcIdx = other.GetSrcIndex();
}
#endif

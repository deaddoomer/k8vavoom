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
//**  Copyright (C) 2018-2021 Ketmar Dark
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

// ////////////////////////////////////////////////////////////////////////// //
// describes location in a source file
class TLocation {
private:
  int Line;
  int Col;
  int SrcIdx;

public:
  static int AddSourceFile (VStr SName) noexcept;
  static void ClearSourceFiles () noexcept;

  static VStr GetSourceFileByIndex (int sidx) noexcept;

public:
  inline TLocation () noexcept : Line(0), Col(0), SrcIdx(0) {}
  inline TLocation (int ASrcIdx, int ALine, int ACol) noexcept : Line(ALine > 0 ? ALine : 0), Col(ACol > 0 ? ACol : 0), SrcIdx(ASrcIdx > 0 ? ASrcIdx : 0) {}

  inline bool isEmpty () const noexcept { return ((SrcIdx|Line|Col) == 0); }

  inline bool isInternal () const noexcept { return (SrcIdx == 0); }

  inline int GetLine () const noexcept { return Line; }
  inline void SetLine (int ALine) noexcept { Line = ALine; }

  inline int GetCol () const noexcept { return Col; }

  inline int GetSrcIndex () const noexcept { return SrcIdx; }

  VStr GetSourceFile () const noexcept;

  VStr toString () const noexcept; // source file, line, column
  VStr toStringNoCol () const noexcept; // source file, line
  VStr toStringLineCol () const noexcept; // line, column
  VStr toStringShort () const noexcept; // source file w/o path, line

  inline void ConsumeChar (bool doNewline) noexcept {
    if (doNewline) { ++Line; Col = 1; } else ++Col;
  }
};

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

#define VAVOOMC_SMALL_TLOCATION

class TLocationLine;

// ////////////////////////////////////////////////////////////////////////// //
// describes location in a source file
class __attribute__((packed)) TLocation {
private:
  #ifdef VAVOOMC_SMALL_TLOCATION
  vuint32 LineCol;
  vuint16 SrcIdx16;
  #else
  int Line;
  int Col;
  int SrcIdx;
  #endif

public:
  static int AddSourceFile (VStr SName) noexcept;
  static void ClearSourceFiles () noexcept;

  static VStr GetSourceFileByIndex (int sidx) noexcept;

private:
  #ifdef VAVOOMC_SMALL_TLOCATION
  inline void SetSrcIdx (int v) noexcept { if (v < 0 || v > 65535) v = 0; SrcIdx16 = (vuint16)v; }

  inline void SetCol (int v) {
    if (v < 0) v = 0; else if (v > 0xfff) v = 0xfff;
    LineCol = (LineCol&0x000fffffu)|(((vuint32)v)<<20);
  }
  #endif

public:
  #ifdef VAVOOMC_SMALL_TLOCATION
  inline TLocation () noexcept : LineCol(0), SrcIdx16(0) {}
  inline TLocation (int ASrcIdx, int ALine, int ACol) noexcept : LineCol(0), SrcIdx16(0) { SetSrcIdx(ASrcIdx); SetLine(ALine); SetCol(ACol); }
  TLocation (const TLocationLine &aloc) noexcept;

  inline void operator = (const TLocation &other) noexcept { LineCol = other.LineCol; SrcIdx16 = other.SrcIdx16; }
  void operator = (const TLocationLine &other) noexcept;
  #else
  inline TLocation () noexcept : Line(0), Col(0), SrcIdx(0) {}
  inline TLocation (int ASrcIdx, int ALine, int ACol) noexcept : Line(ALine > 0 ? ALine : 0), Col(ACol > 0 ? ACol : 0), SrcIdx(ASrcIdx > 0 ? ASrcIdx : 0) {}
  inline TLocation (const TLocationLine &aloc) noexcept : Line(0), Col(0), SrcIdx(0) { SetSrcIdx(aloc.GetSrcIndex()); SetLine(aloc.GetLine()); }

  inline void operator = (const TLocation &other) noexcept { Line = other.Line; Col = other.Col; SrcIdx = other.SrcIdx; }
  #endif

  #ifdef VAVOOMC_SMALL_TLOCATION
  inline bool isEmpty () const noexcept { return ((SrcIdx16|LineCol) == 0); }
  inline bool isInternal () const noexcept { return (SrcIdx16 == 0); }

  inline int GetLine () const noexcept { return (LineCol&0xfffff); }
  inline void SetLine (int v) noexcept {
    if (v < 0) v = 0; else if (v > 0xfffff) v = 0xfffff;
    LineCol = (LineCol&0x100000u)|v;
  }

  inline int GetCol () const noexcept { return (LineCol>>20)&0xfff; }

  inline int GetSrcIndex () const noexcept { return SrcIdx16; }

  #else
  inline bool isEmpty () const noexcept { return ((SrcIdx|Line|Col) == 0); }
  inline bool isInternal () const noexcept { return (SrcIdx == 0); }

  inline int GetLine () const noexcept { return Line; }
  inline void SetLine (int v) noexcept { Line = (v > 0 ? v : 0); }

  inline int GetCol () const noexcept { return Col; }

  inline int GetSrcIndex () const noexcept { return SrcIdx; }
  #endif

  VStr GetSourceFile () const noexcept;

  VStr toString () const noexcept; // source file, line, column
  VStr toStringNoCol () const noexcept; // source file, line
  VStr toStringLineCol () const noexcept; // line, column
  VStr toStringShort () const noexcept; // source file w/o path, line

  inline void ConsumeChar (bool doNewline) noexcept {
    #ifdef VAVOOMC_SMALL_TLOCATION
    if (doNewline) { SetLine(GetLine()+1); SetCol(1); } else SetCol(GetCol()+1);
    #else
    if (doNewline) { ++Line; Col = 1; } else ++Col;
    #endif
  }
};


// ////////////////////////////////////////////////////////////////////////// //
// describes location in a source file
class __attribute__((packed)) TLocationLine {
private:
  #ifdef VAVOOMC_SMALL_TLOCATION
  vuint16 Line;
  vuint16 SrcIdx;
  #else
  int Line;
  int SrcIdx;
  #endif

private:
  #ifdef VAVOOMC_SMALL_TLOCATION
  inline void SetSrcIdx (int v) noexcept { if (v < 0 || v > 65535) v = 0; SrcIdx = (vuint16)v; }
  #else
  inline void SetSrcIdx (int v) noexcept { if (v < 0) v = 0; SrcIdx = v; }
  #endif

public:
  inline TLocationLine () noexcept : Line(0), SrcIdx(0) {}
  inline TLocationLine (int ASrcIdx, int ALine) noexcept : Line(0), SrcIdx(0) { SetSrcIdx(ASrcIdx); SetLine(ALine); }
  inline TLocationLine (const TLocation &aloc) noexcept : Line(0), SrcIdx(0) { SetSrcIdx(aloc.GetSrcIndex()); SetLine(aloc.GetLine()); }

  inline void operator = (const TLocationLine &other) noexcept { Line = other.Line; SrcIdx = other.SrcIdx; }
  inline void operator = (const TLocation &other) noexcept { SetLine(other.GetLine()); SetSrcIdx(other.GetSrcIndex()); }

  inline bool isEmpty () const noexcept { return ((SrcIdx|Line) == 0); }
  inline bool isInternal () const noexcept { return (SrcIdx == 0); }

  inline int GetLine () const noexcept { return Line; }
  inline void SetLine (int v) noexcept {
    if (v < 0) v = 0;
    #ifdef VAVOOMC_SMALL_TLOCATION
    else if (v > 0xfffff) v = 0xfffff;
    Line = (vuint16)v;
    #else
    Line = v;
    #endif
  }

  inline int GetSrcIndex () const noexcept { return SrcIdx; }

  VStr GetSourceFile () const noexcept { return TLocation(*this).GetSourceFile(); }

  VStr toString () const noexcept { return TLocation(*this).toStringNoCol(); }
  VStr toStringNoCol () const noexcept { return TLocation(*this).toStringNoCol(); }
  VStr toStringLineCol () const noexcept { return TLocation(*this).toStringNoCol(); }
  VStr toStringShort () const noexcept { return TLocation(*this).toStringShort(); }
};

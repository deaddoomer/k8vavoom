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
#if !defined(VCC_STANDALONE_EXECUTOR)
# include "../gamedefs.h"
#else
# include "../../libs/core/core.h"
# include "misc.h"
#endif


#if !defined(VCC_STANDALONE_EXECUTOR)
//==========================================================================
//
//  FOutputDevice implementation
//
//==========================================================================
FOutputDevice::~FOutputDevice () noexcept {}
void FOutputDevice:: Log (const char *S) noexcept { Serialise(S, NAME_Log); }
void FOutputDevice::Log (EName Type, const char *S) noexcept { Serialise(S, Type); }
void FOutputDevice::Log (VStr S) noexcept { Serialise(*S, NAME_Log); }
void FOutputDevice::Log (EName Type, VStr S) noexcept { Serialise(*S, Type); }


static char string[32768];

__attribute__((format(printf, 2, 3))) void FOutputDevice::Logf (const char *Fmt, ...) noexcept {
  va_list argptr;

  va_start(argptr, Fmt);
  vsnprintf(string, sizeof(string), Fmt, argptr);
  va_end(argptr);

  Serialise(string, NAME_Log);
}

__attribute__((format(printf, 3, 4))) void FOutputDevice::Logf (EName Type, const char *Fmt, ...) noexcept {
  va_list argptr;

  va_start(argptr, Fmt);
  vsnprintf(string, sizeof(string), Fmt, argptr);
  va_end(argptr);

  Serialise(string, Type);
}


//==========================================================================
//
//  superatoi
//
//==========================================================================
/*
int superatoi (const char *s) noexcept {
  int n = 0, r = 10, x, mul = 1;
  const char *c=s;

  for (; *c; ++c) {
    x = (*c&223)-16;
    if (x == -3) {
      mul = -mul;
    } else if (x == 72 && r == 10) {
      n -= (r = n);
      if (!r) r = 16;
      if (r < 2 || r > 36) return -1;
    } else {
      if (x > 10) x -= 39;
      if (x >= r) return -1;
      n = (n*r)+x;
    }
  }
  return mul*n;
}
*/


static TMapNC<VName, vuint32> colorNameMap; // names are lowercased
static bool colorNamesLoaded = false;


//==========================================================================
//
//  M_LookupColorName
//
//==========================================================================
vuint32 M_LookupColorName (const char *Name) {
  char tmpbuf[64];

  if (!Name || !Name[0]) return 0;

  if (!colorNamesLoaded) {
    colorNamesLoaded = true;

    // check that X111R6RGB lump exists
    int Lump = W_CheckNumForName(NAME_x11r6rgb);
    if (Lump < 0) {
      GCon->Logf(NAME_Warning, "X11R6RGB lump not found, so no color names for you. sorry.");
      return 0;
    }

    // read the lump
    VStream *Strm = W_CreateLumpReaderNum(Lump);
    int sz = Strm->TotalSize();
    char *Buf = new char[sz+2];
    if (sz) Strm->Serialise(Buf, sz);
    Buf[sz] = '\n';
    Buf[sz+1] = 0;
    if (Strm->IsError()) Buf[0] = 0;
    VStream::Destroy(Strm);

    // parse it
    vuint8 *pBuf = (vuint8 *)Buf;
    for (;;) {
      vuint8 ch = *pBuf++;
      if (ch == 0) break;
      if (ch <= ' ') continue;
      if (ch == '!') {
        // comment, skip line
        while (*pBuf != '\n') ++pBuf;
        continue;
      }
      // should have three decimal numbers
      --pBuf;
      vuint8 *start = pBuf;
      int cc[3];
      cc[0] = cc[1] = cc[2] = -1;
      for (int f = 0; f < 3; ++f) {
        while (*pBuf && *pBuf <= ' ' && *pBuf != '\n') ++pBuf;
        if (pBuf[0] == '\n') break;
        int n = -1;
        while (*pBuf > ' ') {
          int d = VStr::digitInBase(*pBuf, 10);
          if (d < 0) { n = -1; break; }
          if (n < 0) n = 0;
          n = n*10+d;
          //GCon->Logf("  n=%d; d=%d; char=%c", n, d, (char)*pBuf);
          if (n > 255) { n = -1; break; }
          ++pBuf;
        }
        //GCon->Logf("DONE: n=%d", n);
        if (n >= 0 && pBuf[0] == '\n') n = -1;
        if (n < 0) { cc[0] = cc[1] = cc[2] = -1; break; }
        cc[f] = n;
      }
      if (cc[0] < 0 || cc[1] < 0 || cc[2] < 0 || pBuf[0] == '\n') {
        // invalid, skip line
       error:
        while (*pBuf != '\n') ++pBuf;
        *pBuf = 0;
        GCon->Logf(NAME_Warning, "Invalid color definition: <%s>", (char *)start);
        *pBuf = '\n';
        continue;
      }
      //GCon->Logf("CC: [0]=%d; [1]=%d; [2]=%d", cc[0], cc[1], cc[2]);
      // get name
      while (*pBuf != '\n' && *pBuf <= ' ') ++pBuf;
      if (pBuf[0] == '\n') continue;
      // collect name
      size_t tbpos = 0;
      while (pBuf[0] != '\n') {
        ch = *pBuf++;
        if (ch <= ' ') {
          /*
          if (tbpos && tmpbuf[tbpos-1] != ' ') {
            if (tbpos >= sizeof(tmpbuf)-1) goto error;
            tmpbuf[tbpos++] = ' ';
          }
          */
        } else {
          if (tbpos >= sizeof(tmpbuf)-1) goto error;
          if (ch >= 'A' && ch <= 'Z') ch += 32; // poor man's tolower
          tmpbuf[tbpos++] = (char)ch;
        }
      }
      // remove trailing spaces
      while (tbpos > 0 && tmpbuf[tbpos-1] == ' ') --tbpos;
      if (tbpos > 0) {
        vuint32 clr = 0xff000000U|(((vuint32)cc[0])<<16)|(((vuint32)cc[1])<<8)|((vuint32)cc[2]);
        tmpbuf[tbpos] = 0;
        VName n = VName(tmpbuf);
        colorNameMap.put(n, clr);
        /*
        char *dstr = (char *)Z_Malloc(tbpos+1);
        strcpy(dstr, tmpbuf);
        colorNameMap.put(dstr, clr);
        if (!colorNameMap.get(dstr)) Sys_Error("!!! <%s>", dstr);
        *pBuf = 0;
        GCon->Logf("COLOR: %3d %3d %3d  <%s> %08x  <%s>", cc[0], cc[1], cc[2], dstr, clr, (char *)start);
        *pBuf = '\n';
        */
      }
    }
    GCon->Logf(NAME_Init, "loaded %d color names", colorNameMap.length());

    if (!colorNameMap.get(VName("ivory"))) Sys_Error("!!! IVORY");

    delete[] Buf;
  }

  // normalize color name
  size_t dpos = 0;
  bool hadSpaces = false;
  for (const vuint8 *s = (const vuint8 *)Name; *s; ++s) {
    vuint8 ch = *s;
    if (ch == '"' || ch == '\'') continue; // why not?
    if (ch <= ' ') {
      /*
      if (dpos > 0 && tmpbuf[dpos-1] != ' ') {
        if (dpos >= sizeof(tmpbuf)-1) { dpos = 0; break; }
        tmpbuf[dpos++] = ' ';
      }
      */
      hadSpaces = true;
    } else {
      if (dpos >= sizeof(tmpbuf)-1) { dpos = 0; break; }
      if (ch >= 'A' && ch <= 'Z') ch += 32; // poor man's tolower
      tmpbuf[dpos++] = (char)ch;
    }
  }
  if (dpos == 0) return 0;
  tmpbuf[dpos] = 0;

  if (tmpbuf[0] == '#') {
    //GCon->Logf("HTML COLOR <%s> (%u)", tmpbuf, (unsigned)dpos);
    // looks like an HTML-style colur
    if (dpos == 7) {
      vuint32 clr = 0;
      for (int f = 1; f < 7; ++f) {
        int d = VStr::digitInBase(tmpbuf[f], 16);
        if (d < 0) return 0;
        clr = (clr<<4)|(d&0x0f);
      }
      //GCon->Logf("HTML COLOR <%s>:<%s>=0x%08x", tmpbuf, Name, clr);
      return clr|0xff000000U;
    } else if (dpos == 4) {
      vuint32 clr = 0;
      for (int f = 1; f < 4; ++f) {
        int d = VStr::digitInBase(tmpbuf[f], 16);
        if (d < 0) return 0;
        clr = (clr<<4)|(d&0x0f);
        clr = (clr<<4)|(d&0x0f);
      }
      //GCon->Logf("HTML COLOR <%s>:<%s>=0x%08x", tmpbuf, Name, clr);
      return clr|0xff000000U;
    }
    return 0;
  } else if (dpos == 6 && !hadSpaces) {
    bool valid = true;
    vuint32 clr = 0;
    for (int f = 0; f < 6; ++f) {
      int d = VStr::digitInBase(tmpbuf[f], 16);
      if (d < 0) { valid = false; break; }
      clr = (clr<<4)|(d&0x0f);
    }
    //GCon->Logf("HTML COLOR <%s>:<%s>=0x%08x", tmpbuf, Name, clr);
    if (valid) return clr|0xff000000U;
  } else if (dpos == 3 && !hadSpaces) {
    bool valid = true;
    vuint32 clr = 0;
    for (int f = 1; f < 4; ++f) {
      int d = VStr::digitInBase(tmpbuf[f], 16);
      if (d < 0) { valid = false; break; }
      clr = (clr<<4)|(d&0x0f);
      clr = (clr<<4)|(d&0x0f);
    }
    //GCon->Logf("HTML COLOR <%s>:<%s>=0x%08x", tmpbuf, Name, clr);
    if (valid) return clr|0xff000000U;
  }

  VName cnx = VName(tmpbuf, VName::Find);
  if (cnx == NAME_None) return 0;

  auto cpp = colorNameMap.get(tmpbuf);
  /*
  if (cpp) {
    GCon->Logf("*** FOUND COLOR <%s> : <%s> : 0x%08x", Name, tmpbuf, *cpp);
  } else {
    GCon->Logf("*** NOT FOUND COLOR <%s> : <%s>", Name, tmpbuf);
  }
  */
  //if (cpp) GCon->Logf("*** FOUND COLOR <%s> : <%s> : 0x%08x", Name, tmpbuf, *cpp);
  return (cpp ? *cpp : 0);
}


//==========================================================================
//
//  tryHex
//
//==========================================================================
static int tryHexByte (const char *s) noexcept {
  if (!s || !s[0]) return -1;
  int res = 0;
  while (*s) {
    int d = VStr::digitInBase(*s, 16);
    if (d < 0) return -1;
    res = (res*16)+d;
    if (res > 255) return -1;
  }
  return res;
}


//==========================================================================
//
//  M_ParseColor
//
//==========================================================================
vuint32 M_ParseColor (const char *Name, bool retZeroIfInvalid, bool showError) {
  if (!Name || !Name[0]) return (retZeroIfInvalid ? 0U : 0xff000000U);
  vuint32 res = M_LookupColorName(Name);
  if (res) {
    //if (retZeroIfInvalid) GCon->Logf(NAME_Debug, "COLOR <%s>: PARSED to 0x%08x", Name, res);
    return res;
  }
  //if (retZeroIfInvalid) GCon->Logf(NAME_Debug, "COLOR <%s>: wtf?", Name);
  vuint8 Col[3];
  if (Name[0] == '#') {
    const size_t nlen = strlen(Name);
    // looks like an HTML-style color
    if (nlen == 7) {
      // #rrggbb format color
      for (int i = 0; i < 3; ++i) {
        char Val[3];
        Val[0] = Name[i*2+1];
        Val[1] = Name[i*2+2];
        Val[2] = 0;
        int v = tryHexByte(Val);
        if (v < 0) return (retZeroIfInvalid ? 0U : 0xff000000U);
        Col[i] = clampToByte(v);
      }
    } else if (nlen == 4) {
      // #rgb format color
      for (int i = 0; i < 3; ++i) {
        char Val[3];
        Val[0] = Name[i+1];
        Val[1] = Name[i+1];
        Val[2] = 0;
        int v = tryHexByte(Val);
        if (v < 0) return (retZeroIfInvalid ? 0U : 0xff000000U);
        Col[i] = clampToByte(v);
      }
    } else {
      // assume it's a bad color value, set it to black
      /*
      Col[0] = 0;
      Col[1] = 0;
      Col[2] = 0;
      */
      return (retZeroIfInvalid ? 0U : 0xff000000U);
    }
  } else {
    bool warnColor = false;
    int colDec[3] = {-1, -1, -1};
    // treat like space separated hex values
    const vuint8 *s = (const vuint8 *)Name;
    for (int i = 0; i < 3; ++i) {
      // skip whitespace and quotes
      while (*s && (*s <= ' ' || *s == '"' || *s == '\'')) ++s;
      if (!s[0] || VStr::digitInBase(s[0], 16) < 0) {
        if (showError) GCon->Logf(NAME_Warning, "Invalid color <%s> (0)", Name);
        return (retZeroIfInvalid ? 0U : 0xff000000U);
      }
      // parse hex
      int digCount = 0;
      int n = 0, decn = 0;
      while (*s) {
        if (s[0] <= ' ' || *s == '"' || *s == '\'') break;
        int d = VStr::digitInBase(s[0], 16);
        if (d < 0) {
          if (showError) GCon->Logf(NAME_Warning, "Invalid color <%s> (1)", Name);
          return (retZeroIfInvalid ? 0U : 0xff000000U);
        }
        n = n*16+d;
        if (n > 0xffffff) n = 0xffff;
        // collect decimals
        if (decn >= 0) {
          d = VStr::digitInBase(s[0], 10);
          if (d < 0) {
            decn = -1;
          } else {
            decn = decn*10+d;
            if (decn > 255) decn = -1;
          }
        }
        ++s;
        ++digCount;
      }
      if (n > 255) { warnColor = true; n = 255; }
      if (digCount >= 3) warnColor = true;
      if (digCount == 1) n = (n<<4)|n;
      Col[i] = n;
      colDec[i] = decn;
    }
    //GCon->Logf(NAME_Debug, "COLOR <%s>: dec(%d,%d,%d); hex(%02x,%02x,%02x)", Name, colDec[0], colDec[1], colDec[2], Col[0], Col[1], Col[2]);
    if (warnColor) {
      if (colDec[0] >= 0 && colDec[1] >= 0 && colDec[2] >= 0 &&
          colDec[0] <= 255 && colDec[1] <= 255 && colDec[2] <= 255)
      {
        if (showError) GCon->Logf(NAME_Warning, "Decimal color <%s> (0x00_%02x_%02x_%02x)", Name, colDec[0], colDec[1], colDec[2]);
        Col[0] = colDec[0];
        Col[1] = colDec[1];
        Col[2] = colDec[2];
      } else {
        if (showError) GCon->Logf(NAME_Warning, "Invalid color <%s> (2)", Name);
      }
    }
    //GCon->Logf("*** COLOR <%s> is 0x%08x", *Name, 0xff000000U|(((vuint32)Col[0])<<16)|(((vuint32)Col[1])<<8)|((vuint32)Col[2]));
  }
  return 0xff000000U|(((vuint32)Col[0])<<16)|(((vuint32)Col[1])<<8)|((vuint32)Col[2]);
}
#endif


//==========================================================================
//
//  M_SkillFromName
//
//==========================================================================
VVA_CHECKRESULT int M_SkillFromName (const char *skname) {
  if (!skname || !skname[0]) return -1;
       if (VStr::strEquCI(skname, "itytd") || VStr::strEquCI(skname, "baby")) return 1-1;
  else if (VStr::strEquCI(skname, "hntr") || VStr::strEquCI(skname, "ntr") || VStr::strEquCI(skname, "easy")) return 2-1;
  else if (VStr::strEquCI(skname, "hmp") || VStr::strEquCI(skname, "medium")) return 3-1;
  else if (VStr::strEquCI(skname, "uv") || VStr::strEquCI(skname, "hard")) return 4-4;
  else if (VStr::strEquCI(skname, "nightmare")) return 5-1;
  int skn = -1;
  if (!VStr::convertInt(skname, &skn)) return -1;
  if (skn == 0) return 0;
  if (skn > 0 && skn <= 16) return skn-1; // arbitrary limit
  return -1;
}


//==========================================================================
//
//  ConDumpMatrix
//
//==========================================================================
void ConDumpMatrix (const VMatrix4 &mat) {
  for (int y = 0; y < 4; ++y) {
    VStr s;
    for (int x = 0; x < 4; ++x) {
      s += va(" %+.8f", mat[y][x]);
    }
    GLog.Log(NAME_Debug, *s);
  }
}

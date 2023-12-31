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
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**  Copyright (C) 2023 Ketmar Dark
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
#if defined (VCCRUN_HAS_SDL) && defined(VCCRUN_HAS_OPENGL)

//#define GL_GLEXT_PROTOTYPES

#include "mod_sdlgl.h"
#include "../../filesys/fsys.h"

#include "../mod_socket.h"

#include <sys/time.h>

#ifdef VAVOOM_CUSTOM_SPECIAL_SDL
# include <SDL.h>
#else
# include <SDL2/SDL.h>
#endif
#include <GL/gl.h>
#include <GL/glext.h>
static SDL_Window *hw_window = nullptr;
static SDL_GLContext hw_glctx = nullptr;
static VOpenGLTexture *txHead = nullptr, *txTail = nullptr;
static TMap<VStr, VOpenGLTexture *> txLoaded;
//static TArray<VOpenGLTexture *> txLoadedUnnamed;

bool VGLVideo::doGLSwap = false;
bool VGLVideo::doRefresh = false;
int VGLVideo::quitSignal = 0; // 1: need to post; 2: posted
bool VGLVideo::hasNPOT = false;

extern VObject *mainObject;

static const float zNear = 1.0f;
static const float zFar = 42.0f;

static SDL_Joystick *joystick = nullptr;
static SDL_GameController *controller = nullptr;
//static SDL_Haptic *haptic;
static SDL_JoystickID jid = 0;
static int joynum = -1;
//static bool joystick_started = false;
static bool joystick_controller = false;
//static bool has_haptic;
static int joy_x[2] = {0,0}, joy_y[2] = {0,0};
static unsigned ctl_trigger = 0;

static bool sdlTimerInited = false;
static bool sdlVideoInited = false;
static bool sdlJoystickInited = false;


//static PFNGLBLENDEQUATIONPROC glBlendEquation;
typedef void (APIENTRY *glBlendEquationFn) (GLenum mode);
static glBlendEquationFn glBlendEquationFunc = nullptr;
/*
#else
# define glBlendEquationFunc glBlendEquation
#endif
*/

/*
#ifndef GL_CLAMP_TO_EDGE
# define GL_CLAMP_TO_EDGE  0x812F
#endif

#ifdef WIN32
# ifndef GL_FUNC_ADD
#  define GL_FUNC_ADD
# endif
#endif
*/

//#ifndef _WIN32
typedef void (APIENTRY *glMultiTexCoord2fARB_t) (GLenum, GLfloat, GLfloat);
static glMultiTexCoord2fARB_t p_glMultiTexCoord2fARB = nullptr;

typedef void (APIENTRY *glActiveTextureARB_t) (GLenum);
static glActiveTextureARB_t p_glActiveTextureARB = nullptr;
//#endif


// ////////////////////////////////////////////////////////////////////////// //
class PcfFont {
public:
  const vuint32 PCF_PROPERTIES = 1<<0;
  const vuint32 PCF_ACCELERATORS = 1<<1;
  const vuint32 PCF_METRICS = 1<<2;
  const vuint32 PCF_BITMAPS = 1<<3;
  const vuint32 PCF_INK_METRICS = 1<<4;
  const vuint32 PCF_BDF_ENCODINGS = 1<<5;
  const vuint32 PCF_SWIDTHS = 1<<6;
  const vuint32 PCF_GLYPH_NAMES = 1<<7;
  const vuint32 PCF_BDF_ACCELERATORS = 1<<8;

  const vuint32 PCF_DEFAULT_FORMAT = 0x00000000;
  const vuint32 PCF_INKBOUNDS = 0x00000200;
  const vuint32 PCF_ACCEL_W_INKBOUNDS = 0x00000100;
  const vuint32 PCF_COMPRESSED_METRICS = 0x00000100;

  const vuint32 PCF_GLYPH_PAD_MASK = 3<<0; // See the bitmap table for explanation
  const vuint32 PCF_BYTE_MASK = 1<<2; // If set then Most Sig Byte First
  const vuint32 PCF_BIT_MASK = 1<<3; // If set then Most Sig Bit First
  const vuint32 PCF_SCAN_UNIT_MASK = 3<<4; // See the bitmap table for explanation

  struct CharMetrics {
    int leftSidedBearing;
    int rightSideBearing;
    int width;
    int ascent;
    int descent;
    vuint32 attrs;

    inline CharMetrics () noexcept : leftSidedBearing(0), rightSideBearing(0), width(0), ascent(0), descent(0), attrs(0) {}

    inline void clear () noexcept { leftSidedBearing = rightSideBearing = width = ascent = descent = 0; attrs = 0; }

    inline bool operator == (const CharMetrics &ci) const noexcept {
      return
        leftSidedBearing == ci.leftSidedBearing &&
        rightSideBearing == ci.rightSideBearing &&
        width == ci.width &&
        ascent == ci.ascent &&
        descent == ci.descent &&
        attrs == ci.attrs;
    }

    inline bool operator != (const CharMetrics &ci) const noexcept { return !(operator==(ci)); }

    /*
    string toString () const {
      import std.format : format;
      return "leftbearing=%d; rightbearing=%d; width=%d; ascent=%d; descent=%d; attrs=%u".format(leftSidedBearing, rightSideBearing, width, ascent, descent, attrs);
    }
    */
  };

  struct TocEntry {
    vuint32 type; // See below, indicates which table
    vuint32 format; // See below, indicates how the data are formatted in the table
    vuint32 size; // In bytes
    vuint32 offset; // from start of file

    inline bool isTypeCorrect () const noexcept {
      if (type == 0) return false;
      if (type&((~1)<<8)) return false;
      bool was1 = false;
      for (unsigned shift = 0; shift < 9; ++shift) {
        if (type&(1<<shift)) {
          if (was1) return false;
          was1 = true;
        }
      }
      return true;
    }
  };

  /*
  struct Prop {
    VStr name;
    VStr sv; // empty: this is integer property
    vuint32 iv;

    inline bool isString () const { return (*sv != nullptr); }
    inline bool isInt () const { return (*sv == nullptr); }
  };
  */

  struct Glyph {
    //vuint32 index; // glyph index
    vuint32 codepoint;
    CharMetrics metrics;
    CharMetrics inkmetrics;
    vuint8 *bitmap; // may be greater than necessary; shared between all glyphs; don't destroy
    vuint32 bmpfmt;
    //VStr name; // may be absent

    inline Glyph () noexcept : codepoint(0), metrics(), inkmetrics(), bitmap(nullptr), bmpfmt(0) {}

    inline Glyph (const Glyph &g) noexcept : codepoint(0), metrics(), inkmetrics(), bitmap(nullptr), bmpfmt(0) {
      //index = g.index;
      codepoint = g.codepoint;
      metrics = g.metrics;
      inkmetrics = g.inkmetrics;
      bitmap = g.bitmap;
      bmpfmt = g.bmpfmt;
      //name = g.name;
    }

    inline void clear () noexcept {
      //index = 0;
      codepoint = 0;
      metrics.clear();
      inkmetrics.clear();
      bitmap = nullptr;
      bmpfmt = 0;
      //name.clear();
    }

    inline void operator = (const Glyph &g) noexcept {
      if (&g != this) {
        //index = g.index;
        codepoint = g.codepoint;
        metrics = g.metrics;
        inkmetrics = g.inkmetrics;
        bitmap = g.bitmap;
        bmpfmt = g.bmpfmt;
        //name = g.name;
      }
    }

    inline int bmpWidth () const noexcept { return (metrics.width > 0 ? metrics.width : 0); }
    inline int bmpHeight () const noexcept { return (metrics.ascent+metrics.descent > 0 ? metrics.ascent+metrics.descent : 0); }

    void dumpMetrics () const noexcept;
    void dumpBitmapFormat () const noexcept;

    bool getPixel (int x, int y) const noexcept;
  };

public:
  //Prop[VStr] props;
  TArray<Glyph> glyphs;
  //vuint32[dchar] glyphc2i; // codepoint -> glyph index
  /*dchar*/vuint32 defaultchar;
  vuint8* bitmaps;
  vuint32 bitmapsSize;

public:
  CharMetrics minbounds, maxbounds;
  CharMetrics inkminbounds, inkmaxbounds;
  bool noOverlap;
  bool constantMetrics;
  bool terminalFont;
  bool constantWidth;
  bool inkInside;
  bool inkMetrics;
  bool drawDirectionLTR;
  int padding;
  int fntAscent;
  int fntDescent;
  int maxOverlap;

private:
  void clearInternal () noexcept;

public:
  inline PcfFont () noexcept : glyphs(), bitmaps(nullptr), bitmapsSize(0) { clearInternal(); }

  inline ~PcfFont () noexcept { clear(); }

  inline bool isValid () const noexcept { return (glyphs.length() > 0); }

  inline int height () const noexcept { return fntAscent+fntDescent; }

  inline void clear () noexcept { clearInternal(); }

  VOpenGLTexture *createGlyphTexture (int gidx) noexcept;

  // return width
  /*
  bool drawGlyph (ref CharMetrics mtr, dchar dch, scope void delegate (in ref CharMetrics mtr, int x, int y) nothrow @trusted @nogc putPixel) nothrow @trusted @nogc {
    if (!valid) return false;
    auto gip = dch in glyphc2i;
    if (gip is null) {
      gip = defaultchar in glyphc2i;
      if (gip is null) return false;
    }
    if (*gip >= glyphs.length) return false;
    auto glp = glyphs.ptr+(*gip);
    if (putPixel !is null) {
      // draw it
      int y = -glp.metrics.ascent;
      version(all) {
        foreach (immutable dy; 0..glp.bmpheight) {
          foreach (immutable dx; 0..glp.bmpwidth) {
            if (glp.getPixel(dx, dy)) putPixel(glp.metrics, dx, y);
          }
          ++y;
        }
      } else {
        foreach (immutable dy; 0..glp.bmpheight) {
          foreach (immutable dx; glp.metrics.leftSidedBearing..glp.metrics.rightSideBearing+1) {
            if (glp.getPixel(dx-glp.metrics.leftSidedBearing, dy)) putPixel(glp.metrics, dx-glp.metrics.leftSidedBearing, y);
          }
          ++y;
        }
      }
    }
    mtr = glp.metrics;
    return true;
    //return glp.inkmetrics.leftSidedBearing+glp.inkmetrics.rightSideBearing;
    //return glp.metrics.rightSideBearing;
    //return glp.metrics.width;
  }
  */

  bool load (VStream &fl) noexcept;
};


//==========================================================================
//
//  PcfFont::Glyph::getPixel
//
//==========================================================================
bool PcfFont::Glyph::getPixel (int x, int y) const noexcept {
  int hgt = metrics.ascent+metrics.descent;
  if (x < 0 || y < 0 || x >= metrics.width || y >= hgt) return false;
  /* the byte order (format&4 => LSByte first)*/
  /* the bit order (format&8 => LSBit first) */
  /* how each row in each glyph's bitmap is padded (format&3) */
  /*  0=>bytes, 1=>shorts, 2=>ints */
  /* what the bits are stored in (bytes, shorts, ints) (format>>4)&3 */
  /*  0=>bytes, 1=>shorts, 2=>ints */
  const vuint32 bytesPerItem = 1<<(bmpfmt&3);
  const vuint32 bytesWidth = metrics.width/8+(metrics.width%8 ? 1 : 0);
  const vuint32 bytesPerLine = ((bytesWidth/bytesPerItem)+(bytesWidth%bytesPerItem ? 1 : 0))*bytesPerItem;
  const vuint32 ofs = y*bytesPerLine+x/(bytesPerItem*8)+(bmpfmt&4 ? (x/8%bytesPerItem) : 1-(x/8%bytesPerItem));
  //!!!if (ofs >= bitmap.length) return false;
  const vuint8 mask = (vuint8)(bmpfmt&8 ? 0x80>>(x%8) : 0x01<<(x%8));
  return ((bitmap[ofs]&mask) != 0);
}


//==========================================================================
//
//  PcfFont::Glyph::dumpMetrics
//
//==========================================================================
void PcfFont::Glyph::dumpMetrics () const noexcept {
  GLog.Logf(NAME_Debug, "left bearing : %d", metrics.leftSidedBearing);
  GLog.Logf(NAME_Debug, "right bearing: %d", metrics.rightSideBearing);
  GLog.Logf(NAME_Debug, "width        : %d", metrics.width);
  GLog.Logf(NAME_Debug, "ascent       : %d", metrics.ascent);
  GLog.Logf(NAME_Debug, "descent      : %d", metrics.descent);
  GLog.Logf(NAME_Debug, "attrs        : 0x%08x", metrics.attrs);
  if (inkmetrics != metrics) {
    GLog.Logf(NAME_Debug, "ink left bearing : %d", inkmetrics.leftSidedBearing);
    GLog.Logf(NAME_Debug, "ink right bearing: %d", inkmetrics.rightSideBearing);
    GLog.Logf(NAME_Debug, "ink width        : %d", inkmetrics.width);
    GLog.Logf(NAME_Debug, "ink ascent       : %d", inkmetrics.ascent);
    GLog.Logf(NAME_Debug, "ink descent      : %d", inkmetrics.descent);
    GLog.Logf(NAME_Debug, "ink attrs        : 0x%08x", inkmetrics.attrs);
  }
}


//==========================================================================
//
//  PcfFont::Glyph::dumpBitmapFormat
//
//==========================================================================
void PcfFont::Glyph::dumpBitmapFormat () const noexcept {
  const vuint32 bytesPerItem = 1<<(bmpfmt&3);
  const vuint32 bytesWidth = metrics.width/8+(metrics.width%8 ? 1 : 0);
  const vuint32 bytesPerLine = ((bytesWidth/bytesPerItem)+(bytesWidth%bytesPerItem ? 1 : 0))*bytesPerItem;
  GLog.Logf(NAME_Debug, "bitmap width (in pixels) : %u", bytesPerLine*8);
  GLog.Logf(NAME_Debug, "bitmap element size      : %d bytes", 1<<(bmpfmt&3));
  GLog.Logf(NAME_Debug, "byte order               : %cSByte first", (bmpfmt&4 ? 'M' : 'L'));
  GLog.Logf(NAME_Debug, "bit order                : %cSBit first", (bmpfmt&8 ? 'M' : 'L'));
}


//==========================================================================
//
//  PcfFont::clearInternal
//
//==========================================================================
void PcfFont::clearInternal () noexcept {
  glyphs.clear();
  defaultchar = 0;
  delete[] bitmaps;
  bitmaps = nullptr;
  bitmapsSize = 0;
  minbounds.clear();
  maxbounds.clear();
  inkminbounds.clear();
  inkmaxbounds.clear();
  noOverlap = false;
  constantMetrics = false;
  terminalFont = false;
  constantWidth = false;
  inkInside = false;
  inkMetrics = false;
  drawDirectionLTR = false;
  padding = 0;
  fntAscent = 0;
  fntDescent = 0;
  maxOverlap = 0;
}


//==========================================================================
//
//  PcfFont::createGlyphTexture
//
//==========================================================================
VOpenGLTexture *PcfFont::createGlyphTexture (int gidx) noexcept {
  Glyph &gl = glyphs[gidx];
  //!GLog.Logf(NAME_Debug, "glyph #%d (ch=%u); wdt=%d; hgt=%d", gidx, gl.codepoint, gl.bmpWidth(), gl.bmpHeight());
  if (gl.bmpWidth() < 1 || gl.bmpHeight() < 1) return nullptr;
  VOpenGLTexture *tex = VOpenGLTexture::CreateEmpty(NAME_None, gl.bmpWidth(), gl.bmpHeight());
  for (int dy = 0; dy < gl.bmpHeight(); ++dy) {
    //!fprintf(stderr, "  ");
    for (int dx = 0; dx < gl.bmpWidth(); ++dx) {
      bool pix = gl.getPixel(dx, dy);
      //!fprintf(stderr, "%c", (pix ? '#' : '.'));
      tex->img->setPixel(dx, dy, (pix ? VImage::RGBA(255, 255, 255, 255) : VImage::RGBA(0, 0, 0, 0)));
    }
    //!fprintf(stderr, "\n");
  }
  return tex;
}


#define resetFormat()  do { bebyte = false; bebit = false; } while (0)

#define setupFormat(atbl)  do { \
  curfmt = atbl.format; \
  bebyte = ((atbl.format&(1<<2)) != 0); \
  bebit = ((atbl.format&(1<<3)) == 0); \
} while (0)

#define readInt(atype,dest)  do { \
  dest = 0; \
  vuint8 tmpb; \
  if (bebyte) { \
    for (unsigned f = 0; f < sizeof(atype); ++f) { \
      fl.Serialise(&tmpb, 1); \
      dest <<= 8; \
      dest |= tmpb; \
    } \
  } else { \
    for (unsigned f = 0; f < sizeof(atype); ++f) { \
      fl.Serialise(&tmpb, 1); \
      dest |= ((vuint32)tmpb)<<(f*8); \
    } \
  } \
  \
  if (bebit) { \
    atype nv = 0; \
    for (unsigned sft = 0; sft < sizeof(atype)*8; ++sft) { \
      bool set = ((dest&(1U<<sft)) != 0); \
      if (set) nv |= 1U<<(sizeof(atype)-sft-1); \
    } \
    dest = nv; \
  } \
  if (fl.IsError()) return false; \
} while (0)

#define readMetrics(mt)  do { \
  if ((curfmt&~0xffU) == PCF_DEFAULT_FORMAT) { \
    readInt(vint16, mt.leftSidedBearing); \
    readInt(vint16, mt.rightSideBearing); \
    readInt(vint16, mt.width); \
    readInt(vint16, mt.ascent); \
    readInt(vint16, mt.descent); \
    readInt(vuint16, mt.attrs); \
  } else if ((curfmt&~0xffU) == PCF_COMPRESSED_METRICS) { \
    readInt(vuint8, mt.leftSidedBearing); mt.leftSidedBearing -= 0x80; \
    readInt(vuint8, mt.rightSideBearing); mt.rightSideBearing -= 0x80; \
    readInt(vuint8, mt.width); mt.width -= 0x80; \
    readInt(vuint8, mt.ascent); mt.ascent -= 0x80; \
    readInt(vuint8, mt.descent); mt.descent -= 0x80; \
    mt.attrs = 0; \
  } else { \
    return false; \
  } \
} while (0)

#define readBool(dest)  do { \
  vuint8 tmpb; \
  fl.Serialise(&tmpb, 1); \
  dest = (tmpb != 0); \
} while (0)


//==========================================================================
//
//  PcfFont::load
//
//==========================================================================
bool PcfFont::load (VStream &fl) noexcept {
  clear();

  vuint32 bmpfmt = 0;
  bool bebyte = false;
  bool bebit = false;
  vuint32 curfmt;

  char sign[4];
  auto fstart = fl.Tell();
  fl.Serialise(sign, 4);
  if (memcmp(sign, "\x01" "fcp", 4) != 0) return false;
  vuint32 tablecount;
  readInt(vuint32, tablecount);
  //version(pcf_debug) conwriteln("tables: ", tablecount);
  if (tablecount == 0 || tablecount > 128) return false;

  // load TOC
  TocEntry tables[128];
  memset(tables, 0, sizeof(tables));
  for (unsigned tidx = 0; tidx < tablecount; ++tidx) {
    TocEntry &tbl = tables[tidx];
    readInt(vuint32, tbl.type);
    readInt(vuint32, tbl.format);
    readInt(vuint32, tbl.size);
    readInt(vuint32, tbl.offset);
    if (!tbl.isTypeCorrect()) return false;
    //conwriteln("table #", tidx, " is '", tbl.typeName, "'");
  }

  // load properties
  /*
  for (unsigned tidx = 0; tidx < tablecount; ++tidx) {
    TocEntry &tbl = tables[tidx];
    if (tbl.type == PCF_PROPERTIES) {
      if (props !is null) throw new Exception("more than one property table in PCF");
      fl.seek(fstart+tbl.offset);
      auto fmt = fl.readNum!vuint32;
      assert(fmt == tbl.format);
      setupFormat(tbl);
      version(pcf_debug) conwriteln("property table format: ", fmt);
      auto count = readInt!vuint32;
      version(pcf_debug) conwriteln(count, " properties");
      if (count > int.max/16) throw new Exception("string table too big");
      static struct Prp { vuint32 nofs; vuint8 strflag; vuint32 value; }
      Prp[] fprops;
      foreach (; 0..count) {
        fprops.arrAppend(Prp());
        fprops[$-1].nofs = readInt!vuint32;
        fprops[$-1].strflag = readInt!vuint8;
        fprops[$-1].value = readInt!vuint32;
      }
      while ((fl.tell-fstart)&3) fl.readNum!vuint8;
      vuint32 stblsize = readInt!vuint32;
      if (stblsize > int.max/16) throw new Exception("string table too big");
      char[] stbl;
      scope(exit) delete stbl;
      stbl.length = stblsize;
      if (stbl.length) fl.rawReadExact(stbl);
      // build property table
      foreach (const ref prp; fprops) {
        vuint32 nend = prp.nofs;
        if (nend >= stbl.length || stbl.ptr[nend] == 0) throw new Exception("invalid property name offset");
        while (nend < stbl.length && stbl.ptr[nend]) ++nend;
        VStr pname = stbl[prp.nofs..nend].idup;
        if (prp.strflag) {
          nend = prp.value;
          if (nend >= stbl.length) throw new Exception("invalid property value offset");
          while (nend < stbl.length && stbl.ptr[nend]) ++nend;
          VStr pval = stbl[prp.value..nend].idup;
          version(pcf_debug) conwriteln("  STR property '", pname, "' is '", pval, "'");
          props[pname] = Prop(pname, pval, 0);
        } else {
          version(pcf_debug) conwriteln("  INT property '", pname, "' is ", prp.value);
          props[pname] = Prop(pname, null, prp.value);
        }
      }
    }
  }
  //if (props is null) throw new Exception("no property table in PCF");
  */

  //vuint32[] gboffsets; // glyph bitmap offsets
  //scope(exit) delete gboffsets;
  TArray<vuint32> gboffsets;

  // load bitmap table
  for (unsigned tidx = 0; tidx < tablecount; ++tidx) {
    TocEntry &tbl = tables[tidx];
    if (tbl.type == PCF_BITMAPS) {
      if (bitmaps) return false;
      fl.Seek(fstart+tbl.offset);
      resetFormat();
      vuint32 fmt;
      readInt(vuint32, fmt);
      if (fmt != tbl.format) return false;
      setupFormat(tbl);
      bmpfmt = tbl.format;
      vuint32 count;
      readInt(vuint32, count);
      if (count == 0) return false;
      if (count > 0x00ffffff) return false;
      gboffsets.setLength((int)count);
      for (int oidx = 0; oidx < (int)count; ++oidx) {
        vuint32 v;
        readInt(vuint32, v);
        gboffsets[oidx] = v;
      }
      vuint32 sizes[4];
      readInt(vuint32, sizes[0]);
      readInt(vuint32, sizes[1]);
      readInt(vuint32, sizes[2]);
      readInt(vuint32, sizes[3]);
      vuint32 realsize = sizes[bmpfmt&3];
      if (realsize == 0) return 0;
      if (realsize > 0x00ffffff) return 0;
      bitmaps = new vuint8[realsize];
      bitmapsSize = realsize;
      fl.Serialise(bitmaps, realsize);
      if (fl.IsError()) return false;
      //bitmaps.length = realsize;
      //bitmaps.gcMarkHeadAnchor;
      //fl.rawReadExact(bitmaps);
      //version(pcf_debug) conwriteln(realsize, " bytes of bitmap data for ", count, " glyphs");
      /*!!!
      foreach (immutable idx, immutable v; gboffsets) {
        if (v >= bitmaps.length) { import std.format : format; throw new Exception("invalid bitmap data offset (0x%08x) for glyph #%d in PCF".format(v, idx)); }
      }
      */
    }
  }
  if (!bitmaps) return false;

  // load encoding table, fill glyph table
  glyphs.setLength(gboffsets.length());
  //immutable int bytesPerItem = 1<<(bmpfmt&3);
  for (int idx = 0; idx < glyphs.length(); ++idx) {
    Glyph &gl = glyphs[idx];
    //gl.index = (vuint32)idx;
    //uint realofs = gboffsets[idx]*bytesPerItem;
    //if (realofs >= bitmaps.length) throw new Exception("invalid glyph bitmap offset in PCF");
    vuint32 realofs = gboffsets[idx];
    gl.bitmap = bitmaps+realofs;
    gl.bmpfmt = bmpfmt;
  }

  bool encodingsFound = false;
  for (unsigned tidx = 0; tidx < tablecount; ++tidx) {
    TocEntry &tbl = tables[tidx];
    if (tbl.type == PCF_BDF_ENCODINGS) {
      if (encodingsFound) return false;
      encodingsFound = true;
      fl.Seek(fstart+tbl.offset);
      resetFormat();
      vuint32 fmt;
      readInt(vuint32, fmt);
      if (fmt != tbl.format) return false;
      setupFormat(tbl);
      vuint16 mincb2; readInt(vuint16, mincb2);
      vuint16 maxcb2; readInt(vuint16, maxcb2);
      vuint16 minb1; readInt(vuint16, minb1);
      vuint16 maxb1; readInt(vuint16, maxb1);
      readInt(vuint16, defaultchar);
      if (minb1 == 0 && minb1 == maxb1) {
        // single-byte encoding
        for (vuint32 ci = mincb2; ci <= maxcb2; ++ci) {
          //immutable dchar dch = cast(dchar)ci;
          vuint32 gidx; readInt(vuint16, gidx);
          if (gidx != 0xffff) {
            if (gidx >= (vuint32)gboffsets.length()) return false;
            //glyphc2i[dch] = gidx;
            glyphs[gidx].codepoint = ci;
          }
        }
      } else {
        // multi-byte encoding
        for (vuint32 hibyte = minb1; hibyte <= maxb1; ++hibyte) {
          for (vuint32 lobyte = mincb2; lobyte <= maxcb2; ++lobyte) {
            //immutable dchar dch = cast(dchar)((hibyte<<8)|lobyte);
            vuint32 gidx; readInt(vuint16, gidx);
            if (gidx != 0xffff) {
              if (gidx >= (vuint32)gboffsets.length()) return false;
              //glyphc2i[dch] = gidx;
              glyphs[gidx].codepoint = ((hibyte<<8)|lobyte);
            }
          }
        }
      }
      /*
      version(pcf_debug) {{
        import std.algorithm : sort;
        conwriteln(glyphc2i.length, " glyphs in font");
        //foreach (dchar dch; glyphc2i.values.sort) conwritefln!"  \\u%04X"(cast(uint)dch);
      }}
      */
    }
  }
  if (!encodingsFound) return false;

  // load metrics
  bool metricsFound = false;
  for (unsigned tidx = 0; tidx < tablecount; ++tidx) {
    TocEntry &tbl = tables[tidx];
    if (tbl.type == PCF_METRICS) {
      if (metricsFound) return false;
      metricsFound = true;
      fl.Seek(fstart+tbl.offset);
      resetFormat();
      vuint32 fmt;
      readInt(vuint32, fmt);
      if (fmt != tbl.format) return false;
      setupFormat(tbl);
      vuint32 count;
      if ((curfmt&~0xffU) == PCF_DEFAULT_FORMAT) {
        //conwriteln("metrics aren't compressed");
        readInt(vuint32, count);
      } else if ((curfmt&~0xffU) == PCF_COMPRESSED_METRICS) {
        //conwriteln("metrics are compressed");
        readInt(vuint16, count);
      } else {
        return false;
      }
      if (count < (vuint32)glyphs.length()) return false;
      for (int gidx = 0; gidx < glyphs.length(); ++gidx) {
        Glyph &gl = glyphs[gidx];
        readMetrics(gl.metrics);
        gl.inkmetrics = gl.metrics;
      }
    }
  }
  if (!metricsFound) return false;

  // load ink metrics (if any)
  metricsFound = false;
  for (unsigned tidx = 0; tidx < tablecount; ++tidx) {
    TocEntry &tbl = tables[tidx];
    if (tbl.type == PCF_INK_METRICS) {
      if (metricsFound) return false;
      metricsFound = true;
      fl.Seek(fstart+tbl.offset);
      resetFormat();
      vuint32 fmt;
      readInt(vuint32, fmt);
      if (fmt != tbl.format) return false;
      setupFormat(tbl);
      vuint32 count;
      if ((curfmt&~0xffU) == PCF_DEFAULT_FORMAT) {
        //conwriteln("ink metrics aren't compressed");
        readInt(vuint32, count);
      } else if ((curfmt&~0xffU) == PCF_COMPRESSED_METRICS) {
        //conwriteln("ink metrics are compressed");
        readInt(vuint16, count);
      } else {
        return false;
      }
      if (count > (vuint32)glyphs.length()) count = (vuint32)glyphs.length();
      for (int gidx = 0; gidx < glyphs.length(); ++gidx) {
        Glyph &gl = glyphs[gidx];
        readMetrics(gl.inkmetrics);
      }
    }
  }
  //version(pcf_debug) conwriteln("ink metrics found: ", metricsFound);

  // load glyph names (if any)
  /*
  for (unsigned tidx = 0; tidx < tablecount; ++tidx) {
    TocEntry &tbl = tables[tidx];
    if (tbl.type == PCF_GLYPH_NAMES) {
      fl.seek(fstart+tbl.offset);
      auto fmt = fl.readNum!vuint32;
      assert(fmt == tbl.format);
      setupFormat(tbl);
      vuint32 count = readInt!vuint32;
      if (count == 0) break;
      if (count > int.max/16) break;
      auto nofs = new vuint32[](count);
      scope(exit) delete nofs;
      foreach (ref v; nofs) v = readInt!vuint32;
      vuint32 stblsize = readInt!vuint32;
      if (stblsize > int.max/16) break;
      auto stbl = new char[](stblsize);
      scope(exit) delete stbl;
      fl.rawReadExact(stbl);
      if (count > glyphs.length) count = cast(vuint32)glyphs.length;
      foreach (ref gl; glyphs[0..count]) {
        vuint32 stofs = nofs[gl.index];
        if (stofs >= stbl.length) break;
        vuint32 eofs = stofs;
        while (eofs < stbl.length && stbl.ptr[eofs]) ++eofs;
        if (eofs > stofs) gl.name = stbl[stofs..eofs].idup;
      }
    }
  }
  */

  /* not needed
  //int mFontHeight = 0;
  mFontMinWidth = int.max;
  mFontMaxWidth = 0;
  foreach (const ref gl; glyphs) {
    if (mFontMinWidth > gl.inkmetrics.width) mFontMinWidth = gl.inkmetrics.width;
    if (mFontMaxWidth < gl.inkmetrics.width) mFontMaxWidth = gl.inkmetrics.width;
    //int hgt = gl.inkmetrics.ascent+gl.inkmetrics.descent;
    //if (mFontHeight < hgt) mFontHeight = hgt;
  }
  if (mFontMinWidth < 0) mFontMinWidth = 0;
  if (mFontMaxWidth < 0) mFontMaxWidth = 0;
  */

  // load accelerators
  int accelTbl = -1;
  for (unsigned tidx = 0; tidx < tablecount; ++tidx) {
    TocEntry &tbl = tables[tidx];
    if (accelTbl < 0 && (tbl.type == PCF_ACCELERATORS || tbl.type == PCF_BDF_ACCELERATORS)) {
      accelTbl = (int)tidx;
      if (tbl.type == PCF_BDF_ACCELERATORS) break;
    }
  }

  if (accelTbl >= 0) {
    CharMetrics minbounds, maxbounds;
    CharMetrics inkminbounds, inkmaxbounds;
    TocEntry &tbl = tables[accelTbl];
    fl.Seek(fstart+tbl.offset);
    resetFormat();
    vuint32 fmt;
    readInt(vuint32, fmt);
    if (fmt != tbl.format) return false;
    setupFormat(tbl);
    // load font parameters
    readBool(noOverlap);
    readBool(constantMetrics);
    readBool(terminalFont);
    readBool(constantWidth);
    readBool(inkInside);
    readBool(inkMetrics);
    readBool(drawDirectionLTR);
    readInt(vuint8, padding);
    readInt(vint32, fntAscent);
    readInt(vint32, fntDescent);
    readInt(vint32, maxOverlap);
    readInt(vint16, minbounds.leftSidedBearing);
    readInt(vint16, minbounds.rightSideBearing);
    readInt(vint16, minbounds.width);
    readInt(vint16, minbounds.ascent);
    readInt(vint16, minbounds.descent);
    readInt(vuint16, minbounds.attrs);
    readInt(vint16, maxbounds.leftSidedBearing);
    readInt(vint16, maxbounds.rightSideBearing);
    readInt(vint16, maxbounds.width);
    readInt(vint16, maxbounds.ascent);
    readInt(vint16, maxbounds.descent);
    readInt(vuint16, maxbounds.attrs);
    if ((curfmt&~0xff) == PCF_ACCEL_W_INKBOUNDS) {
      readInt(vint16, inkminbounds.leftSidedBearing);
      readInt(vint16, inkminbounds.rightSideBearing);
      readInt(vint16, inkminbounds.width);
      readInt(vint16, inkminbounds.ascent);
      readInt(vint16, inkminbounds.descent);
      readInt(vuint16, inkminbounds.attrs);
      readInt(vint16, inkmaxbounds.leftSidedBearing);
      readInt(vint16, inkmaxbounds.rightSideBearing);
      readInt(vint16, inkmaxbounds.width);
      readInt(vint16, inkmaxbounds.ascent);
      readInt(vint16, inkmaxbounds.descent);
      readInt(vuint16, inkmaxbounds.attrs);
    } else {
      inkminbounds = minbounds;
      inkmaxbounds = maxbounds;
    }
    /*
    version(pcf_debug) {
      conwriteln("noOverlap       : ", noOverlap);
      conwriteln("constantMetrics : ", constantMetrics);
      conwriteln("terminalFont    : ", terminalFont);
      conwriteln("constantWidth   : ", constantWidth);
      conwriteln("inkInside       : ", inkInside);
      conwriteln("inkMetrics      : ", inkMetrics);
      conwriteln("drawDirectionLTR: ", drawDirectionLTR);
      conwriteln("padding         : ", padding);
      conwriteln("fntAscent       : ", fntAscent);
      conwriteln("fntDescent      : ", fntDescent);
      conwriteln("maxOverlap      : ", maxOverlap);
      conwriteln("minbounds       : ", minbounds);
      conwriteln("maxbounds       : ", maxbounds);
      conwriteln("ink_minbounds   : ", inkminbounds);
      conwriteln("ink_maxbounds   : ", inkmaxbounds);
    }
    */
  } else {
    //throw new Exception("no accelerator info found in PCF");
    return false;
  }

  /*
  version(pcf_debug) {
    //conwriteln("min width: ", mFontMinWidth);
    //conwriteln("max width: ", mFontMaxWidth);
    //conwriteln("height   : ", mFontHeight);
    version(pcf_debug_dump_bitmaps) {
      foreach (const ref gl; glyphs) {
        conwritefln!"=== glyph #%u (\\u%04X) %dx%d==="(gl.index, cast(vuint32)gl.codepoint, gl.bmpwidth, gl.bmpheight);
        if (gl.name.length) conwriteln("NAME: ", gl.name);
        conwritefln!"bitmap offset: 0x%08x"(gboffsets[gl.index]);
        gl.dumpMetrics();
        gl.dumpBitmapFormat();
        foreach (immutable int y; 0..gl.bmpheight) {
          conwrite("  ");
          foreach (immutable int x; 0..gl.bmpwidth) {
            if (gl.getPixel(x, y)) conwrite("##"); else conwrite("..");
          }
          conwriteln;
        }
      }
    }
  }
  */
  return true;
}

#undef readBool
#undef readMetrics
#undef readInt
#undef setupFormat

// ////////////////////////////////////////////////////////////////////////// //
struct ScissorRect {
  int x, y, w, h;
  int enabled;
};


// ////////////////////////////////////////////////////////////////////////// //
//native static final int CreateTimer (int intervalms, optional bool oneShot);
IMPLEMENT_FREE_FUNCTION(VObject, CreateTimer) {
  int intervalms;
  VOptParamBool oneShot(false);
  vobjGetParam(intervalms, oneShot);
  RET_INT(VGLVideo::CreateTimerWithId(0, intervalms, oneShot));
}

//native static final bool CreateTimerWithId (int id, int intervalms, optional bool oneShot);
IMPLEMENT_FREE_FUNCTION(VObject, CreateTimerWithId) {
  int id, intervalms;
  VOptParamBool oneShot(false);
  vobjGetParam(id, intervalms, oneShot);
  RET_BOOL(VGLVideo::CreateTimerWithId(id, intervalms, oneShot) > 0);
}

//native static final bool DeleteTimer (int id);
IMPLEMENT_FREE_FUNCTION(VObject, DeleteTimer) {
  int id;
  vobjGetParam(id);
  RET_BOOL(VGLVideo::DeleteTimer(id));
}

//native static final bool IsTimerExists (int id);
IMPLEMENT_FREE_FUNCTION(VObject, IsTimerExists) {
  int id;
  vobjGetParam(id);
  RET_BOOL(VGLVideo::IsTimerExists(id));
}

//native static final bool IsTimerOneShot (int id);
IMPLEMENT_FREE_FUNCTION(VObject, IsTimerOneShot) {
  int id;
  vobjGetParam(id);
  RET_BOOL(VGLVideo::IsTimerOneShot(id));
}

//native static final int GetTimerInterval (int id);
IMPLEMENT_FREE_FUNCTION(VObject, GetTimerInterval) {
  int id;
  vobjGetParam(id);
  RET_INT(VGLVideo::GetTimerInterval(id));
}

//native static final bool SetTimerInterval (int id, int intervalms);
IMPLEMENT_FREE_FUNCTION(VObject, SetTimerInterval) {
  int id, intervalms;
  vobjGetParam(id, intervalms);
  RET_BOOL(VGLVideo::SetTimerInterval(id, intervalms));
}


//native static final float GetTickCount ();
IMPLEMENT_FREE_FUNCTION(VObject, GetTickCount) {
  //RET_FLOAT((float)fsysCurrTick());
  RET_FLOAT((float)(Sys_Time()+2.0-Sys_Time_Offset()));
}


static vuint32 curmodflags = 0;


//==========================================================================
//
//  sdl2TranslateKey
//
//==========================================================================
static int sdl2TranslateKey (SDL_Scancode scan) noexcept {
  if (scan >= SDL_SCANCODE_A && scan <= SDL_SCANCODE_Z) return (int)(scan-SDL_SCANCODE_A+'a');
  if (scan >= SDL_SCANCODE_1 && scan <= SDL_SCANCODE_9) return (int)(scan-SDL_SCANCODE_1+'1');

  switch (scan) {
    case SDL_SCANCODE_0: return '0';
    case SDL_SCANCODE_SPACE: return ' ';
    case SDL_SCANCODE_MINUS: return '-';
    case SDL_SCANCODE_EQUALS: return '=';
    case SDL_SCANCODE_LEFTBRACKET: return '[';
    case SDL_SCANCODE_RIGHTBRACKET: return ']';
    case SDL_SCANCODE_BACKSLASH: return '\\';
    case SDL_SCANCODE_SEMICOLON: return ';';
    case SDL_SCANCODE_APOSTROPHE: return '\'';
    case SDL_SCANCODE_COMMA: return ',';
    case SDL_SCANCODE_PERIOD: return '.';
    case SDL_SCANCODE_SLASH: return '/';

    case SDL_SCANCODE_UP: return K_UPARROW;
    case SDL_SCANCODE_LEFT: return K_LEFTARROW;
    case SDL_SCANCODE_RIGHT: return K_RIGHTARROW;
    case SDL_SCANCODE_DOWN: return K_DOWNARROW;
    case SDL_SCANCODE_INSERT: return K_INSERT;
    case SDL_SCANCODE_DELETE: return K_DELETE;
    case SDL_SCANCODE_HOME: return K_HOME;
    case SDL_SCANCODE_END: return K_END;
    case SDL_SCANCODE_PAGEUP: return K_PAGEUP;
    case SDL_SCANCODE_PAGEDOWN: return K_PAGEDOWN;

    case SDL_SCANCODE_KP_0: return K_PAD0;
    case SDL_SCANCODE_KP_1: return K_PAD1;
    case SDL_SCANCODE_KP_2: return K_PAD2;
    case SDL_SCANCODE_KP_3: return K_PAD3;
    case SDL_SCANCODE_KP_4: return K_PAD4;
    case SDL_SCANCODE_KP_5: return K_PAD5;
    case SDL_SCANCODE_KP_6: return K_PAD6;
    case SDL_SCANCODE_KP_7: return K_PAD7;
    case SDL_SCANCODE_KP_8: return K_PAD8;
    case SDL_SCANCODE_KP_9: return K_PAD9;

    case SDL_SCANCODE_NUMLOCKCLEAR: return K_NUMLOCK;
    case SDL_SCANCODE_KP_DIVIDE: return K_PADDIVIDE;
    case SDL_SCANCODE_KP_MULTIPLY: return K_PADMULTIPLE;
    case SDL_SCANCODE_KP_MINUS: return K_PADMINUS;
    case SDL_SCANCODE_KP_PLUS: return K_PADPLUS;
    case SDL_SCANCODE_KP_ENTER: return K_PADENTER;
    case SDL_SCANCODE_KP_PERIOD: return K_PADDOT;

    case SDL_SCANCODE_ESCAPE: return K_ESCAPE;
    case SDL_SCANCODE_RETURN: return K_ENTER;
    case SDL_SCANCODE_TAB: return K_TAB;
    case SDL_SCANCODE_BACKSPACE: return K_BACKSPACE;

    case SDL_SCANCODE_GRAVE: return K_BACKQUOTE;
    case SDL_SCANCODE_CAPSLOCK: return K_CAPSLOCK;

    case SDL_SCANCODE_F1: return K_F1;
    case SDL_SCANCODE_F2: return K_F2;
    case SDL_SCANCODE_F3: return K_F3;
    case SDL_SCANCODE_F4: return K_F4;
    case SDL_SCANCODE_F5: return K_F5;
    case SDL_SCANCODE_F6: return K_F6;
    case SDL_SCANCODE_F7: return K_F7;
    case SDL_SCANCODE_F8: return K_F8;
    case SDL_SCANCODE_F9: return K_F9;
    case SDL_SCANCODE_F10: return K_F10;
    case SDL_SCANCODE_F11: return K_F11;
    case SDL_SCANCODE_F12: return K_F12;

    case SDL_SCANCODE_LSHIFT: return K_LSHIFT;
    case SDL_SCANCODE_RSHIFT: return K_RSHIFT;
    case SDL_SCANCODE_LCTRL: return K_LCTRL;
    case SDL_SCANCODE_RCTRL: return K_RCTRL;
    case SDL_SCANCODE_LALT: return K_LALT;
    case SDL_SCANCODE_RALT: return K_RALT;

    case SDL_SCANCODE_LGUI: return K_LWIN;
    case SDL_SCANCODE_RGUI: return K_RWIN;
    case SDL_SCANCODE_MENU: return K_MENU;

    case SDL_SCANCODE_PRINTSCREEN: return K_PRINTSCRN;
    case SDL_SCANCODE_SCROLLLOCK: return K_SCROLLLOCK;
    case SDL_SCANCODE_PAUSE: return K_PAUSE;

    default:
      //if (scan >= ' ' && scan < 127) return (vuint8)scan;
      break;
  }

  return 0;
}


//==========================================================================
//
//  texUpload
//
//==========================================================================
static bool texUpload (VOpenGLTexture *tx) {
  if (!tx) return false;
  if (!tx->img) { tx->tid = 0; return false; }
  if (tx->tid) return true;

  tx->tid = 0;
  glGenTextures(1, &tx->tid);
  if (tx->tid == 0) return false;

  //GLog.Logf(NAME_Debug, "uploading texture '%s'", *tx->getPath());

  glBindTexture(GL_TEXTURE_2D, tx->tid);
  /*
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  */
  VGLVideo::forceGLTexFilter();

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tx->img->width, tx->img->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr); // this creates texture

  tx->convertToTrueColor();

  //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tc->width, tc->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tc->pixels);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0/*x*/, 0/*y*/, tx->img->width, tx->img->height, GL_RGBA, GL_UNSIGNED_BYTE, tx->img->pixels); // this updates the texture

  return true;
}


//==========================================================================
//
//  unloadAllTextures
//
//==========================================================================
static void unloadAllTextures () {
  if (!hw_glctx) return;
  glBindTexture(GL_TEXTURE_2D, 0);
  for (VOpenGLTexture *tx = txHead; tx; tx = tx->next) {
    if (tx->tid) { glDeleteTextures(1, &tx->tid); tx->tid = 0; }
  }
}


//==========================================================================
//
//  uploadAllTextures
//
//==========================================================================
static void uploadAllTextures () {
  if (!hw_glctx) return;
  for (VOpenGLTexture *tx = txHead; tx; tx = tx->next) texUpload(tx);
}



//**************************************************************************
//
// VOpenGLTexture
//
//**************************************************************************

//==========================================================================
//
//  VOpenGLTexture::VOpenGLTexture
//
//==========================================================================
VOpenGLTexture::VOpenGLTexture (VImage *aimg, VStr apath, bool doSmoothing)
  : rc(1)
  , mPath(apath)
  , img(aimg)
  , tid(0)
  , type(TT_Auto)
  , realType(TT_Auto)
  , needSmoothing(doSmoothing)
  , prev(nullptr)
  , next(nullptr)
{
  analyzeImage();
  if (hw_glctx) texUpload(this);
  registerMe();
}


//==========================================================================
//
//  VOpenGLTexture::VOpenGLTexture
//
//  dimensions must be valid!
//
//==========================================================================
VOpenGLTexture::VOpenGLTexture (int awdt, int ahgt, bool doSmoothing)
  : rc(1)
  , mPath()
  , img(nullptr)
  , tid(0)
  , type(TT_Auto)
  , realType(TT_Auto)
  , needSmoothing(doSmoothing)
  , prev(nullptr)
  , next(nullptr)
{
  img = new VImage(VImage::IT_RGBA, awdt, ahgt);
  for (int y = 0; y < ahgt; ++y) {
    for (int x = 0; x < awdt; ++x) {
      img->setPixel(x, y, VImage::RGBA(0, 0, 0, 0)); // transparent
    }
  }
  analyzeImage();
  if (hw_glctx) texUpload(this);
  registerMe();
}


//==========================================================================
//
//  VOpenGLTexture::~VOpenGLTexture
//
//==========================================================================
VOpenGLTexture::~VOpenGLTexture () {
  if (hw_glctx && tid) {
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &tid);
  }
  tid = 0;
  delete img;
  img = nullptr;
  if (mPath.length()) {
    txLoaded.del(mPath);
  } else {
    /*
    for (int idx = 0; idx < txLoadedUnnamed.length(); ++idx) {
      if (txLoadedUnnamed[idx] == this) {
        txLoadedUnnamed.removeAt(idx);
        break;
      }
    }
    */
  }
  mPath = VStr();
  if (!prev && !next) {
    if (txHead == this) { txHead = txTail = nullptr; }
  } else {
    if (prev) prev->next = next; else txHead = next;
    if (next) next->prev = prev; else txTail = prev;
  }
  prev = next = nullptr;
}


//==========================================================================
//
//  VOpenGLTexture::registerMe
//
//==========================================================================
void VOpenGLTexture::registerMe () noexcept {
  if (prev || next) return;
  if (txHead == this) return;
  prev = txTail;
  if (txTail) txTail->next = this; else txHead = this;
  txTail = this;
}


//==========================================================================
//
//  VOpenGLTexture::reanalyzeImage
//
//==========================================================================
void VOpenGLTexture::reanalyzeImage () noexcept {
  const bool oldNSM = needSmoothing;
  needSmoothing = false;
  analyzeImage();
  needSmoothing = oldNSM;
}


//==========================================================================
//
//  VOpenGLTexture::analyzeImage
//
//==========================================================================
void VOpenGLTexture::analyzeImage () noexcept {
  if (img) {
    if (needSmoothing) img->smoothEdges();
    if (type != TT_Auto) { realType = type; return; }
    bool hasAlpha0 = false;
    bool hasAlpha255 = false;
    for (int y = 0; y < img->height; ++y) {
      for (int x = 0; x < img->width; ++x) {
        VImage::RGBA pix = img->getPixel(x, y);
             if (pix.a == 0) hasAlpha0 = true;
        else if (pix.a == 255) hasAlpha255 = true;
        else {
          realType = TT_Translucent;
          return; // we're done
        }
      }
    }
    realType =
      hasAlpha0 ?
        (hasAlpha255 ? TT_OneBitAlpha : TT_Transparent) :
        (hasAlpha255 ? TT_Opaque : TT_Transparent);
  } else {
    realType = TT_Transparent;
  }
}


//==========================================================================
//
//  VOpenGLTexture::convertToTrueColor
//
//==========================================================================
void VOpenGLTexture::convertToTrueColor () {
  if (!img || img->isTrueColor) return;

  VImage *tc = new VImage(VImage::ImageType::IT_RGBA, img->width, img->height);
  for (int y = 0; y < img->height; ++y) {
    for (int x = 0; x < img->width; ++x) {
      tc->setPixel(x, y, img->getPixel(x, y));
    }
  }

  delete img;
  img = tc;

  if (needSmoothing) img->smoothEdges();
}


//==========================================================================
//
//  VOpenGLTexture::update
//
//  FIXME: optimize this!
//
//==========================================================================
void VOpenGLTexture::update () {
  if (hw_glctx && tid) {
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &tid);
  }
  tid = 0;
  analyzeImage();
  if (hw_glctx) texUpload(this);
}


//==========================================================================
//
//  VOpenGLTexture::Load
//
//==========================================================================
VOpenGLTexture *VOpenGLTexture::Load (VStr fname, bool doSmoothing) {
  if (fname.length() > 0) {
    VOpenGLTexture **loaded = txLoaded.get(fname);
    if (loaded) { (*loaded)->addRef(); return *loaded; }
  }
  VStr rname = fsysFileFindAnyExt(fname);
  if (rname.length() == 0) return nullptr;
  VOpenGLTexture **loaded = txLoaded.get(rname);
  if (loaded) { (*loaded)->addRef(); return *loaded; }
  VStream *st = fsysOpenFile(rname);
  if (!st) return nullptr;
  VImage *img = VImage::loadFrom(st);
  VStream::Destroy(st);
  if (!img) return nullptr;
  VOpenGLTexture *res = new VOpenGLTexture(img, rname, doSmoothing);
  txLoaded.put(rname, res);
  //GLog.Logf(NAME_Debug, "TXLOADED: '%s' rc=%d, (%p)", *res->mPath, res->rc, res);
  return res;
}


//==========================================================================
//
//  VOpenGLTexture::CreateEmpty
//
//==========================================================================
VOpenGLTexture *VOpenGLTexture::CreateEmpty (VName txname, int wdt, int hgt, bool doSmoothing) {
  VStr sname;
  if (txname != NAME_None) {
    sname = VStr(txname);
    if (sname.length() > 0) {
      VOpenGLTexture **loaded = txLoaded.get(sname);
      if (loaded) {
        if ((*loaded)->width != wdt || (*loaded)->height != hgt) return nullptr; // oops
        (*loaded)->addRef();
        return *loaded;
      }
    }
  }
  if (wdt < 1 || hgt < 1 || wdt > 32768 || hgt > 32768) return nullptr;
  VOpenGLTexture *res = new VOpenGLTexture(wdt, hgt, doSmoothing);
  if (sname.length() > 0) txLoaded.put(sname, res); //else txLoadedUnnamed.append(res);
  //GLog.Logf(NAME_Debug, "TXLOADED: '%s' rc=%d, (%p)", *res->mPath, res->rc, res);
  return res;
}


//==========================================================================
//
//  VOpenGLTexture::blitExt
//
//==========================================================================
void VOpenGLTexture::blitExt (int dx0, int dy0, int dx1, int dy1, int x0, int y0, int x1, int y1, float angle) const noexcept {
  if (!tid /*|| VGLVideo::isFullyTransparent() || mTransparent*/) return;
  if (x1 < 0) x1 = img->width;
  if (y1 < 0) y1 = img->height;
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tid);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  //glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  VGLVideo::forceGLTexFilter();

  if (VGLVideo::getBlendMode() == VGLVideo::BlendNormal) {
    if (realType == TT_Opaque && VGLVideo::isFullyOpaque()) {
      glDisable(GL_BLEND);
    } else {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
  } else {
    VGLVideo::setupBlending();
  }

  const float fx0 = (float)x0/(float)img->width;
  const float fx1 = (float)x1/(float)img->width;
  const float fy0 = (float)y0/(float)img->height;
  const float fy1 = (float)y1/(float)img->height;
  const float z = VGLVideo::currZFloat;

  if (angle != 0.0f) {
    glPushMatrix();
    glTranslatef(dx0+(dx1-dx0)/2.0f, dy0+(dy1-dy0)/2.0f, 0.0f);
    glRotatef(angle, 0.0f, 0.0f, 1.0f);
    glTranslatef(-(dx0+(dx1-dx0)/2.0f), -(dy0+(dy1-dy0)/2.0f), 0.0f);
  }

  glBegin(GL_QUADS);
    glTexCoord2f(fx0, fy0); glVertex3f(dx0, dy0, z);
    glTexCoord2f(fx1, fy0); glVertex3f(dx1, dy0, z);
    glTexCoord2f(fx1, fy1); glVertex3f(dx1, dy1, z);
    glTexCoord2f(fx0, fy1); glVertex3f(dx0, dy1, z);
  glEnd();

  if (angle != 0.0f) glPopMatrix();
}


//==========================================================================
//
//  VOpenGLTexture::blitExtRep
//
//==========================================================================
void VOpenGLTexture::blitExtRep (int dx0, int dy0, int dx1, int dy1, int x0, int y0, int x1, int y1) const noexcept {
  if (!tid /*|| VGLVideo::isFullyTransparent() || mTransparent*/) return;
  if (x1 < 0) x1 = img->width;
  if (y1 < 0) y1 = img->height;
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tid);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  //glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  VGLVideo::forceGLTexFilter();

  if (VGLVideo::getBlendMode() == VGLVideo::BlendNormal) {
    if (realType == TT_Opaque && VGLVideo::isFullyOpaque()) {
      glDisable(GL_BLEND);
    } else {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
  } else {
    VGLVideo::setupBlending();
  }
  const float z = VGLVideo::currZFloat;

  glBegin(GL_QUADS);
    glTexCoord2i(x0, y0); glVertex3f(dx0, dy0, z);
    glTexCoord2i(x1, y0); glVertex3f(dx1, dy0, z);
    glTexCoord2i(x1, y1); glVertex3f(dx1, dy1, z);
    glTexCoord2i(x0, y1); glVertex3f(dx0, dy1, z);
  glEnd();
}


/*
struct TexQuad {
  int x0, y0, x1, y1;
  float tx0, ty0, tx1, ty1;
}
*/

//==========================================================================
//
//  VOpenGLTexture::blitWithLightmap
//
//==========================================================================
void VOpenGLTexture::blitWithLightmap (TexQuad *t0, VOpenGLTexture *lmap, TexQuad *t1) const noexcept {
//#ifndef _WIN32
  if (tid) {
    // bind normal texture
    p_glActiveTextureARB(GL_TEXTURE0_ARB);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    //glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    VGLVideo::forceGLTexFilter();
  }

  // bind lightmap texture
  if (lmap) {
    p_glActiveTextureARB(GL_TEXTURE1_ARB);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, lmap->tid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  }

  if (VGLVideo::getBlendMode() == VGLVideo::BlendNormal) {
    if (realType == TT_Opaque && VGLVideo::isFullyOpaque()) {
      glDisable(GL_BLEND);
    } else {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
  } else {
    VGLVideo::setupBlending();
  }

  const float z = VGLVideo::currZFloat;
  glBegin(GL_QUADS);
    p_glMultiTexCoord2fARB(GL_TEXTURE0_ARB, t0->tx0, t0->ty0);
    p_glMultiTexCoord2fARB(GL_TEXTURE1_ARB, t1->tx0, t1->ty0);
    glVertex3f(t0->x0, t0->y0, z);
    p_glMultiTexCoord2fARB(GL_TEXTURE0_ARB, t0->tx1, t0->ty0);
    p_glMultiTexCoord2fARB(GL_TEXTURE1_ARB, t1->tx1, t1->ty0);
    glVertex3f(t0->x1, t0->y0, z);
    p_glMultiTexCoord2fARB(GL_TEXTURE0_ARB, t0->tx1, t0->ty1);
    p_glMultiTexCoord2fARB(GL_TEXTURE1_ARB, t1->tx1, t1->ty1);
    glVertex3f(t0->x1, t0->y1, z);
    p_glMultiTexCoord2fARB(GL_TEXTURE0_ARB, t0->tx0, t0->ty1);
    p_glMultiTexCoord2fARB(GL_TEXTURE1_ARB, t1->tx0, t1->ty1);
    glVertex3f(t0->x0, t0->y1, z);
  glEnd();

  // unbund lightmap texture
  if (lmap) {
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    p_glActiveTextureARB(GL_TEXTURE0_ARB);
  }
  // default value
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
//#endif
}


//==========================================================================
//
//  VOpenGLTexture::blitAt
//
//==========================================================================
void VOpenGLTexture::blitAt (int dx0, int dy0, float scale, float angle) const noexcept {
  if (!tid /*|| VGLVideo::isFullyTransparent() || scale <= 0 || mTransparent*/) return;
  int w = img->width;
  int h = img->height;
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tid);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  //glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  VGLVideo::forceGLTexFilter();

  if (VGLVideo::getBlendMode() == VGLVideo::BlendNormal) {
    if (realType == TT_Opaque && VGLVideo::isFullyOpaque()) {
      glDisable(GL_BLEND);
    } else {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
  } else {
    VGLVideo::setupBlending();
  }
  const float z = VGLVideo::currZFloat;

  const float dx1 = dx0+w*scale;
  const float dy1 = dy0+h*scale;

  if (angle != 0.0f) {
    glPushMatrix();
    glTranslatef(dx0+(dx1-dx0)/2.0f, dy0+(dy1-dy0)/2.0f, 0.0f);
    glRotatef(angle, 0.0f, 0.0f, 1.0f);
    glTranslatef(-(dx0+(dx1-dx0)/2.0f), -(dy0+(dy1-dy0)/2.0f), 0.0f);
  }

  glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(dx0, dy0, z);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(dx1, dy0, z);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(dx1, dy1, z);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(dx0, dy1, z);
  glEnd();

  if (angle != 0.0f) glPopMatrix();
}


// ////////////////////////////////////////////////////////////////////////// //
static TMapNC<int, VGLTexture *> vcGLTexMap;
//FIXME: rewrite id management
static TArray<int> vcGLFreeIds;
static int vcGLFreeIdsUsed = 0;
static int vcGLLastUsedId = 0;


//==========================================================================
//
//  vcGLAllocId
//
//==========================================================================
static int vcGLAllocId (VGLTexture *obj) noexcept {
  int res;
  if (vcGLFreeIdsUsed > 0) {
    res = vcGLFreeIds[--vcGLFreeIdsUsed];
  } else {
    // no free ids
    res = ++vcGLLastUsedId;
  }
  vcGLTexMap.put(res, obj);
  return res;
}


//==========================================================================
//
//  vcGLFreeId
//
//==========================================================================
static void vcGLFreeId (int id) noexcept {
  if (id < 1 || id > vcGLLastUsedId) return;
  vcGLTexMap.del(id);
  if (vcGLFreeIdsUsed == vcGLFreeIds.length()) {
    vcGLFreeIds.append(id);
    ++vcGLFreeIdsUsed;
  } else {
    vcGLFreeIds[vcGLFreeIdsUsed++] = id;
  }
}



//**************************************************************************
//
// VGLTexture
//
//**************************************************************************

IMPLEMENT_CLASS(V, GLTexture);


//==========================================================================
//
//  VGLTexture::Destroy
//
//==========================================================================
void VGLTexture::Destroy () {
  vcGLFreeId(id);
  //GLog.Logf(NAME_Debug, "destroying texture object %p", this);
  if (tex) {
    //GLog.Logf(NAME_Debug, "  releasing texture '%s'... rc=%d, (%p)", *tex->getPath(), tex->getRC(), tex);
    tex->release();
    tex = nullptr;
  }
  Super::Destroy();
}


IMPLEMENT_FUNCTION(VGLTexture, Destroy) {
  vobjGetParamSelf();
  if (Self) Self->ConditionalDestroy();
}


//native final static GLTexture Load (string fname, optional bool doSmoothing/*=true*/);
IMPLEMENT_FUNCTION(VGLTexture, Load) {
  VStr fname;
  VOptParamBool doSmoothing(true);
  vobjGetParam(fname, doSmoothing);
  VOpenGLTexture *tex = VOpenGLTexture::Load(fname, doSmoothing);
  if (tex) {
    VGLTexture *ifile = SpawnWithReplace<VGLTexture>();
    ifile->tex = tex;
    ifile->id = vcGLAllocId(ifile);
    //GLog.Logf(NAME_Debug, "created texture object %p (%p)", ifile, ifile->tex);
    RET_REF((VObject *)ifile);
  } else {
    RET_REF(nullptr);
  }
}


// native final static GLTexture GetById (int id);
IMPLEMENT_FUNCTION(VGLTexture, GetById) {
  int id;
  vobjGetParam(id);
  if (id > 0 && id <= vcGLLastUsedId) {
    auto opp = vcGLTexMap.get(id);
    if (opp) RET_REF((VGLTexture *)(*opp)); else RET_REF(nullptr);
  } else {
    RET_REF(nullptr);
  }
}


IMPLEMENT_FUNCTION(VGLTexture, width) {
  vobjGetParamSelf();
  RET_INT(Self && Self->tex ? Self->tex->width : 0);
}

IMPLEMENT_FUNCTION(VGLTexture, height) {
  vobjGetParamSelf();
  RET_INT(Self && Self->tex ? Self->tex->height : 0);
}

IMPLEMENT_FUNCTION(VGLTexture, isTransparent) {
  vobjGetParamSelf();
  RET_BOOL(Self && Self->tex ? Self->tex->isTransparent : true);
}

IMPLEMENT_FUNCTION(VGLTexture, isOpaque) {
  vobjGetParamSelf();
  RET_BOOL(Self && Self->tex ? Self->tex->isOpaque : false);
}

IMPLEMENT_FUNCTION(VGLTexture, isOneBitAlpha) {
  vobjGetParamSelf();
  RET_BOOL(Self && Self->tex ? Self->tex->isOneBitAlpha : true);
}


IMPLEMENT_FUNCTION(VGLTexture, get_textureType) {
  vobjGetParamSelf();
  RET_INT(Self && Self->tex ? Self->tex->type : VOpenGLTexture::TT_Auto);
}

IMPLEMENT_FUNCTION(VGLTexture, set_textureType) {
  int ttype;
  vobjGetParamSelf(ttype);
  if (Self && Self->tex) {
    if (ttype < 0 || ttype > VOpenGLTexture::TT_Transparent) ttype = VOpenGLTexture::TT_Auto;
    if (Self->tex->type != (unsigned)ttype) {
      Self->tex->type = (unsigned)ttype;
      Self->tex->reanalyzeImage();
    }
  }
}

IMPLEMENT_FUNCTION(VGLTexture, get_textureRealType) {
  vobjGetParamSelf();
  RET_INT(Self && Self->tex ? Self->tex->realType : VOpenGLTexture::TT_Auto);
}

IMPLEMENT_FUNCTION(VGLTexture, set_textureRealType) {
  int ttype;
  vobjGetParamSelf(ttype);
  if (Self && Self->tex) {
    if (ttype < 0 || ttype > VOpenGLTexture::TT_Transparent) ttype = VOpenGLTexture::TT_Auto;
    if (Self->tex->realType != (unsigned)ttype) {
      if (ttype == VOpenGLTexture::TT_Auto) Self->tex->type = VOpenGLTexture::TT_Auto;
      Self->tex->type = (unsigned)ttype;
      Self->tex->reanalyzeImage();
    }
  }
}

IMPLEMENT_FUNCTION(VGLTexture, get_needSmoothing) {
  vobjGetParamSelf();
  RET_INT(Self && Self->tex ? Self->tex->needSmoothing : true);
}

IMPLEMENT_FUNCTION(VGLTexture, set_needSmoothing) {
  bool v;
  vobjGetParamSelf(v);
  if (Self && Self->tex) Self->tex->needSmoothing = v;
}


// void blitExt (int dx0, int dy0, int dx1, int dy1, int x0, int y0, optional int x1, optional int y1, optional float angle);
IMPLEMENT_FUNCTION(VGLTexture, blitExt) {
  int dx0, dy0, dx1, dy1;
  int x0, y0;
  VOptParamInt x1(-1), y1(-1);
  VOptParamFloat angle(0);
  vobjGetParamSelf(dx0, dy0, dx1, dy1, x0, y0, x1, y1, angle);
  if (Self && Self->tex) Self->tex->blitExt(dx0, dy0, dx1, dy1, x0, y0, x1, y1, angle);
}


// void blitExtRep (int dx0, int dy0, int dx1, int dy1, int x0, int y0, optional int x1, optional int y1);
IMPLEMENT_FUNCTION(VGLTexture, blitExtRep) {
  int dx0, dy0, dx1, dy1;
  int x0, y0;
  VOptParamInt x1(-1), y1(-1);
  vobjGetParamSelf(dx0, dy0, dx1, dy1, x0, y0, x1, y1);
  if (Self && Self->tex) Self->tex->blitExtRep(dx0, dy0, dx1, dy1, x0, y0, x1, y1);
}


// void blitAt (int dx0, int dy0, optional float scale, optional float angle);
IMPLEMENT_FUNCTION(VGLTexture, blitAt) {
  int dx0, dy0;
  VOptParamFloat scale(1), angle(0);
  vobjGetParamSelf(dx0, dy0, scale, angle);
  if (Self && Self->tex) Self->tex->blitAt(dx0, dy0, scale, angle);
}


// native final void blitWithLightmap (const ref TexQuad t0, GLTexture lmap, const ref TexQuad t1);
IMPLEMENT_FUNCTION(VGLTexture, blitWithLightmap) {
  VOpenGLTexture::TexQuad *t0, *t1;
  VGLTexture *lmap;
  vobjGetParamSelf(t0, lmap, t1);
  if (Self && Self->tex) Self->tex->blitWithLightmap(t0, (lmap ? lmap->tex : nullptr), t1);
}


// native final static GLTexture CreateEmpty (int wdt, int hgt, optional name txname, optional bool doSmoothing/*=true*/);
IMPLEMENT_FUNCTION(VGLTexture, CreateEmpty) {
  int wdt, hgt;
  VOptParamBool doSmoothing(true);
  VOptParamName txname(NAME_None);
  vobjGetParam(wdt, hgt, txname, doSmoothing);
  if (wdt < 1 || hgt < 1 || wdt > 32768 || hgt > 32768) { RET_REF(nullptr); return; }
  VOpenGLTexture *tex = VOpenGLTexture::CreateEmpty(txname, wdt, hgt, doSmoothing);
  if (tex) {
    VGLTexture *ifile = SpawnWithReplace<VGLTexture>();
    ifile->tex = tex;
    ifile->id = vcGLAllocId(ifile);
    //GLog.Logf(NAME_Debug, "created texture object %p (%p)", ifile, ifile->tex);
    RET_REF((VObject *)ifile);
  } else {
    RET_REF(nullptr);
  }
}

// native final static void setPixel (int x, int y, int argb); // aarrggbb; a==0 is completely opaque
IMPLEMENT_FUNCTION(VGLTexture, setPixel) {
  int x, y;
  vuint32 argb;
  vobjGetParamSelf(x, y, argb);
  if (Self && Self->tex && Self->tex->img) {
    vuint8 a = 255-((argb>>24)&0xff);
    vuint8 r = (argb>>16)&0xff;
    vuint8 g = (argb>>8)&0xff;
    vuint8 b = argb&0xff;
    Self->tex->img->setPixel(x, y, VImage::RGBA(r, g, b, a));
  }
}

// native final static int getPixel (int x, int y); // aarrggbb; a==0 is completely opaque
IMPLEMENT_FUNCTION(VGLTexture, getPixel) {
  int x, y;
  vobjGetParamSelf(x, y);
  if (Self && Self->tex && Self->tex->img) {
    auto c = Self->tex->img->getPixel(x, y);
    vuint32 argb = (((vuint32)c.r)<<16)|(((vuint32)c.g)<<8)|((vuint32)c.b)|(((vuint32)(255-c.a))<<24);
    RET_INT((vint32)argb);
  } else {
    RET_INT(0xff000000); // completely transparent
  }
}

// native final static void upload ();
IMPLEMENT_FUNCTION(VGLTexture, upload) {
  vobjGetParamSelf();
  if (Self && Self->tex) Self->tex->update();
}

// native final void smoothEdges (); // call after manual texture building
IMPLEMENT_FUNCTION(VGLTexture, smoothEdges) {
  vobjGetParamSelf();
  if (Self && Self->tex && Self->tex->img) {
    Self->tex->img->smoothEdges();
  }
}


bool VGLTexture::savingAllowed = false;

// should be explicitly enabled
// native final void saveAsPNG (string fname);
IMPLEMENT_FUNCTION(VGLTexture, saveAsPNG) {
  VStr fname;
  vobjGetParamSelf(fname);
  if (savingAllowed && Self && Self->tex && Self->tex->img && !fname.isEmpty()) {
    VStream *st = fsysOpenDiskFileWrite(fname);
    if (!st) return;
    Self->tex->img->saveAsPNG(st);
    VStream::Destroy(st);
  }
}


//native final void fillRect (int x, int y, int w, int h, int argb);
IMPLEMENT_FUNCTION(VGLTexture, fillRect) {
  int x, y, w, h;
  vuint32 argb;
  vobjGetParamSelf(x, y, w, h, argb);
  if (w < 1 || h < 1) return;
  if (x < 0) {
    w += x;
    if (w <= 0) return;
    x = 0;
  }
  if (y < 0) {
    h += y;
    if (h <= 0) return;
    y = 0;
  }
  if (Self && Self->tex) {
    VOpenGLTexture *tx = Self->tex;
    if (!tx->img) return;
    const int iw = tx->width;
    const int ih = tx->height;
    if (x >= iw || y >= ih) return;
    if (w > iw) w = iw;
    if (h > ih) h = ih;
    w = min2(w, iw-x);
    h = min2(h, ih-y);
    tx->convertToTrueColor();
    const VImage::RGBA clr(argb);
    const unsigned pitch = tx->img->getPitch();
    const unsigned linesize = (unsigned)iw*pitch;
    //GLog.Logf(NAME_Debug, "x=%d; y=%d; w=%d; h=%d; pitch=%u; linesize=%u; argb=0x%08x (%02x %02x %02x %02x)", x, y, w, h, pitch, linesize, argb, clr.a, clr.r, clr.g, clr.b);
    vuint8 *px = tx->img->writeablePixels;
    px += (unsigned)x*pitch+((unsigned)y*linesize);
    // fill the first line
    VImage::RGBA *d = (VImage::RGBA *)px;
    while (w--) *d++ = clr;
    // copy lines
    while (--h) {
      memcpy(px+linesize, px, linesize);
      px += linesize;
    }
  }
}


//native final void hline (int x, int y, int len, int argb);
IMPLEMENT_FUNCTION(VGLTexture, hline) {
  int x, y, len;
  vuint32 argb;
  vobjGetParamSelf(x, y, len, argb);
  if (len < 1 || y < 0) return;
  if (x < 0) {
    len += x;
    if (len <= 0) return;
    x = 0;
  }
  if (Self && Self->tex) {
    VOpenGLTexture *tx = Self->tex;
    if (!tx->img) return;
    const int iw = tx->width;
    const int ih = tx->height;
    if (x >= iw || y >= ih) return;
    if (len > iw) len = iw;
    len = min2(len, iw-x);
    if (x+len > iw) len = iw-x;
    tx->convertToTrueColor();
    const VImage::RGBA clr(argb);
    VImage::RGBA *d = (VImage::RGBA *)tx->img->getWriteablePixels();
    d += (unsigned)x+((unsigned)y*(unsigned)iw);
    // fill the line
    while (len--) *d++ = clr;
  }
}


//native final void vline (int x, int y, int len, int argb);
IMPLEMENT_FUNCTION(VGLTexture, vline) {
  int x, y, len;
  vuint32 argb;
  vobjGetParamSelf(x, y, len, argb);
  if (len < 1 || x < 0) return;
  if (y < 0) {
    len += y;
    if (len <= 0) return;
    y = 0;
  }
  if (Self && Self->tex) {
    VOpenGLTexture *tx = Self->tex;
    if (!tx->img) return;
    const int iw = tx->width;
    const int ih = tx->height;
    if (x >= iw || y >= ih) return;
    if (len > ih) len = ih;
    len = min2(len, ih-y);
    tx->convertToTrueColor();
    const VImage::RGBA clr(argb);
    VImage::RGBA *d = (VImage::RGBA *)tx->img->getWriteablePixels();
    d += (unsigned)x+((unsigned)y*(unsigned)iw);
    // fill the line
    while (len--) {
      *d = clr;
      d += iw;
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, GLVideo);

bool VGLVideo::mInited = false;
bool VGLVideo::mPreInitWasDone = false;
int VGLVideo::mWidth = 0;
int VGLVideo::mHeight = 0;
bool VGLVideo::smoothLine = false;
bool VGLVideo::directMode = false;
bool VGLVideo::depthTest = false;
bool VGLVideo::depthWrite = false;
bool VGLVideo::stencilEnabled = false;
int VGLVideo::depthFunc = VGLVideo::ZFunc_Less;
int VGLVideo::currZ = 0;
float VGLVideo::currZFloat = 1.0f;
int VGLVideo::swapInterval = 0;
bool VGLVideo::texFiltering = false;
int VGLVideo::colorMask = CMask_Red|CMask_Green|CMask_Blue|CMask_Alpha;
int VGLVideo::stencilBits = 0;
int VGLVideo::alphaTestFunc = VGLVideo::STC_Always;
float VGLVideo::alphaFuncVal = 0.0f;
bool VGLVideo::in3dmode = false;
bool VGLVideo::dbgDumpOpenGLInfo = false;
float VGLVideo::glPolyOfsFactor = 0.0f;
float VGLVideo::glPolyOfsUnit = 0.0f;


struct TimerInfo {
  SDL_TimerID sdlid;
  int id; // script id (0: not used)
  int interval;
  bool oneShot;
};

static TMapNC<int, TimerInfo> timerMap; // key: timer id; value: timer info
static int timerLastUsedId = 0;


//==========================================================================
//
//  timerAllocId
//
//  doesn't insert anything in `timerMap`!
//
//==========================================================================
static int timerAllocId () noexcept {
  int res = 0;
  if (timerLastUsedId == 0) timerLastUsedId = 1;
  if (timerLastUsedId < 0x7ffffff && timerMap.has(timerLastUsedId)) ++timerLastUsedId;
  if (timerLastUsedId < 0x7ffffff) {
    res = timerLastUsedId++;
  } else {
    for (int f = 1; f < timerLastUsedId; ++f) if (!timerMap.has(f)) { res = f; break; }
  }
  return res;
}


//==========================================================================
//
//  timerFreeId
//
//  removes element from `timerMap`!
//
//==========================================================================
static void timerFreeId (int id) noexcept {
  if (id <= 0) return;
  TimerInfo *ti = timerMap.get(id);
  if (ti) {
    SDL_RemoveTimer(ti->sdlid);
    timerMap.del(id);
  }
  if (id == timerLastUsedId) {
    while (timerLastUsedId > 0 && !timerMap.has(timerLastUsedId)) --timerLastUsedId;
  }
}


//==========================================================================
//
//  sdlTimerCallback
//
//==========================================================================
static Uint32 sdlTimerCallback (Uint32 interval, void *param) noexcept {
  SDL_Event event;
  SDL_UserEvent userevent;

  int id = (int)(intptr_t)param;

  TimerInfo *ti = timerMap.get(id);
  if (ti) {
    //GLog.Logf(NAME_Debug, "timer cb: id=%d", id);
    userevent.type = SDL_USEREVENT;
    userevent.code = 1;
    userevent.data1 = (void *)(intptr_t)ti->id;

    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent(&event);
    // don't delete timer here, 'cause callback is running in separate thread
    return (ti->oneShot ? 0 : ti->interval);
  }

  return 0; // this timer is dead
}


//==========================================================================
//
//  VGLVideo::CreateTimerWithId
//
//  returns timer id or 0
//  if id <= 0, creates new unique timer id
//  if interval is < 1, returns with error and won't create timer
//
//==========================================================================
int VGLVideo::CreateTimerWithId (int id, int intervalms, bool oneShot) noexcept {
  if (intervalms < 1) return 0;
  if (id <= 0) {
    // get new id
    id = timerAllocId();
  } else {
    if (timerMap.has(id)) return 0;
  }

  vassert(sdlTimerInited);
  /*
  if (!sdlTimerInited) {
    sdlTimerInited = true;
    if (SDL_InitSubSystem(SDL_INIT_TIMER|SDL_INIT_EVENTS) < 0) Sys_Error("SDL_InitSubSystem(TIMER): %s\n", SDL_GetError());
  }
  */

  //GLog.Logf(NAME_Debug, "id=%d; interval=%d; one=%d", id, intervalms, (int)oneShot);
  TimerInfo ti;
  ti.sdlid = SDL_AddTimer(intervalms, &sdlTimerCallback, (void *)(intptr_t)id);
  if (ti.sdlid == 0) {
    GLog.Logf(NAME_Error, "CANNOT create timer: id=%d; interval=%d; one=%d", id, intervalms, (int)oneShot);
    timerFreeId(id);
    return 0;
  }
  ti.id = id;
  ti.interval = intervalms;
  ti.oneShot = oneShot;
  timerMap.put(id, ti);
  return id;
}


//==========================================================================
//
//  VGLVideo::DeleteTimer
//
//  `true`: deleted, `false`: no such timer
//
//==========================================================================
bool VGLVideo::DeleteTimer (int id) noexcept {
  if (id <= 0 || !timerMap.has(id)) return false;
  timerFreeId(id);
  return true;
}


//==========================================================================
//
//  VGLVideo::IsTimerExists
//
//==========================================================================
bool VGLVideo::IsTimerExists (int id) noexcept {
  return (id > 0 && timerMap.has(id));
}


//==========================================================================
//
//  VGLVideo::IsTimerOneShot
//
//==========================================================================
bool VGLVideo::IsTimerOneShot (int id) noexcept {
  TimerInfo *ti = timerMap.get(id);
  return (ti && ti->oneShot);
}


//==========================================================================
//
//  VGLVideo::GetTimerInterval
//
//  0: no such timer
//
//==========================================================================
int VGLVideo::GetTimerInterval (int id) noexcept {
  TimerInfo *ti = timerMap.get(id);
  return (ti ? ti->interval : 0);
}


//==========================================================================
//
//  VGLVideo::SetTimerInterval
//
//  returns success flag; won't do anything if interval is < 1
//
//==========================================================================
bool VGLVideo::SetTimerInterval (int id, int intervalms) noexcept {
  if (intervalms < 1) return false;
  TimerInfo *ti = timerMap.get(id);
  if (ti) {
    ti->interval = intervalms;
    return true;
  }
  return false;
}


//==========================================================================
//
//  ShutdownJoystick
//
//==========================================================================
static void ShutdownJoystick (bool closesubsys) noexcept{
  if (!sdlJoystickInited) return;
  //if (haptic) { SDL_HapticClose(haptic); haptic = nullptr; }
  if (joystick) { SDL_JoystickClose(joystick); joystick = nullptr; }
  if (controller) { SDL_GameControllerClose(controller); controller = nullptr; }
  if (closesubsys) {
    //if (has_haptic) { SDL_QuitSubSystem(SDL_INIT_HAPTIC); has_haptic = false; }
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER);
    sdlJoystickInited = false;
  }
  joystick_controller = false;
  joynum = -1;
  jid = 0;
  ctl_trigger = 0;
  memset(&joy_x[0], 0, 2*sizeof(joy_x[0]));
  memset(&joy_y[0], 0, 2*sizeof(joy_y[0]));
  /*
  memset(&joy_oldx[0], 0, 2*sizeof(joy_oldx[0]));
  memset(&joy_oldy[0], 0, 2*sizeof(joy_oldy[0]));
  */
  if (closesubsys) {
    SDL_JoystickEventState(SDL_IGNORE);
    SDL_GameControllerEventState(SDL_IGNORE);
  }
}


static void OpenJoystick (int jnum) {
  if (!sdlJoystickInited) return;

  ShutdownJoystick(false);

  int joycount = SDL_NumJoysticks();
  if (joycount < 0) {
    GLog.Log(NAME_Error, "SDL: joystick/controller initialisation failed (cannot get number of joysticks)");
    ShutdownJoystick(true);
    return;
  }

  if (joycount == 0) return;

  if (jnum >= joycount) jnum = joycount-1;
  if (jnum < 0) jnum = 0;
  joynum = jnum;

  if (SDL_IsGameController(joynum)) {
    joystick_controller = true;
    //joy_num_buttons = 0;
    controller = SDL_GameControllerOpen(joynum);
    if (!controller) {
      GLog.Logf(NAME_Error, "SDL: cannot initialise controller #%d", joynum);
      ShutdownJoystick(false);
      return;
    }
    //GLog.Logf(NAME_Init, "SDL: joystick is a controller (%s)", SDL_GameControllerNameForIndex(joynum));
    SDL_Joystick *cj = SDL_GameControllerGetJoystick(controller);
    if (!cj) {
      GLog.Log(NAME_Error, "SDL: controller initialisation failed (cannot get joystick info)");
      ShutdownJoystick(true);
      return;
    }
    jid = SDL_JoystickInstanceID(cj);
    /*
    if (has_haptic) {
      haptic = SDL_HapticOpenFromJoystick(cj);
      if (haptic) {
        GLog.Logf(NAME_Init, "SDL: found haptic support for controller");
      } else {
        GLog.Logf(NAME_Init, "SDL: cannot open haptic interface");
      }
    }
    */
  } else {
    joystick = SDL_JoystickOpen(joynum);
    if (!joystick) {
      GLog.Logf(NAME_Error, "SDL: cannot initialise joystick #%d", joynum);
      ShutdownJoystick(false);
      return;
    }
    jid = SDL_JoystickInstanceID(joystick);
    //joy_num_buttons = SDL_JoystickNumButtons(joystick);
    //GLog.Logf(NAME_Init, "SDL: found joystick with %d buttons", joy_num_buttons);
  }

  SDL_JoystickEventState(SDL_ENABLE);
  SDL_GameControllerEventState(SDL_ENABLE);
}


#include "ctldb.c"

//==========================================================================
//
//  StartupJoystick
//
//==========================================================================
static void StartupJoystick () noexcept {
  ShutdownJoystick(true);

  // load user controller mappings
  size_t dblen = strlen(ControllerDB);
  SDL_RWops *rwo = SDL_RWFromConstMem(ControllerDB, (int)dblen);
  if (rwo) {
    const int count = SDL_GameControllerAddMappingsFromRW(rwo, 1); // free rwo
    if (count < 0) {
      GLog.Log(NAME_Error, "cannot read controller mappings");
    } else if (count > 0) {
      //GLog.Logf(NAME_Init, "read %d controller mapping%s from '%s'", count, (count != 1 ? "s" : ""), *stname);
    }
  }

  int jnum = 0;

  sdlJoystickInited = true;
  if (SDL_InitSubSystem(SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER) < 0) {
    GLog.Logf(NAME_Error, "SDL_InitSubSystem(JOYSTICK): %s\n", SDL_GetError());
    return;
  }

  /*
  if (cli_NoCotl) {
    has_haptic = false;
  } else if (SDL_InitSubSystem(SDL_INIT_HAPTIC) < 0) {
    has_haptic = false;
    GLog.Log(NAME_Init, "SDL: cannot init haptic subsystem, force feedback disabled");
  } else {
    has_haptic = true;
    GLog.Log(NAME_Init, "SDL: force feedback enabled");
  }
  */

  int joycount = SDL_NumJoysticks();
  if (joycount < 0) {
    GLog.Log("SDL: joystick/controller initialisation failed (cannot get number of joysticks)");
    ShutdownJoystick(true);
    return;
  }

  SDL_JoystickEventState(SDL_ENABLE);
  SDL_GameControllerEventState(SDL_ENABLE);

  if (joycount == 0) {
    //GLog.Log("SDL: no joysticks found");
    return;
  }

  //GLog.Logf("SDL: %d joystick%s found", joycount, (joycount == 1 ? "" : "s"));

  //if (joycount == 1 || (jnum >= 0 && jnum < joycount)) OpenJoystick(jnum);
  OpenJoystick(jnum);
}


// ////////////////////////////////////////////////////////////////////////// //
/*
#ifdef WIN32
enum {
  GL_INCR_WRAP = 0x8507u,
  GL_DECR_WRAP = 0x8508u,
};
#endif
*/


//==========================================================================
//
//  convertStencilOp
//
//==========================================================================
static GLenum convertStencilOp (const int op) noexcept {
  switch (op) {
    case VGLVideo::STC_Keep: return GL_KEEP;
    case VGLVideo::STC_Zero: return GL_ZERO;
    case VGLVideo::STC_Replace: return GL_REPLACE;
    case VGLVideo::STC_Incr: return GL_INCR;
    case VGLVideo::STC_IncrWrap: return GL_INCR_WRAP;
    case VGLVideo::STC_Decr: return GL_DECR;
    case VGLVideo::STC_DecrWrap: return GL_DECR_WRAP;
    case VGLVideo::STC_Invert: return GL_INVERT;
    default: break;
  }
  return GL_KEEP;
}


//==========================================================================
//
//  convertStencilFunc
//
//==========================================================================
static GLenum convertStencilFunc (const int op) noexcept {
  switch (op) {
    case VGLVideo::STC_Never: return GL_NEVER;
    case VGLVideo::STC_Less: return GL_LESS;
    case VGLVideo::STC_LEqual: return GL_LEQUAL;
    case VGLVideo::STC_Greater: return GL_GREATER;
    case VGLVideo::STC_GEqual: return GL_GEQUAL;
    case VGLVideo::STC_NotEqual: return GL_NOTEQUAL;
    case VGLVideo::STC_Equal: return GL_EQUAL;
    case VGLVideo::STC_Always: return GL_ALWAYS;
    default: break;
  }
  return GL_ALWAYS;
}


//==========================================================================
//
//  convertBlendFunc
//
//==========================================================================
static GLenum convertBlendFunc (const int op) noexcept {
  switch (op) {
    case VGLVideo::BlendFunc_Add: return GL_FUNC_ADD;
    case VGLVideo::BlendFunc_Sub: return GL_FUNC_SUBTRACT;
    case VGLVideo::BlendFunc_SubRev: return GL_FUNC_REVERSE_SUBTRACT;
    case VGLVideo::BlendFunc_Min: return GL_MIN;
    case VGLVideo::BlendFunc_Max: return GL_MAX;
    default: break;
  }
  return GL_FUNC_ADD;
}


//==========================================================================
//
//  VGLVideo::forceAlphaFunc
//
//==========================================================================
void VGLVideo::forceAlphaFunc () noexcept {
  if (mInited) {
    auto fn = convertStencilFunc(alphaTestFunc);
    if (fn == GL_ALWAYS) {
      glDisable(GL_ALPHA_TEST);
    } else {
      glEnable(GL_ALPHA_TEST);
      glAlphaFunc(fn, alphaFuncVal);
    }
  }
}

//==========================================================================
//
//  VGLVideo::forceBlendFunc
//
//==========================================================================
void VGLVideo::forceBlendFunc () noexcept {
  if (mInited) glBlendEquationFunc(convertBlendFunc(mBlendFunc));
}


//==========================================================================
//
//  VGLVideo::forceGLTexFilter
//
//==========================================================================
void VGLVideo::forceGLTexFilter () noexcept {
  if (mInited) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (texFiltering ? GL_LINEAR : GL_NEAREST));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (texFiltering ? GL_LINEAR : GL_NEAREST));
  }
}


//==========================================================================
//
//  VGLVideo::forceColorMask
//
//==========================================================================
void VGLVideo::forceColorMask () noexcept {
  if (mInited) {
    glColorMask(
      (colorMask&CMask_Red ? GL_TRUE : GL_FALSE),
      (colorMask&CMask_Green ? GL_TRUE : GL_FALSE),
      (colorMask&CMask_Blue ? GL_TRUE : GL_FALSE),
      (colorMask&CMask_Alpha ? GL_TRUE : GL_FALSE));
  }
}


//==========================================================================
//
//  VGLVideo::setupBlending
//
//==========================================================================
bool VGLVideo::setupBlending () noexcept {
  if (!mInited) return false;
  if (mBlendMode == BlendNone) {
    glDisable(GL_BLEND);
    return true;
  } else if (mBlendMode == BlendNormal) {
    if ((colorARGB&0xff000000u) == 0) {
      // opaque
      glDisable(GL_BLEND);
      return true;
    } else {
      // either alpha, or completely transparent
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      return ((colorARGB&0xff000000u) != 0xff000000u);
    }
  } else {
    glEnable(GL_BLEND);
         if (mBlendMode == BlendBlend) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    else if (mBlendMode == BlendFilter) glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
    else if (mBlendMode == BlendInvert) glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
    else if (mBlendMode == BlendParticle) glBlendFunc(GL_DST_COLOR, GL_ZERO);
    else if (mBlendMode == BlendHighlight) glBlendFunc(GL_DST_COLOR, GL_ONE);
    else if (mBlendMode == BlendDstMulDstAlpha) glBlendFunc(GL_ZERO, GL_DST_ALPHA);
    else if (mBlendMode == InvModulate) glBlendFunc(GL_ZERO, GL_SRC_COLOR);
    else if (mBlendMode == Premult) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    return ((colorARGB&0xff000000u) != 0xff000000u);
  }
}


//==========================================================================
//
//  VGLVideo::realizeZFunc
//
//==========================================================================
void VGLVideo::realizeZFunc () noexcept {
  if (mInited) {
    switch (depthFunc) {
      case ZFunc_Never: glDepthFunc(GL_NEVER); break;
      case ZFunc_Always: glDepthFunc(GL_ALWAYS); break;
      case ZFunc_Equal: glDepthFunc(GL_EQUAL); break;
      case ZFunc_NotEqual: glDepthFunc(GL_NOTEQUAL); break;
      case ZFunc_Less: glDepthFunc(GL_LESS); break;
      case ZFunc_LessEqual: glDepthFunc(GL_LEQUAL); break;
      case ZFunc_Greater: glDepthFunc(GL_GREATER); break;
      case ZFunc_GreaterEqual: glDepthFunc(GL_GEQUAL); break;
    }
  }
}


//==========================================================================
//
//  VGLVideo::realiseGLColor
//
//==========================================================================
void VGLVideo::realiseGLColor () noexcept {
  if (mInited) {
    glColor4f(
      ((colorARGB>>16)&0xff)/255.0f,
      ((colorARGB>>8)&0xff)/255.0f,
      (colorARGB&0xff)/255.0f,
      1.0f-(((colorARGB>>24)&0xff)/255.0f)
    );
    setupBlending();
  }
}


//==========================================================================
//
//  VGLVideo::canInit
//
//==========================================================================
bool VGLVideo::canInit () noexcept {
  return true;
}


//==========================================================================
//
//  VGLVideo::hasOpenGL
//
//==========================================================================
bool VGLVideo::hasOpenGL () noexcept {
  return true;
}


//==========================================================================
//
//  VGLVideo::isInitialized
//
//==========================================================================
bool VGLVideo::isInitialized () noexcept {
  return mInited;
}


//==========================================================================
//
//  VGLVideo::getWidth
//
//==========================================================================
int VGLVideo::getWidth () noexcept {
  return mWidth;
}


//==========================================================================
//
//  VGLVideo::getHeight
//
//==========================================================================
int VGLVideo::getHeight () noexcept {
  return mHeight;
}


//==========================================================================
//
//  VGLVideo::close
//
//==========================================================================
void VGLVideo::close () noexcept {
  if (mInited) {
    if (hw_glctx) {
      if (hw_window) {
        SDL_GL_MakeCurrent(hw_window, hw_glctx);
        unloadAllTextures();
      }
      SDL_GL_MakeCurrent(hw_window, nullptr);
      SDL_GL_DeleteContext(hw_glctx);
    }
    if (hw_window) SDL_DestroyWindow(hw_window);
    mInited = false;
    mWidth = 0;
    mHeight = 0;
    directMode = false;
    depthTest = false;
    depthWrite = false;
    depthFunc = VGLVideo::ZFunc_Less;
    currZ = 0;
    currZFloat = 1.0f;
    texFiltering = false;
    colorMask = CMask_Red|CMask_Green|CMask_Blue|CMask_Alpha;
    stencilBits = 0;
  }
  hw_glctx = nullptr;
  hw_window = nullptr;
}


//==========================================================================
//
//  sdlSetGLAttrs
//
//==========================================================================
static void sdlSetGLAttrs () noexcept {
  // require OpenGL 2.1
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  //SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, r_vsync);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
}


//==========================================================================
//
//  VGLVideo::fuckfucksdl
//
//  k8: no, i really don't know why i have to repeat this twice,
//  but at the first try i'll get no stencil buffer for some reason
//  (and no rgba framebuffer too)
//
//==========================================================================
void VGLVideo::fuckfucksdl () noexcept {
  if (mInited) return;

  mPreInitWasDone = true;

  sdlSetGLAttrs();

  hw_window = SDL_CreateWindow("VavoomC runner hidden SDL fucker", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    640, 480, SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  if (!hw_window) {
    GLog.Log(NAME_Error, "ALAS: cannot create SDL2 window.");
    return;
  }

  hw_glctx = SDL_GL_CreateContext(hw_window);
  if (!hw_glctx) {
    SDL_DestroyWindow(hw_window);
    hw_window = nullptr;
    GLog.Log(NAME_Error, "ALAS: cannot create SDL2 OpenGL context.");
    return;
  }

  SDL_GL_MakeCurrent(hw_window, hw_glctx);
  glGetError();

  SDL_GL_MakeCurrent(hw_window, nullptr);
  SDL_GL_DeleteContext(hw_glctx);
  SDL_DestroyWindow(hw_window);
  hw_glctx = nullptr;
  hw_window = nullptr;
}


//==========================================================================
//
//  VGLVideo::open
//
//==========================================================================
bool VGLVideo::open (VStr winname, int width, int height, int fullscreen) {
  if (width < 1 || height < 1) {
    width = 800;
    height = 600;
  }

  if (!sdlVideoInited) {
    sdlVideoInited = true;
    sdlTimerInited = true;
    if (SDL_InitSubSystem(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_EVENTS) < 0) Sys_Error("SDL_InitSubSystem(VIDEO): %s\n", SDL_GetError());
    if (!sdlJoystickInited) StartupJoystick(); // why not?
  }

  close();

  if (!mPreInitWasDone) fuckfucksdl();

  Uint32 flags = SDL_WINDOW_OPENGL;
  if (fullscreen) flags |= (fullscreen == 1 ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP);

  sdlSetGLAttrs();

  int si = swapInterval;
  if (si < 0) si = -1; else if (si > 0) si = 1;
  if (SDL_GL_SetSwapInterval(si) == -1 && si < 0) SDL_GL_SetSwapInterval(1);

  glGetError();

  hw_window = SDL_CreateWindow((winname.length() ? *winname : "Untitled"), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);
  if (!hw_window) {
    GLog.Log(NAME_Error, "ALAS: cannot create SDL2 window.");
    return false;
  }

  hw_glctx = SDL_GL_CreateContext(hw_window);
  if (!hw_glctx) {
    SDL_DestroyWindow(hw_window);
    hw_window = nullptr;
    GLog.Log(NAME_Error, "ALAS: cannot create SDL2 OpenGL context.");
    return false;
  }

  SDL_GL_MakeCurrent(hw_window, hw_glctx);
  glGetError();

  int stb = -1;
  SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stb);
  stencilBits = (stb < 1 ? 0 : stb);
  //if (stb < 1) GLog.Log(NAME_Warning, "WARNING: no stencil buffer available!");
  //if (flags&SDL_WINDOW_HIDDEN) SDL_ShowWindow(hw_window);

  if (dbgDumpOpenGLInfo) {
    int ghi, glo;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &ghi);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &glo);
    GLog.Logf(NAME_Debug, "OpenGL version: %d.%d", ghi, glo);

    int ltmp = 666;
    SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &ltmp); GLog.Logf(NAME_Debug, "STENCIL BUFFER BITS: %d", ltmp);
    SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &ltmp); GLog.Logf(NAME_Debug, "RED BITS: %d", ltmp);
    SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &ltmp); GLog.Logf(NAME_Debug, "GREEN BITS: %d", ltmp);
    SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &ltmp); GLog.Logf(NAME_Debug, "BLUE BITS: %d", ltmp);
    SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &ltmp); GLog.Logf(NAME_Debug, "ALPHA BITS: %d", ltmp);
    SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &ltmp); GLog.Logf(NAME_Debug, "DEPTH BITS: %d", ltmp);
  }

  //SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, r_vsync);
  if (si < 0) si = -1; else if (si > 0) si = 1;
  if (SDL_GL_SetSwapInterval(si) == -1) { if (si < 0) { si = 1; SDL_GL_SetSwapInterval(1); } }
  swapInterval = si;

  // everything is fine, set some globals and finish
  mInited = true;
  mWidth = width;
  mHeight = height;

  //SDL_DisableScreenSaver();

  forceColorMask();
  forceAlphaFunc();

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

  if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
  if (depthWrite) glDepthMask(GL_TRUE); else glDepthMask(GL_FALSE);
  realizeZFunc();
  glDisable(GL_CULL_FACE);
  //glDisable(GL_BLEND);
  //glEnable(GL_LINE_SMOOTH);
  if (smoothLine) glEnable(GL_LINE_SMOOTH); else glDisable(GL_LINE_SMOOTH);
  if (stencilEnabled) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  glStencilFunc(GL_ALWAYS, 0, 0xffffffff);

  glDisable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
  //glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  realiseGLColor(); // this will setup blending
  //glEnable(GL_BLEND);
  //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  int realw = width, realh = height;
  SDL_GL_GetDrawableSize(hw_window, &realw, &realh);

  //glViewport(0, 0, width, height);
  glViewport(0, 0, realw, realh);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  glOrtho(0, realw, realh, 0, -zNear, -zFar);
  mWidth = realw;
  mHeight = realh;

  in3dmode = false;
  glDisable(GL_CULL_FACE);

  /*
  if (realw == width && realh == height) {
    glOrtho(0, width, height, 0, -zNear, -zFar);
  } else {
    int sx0 = (realw-width)/2;
    int sy0 = (realh-height)/2;
    GLog.Logf(NAME_Debug, "size:(%d,%d); real:(%d,%d); sofs:(%d,%d)", width, height, realw, realh, sx0, sy0);
    //glOrtho(-sx0, realw-sx0, sy0+realh, sy0, -zNear, -zFar);
    //glOrtho(-sx0, width-sx0, height, 0, -zNear, -zFar);
    //glOrtho(0, width, height, 0, -zNear, -zFar);

    glOrtho(0, realw, realh, 0, -zNear, -zFar);
    mWidth = realw;
    mHeight = realh;

    //glOrtho(-500, width-500, height, 0, -zNear, -zFar);
    //width = realw;
    //height = realh;
  }
  */

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

  /*
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (texFiltering  ? GL_LINEAR : GL_NEAREST));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (texFiltering  ? GL_LINEAR : GL_NEAREST));
  */

  //glDrawBuffer(directMode ? GL_FRONT : GL_BACK);

  glBlendEquationFunc = (/*PFNGLBLENDEQUATIONPROC*/glBlendEquationFn)SDL_GL_GetProcAddress("glBlendEquation");
  if (!glBlendEquationFunc) Sys_Error("`glBlendEquation` not found");

//#ifndef _WIN32
  p_glMultiTexCoord2fARB = (glMultiTexCoord2fARB_t)SDL_GL_GetProcAddress("glMultiTexCoord2fARB");
  if (!p_glMultiTexCoord2fARB) Sys_Error("`glMultiTexCoord2fARB` not found");

  p_glActiveTextureARB = (glActiveTextureARB_t)SDL_GL_GetProcAddress("glActiveTextureARB");
  if (!p_glActiveTextureARB) Sys_Error("`glActiveTextureARB` not found");
//#endif

  clear();
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

  uploadAllTextures();

  if (SDL_GL_ExtensionSupported("GL_ARB_texture_non_power_of_two") ||
      SDL_GL_ExtensionSupported("GL_OES_texture_npot"))
  {
    hasNPOT = true;
  } else {
    hasNPOT = false;
  }

  return true;
}


//==========================================================================
//
//  VGLVideo::clear
//
//==========================================================================
void VGLVideo::clear (int rgb) noexcept {
  if (!mInited) return;

  glClearColor(((rgb>>16)&0xff)/255.0f, ((rgb>>8)&0xff)/255.0f, (rgb&0xff)/255.0f, 0.0);
  glClearDepth(1.0);
  glClearStencil(0);

  //glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
  //glClear(GL_COLOR_BUFFER_BIT|(depthTest ? GL_DEPTH_BUFFER_BIT : 0));
  glClear(GL_COLOR_BUFFER_BIT|(depthTest ? GL_DEPTH_BUFFER_BIT : 0)|(stencilEnabled ? GL_STENCIL_BUFFER_BIT : 0));
}


//==========================================================================
//
//  VGLVideo::clearDepth
//
//==========================================================================
void VGLVideo::clearDepth (float val) noexcept {
  if (!mInited) return;
  glClearDepth(val);
  glClear(GL_DEPTH_BUFFER_BIT);
  glClearDepth(1.0);
}


//==========================================================================
//
//  VGLVideo::clearStencil
//
//==========================================================================
void VGLVideo::clearStencil (int val) noexcept {
  if (!mInited) return;
  glClearStencil(val);
  glClear(GL_STENCIL_BUFFER_BIT);
  glClearStencil(0);
}


//==========================================================================
//
//  VGLVideo::clearColor
//
//==========================================================================
void VGLVideo::clearColor (int rgb) noexcept {
  if (!mInited) return;
  glClearColor(((rgb>>16)&0xff)/255.0f, ((rgb>>8)&0xff)/255.0f, (rgb&0xff)/255.0f, 0.0);
  glClear(GL_COLOR_BUFFER_BIT);
}


// ////////////////////////////////////////////////////////////////////////// //
VMethod *VGLVideo::onDrawVC = nullptr;
VMethod *VGLVideo::onEventVC = nullptr;
VMethod *VGLVideo::onNewFrameVC = nullptr;
VMethod *VGLVideo::onSwappedVC = nullptr;


//==========================================================================
//
//  VGLVideo::initMethods
//
//==========================================================================
void VGLVideo::initMethods () {
  onDrawVC = nullptr;
  onEventVC = nullptr;
  onSwappedVC = nullptr;

  VClass *mklass = VClass::FindClass("Main");
  if (!mklass) return;

  VMethod *mmain = mklass->FindMethod("onDraw");
  if (mmain && (mmain->Flags&FUNC_VarArgs) == 0 && mmain->ReturnType.Type == TYPE_Void && mmain->NumParams == 0) {
    onDrawVC = mmain;
  }

  mmain = mklass->FindMethod("onSwapped");
  if (mmain && (mmain->Flags&FUNC_VarArgs) == 0 && mmain->ReturnType.Type == TYPE_Void && mmain->NumParams == 0) {
    onSwappedVC = mmain;
  }

  mmain = mklass->FindMethod("onEvent");
  if (mmain && (mmain->Flags&FUNC_VarArgs) == 0 && mmain->ReturnType.Type == TYPE_Void && mmain->NumParams == 1 &&
      ((mmain->ParamTypes[0].Type == TYPE_Pointer &&
        mmain->ParamFlags[0] == 0 &&
        mmain->ParamTypes[0].GetPointerInnerType().Type == TYPE_Struct &&
        mmain->ParamTypes[0].GetPointerInnerType().Struct->Name == "event_t") ||
       ((mmain->ParamFlags[0]&(FPARM_Out|FPARM_Ref)) != 0 &&
        mmain->ParamTypes[0].Type == TYPE_Struct &&
        mmain->ParamTypes[0].Struct->Name == "event_t")))
  {
    //GLog.Logf(NAME_Debug, "onevent found");
    onEventVC = mmain;
  } else {
    //GLog.Logf(NAME_Debug, ":: (%d) %s", mmain->ParamFlags[0], *mmain->ParamTypes[0].GetName());
    //abort();
  }

  mmain = mklass->FindMethod("onNewFrame");
  if (mmain && (mmain->Flags&FUNC_VarArgs) == 0 && mmain->ReturnType.Type == TYPE_Void && mmain->NumParams == 0) {
    onNewFrameVC = mmain;
  }
}


//==========================================================================
//
//  VGLVideo::onDraw
//
//==========================================================================
void VGLVideo::onDraw () {
  doRefresh = false;
  if (!hw_glctx || !onDrawVC) return;
  if ((onDrawVC->Flags&FUNC_Static) == 0) P_PASS_REF((VObject *)mainObject);
  VObject::ExecuteFunction(onDrawVC);
  doGLSwap = true;
}


//==========================================================================
//
//  VGLVideo::onSwapped
//
//==========================================================================
void VGLVideo::onSwapped () {
  if (!hw_glctx || !onSwappedVC) return;
  if ((onSwappedVC->Flags&FUNC_Static) == 0) P_PASS_REF((VObject *)mainObject);
  VObject::ExecuteFunction(onSwappedVC);
}


//==========================================================================
//
//  VGLVideo::onEvent
//
//==========================================================================
void VGLVideo::onEvent (event_t &evt) {
  if (!hw_glctx || !onEventVC) return;
  if ((onEventVC->Flags&FUNC_Static) == 0) P_PASS_REF((VObject *)mainObject);
  P_PASS_REF((event_t *)&evt);
  VObject::ExecuteFunction(onEventVC);
}


//==========================================================================
//
//  VGLVideo::onNewFrame
//
//==========================================================================
void VGLVideo::onNewFrame () {
  if (!hw_glctx || !onNewFrameVC) return;
  if ((onNewFrameVC->Flags&FUNC_Static) == 0) P_PASS_REF((VObject *)mainObject);
  VObject::ExecuteFunction(onNewFrameVC);
}


// ////////////////////////////////////////////////////////////////////////// //
int VGLVideo::currFrameTime = 0;
uint64_t VGLVideo::nextFrameTime = 0;


//==========================================================================
//
//  VGLVideo::getFrameTime
//
//==========================================================================
int VGLVideo::getFrameTime () noexcept {
  return currFrameTime;
}


//==========================================================================
//
//  VGLVideo::setFrameTime
//
//==========================================================================
void VGLVideo::setFrameTime (int newft) noexcept {
  if (newft < 0) newft = 0;
  if (currFrameTime == newft) return;
  nextFrameTime = 0; //FIXME!
  currFrameTime = newft;
}


//==========================================================================
//
//  VGLVideo::getMousePosition
//
//==========================================================================
void VGLVideo::getMousePosition (int *mx, int *my) {
  if (mInited) {
    //SDL_GetMouseState(xp, yp); //k8: alas, this returns until a mouse has been moved
    int xp = 0, yp = 0;
    SDL_GetGlobalMouseState(&xp, &yp);
    int wx, wy;
    SDL_GetWindowPosition(hw_window, &wx, &wy);
    xp -= wx;
    yp -= wy;
    if (mx) *mx = xp;
    if (my) *my = yp;
  } else {
    if (mx) *mx = 0;
    if (my) *my = 0;
  }
}


//==========================================================================
//
//  VGLVideo::VCC_ProcessEvent
//
//==========================================================================
void VGLVideo::VCC_ProcessEvent (event_t &ev, void *udata) {
  onEvent(ev);
}


//==========================================================================
//
//  PostKeyEvent
//
//==========================================================================
static bool PostKeyEvent (int key, int press, vuint32 modflags) noexcept {
  if (!key) return true; // always succeed
  event_t ev;
  ev.clear();
  ev.type = (press ? ev_keydown : ev_keyup);
  ev.keycode = key;
  ev.modflags = modflags;
  return VObject::PostEvent(ev);
}


//==========================================================================
//
//  PostJoyMotionEvent
//
//==========================================================================
static void PostJoyMotionEvent (int axisidx) noexcept {
  if (axisidx >= 0 && axisidx <= 1) {
    event_t event;
    event.clear();
    event.type = ev_joystick;
    event.dx = joy_x[axisidx];
    event.dy = joy_y[axisidx];
    event.joyidx = axisidx;
    event.modflags = curmodflags;
    VObject::PostEvent(event);
  }
}


//==========================================================================
//
//  CtlTriggerButton
//
//==========================================================================
static void CtlTriggerButton (int idx, bool down) noexcept {
  if (idx < 0 || idx > 1) return;
  const unsigned mask = 1u<<idx;
  if (down) {
    // pressed
    if ((ctl_trigger&mask) == 0) {
      ctl_trigger |= mask;
      PostKeyEvent(K_BUTTON_TRIGGER_LEFT+idx, true, curmodflags);
    }
  } else {
    // released
    if ((ctl_trigger&mask) != 0) {
      ctl_trigger &= ~mask;
      PostKeyEvent(K_BUTTON_TRIGGER_LEFT+idx, false, curmodflags);
    }
  }
}


static bool inEventLoop = false;
static atomic_int VCC_PingSent = 0;
static double lastCollect = 0.0;


//==========================================================================
//
//  VGLVideo::VCC_WaitEvents
//
//==========================================================================
void VGLVideo::VCC_WaitEvents () {
  if (!mInited) Sys_Error("VGLVideo::VCC_WaitEvents: wtf?! (inited)");
  if (!inEventLoop) Sys_Error("VGLVideo::VCC_WaitEvents: wtf?! (inloop)");

  //uint64_t prevFrameSwapTime = 0;
  //prevFrameSwapTime = Sys_Time_Micro();

  int mx, my;
  for (;;) {
    if (quitSignal == 1) {
      PostQuitEvent(0);
      quitSignal = 2;
    }

    if (VObject::PeekEvent(0, nullptr)) return; // has some events to process

    SDL_Event ev;
    event_t evt;

    int mstimeout = -1;
    //GLog.Logf(NAME_Debug, "VGLVideo::VCC_WaitEvents:000: currFrameTime=%d", currFrameTime);
    if (currFrameTime > 0) {
      uint64_t ctt = 0;
      const double dctt = Sys_Time_ExU(&ctt);
      if (nextFrameTime == 0) nextFrameTime = ctt;
      bool wasNewFrame = false;
      //GLog.Logf(NAME_Debug, "VGLVideo::VCC_WaitEvents:001:   ctt=%llu; nextFrameTime=%llu", ctt, nextFrameTime);
      while (ctt >= nextFrameTime) {
        nextFrameTime += currFrameTime;
        //onNewFrame();
        wasNewFrame = true;
        //GLog.Logf(NAME_Debug, "VGLVideo::VCC_WaitEvents:002:   ctt=%llu; nextFrameTime=%llu", ctt, nextFrameTime);
      }
      vassert(ctt < nextFrameTime);
      mstimeout = (int)(nextFrameTime-ctt);
      vassert(mstimeout != 0);
      if (wasNewFrame) onNewFrame();

      //GLog.Log(NAME_Debug, "VGLVideo::VCC_WaitEvents:004:   checking gc...");
      //double currTick = fsysCurrTick();
      if (dctt-lastCollect >= 3.0) {
        lastCollect = dctt;
        //GLog.Log(NAME_Debug, "VGLVideo::VCC_WaitEvents:004:   collecting garbage...");
        VObject::CollectGarbage(); // why not?
        //GLog.Log(NAME_Debug, "VGLVideo::VCC_WaitEvents:004:   garbage collection complete.");
        //GLog.Logf(NAME_Debug, "objc=%d", VObject::GetObjectsCount());
      }
    } else {
      // unlimited framerate
      // refresh signal must be sent by the VC code
      const double currTick = Sys_Time();
      if (currTick-lastCollect >= 3.0) {
        lastCollect = currTick;
        VObject::CollectGarbage(); // why not?
        //GLog.Logf(NAME_Debug, "objc=%d", VObject::GetObjectsCount());
      }
    }

    if (doRefresh) onDraw();
    if (doGLSwap) {
      //GLog.Log(NAME_Debug, "VGLVideo::VCC_WaitEvents:004:   doGLSwap");
      doGLSwap = false;
      SDL_GL_SwapWindow(hw_window);
      onSwapped();
    }

    //GLog.Logf(NAME_Debug, "VGLVideo::VCC_WaitEvents:100: hasevents=%d", (int)VObject::PeekEvent(0, nullptr));
    if (VObject::PeekEvent(0, nullptr)) return; // has some events to process

    //GLog.Logf(NAME_Debug, "VGLVideo::VCC_WaitEvents:110: mstimeout=%d", mstimeout);
    //SDL_PumpEvents(); // no need to call this
    if (mstimeout > 0) {
      if (!SDL_WaitEventTimeout(&ev, mstimeout)) continue; // timeout (or error, but we don't care)
    } else {
      if (!SDL_WaitEvent(&ev)) Sys_Error("error waiting for the event");
    }

    //GLog.Log(NAME_Debug, "VGLVideo::VCC_WaitEvents:200: processing SDL events...");
    bool doCheck = true;
    while (doCheck) {
      evt.clear();
      evt.modflags = curmodflags;
      switch (ev.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
          {
            int kk = sdl2TranslateKey(ev.key.keysym.scancode);
            if (kk > 0) {
              getMousePosition(&mx, &my);
              evt.type = (ev.type == SDL_KEYDOWN ? ev_keydown : ev_keyup);
              evt.data1 = kk;
              evt.data2 = mx;
              evt.data3 = my;
              //onEvent(evt);
              VObject::PostEvent(evt);
            }
            // now fix flags
            switch (ev.key.keysym.sym) {
              case SDLK_LSHIFT: if (ev.type == SDL_KEYDOWN) curmodflags |= bShiftLeft; else curmodflags &= ~bShiftLeft; break;
              case SDLK_RSHIFT: if (ev.type == SDL_KEYDOWN) curmodflags |= bShiftRight; else curmodflags &= ~bShiftRight; break;
              case SDLK_LCTRL: if (ev.type == SDL_KEYDOWN) curmodflags |= bCtrlLeft; else curmodflags &= ~bCtrlLeft; break;
              case SDLK_RCTRL: if (ev.type == SDL_KEYDOWN) curmodflags |= bCtrlRight; else curmodflags &= ~bCtrlRight; break;
              case SDLK_LALT: if (ev.type == SDL_KEYDOWN) curmodflags |= bAltLeft; else curmodflags &= ~bAltLeft; break;
              case SDLK_RALT: if (ev.type == SDL_KEYDOWN) curmodflags |= bAltRight; else curmodflags &= ~bAltRight; break;
              case SDLK_LGUI: if (ev.type == SDL_KEYDOWN) curmodflags |= bHyper; else curmodflags &= ~bHyper; break;
              case SDLK_RGUI: if (ev.type == SDL_KEYDOWN) curmodflags |= bHyper; else curmodflags &= ~bHyper; break;
              default: break;
            }
            if (curmodflags&(bShiftLeft|bShiftRight)) curmodflags |= bShift; else curmodflags &= ~bShift;
            if (curmodflags&(bCtrlLeft|bCtrlRight)) curmodflags |= bCtrl; else curmodflags &= ~bCtrl;
            if (curmodflags&(bAltLeft|bAltRight)) curmodflags |= bAlt; else curmodflags &= ~bAlt;
          }
          break;
        case SDL_MOUSEMOTION:
          getMousePosition(&mx, &my);
          evt.type = ev_mouse;
          evt.data1 = 0;
          //evt.data2 = ev.motion.x;
          //evt.data3 = ev.motion.y;
          evt.data2 = mx;
          evt.data3 = my;
          //onEvent(evt);
          VObject::PostEvent(evt);
          memset((void *)&evt, 0, sizeof(evt));
          evt.modflags = curmodflags;
          evt.type = ev_uimouse;
          evt.data1 = 0;
          evt.data2 = ev.motion.xrel;
          evt.data3 = ev.motion.yrel;
          //onEvent(evt);
          VObject::PostEvent(evt);
          break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
          evt.type = (ev.button.state == SDL_PRESSED ? ev_keydown : ev_keyup);
               if (ev.button.button == SDL_BUTTON_LEFT) evt.data1 = K_MOUSE1;
          else if (ev.button.button == SDL_BUTTON_RIGHT) evt.data1 = K_MOUSE2;
          else if (ev.button.button == SDL_BUTTON_MIDDLE) evt.data1 = K_MOUSE3;
          else if (ev.button.button == SDL_BUTTON_X1) evt.data1 = K_MOUSE4;
          else if (ev.button.button == SDL_BUTTON_X2) evt.data1 = K_MOUSE5;
          else break;
          getMousePosition(&mx, &my);
          //evt.data2 = ev.button.x;
          //evt.data3 = ev.button.y;
          evt.data2 = mx;
          evt.data3 = my;
          //onEvent(evt);
          VObject::PostEvent(evt);
          // now fix flags
               if (ev.button.button == SDL_BUTTON_LEFT) { if (ev.button.state == SDL_PRESSED) curmodflags |= bLMB; else curmodflags &= ~bLMB; }
          else if (ev.button.button == SDL_BUTTON_RIGHT) { if (ev.button.state == SDL_PRESSED) curmodflags |= bRMB; else curmodflags &= ~bRMB; }
          else if (ev.button.button == SDL_BUTTON_MIDDLE) { if (ev.button.state == SDL_PRESSED) curmodflags |= bMMB; else curmodflags &= ~bMMB; }
          break;
        case SDL_MOUSEWHEEL:
          evt.type = ev_keydown;
               if (ev.wheel.y > 0) evt.data1 = K_MWHEELUP;
          else if (ev.wheel.y < 0) evt.data1 = K_MWHEELDOWN;
          else break;
          getMousePosition(&mx, &my);
          evt.data2 = mx;
          evt.data3 = my;
          //onEvent(evt);
          VObject::PostEvent(evt);
          break;
        // joysticks
        case SDL_JOYAXISMOTION:
          if (joystick) {
            //GLog.Logf(NAME_Debug, "JOY AXIS %d: motion=%d", ev.jaxis.axis, ev.jaxis.value);
            int normal_value = clampval(ev.jaxis.value*127/32767, -127, 127);
                 if (ev.jaxis.axis == 0) joy_x[0] = normal_value;
            else if (ev.jaxis.axis == 1) joy_y[0] = normal_value;
            else break;
            PostJoyMotionEvent(0);
            //joy_oldx[jn] = joy_x[jn];
            //joy_oldy[jn] = joy_y[jn];
          }
          break;
        case SDL_JOYBALLMOTION:
          if (joystick) {
            //GLog.Logf(NAME_Debug, "JOY BALL");
          }
          break;
        case SDL_JOYHATMOTION:
          if (joystick) {
            //GLog.Logf(NAME_Debug, "JOY HAT");
          }
          break;
        case SDL_JOYBUTTONDOWN:
          if (joystick) {
            //GLog.Logf(NAME_Debug, "JOY BUTTON %d", ev.jbutton.button);
            if (ev.jbutton.button >= 0 && ev.jbutton.button <= 15) {
              //joy_newb |= 1u<<ev.jbutton.button;
              PostKeyEvent(K_JOY1+ev.jbutton.button, true, curmodflags);
            }
          }
          break;
        case SDL_JOYBUTTONUP:
          if (joystick) {
            if (ev.jbutton.button >= 0 && ev.jbutton.button <= 15) {
              //joy_newb &= ~(1u<<ev.jbutton.button);
              PostKeyEvent(K_JOY1+ev.jbutton.button, false, curmodflags);
            }
          }
          break;
        // controllers
        case SDL_CONTROLLERAXISMOTION:
          if (controller && ev.caxis.which == jid) {
            int normal_value = clampval(ev.caxis.value*127/32767, -127, 127);
            #if 0
            const char *axisName = "<unknown>";
            switch (ev.caxis.axis) {
              case SDL_CONTROLLER_AXIS_LEFTX: axisName = "LEFTX"; break;
              case SDL_CONTROLLER_AXIS_LEFTY: axisName = "LEFTY"; break;
              case SDL_CONTROLLER_AXIS_RIGHTX: axisName = "RIGHTX"; break;
              case SDL_CONTROLLER_AXIS_RIGHTY: axisName = "RIGHTY"; break;
              case SDL_CONTROLLER_AXIS_TRIGGERLEFT: axisName = "TRIGLEFT"; break;
              case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: axisName = "TRIGRIGHT"; break;
              default: break;
            }
            GLog.Logf(NAME_Debug, "CTL AXIS '%s' (%d): motion=%d; nval=%d", axisName, ev.caxis.axis, ev.caxis.value, normal_value);
            #endif
            switch (ev.caxis.axis) {
              case SDL_CONTROLLER_AXIS_LEFTX: joy_x[0] = normal_value; PostJoyMotionEvent(0); break;
              case SDL_CONTROLLER_AXIS_LEFTY: joy_y[0] = normal_value; PostJoyMotionEvent(0); break;
              case SDL_CONTROLLER_AXIS_RIGHTX: joy_x[1] = normal_value; PostJoyMotionEvent(1); break;
              case SDL_CONTROLLER_AXIS_RIGHTY: joy_y[1] = normal_value; PostJoyMotionEvent(1); break;
              //HACK: consider both triggers as buttons for now
              case SDL_CONTROLLER_AXIS_TRIGGERLEFT: CtlTriggerButton(0, (normal_value >= 110)); break;
              case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: CtlTriggerButton(1, (normal_value >= 110)); break;
            }
          }
          break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
          if (controller && ev.cbutton.which == jid) {
            #if 0
            const char *buttName = "<unknown>";
            switch (ev.cbutton.button) {
              case SDL_CONTROLLER_BUTTON_A: buttName = "K_BUTTON_A"; break;
              case SDL_CONTROLLER_BUTTON_B: buttName = "K_BUTTON_B"; break;
              case SDL_CONTROLLER_BUTTON_X: buttName = "K_BUTTON_X"; break;
              case SDL_CONTROLLER_BUTTON_Y: buttName = "K_BUTTON_Y"; break;
              case SDL_CONTROLLER_BUTTON_BACK: buttName = "K_BUTTON_BACK"; break;
              case SDL_CONTROLLER_BUTTON_GUIDE: buttName = "K_BUTTON_GUIDE"; break;
              case SDL_CONTROLLER_BUTTON_START: buttName = "K_BUTTON_START"; break;
              case SDL_CONTROLLER_BUTTON_LEFTSTICK: buttName = "K_BUTTON_LEFTSTICK"; break;
              case SDL_CONTROLLER_BUTTON_RIGHTSTICK: buttName = "K_BUTTON_RIGHTSTICK"; break;
              case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: buttName = "K_BUTTON_LEFTSHOULDER"; break;
              case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: buttName = "K_BUTTON_RIGHTSHOULDER"; break;
              case SDL_CONTROLLER_BUTTON_DPAD_UP: buttName = "K_BUTTON_DPAD_UP"; break;
              case SDL_CONTROLLER_BUTTON_DPAD_DOWN: buttName = "K_BUTTON_DPAD_DOWN"; break;
              case SDL_CONTROLLER_BUTTON_DPAD_LEFT: buttName = "K_BUTTON_DPAD_LEFT"; break;
              case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: buttName = "K_BUTTON_DPAD_RIGHT"; break;
              default: break;
            }
            GLog.Logf(NAME_Debug, "CTL %d BUTTON %s: '%s' (%d)", ev.cbutton.which, (ev.cbutton.state == SDL_PRESSED ? "DOWN" : "UP"), buttName, ev.cbutton.button);
            #endif
            switch (ev.cbutton.button) {
              case SDL_CONTROLLER_BUTTON_A: PostKeyEvent(K_BUTTON_A, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
              case SDL_CONTROLLER_BUTTON_B: PostKeyEvent(K_BUTTON_B, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
              case SDL_CONTROLLER_BUTTON_X: PostKeyEvent(K_BUTTON_X, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
              case SDL_CONTROLLER_BUTTON_Y: PostKeyEvent(K_BUTTON_Y, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
              case SDL_CONTROLLER_BUTTON_BACK: PostKeyEvent(K_BUTTON_BACK, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
              case SDL_CONTROLLER_BUTTON_GUIDE: PostKeyEvent(K_BUTTON_GUIDE, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
              case SDL_CONTROLLER_BUTTON_START: PostKeyEvent(K_BUTTON_START, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
              case SDL_CONTROLLER_BUTTON_LEFTSTICK: PostKeyEvent(K_BUTTON_LEFTSTICK, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
              case SDL_CONTROLLER_BUTTON_RIGHTSTICK: PostKeyEvent(K_BUTTON_RIGHTSTICK, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
              case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: PostKeyEvent(K_BUTTON_LEFTSHOULDER, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
              case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: PostKeyEvent(K_BUTTON_RIGHTSHOULDER, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
              case SDL_CONTROLLER_BUTTON_DPAD_UP: PostKeyEvent(K_BUTTON_DPAD_UP, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
              case SDL_CONTROLLER_BUTTON_DPAD_DOWN: PostKeyEvent(K_BUTTON_DPAD_DOWN, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
              case SDL_CONTROLLER_BUTTON_DPAD_LEFT: PostKeyEvent(K_BUTTON_DPAD_LEFT, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
              case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: PostKeyEvent(K_BUTTON_DPAD_RIGHT, (ev.cbutton.state == SDL_PRESSED ? 1 : 0), curmodflags); break;
              default: break;
            }
          }
          break;
        case SDL_CONTROLLERDEVICEADDED:
          //GLog.Logf(NAME_Debug, "CTL %d added", ev.cdevice.which);
          if (SDL_NumJoysticks() == 1 || !(joystick && !controller)) {
            GLog.Logf(NAME_Debug, "controller attached");
            OpenJoystick(0);
          }
          break;
        case SDL_CONTROLLERDEVICEREMOVED:
          //GLog.Logf(NAME_Debug, "CTL %d removed", ev.cdevice.which);
          if (joystick_controller && ev.cdevice.which == jid) {
            ShutdownJoystick(false);
            GLog.Logf(NAME_Debug, "controller detached");
          }
          break;
        /*k8: i don't know what to do here
        case SDL_CONTROLLERDEVICEREMAPPED:
          GLog.Logf(NAME_Debug, "CTL %d remapped", ev.cdevice.which);
          if (joystick_controller && ev.cdevice.which == jid) {
          }
          break;
        */
        case SDL_WINDOWEVENT:
          switch (ev.window.event) {
            case SDL_WINDOWEVENT_FOCUS_GAINED:
              curmodflags = 0; // just in case
              evt.type = ev_winfocus;
              evt.data1 = 1;
              evt.data2 = 0;
              evt.data3 = 0;
              evt.modflags = 0;
              //onEvent(evt);
              VObject::PostEvent(evt);
              break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
              curmodflags = 0;
              evt.type = ev_winfocus;
              evt.data1 = 0;
              evt.data2 = 0;
              evt.data3 = 0;
              evt.modflags = 0;
              //onEvent(evt);
              VObject::PostEvent(evt);
              break;
            case SDL_WINDOWEVENT_EXPOSED:
              onDraw();
              break;
          }
          break;
        case SDL_QUIT:
          evt.type = ev_closequery;
          evt.data1 = 0; // alas, there is no way to tell why we're quiting; fuck you, sdl!
          evt.data2 = 0;
          evt.data3 = 0;
          //onEvent(evt);
          VObject::PostEvent(evt);
          doCheck = false;
          break;
        case SDL_USEREVENT:
          //GLog.Logf(NAME_Debug, "SDL: userevent, code=%d", ev.user.code);
          if (ev.user.code == 1) {
            TimerInfo *ti = timerMap.get((int)(intptr_t)ev.user.data1);
            if (ti) {
              int id = ti->id; // save id
              // remove one-shot timer
              if (ti->oneShot) timerFreeId(ti->id);
              evt.type = ev_timer;
              evt.data1 = id;
              evt.data2 = 0;
              evt.data3 = 0;
              //onEvent(evt);
              VObject::PostEvent(evt);
            }
          }
          atomic_set(&VCC_PingSent, 0);
          break;
        default:
          break;
      }
      if (doCheck) {
        if (!SDL_PollEvent(&ev)) break;
      }
    }
  }
}


static VCC_PingEventLoopFn oldPingEventLoop = nullptr;
static VCC_WaitEventsFn oldVCC_WaitEvents = nullptr;


//==========================================================================
//
//  VGLVideo::VCC_PingEventLoop
//
//==========================================================================
void VGLVideo::VCC_PingEventLoop () {
  if (!mInited) {
    if (oldPingEventLoop) oldPingEventLoop();
    return;
  }

  if (atomic_set(&VCC_PingSent, 1) == 0) {
    SDL_Event event;
    SDL_UserEvent userevent;

    userevent.type = SDL_USEREVENT;
    userevent.code = 0;

    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent(&event);
  }
}


//==========================================================================
//
//  VGLVideo::sendPing
//
//==========================================================================
void VGLVideo::sendPing () noexcept {
  VCC_PingEventLoop();
}


//==========================================================================
//
//  VGLVideo::runEventLoop
//
//==========================================================================
void VGLVideo::runEventLoop () {
  if (!mInited) Sys_Error("VGLVideo::runEventLoop: wtf?!");
  if (inEventLoop) Sys_Error("VGLVideo::runEventLoop: wtf?!");

  //if (!mInited) return;

  initMethods();
  onDraw();
  RegisterEventProcessor(&VGLVideo::VCC_ProcessEvent, nullptr);
  oldPingEventLoop = ::VCC_PingEventLoop;
  ::VCC_PingEventLoop = &VCC_PingEventLoop;
  oldVCC_WaitEvents = ::VCC_WaitEvents;
  ::VCC_WaitEvents = &VCC_WaitEvents;
  atomic_set(&VCC_PingSent, 0);
  inEventLoop = true;
  VCC_RunEventLoop(nullptr);
  inEventLoop = false;
  UnregisterEventProcessor(&VGLVideo::VCC_ProcessEvent, nullptr);
  vassert(::VCC_PingEventLoop == &VCC_PingEventLoop);
  vassert(::VCC_WaitEvents == &VCC_WaitEvents);
  ::VCC_PingEventLoop = oldPingEventLoop;
  ::VCC_WaitEvents = oldVCC_WaitEvents;
}


// ////////////////////////////////////////////////////////////////////////// //
vuint32 VGLVideo::colorARGB = 0xffffff;
int VGLVideo::mBlendMode = VGLVideo::BlendNormal;
int VGLVideo::mBlendFunc = BlendFunc_Add;
VFont *VGLVideo::currFont = nullptr;


//==========================================================================
//
//  VGLVideo::setFont
//
//==========================================================================
void VGLVideo::setFont (VName fontname) noexcept {
  if (currFont && currFont->getName() == fontname) return;
  currFont = VFont::findFont(fontname);
}


//==========================================================================
//
//  VGLVideo::drawTextAt
//
//==========================================================================
void VGLVideo::drawTextAt (int x, int y, VStr text) {
  if (!currFont /*|| isFullyTransparent()*/ || text.isEmpty()) return;
  if (!mInited) return;

  const VOpenGLTexture *tex = nullptr;
  if (currFont->singleTexture) {
    tex = currFont->getTexture();
    if (!tex || !tex->tid) return; // oops
  }

  glEnable(GL_TEXTURE_2D);
  //glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  if (currFont->singleTexture) glBindTexture(GL_TEXTURE_2D, tex->tid);
  glEnable(GL_BLEND); // font is rarely fully opaque, so don't bother checking
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  const float z = currZFloat;
  if (currFont->singleTexture) glBegin(GL_QUADS);
  int sx = x;
  for (int f = 0; f < text.length(); ++f) {
    int ch = (vuint8)text[f];
    //if (ch == '\r') { x = sx; continue; }
    if (ch == '\n') { x = sx; y += currFont->getHeight(); continue; }
    auto fc = currFont->getChar(ch);
    if (!fc) {
      //GLog.Logf(NAME_Debug, "NO CHAR #%d", ch);
      continue;
    }
    // draw char
    if (!currFont->singleTexture) {
      glBindTexture(GL_TEXTURE_2D, fc->tex->tid);
      glBegin(GL_QUADS);
      //GLog.Logf(NAME_Debug, "rebound texture (tid=%u) for char %d", fc->tex->tid, ch);
    }
    int xofs = fc->leftbear;
    glTexCoord2f(fc->tx0, fc->ty0); glVertex3f(x+xofs, y+fc->topofs, z);
    glTexCoord2f(fc->tx1, fc->ty0); glVertex3f(x+xofs+fc->width, y+fc->topofs, z);
    glTexCoord2f(fc->tx1, fc->ty1); glVertex3f(x+xofs+fc->width, y+fc->topofs+fc->height, z);
    glTexCoord2f(fc->tx0, fc->ty1); glVertex3f(x+xofs, y+fc->topofs+fc->height, z);
    if (!currFont->singleTexture) glEnd();
    // advance
    //x += fc->advance;
    x += fc->leftbear+fc->rightbear;
  }
  if (currFont->singleTexture) glEnd();
}


//==========================================================================
//
//  VGLVideo::drawTextAtTexture
//
//==========================================================================
void VGLVideo::drawTextAtTexture (VOpenGLTexture *tx, int x, int y, VStr text, const vuint32 color) {
  if (!currFont /*|| isFullyTransparent()*/ || text.isEmpty() || !tx || tx->width < 1 || tx->height < 1) return;
  //if (!mInited) return;

  const VOpenGLTexture *tex = nullptr;
  if (currFont->singleTexture) {
    tex = currFont->getTexture();
    if (!tex /*|| !tex->tid*/) return; // oops
  }

  //GLog.Logf(NAME_Debug, "!!!!");
  int sx = x;
  for (int f = 0; f < text.length(); ++f) {
    int ch = (vuint8)text[f];
    //if (ch == '\r') { x = sx; continue; }
    if (ch == '\n') { x = sx; y += currFont->getHeight(); continue; }
    auto fc = currFont->getChar(ch);
    if (!fc) {
      //GLog.Logf(NAME_Debug, "NO CHAR #%d", ch);
      continue;
    }
    // draw char
    if (!currFont->singleTexture) {
      tex = fc->tex;
      if (!tex) continue;
    }
    int xofs = fc->leftbear;
    int cx0 = (int)(fc->tx0*tex->width);
    int cy0 = (int)(fc->ty0*tex->height);
    int cx1 = (int)(fc->tx1*tex->width);
    int cy1 = (int)(fc->ty1*tex->height);
    //GLog.Logf(NAME_Debug, "  f=%d; ch=%c; (%d,%d)-(%d,%d) (%dx%d); (%g,%g)-(%g,%g)", f, (char)ch, cx0, cy0, cx1, cy1, (int)tex->width, (int)tex->height, fc->tx0, fc->ty0, fc->tx1, fc->ty1);
    for (int dy = cy0; dy < cy1; ++dy) {
      for (int dx = cx0; dx < cx1; ++dx) {
        auto clr = tex->img->getPixel(dx, dy);
        if (color && clr.a) {
          clr.r = clampToByte((int)(clr.r/255.0f*(((color>>16)&0xff)/255.0f)*255.0f));
          clr.g = clampToByte((int)(clr.g/255.0f*(((color>>8)&0xff)/255.0f)*255.0f));
          clr.b = clampToByte((int)(clr.b/255.0f*((color&0xff)/255.0f)*255.0f));
        }
        tx->img->setPixel(x+xofs+dx-cx0, y+fc->topofs+dy-cy0, clr);
      }
    }
    // advance
    //x += fc->advance;
    x += fc->leftbear+fc->rightbear;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_FUNCTION(VGLVideo, canInit) { RET_BOOL(VGLVideo::canInit()); }
IMPLEMENT_FUNCTION(VGLVideo, hasOpenGL) { RET_BOOL(VGLVideo::hasOpenGL()); }
IMPLEMENT_FUNCTION(VGLVideo, isInitialized) { RET_BOOL(VGLVideo::isInitialized()); }
IMPLEMENT_FUNCTION(VGLVideo, screenWidth) { RET_INT(VGLVideo::getWidth()); }
IMPLEMENT_FUNCTION(VGLVideo, screenHeight) { RET_INT(VGLVideo::getHeight()); }

IMPLEMENT_FUNCTION(VGLVideo, isMouseCursorVisible) { RET_BOOL(mInited ? SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE : true); }
IMPLEMENT_FUNCTION(VGLVideo, hideMouseCursor) { if (mInited) SDL_ShowCursor(SDL_DISABLE); }
IMPLEMENT_FUNCTION(VGLVideo, showMouseCursor) { if (mInited) SDL_ShowCursor(SDL_ENABLE); }

IMPLEMENT_FUNCTION(VGLVideo, get_frameTime) { RET_INT(VGLVideo::getFrameTime()); }
IMPLEMENT_FUNCTION(VGLVideo, set_frameTime) { int newft; vobjGetParam(newft); VGLVideo::setFrameTime(newft); VGLVideo::sendPing(); }

IMPLEMENT_FUNCTION(VGLVideo, get_dbgDumpOpenGLInfo) { RET_BOOL(VGLVideo::dbgDumpOpenGLInfo); }
IMPLEMENT_FUNCTION(VGLVideo, set_dbgDumpOpenGLInfo) { bool v; vobjGetParam(v); VGLVideo::dbgDumpOpenGLInfo = v; }

// native final static bool openScreen (string winname, int width, int height, optional int fullscreen);
IMPLEMENT_FUNCTION(VGLVideo, openScreen) {
  VStr wname;
  int wdt, hgt;
  VOptParamInt fs(0);
  vobjGetParam(wname, wdt, hgt, fs);
  RET_BOOL(VGLVideo::open(wname, wdt, hgt, fs));
}

IMPLEMENT_FUNCTION(VGLVideo, closeScreen) {
  VGLVideo::close();
  VGLVideo::sendPing();
}


// native final static void getRealWindowSize (out int w, out int h);
IMPLEMENT_FUNCTION(VGLVideo, getRealWindowSize) {
  int *w, *h;
  vobjGetParam(w, h);
  if (mInited) {
    //SDL_GetWindowSize(hw_window, w, h);
    SDL_GL_GetDrawableSize(hw_window, w, h);
    //GLog.Logf(NAME_Debug, "w=%d; h=%d", *w, *h);
  }
}


IMPLEMENT_FUNCTION(VGLVideo, runEventLoop) { VGLVideo::runEventLoop(); }

IMPLEMENT_FUNCTION(VGLVideo, clearScreen) {
  VOptParamUInt rgb(0);
  vobjGetParam(rgb);
  VGLVideo::clear(rgb);
}

IMPLEMENT_FUNCTION(VGLVideo, clearDepth) {
  VOptParamFloat val(1.0f);
  vobjGetParam(val);
  VGLVideo::clearDepth(val);
}

IMPLEMENT_FUNCTION(VGLVideo, clearStencil) {
  VOptParamUInt val(0);
  vobjGetParam(val);
  VGLVideo::clearStencil(val);
}

IMPLEMENT_FUNCTION(VGLVideo, clearColor) {
  VOptParamUInt rgb(0);
  vobjGetParam(rgb);
  VGLVideo::clearColor(rgb);
}


IMPLEMENT_FUNCTION(VGLVideo, requestQuit) {
  if (!VGLVideo::quitSignal) {
    if (!VGLVideo::quitSignal) VGLVideo::quitSignal = 1;
    VGLVideo::sendPing();
    //PostQuitEvent(0);
  }
}

IMPLEMENT_FUNCTION(VGLVideo, resetQuitRequest) {
  VGLVideo::quitSignal = 0;
}

IMPLEMENT_FUNCTION(VGLVideo, requestRefresh) {
  if (!VGLVideo::doRefresh) {
    VGLVideo::doRefresh = true;
    if (VGLVideo::getFrameTime() <= 0) VGLVideo::sendPing();
  }
}

IMPLEMENT_FUNCTION(VGLVideo, forceSwap) {
  if (!mInited) return;
  doGLSwap = false;
  SDL_GL_SwapWindow(hw_window);
}


//native final static bool glHasExtension (string extname);
IMPLEMENT_FUNCTION(VGLVideo, glHasExtension) {
  VStr extname;
  vobjGetParam(extname);
  if (extname.isEmpty() || !mInited) {
    RET_BOOL(false);
  } else {
    if (SDL_GL_ExtensionSupported(*extname)) {
      RET_BOOL(true);
    } else {
      RET_BOOL(false);
    }
  }
}


IMPLEMENT_FUNCTION(VGLVideo, get_glHasNPOT) {
  RET_BOOL(mInited ? hasNPOT : false);
}

IMPLEMENT_FUNCTION(VGLVideo, get_directMode) {
  RET_BOOL(directMode);
}

IMPLEMENT_FUNCTION(VGLVideo, set_directMode) {
  bool m;
  vobjGetParam(m);
  if (!mInited) { directMode = m; return; }
  if (m != directMode) {
    directMode = m;
    glDrawBuffer(m ? GL_FRONT : GL_BACK);
  }
}

IMPLEMENT_FUNCTION(VGLVideo, get_depthTest) {
  RET_BOOL(depthTest);
}

IMPLEMENT_FUNCTION(VGLVideo, set_depthTest) {
  bool m;
  vobjGetParam(m);
  if (!mInited) { depthTest = m; return; }
  if (m != depthTest) {
    depthTest = m;
    if (m) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
  }
}

IMPLEMENT_FUNCTION(VGLVideo, get_depthWrite) {
  RET_BOOL(depthWrite);
}

IMPLEMENT_FUNCTION(VGLVideo, set_depthWrite) {
  bool m;
  vobjGetParam(m);
  if (!mInited) { depthWrite = m; return; }
  if (m != depthWrite) {
    depthWrite = m;
    if (m) glDepthMask(GL_TRUE); else glDepthMask(GL_FALSE);
  }
}

IMPLEMENT_FUNCTION(VGLVideo, get_depthFunc) {
  RET_INT(depthFunc);
}

IMPLEMENT_FUNCTION(VGLVideo, set_depthFunc) {
  int v;
  vobjGetParam(v);
  if (v < 0 || v >= ZFunc_Max) return;
  if (!mInited) { depthFunc = v; return; }
  if (v != depthFunc) {
    depthFunc = v;
    realizeZFunc();
  }
}

IMPLEMENT_FUNCTION(VGLVideo, get_currZ) {
  RET_INT(currZ);
}

IMPLEMENT_FUNCTION(VGLVideo, get_currZFloat) {
  RET_FLOAT(currZFloat);
}

IMPLEMENT_FUNCTION(VGLVideo, set_currZ) {
  int z;
  vobjGetParam(z);
  if (z < 0) z = 0; else if (z > 65535) z = 65535;
  if (z == currZ) return;
  const float zNear = 1;
  const float zFar = 64;
  const float a = zFar/(zFar-zNear);
  const float b = zFar*zNear/(zNear-zFar);
  const float k = 1<<16;
  currZ = z;
  currZFloat = (k*b)/(z-(k*a));
}

IMPLEMENT_FUNCTION(VGLVideo, get_scissorEnabled) {
  if (mInited) {
    RET_BOOL((glIsEnabled(GL_SCISSOR_TEST) ? 1 : 0));
  } else {
    RET_BOOL(0);
  }
}

IMPLEMENT_FUNCTION(VGLVideo, set_scissorEnabled) {
  bool v;
  vobjGetParam(v);
  if (mInited) {
    if (v) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
  }
}

IMPLEMENT_FUNCTION(VGLVideo, copyScissor) {
  ScissorRect *s, *d;
  vobjGetParam(s, d);
  if (d) {
    if (s) {
      *d = *s;
    } else {
      d->x = d->y = 0;
      d->w = mWidth;
      d->h = mHeight;
      d->enabled = 0;
    }
  }
}

IMPLEMENT_FUNCTION(VGLVideo, getScissor) {
  ScissorRect *sr;
  vobjGetParam(sr);
  if (sr) {
    if (!mInited) { sr->x = sr->y = sr->w = sr->h = sr->enabled = 0; return; }
    sr->enabled = (glIsEnabled(GL_SCISSOR_TEST) ? 1 : 0);
    GLint scxywh[4];
    glGetIntegerv(GL_SCISSOR_BOX, scxywh);
    int y0 = mHeight-(scxywh[1]+scxywh[3]);
    int y1 = mHeight-scxywh[1]-1;
    sr->x = scxywh[0];
    sr->y = y0;
    sr->w = scxywh[2];
    sr->h = y1-y0+1;
  }
}

IMPLEMENT_FUNCTION(VGLVideo, setScissor) {
  VOptParamPtr<ScissorRect> sr;
  vobjGetParam(sr);
  if (!sr.isNull()) {
    if (!mInited) return;
    if (sr->enabled) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    //glScissor(sr->x0, mHeight-sr->y0-1, sr->x1, mHeight-sr->y1-1);
    int w = (sr->w > 0 ? sr->w : 0);
    int h = (sr->h > 0 ? sr->h : 0);
    int y1 = mHeight-sr->y-1;
    int y0 = mHeight-(sr->y+h);
    glScissor(sr->x, y0, w, y1-y0+1);
  } else {
    if (mInited) {
      glDisable(GL_SCISSOR_TEST);
      GLint scxywh[4];
      glGetIntegerv(GL_VIEWPORT, scxywh);
      glScissor(scxywh[0], scxywh[1], scxywh[2], scxywh[3]);
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// static native final void glPushMatrix ();
IMPLEMENT_FUNCTION(VGLVideo, glPushMatrix) { if (mInited) glPushMatrix(); }

// static native final void glPopMatrix ();
IMPLEMENT_FUNCTION(VGLVideo, glPopMatrix) { if (mInited) glPopMatrix(); }

// static native final void glLoadIdentity ();
IMPLEMENT_FUNCTION(VGLVideo, glLoadIdentity) { if (mInited) glLoadIdentity(); }

// static native final void glScale (float sx, float sy, optional float sz);
IMPLEMENT_FUNCTION(VGLVideo, glScale) {
  float x, y;
  VOptParamFloat z(1.0f);
  vobjGetParam(x, y, z);
  if (mInited) glScalef(x, y, z);
}

// static native final void glTranslate (float dx, float dy, optional float dz);
IMPLEMENT_FUNCTION(VGLVideo, glTranslate) {
  float x, y;
  VOptParamFloat z(0.0f);
  vobjGetParam(x, y, z);
  if (mInited) glTranslatef(x, y, z);
}

// static native final void glRotate (float angle, TVec axis);
IMPLEMENT_FUNCTION(VGLVideo, glRotate) {
  float angle;
  TVec axis;
  vobjGetParam(angle, axis);
  if (mInited) glRotatef(angle, axis.x, axis.y, axis.z);
}

//static native final void glMatrixMode (int GLMatrix mode);
IMPLEMENT_FUNCTION(VGLVideo, set_glMatrixMode) {
  int mode;
  vobjGetParam(mode);
  if (mInited) {
    switch (mode) {
      case GLMatrixMode_ModelView: glMatrixMode(GL_MODELVIEW); break;
      case GLMatrixMode_Projection: glMatrixMode(GL_PROJECTION); break;
    }
  }
}

//static native final void glGetMatrix (GLMatrix mode, ref TMatrix4 m);
IMPLEMENT_FUNCTION(VGLVideo, glGetMatrix) {
  int mode;
  VMatrix4 *mt;
  vobjGetParam(mode, mt);
  if (mInited) {
    switch (mode) {
      case GLMatrixMode_ModelView: glGetFloatv(GL_MODELVIEW_MATRIX, (*mt)[0]); break;
      case GLMatrixMode_Projection: glGetFloatv(GL_PROJECTION_MATRIX, (*mt)[0]); break;
      default: mt->SetIdentity(); break;
    }
  } else {
    mt->SetIdentity();
  }
}

//static native final void glSetMatrix (ref TMatrix4 m);
IMPLEMENT_FUNCTION(VGLVideo, glSetMatrix) {
  VMatrix4 *mt;
  vobjGetParam(mt);
  if (mInited) {
    glLoadMatrixf((*mt)[0]);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
//native final static void glSetup2D (optional bool bottomUp);
IMPLEMENT_FUNCTION(VGLVideo, glSetup2D) {
  VOptParamBool bottomUp(false);
  vobjGetParam(bottomUp);

  if (!mInited) return;

  forceColorMask();
  forceAlphaFunc();

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

  depthWrite = false;
  if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
  if (depthWrite) glDepthMask(GL_TRUE); else glDepthMask(GL_FALSE);
  realizeZFunc();
  glDisable(GL_CULL_FACE);
  //glDisable(GL_BLEND);
  //glEnable(GL_LINE_SMOOTH);
  if (smoothLine) glEnable(GL_LINE_SMOOTH); else glDisable(GL_LINE_SMOOTH);
  if (stencilEnabled) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  glStencilFunc(GL_ALWAYS, 0, 0xffffffff);

  glDisable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
  //glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  realiseGLColor(); // this will setup blending
  //glEnable(GL_BLEND);
  //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  int realw = mWidth, realh = mHeight;
  SDL_GL_GetDrawableSize(hw_window, &realw, &realh);

  //glViewport(0, 0, width, height);
  glViewport(0, 0, realw, realh);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  if (bottomUp) {
    glOrtho(0, realw, 0, realh, -zNear, -zFar);
  } else {
    glOrtho(0, realw, realh, 0, -zNear, -zFar);
  }
  mWidth = realw;
  mHeight = realh;

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

  in3dmode = false;
  glDisable(GL_CULL_FACE);

  glPolyOfsFactor = glPolyOfsUnit = 0.0f;
  glPolygonOffset(0.0f, 0.0f);
  glDisable(GL_POLYGON_OFFSET_FILL);
}


// ////////////////////////////////////////////////////////////////////////// //
//native final static void glSetup3D (float fov, optional float aspect);
IMPLEMENT_FUNCTION(VGLVideo, glSetup3D) {
  float fov;
  VOptParamFloat aspect(1);
  vobjGetParam(fov, aspect);

  if (fov < 1) fov = 90; else if (fov > 179) fov = 179;
  if (aspect < 0.1f) aspect = 0.1f;

  if (!mInited) return;

  forceColorMask();
  forceAlphaFunc();

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

  depthWrite = true;
  if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
  if (depthWrite) glDepthMask(GL_TRUE); else glDepthMask(GL_FALSE);
  realizeZFunc();
  glDisable(GL_CULL_FACE);
  //glDisable(GL_BLEND);
  //glEnable(GL_LINE_SMOOTH);
  if (smoothLine) glEnable(GL_LINE_SMOOTH); else glDisable(GL_LINE_SMOOTH);
  if (stencilEnabled) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  glStencilFunc(GL_ALWAYS, 0, 0xffffffff);

  glDisable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
  //glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  realiseGLColor(); // this will setup blending
  //glEnable(GL_BLEND);
  //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  int realw = mWidth, realh = mHeight;
  SDL_GL_GetDrawableSize(hw_window, &realw, &realh);

  //glViewport(0, 0, width, height);
  glViewport(0, 0, realw, realh);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  mWidth = realw;
  mHeight = realh;

  TClipBase clip_base;
  clip_base.setupViewport(mWidth, mHeight, fov, aspect);
  //refdef.fovx = clip_base.fovx;
  //refdef.fovy = clip_base.fovy;
  //refdef.drawworld = true;

  // normal projection
  VMatrix4 ProjMat;
  {
    glClearDepth(1.0f);
    depthFunc = ZFunc_LessEqual;
    glDepthFunc(GL_LEQUAL);
    ProjMat.SetIdentity();
    ProjMat[0][0] = 1.0f/clip_base.fovx;
    ProjMat[1][1] = 1.0f/clip_base.fovy;
    ProjMat[2][3] = -1.0f;
    ProjMat[3][3] = 0.0f;
    /*
    if (RendLev && RendLev->NeedsInfiniteFarClip && !HaveDepthClamp) {
      ProjMat[2][2] = -1.0f;
      ProjMat[3][2] = -2.0f;
    } else
    */
    const float maxdist = 8192.0f;
    {
      ProjMat[2][2] = -(maxdist+1.0f)/(maxdist-1.0f);
      ProjMat[3][2] = -2.0f*maxdist/(maxdist-1.0f);
    }
  }

  glMatrixMode(GL_PROJECTION);
  glLoadMatrixf(ProjMat[0]);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

  in3dmode = true;
  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);

  glPolyOfsFactor = glPolyOfsUnit = 0.0f;
  glPolygonOffset(0.0f, 0.0f);
  glDisable(GL_POLYGON_OFFSET_FILL);
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_FUNCTION(VGLVideo, set_glCullFace) {
  int mode;
  vobjGetParam(mode);
  if (mInited) {
    switch (mode) {
      case GLFaceCull_None: glDisable(GL_CULL_FACE); break;
      case GLFaceCull_Front: glEnable(GL_CULL_FACE); glCullFace(GL_FRONT); break;
      case GLFaceCull_Back: glEnable(GL_CULL_FACE); glCullFace(GL_BACK); break;
    }
  }
}

//native final static void glSetTexture (GLTexture tx, optional bool repeat); // `none` means disable texturing
IMPLEMENT_FUNCTION(VGLVideo, glSetTexture) {
  VGLTexture *tx;
  VOptParamBool repeat(false);
  vobjGetParam(tx, repeat);
  if (mInited) {
    if (tx && tx->tex) {
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, tx->tex->tid);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE));
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE));
      //glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
      VGLVideo::forceGLTexFilter();
    } else {
      glDisable(GL_TEXTURE_2D);
    }
    VGLVideo::setupBlending();
  }
}

//native final static void glSetColor (float r, float g, float b, optional float a/*=1*/);
IMPLEMENT_FUNCTION(VGLVideo, glSetColor) {
  VOptParamFloat a(1.0f);
  float r, g, b;
  vobjGetParam(r, g, b, a);
  r = clampval(r, 0.0f, 1.0f);
  g = clampval(g, 0.0f, 1.0f);
  b = clampval(b, 0.0f, 1.0f);
  a = clampval(b, 0.0f, 1.0f);
  if (mInited) glColor4f(r, g, b, a);
}

// native final static void glBegin (GLBegin mode, optional bool for2d/*=false*/);
IMPLEMENT_FUNCTION(VGLVideo, glBegin) {
  int mode;
  VOptParamBool for2d(false);
  vobjGetParam(mode, for2d);
  if (mInited) {
    if (for2d) {
      setupBlending();
      glDisable(GL_TEXTURE_2D);
    }
    switch (mode) {
      case GLBegin_Points: glBegin(GL_POINTS); break;
      case GLBegin_Lines: glBegin(GL_LINES); break;
      case GLBegin_LineStrip: glBegin(GL_LINE_STRIP); break;
      case GLBegin_LineLoop: glBegin(GL_LINE_LOOP); break;
      case GLBegin_Triangles: glBegin(GL_TRIANGLES); break;
      case GLBegin_TriangleStrip: glBegin(GL_TRIANGLE_STRIP); break;
      case GLBegin_TriangleFan: glBegin(GL_TRIANGLE_FAN); break;
      case GLBegin_Quads: glBegin(GL_QUADS); break;
      case GLBegin_QuadStrip: glBegin(GL_QUAD_STRIP); break;
      case GLBegin_Polygon: glBegin(GL_POLYGON); break;
      default: GLog.Log(NAME_Error, "invalid `glBegin()` argument"); break;
    }
  }
}

// native final static void glEnd ();
IMPLEMENT_FUNCTION(VGLVideo, glEnd) {
  if (mInited) glEnd();
}

// native final static void glVertex (TVec p, optional float s, optional float t);
IMPLEMENT_FUNCTION(VGLVideo, glVertex) {
  TVec p;
  VOptParamFloat s(0), t(0);
  vobjGetParam(p, s, t);
  if (mInited) {
    if (t.specified || s.specified) glTexCoord2f(s, t);
    glVertex3f(p.x, p.y, p.z);
  }
}


IMPLEMENT_FUNCTION(VGLVideo, get_glPolygonOffsetFactor) { RET_FLOAT(glPolyOfsFactor); }
IMPLEMENT_FUNCTION(VGLVideo, get_glPolygonOffsetUnits) { RET_FLOAT(glPolyOfsUnit); }

IMPLEMENT_FUNCTION(VGLVideo, glPolygonOffset) {
  float factor, unit;
  vobjGetParam(factor, unit);
  if (mInited && (factor != glPolyOfsFactor || unit != glPolyOfsUnit)) {
    glPolygonOffset(factor, unit);
    if (factor || unit) {
      glEnable(GL_POLYGON_OFFSET_FILL);
    } else {
      glDisable(GL_POLYGON_OFFSET_FILL);
    }
  }
  glPolyOfsFactor = factor;
  glPolyOfsUnit = unit;
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_FUNCTION(VGLVideo, set_glFogMode) {
  int mode;
  vobjGetParam(mode);
  if (mInited) {
    switch (mode) {
      //case FM_None:
      case FM_Linear: glFogi(GL_FOG_MODE, GL_LINEAR); glEnable(GL_FOG); glHint(GL_FOG_HINT, GL_NICEST); break;
      case FM_Exp: glFogi(GL_FOG_MODE, GL_EXP); glEnable(GL_FOG); glHint(GL_FOG_HINT, GL_NICEST); break;
      case FM_Exp2: glFogi(GL_FOG_MODE, GL_EXP2); glEnable(GL_FOG); glHint(GL_FOG_HINT, GL_NICEST); break;
      default: glDisable(GL_FOG); break;
    }
  }
}

IMPLEMENT_FUNCTION(VGLVideo, set_glFogDensity) {
  float v;
  vobjGetParam(v);
  if (mInited) glFogf(GL_FOG_DENSITY, v);
}

IMPLEMENT_FUNCTION(VGLVideo, set_glFogStart) {
  float v;
  vobjGetParam(v);
  if (mInited) glFogf(GL_FOG_START, v);
}

IMPLEMENT_FUNCTION(VGLVideo, set_glFogEnd) {
  float v;
  vobjGetParam(v);
  if (mInited) glFogf(GL_FOG_END, v);
}

// native final static void glSetFogColor (float r, float g, float b, optional float a);
IMPLEMENT_FUNCTION(VGLVideo, glSetFogColor) {
  VOptParamFloat a(0.0f);
  float r, g, b;
  vobjGetParam(r, g, b, a);
  r = clampval(r, 0.0f, 1.0f);
  g = clampval(g, 0.0f, 1.0f);
  b = clampval(b, 0.0f, 1.0f);
  a = clampval(b, 0.0f, 1.0f);
  GLfloat v[4] = { r, g, b, a };
  if (mInited) glFogfv(GL_FOG_COLOR, v);
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_FUNCTION(VGLVideo, get_smoothLine) {
  RET_BOOL(smoothLine);
}

IMPLEMENT_FUNCTION(VGLVideo, set_smoothLine) {
  bool v;
  vobjGetParam(v);
  if (smoothLine != v) {
    smoothLine = v;
    if (mInited) {
      if (v) glEnable(GL_LINE_SMOOTH); else glDisable(GL_LINE_SMOOTH);
    }
  }
}


// native final static AlphaFunc get_alphaTestFunc ()
IMPLEMENT_FUNCTION(VGLVideo, get_alphaTestFunc) {
  RET_INT(alphaTestFunc);
}

// native final static void set_alphaTestFunc (AlphaFunc v)
IMPLEMENT_FUNCTION(VGLVideo, set_alphaTestFunc) {
  int atf;
  vobjGetParam(atf);
  if (alphaTestFunc != atf) {
    alphaTestFunc = atf;
    forceAlphaFunc();
  }
}

//native final static float get_alphaTestVal ()
IMPLEMENT_FUNCTION(VGLVideo, get_alphaTestVal) {
  RET_FLOAT(alphaFuncVal);
}

// native final static void set_alphaTestVal (float v)
IMPLEMENT_FUNCTION(VGLVideo, set_alphaTestVal) {
  float v;
  vobjGetParam(v);
  if (alphaFuncVal != v) {
    alphaFuncVal = v;
    forceAlphaFunc();
  }
}


IMPLEMENT_FUNCTION(VGLVideo, get_realStencilBits) {
  RET_INT(stencilBits);
}

IMPLEMENT_FUNCTION(VGLVideo, get_framebufferHasAlpha) {
  int res;
  SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &res);
  RET_BOOL(res == 8);
}

IMPLEMENT_FUNCTION(VGLVideo, get_stencil) {
  RET_BOOL(stencilEnabled);
}

IMPLEMENT_FUNCTION(VGLVideo, set_stencil) {
  bool v;
  vobjGetParam(v);
  if (stencilEnabled != v) {
    stencilEnabled = v;
    if (mInited) {
      if (v) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
      //GLog.Logf(NAME_Debug, "stencil test: %d", (v ? 1 : 0));
    }
  }
}

//native final static void stencilOp (StencilOp sfail, StencilOp dpfail, optional StencilOp dppass);
IMPLEMENT_FUNCTION(VGLVideo, stencilOp) {
  int sfail, dpfail;
  VOptParamInt dppass(STC_Keep);
  vobjGetParam(sfail, dpfail, dppass);
  if (!dppass.specified) dppass = dpfail;
  if (mInited) {
    glStencilOp(convertStencilOp(sfail), convertStencilOp(dpfail), convertStencilOp(dppass));
  }
}

//native final static void stencilFunc (StencilFunc func, int refval, optional int mask);
IMPLEMENT_FUNCTION(VGLVideo, stencilFunc) {
  int func, refval;
  VOptParamUInt mask(0xffffffffU);
  vobjGetParam(func, refval, mask);
  if (mInited) {
    glStencilFunc(convertStencilFunc(func), refval, mask);
  }
}


//native final static int getColorARGB ();
IMPLEMENT_FUNCTION(VGLVideo, get_colorARGB) {
  RET_INT(colorARGB);
}

//native final static void setColorARGB (int v);
IMPLEMENT_FUNCTION(VGLVideo, set_colorARGB) {
  vuint32 c;
  vobjGetParam(c);
  setColor(c);
}

//native final static int getBlendMode ();
IMPLEMENT_FUNCTION(VGLVideo, get_blendMode) {
  RET_INT(getBlendMode());
}

//native final static void set_blendMode (int v);
IMPLEMENT_FUNCTION(VGLVideo, set_blendMode) {
  int c;
  vobjGetParam(c);
  setBlendMode(c);
}

//native final static int getBlendFunc ();
IMPLEMENT_FUNCTION(VGLVideo, get_blendFunc) {
  RET_INT(mBlendFunc);
}

//native final static void set_blendMode (int v);
IMPLEMENT_FUNCTION(VGLVideo, set_blendFunc) {
  int v;
  vobjGetParam(v);
  if (mBlendFunc != v) {
    mBlendFunc = v;
    forceBlendFunc();
  }
}


//native final static CMask get_colorMask ();
IMPLEMENT_FUNCTION(VGLVideo, get_colorMask) {
  RET_INT(colorMask);
}


//native final static void set_colorMask (CMask mask);
IMPLEMENT_FUNCTION(VGLVideo, set_colorMask) {
  int mask;
  vobjGetParam(mask);
  mask &= CMask_Red|CMask_Green|CMask_Blue|CMask_Alpha;
  if (mask != colorMask) {
    colorMask = mask;
    forceColorMask();
  }
}


//native final static bool get_textureFiltering ();
IMPLEMENT_FUNCTION(VGLVideo, get_textureFiltering) {
  RET_BOOL(getTexFiltering());
}

//native final static void set_textureFiltering (bool v);
IMPLEMENT_FUNCTION(VGLVideo, set_textureFiltering) {
  bool tf;
  vobjGetParam(tf);
  setTexFiltering(tf);
}


// ////////////////////////////////////////////////////////////////////////// //
// aborts if font cannot be loaded
//native final static void loadFontDF (name fname, string fnameIni, string fnameTexture);
IMPLEMENT_FUNCTION(VGLVideo, loadFontDF) {
  VName fname;
  VStr fnameIni;
  VStr fnameTexture;
  vobjGetParam(fname, fnameIni, fnameTexture);
  //GLog.Logf(NAME_Debug, "fname=<%s>; ini=<%s>; tx=<%s>", *fname, *fnameIni, *fnameTexture);
  if (VFont::findFont(fname)) return;
  VFont::LoadDF(fname, fnameIni, fnameTexture);
}


// aborts if font cannot be loaded
//native final static void loadFontPCF (name fname, string filename);
IMPLEMENT_FUNCTION(VGLVideo, loadFontPCF) {
  VName fontname;
  VStr filename;
  vobjGetParam(fontname, filename);
  if (VFont::findFont(fontname)) return;
  VFont::LoadPCF(fontname, filename);
}


struct FontCharInfo {
  int ch;
  //int width, height; // height may differ from font height
  //int advance; // horizontal advance to print next char
  //int topofs; // offset from font top (i.e. y+topofs should be used to draw char)
  int leftbear, rightbear; // positive means "more to the respective bbox side"
  int ascent, descent; // both are positive, which actually means "offset from baseline to the respective direction"
};


// native final static bool getCharInfo (int ch, out FontCharInfo ci); // returns `false` if char wasn't found
IMPLEMENT_FUNCTION(VGLVideo, getCharInfo) {
  FontCharInfo *fc;
  int ch;
  vobjGetParam(fc, ch);
  if (!fc) { RET_BOOL(false); return; }
  if (!currFont) {
    memset(fc, 0, sizeof(*fc));
    RET_BOOL(false);
    return;
  }
  auto fci = currFont->getChar(ch);
  if (!fci || fci->ch == 0) {
    memset(fc, 0, sizeof(*fc));
    RET_BOOL(false);
    return;
  }
  fc->ch = fci->ch;
  //fc->width = fci->width;
  //fc->height = fci->height;
  //fc->advance = fci->advance;
  //fc->topofs = fci->topofs;
  fc->leftbear = fci->leftbear;
  fc->rightbear = fci->rightbear;
  fc->ascent = fci->ascent;
  fc->descent = fci->descent;
  RET_BOOL(true);
}


//native final static void setFont (name fontname);
IMPLEMENT_FUNCTION(VGLVideo, set_fontName) {
  VName fontname;
  vobjGetParam(fontname);
  setFont(fontname);
}

//native final static name getFont ();
IMPLEMENT_FUNCTION(VGLVideo, get_fontName) {
  if (!currFont) {
    RET_NAME(NAME_None);
  } else {
    RET_NAME(currFont->getName());
  }
}

//native final static void fontHeight ();
IMPLEMENT_FUNCTION(VGLVideo, fontHeight) {
  RET_INT(currFont ? currFont->getHeight() : 0);
}

//native final static int spaceWidth ();
IMPLEMENT_FUNCTION(VGLVideo, spaceWidth) {
  RET_INT(currFont ? currFont->getSpaceWidth() : 0);
}

//native final static int charWidth (int ch);
IMPLEMENT_FUNCTION(VGLVideo, charWidth) {
  int ch;
  vobjGetParam(ch);
  RET_INT(currFont ? currFont->charWidth(ch) : 0);
}

//native final static int textWidth (string text);
IMPLEMENT_FUNCTION(VGLVideo, textWidth) {
  VStr text;
  vobjGetParam(text);
  RET_INT(currFont ? currFont->textWidth(text) : 0);
}

//native final static int textHeight (string text);
IMPLEMENT_FUNCTION(VGLVideo, textHeight) {
  VStr text;
  vobjGetParam(text);
  RET_INT(currFont ? currFont->textHeight(text) : 0);
}

//native final static void drawTextAt (int x, int y, string text);
IMPLEMENT_FUNCTION(VGLVideo, drawTextAt) {
  int x, y;
  VStr text;
  vobjGetParam(x, y, text);
  drawTextAt(x, y, text);
}


//native final static void drawTextAtTexture (GLTexture tx, int x, int y, string text, optional int color);
IMPLEMENT_FUNCTION(VGLVideo, drawTextAtTexture) {
  VGLTexture *tx;
  int x, y;
  VStr text;
  VOptParamInt color(0);
  vobjGetParam(tx, x, y, text, color);
  if (tx && tx->tex) drawTextAtTexture(tx->tex, x, y, text, color);
}


//native final static void drawLine (int x0, int y0, int x1, int y1);
IMPLEMENT_FUNCTION(VGLVideo, drawLine) {
  int x0, y0, x1, y1;
  vobjGetParam(x0, y0, x1, y1);
  if (!mInited /*|| isFullyTransparent()*/) return;
  setupBlending();
  glDisable(GL_TEXTURE_2D);
  const float z = VGLVideo::currZFloat;
  glBegin(GL_LINES);
    glVertex3f(x0+0.5f, y0+0.5f, z);
    glVertex3f(x1+0.5f, y1+0.5f, z);
  glEnd();
}


//native final static void drawRect (int x0, int y0, int w, int h);
IMPLEMENT_FUNCTION(VGLVideo, drawRect) {
  int x0, y0, w, h;
  vobjGetParam(x0, y0, w, h);
  if (!mInited /*|| isFullyTransparent()*/ || w < 1 || h < 1) return;
  setupBlending();
  glDisable(GL_TEXTURE_2D);
  const float z = currZFloat;
  glBegin(GL_LINE_LOOP);
    glVertex3f(x0+0+0.5f, y0+0+0.5f, z);
    glVertex3f(x0+w-0.5f, y0+0+0.5f, z);
    glVertex3f(x0+w-0.5f, y0+h-0.5f, z);
    glVertex3f(x0+0+0.5f, y0+h-0.5f, z);
  glEnd();
}


//native final static void fillRect (int x0, int y0, int w, int h);
IMPLEMENT_FUNCTION(VGLVideo, fillRect) {
  int x0, y0, w, h;
  vobjGetParam(x0, y0, w, h);
  if (!mInited /*|| isFullyTransparent()*/ || w < 1 || h < 1) return;
  setupBlending();
  glDisable(GL_TEXTURE_2D);

  // no need for 0.5f here, or rect will be offset
  const float z = currZFloat;
  glBegin(GL_QUADS);
    glVertex3f(x0+0, y0+0, z);
    glVertex3f(x0+w, y0+0, z);
    glVertex3f(x0+w, y0+h, z);
    glVertex3f(x0+0, y0+h, z);
  glEnd();
}

// native final static bool getMousePos (out int x, out int y)
IMPLEMENT_FUNCTION(VGLVideo, getMousePos) {
  int *xp, *yp;
  vobjGetParam(xp, yp);
  getMousePosition(xp, yp);
  RET_BOOL(mInited);
}


IMPLEMENT_FUNCTION(VGLVideo, get_swapInterval) {
  RET_INT(swapInterval);
}

IMPLEMENT_FUNCTION(VGLVideo, set_swapInterval) {
  int si;
  vobjGetParam(si);
  if (si < 0) si = -1; else if (si > 0) si = 1;
  if (!mInited) { swapInterval = si; return; }
  if (SDL_GL_SetSwapInterval(si) == -1) { if (si < 0) { si = 1; SDL_GL_SetSwapInterval(1); } }
  swapInterval = si;
}

// ////////////////////////////////////////////////////////////////////////// //
static VStr readLine (VStream *strm, bool allTrim=true) {
  if (!strm || strm->IsError()) return VStr();
  VStr res;
  while (!strm->AtEnd()) {
    char ch;
    strm->Serialize(&ch, 1);
    if (strm->IsError()) return VStr();
    if (ch == '\r') {
      if (!strm->AtEnd()) {
        strm->Serialize(&ch, 1);
        if (strm->IsError()) return VStr();
        if (ch != '\n') strm->Seek(strm->Tell()-1);
      }
      break;
    }
    if (ch == '\n') break;
    if (ch == 0) ch = ' ';
    res += ch;
  }
  if (allTrim) {
    while (!res.isEmpty() && (vuint8)res[0] <= ' ') res.chopLeft(1);
    while (!res.isEmpty() && (vuint8)res[res.length()-1] <= ' ') res.chopRight(1);
  }
  return res;
}


static VStr getKey (VStr s) {
  int epos = s.indexOf('=');
  if (epos < 0) return s;
  VStr res = s.left(epos);
  while (!res.isEmpty() && (vuint8)res[res.length()-1] <= ' ') res.chopRight(1);
  return res;
}


static VStr getValue (VStr s) {
  int epos = s.indexOf('=');
  if (epos < 0) return VStr();
  VStr res = s;
  res.chopLeft(epos+1);
  while (!res.isEmpty() && (vuint8)res[0] <= ' ') res.chopLeft(1);
  while (!res.isEmpty() && (vuint8)res[res.length()-1] <= ' ') res.chopRight(1);
  return res;
}


static int getIntValue (VStr s) {
  VStr v = getValue(s);
  if (v.isEmpty()) return 0;
  bool neg = v.startsWith("-");
  if (neg) v.chopLeft(1);
  int res = 0;
  for (int f = 0; f < v.length(); ++f) {
    int d = VStr::digitInBase(v[f]);
    if (d < 0) break;
    res = res*10+d;
  }
  if (neg) res = -res;
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
VFont *VFont::fontList;


//==========================================================================
//
//  VFont::findFont
//
//==========================================================================
VFont *VFont::findFont (VName name) noexcept {
  for (VFont *cur = fontList; cur; cur = cur->next) if (cur->name == name) return cur;
  return nullptr;
}


//==========================================================================
//
//  VFont::VFont
//
//==========================================================================
VFont::VFont ()
  : name(NAME_None)
  , next(nullptr)
  , tex(nullptr)
  , singleTexture(true)
  , chars()
  , spaceWidth(0)
  , fontHeight(0)
  , minWidth(0)
  , maxWidth(0)
  , avgWidth(0)
{
}


//==========================================================================
//
//  VFont::~VFont
//
//==========================================================================
VFont::~VFont() {
  if (singleTexture) {
    if (tex) tex->release();
  } else {
    for (int f = 0; f < chars.length(); ++f) {
      FontChar &fc = chars[f];
      if (fc.tex) fc.tex->release();
    }
  }
  VFont *prev = nullptr, *cur = fontList;
  while (cur && cur != this) { prev = cur; cur = cur->next; }
  if (cur) {
    if (prev) prev->next = next; else fontList = next;
  }
}


//==========================================================================
//
//  VFont::LoadDF
//
//==========================================================================
VFont *VFont::LoadDF (VName aname, VStr fnameIni, VStr fnameTexture) {
  VOpenGLTexture *tex = VOpenGLTexture::Load(fnameTexture);
  if (!tex) Sys_Error("cannot load font '%s' (texture not found)", *aname);

  auto inif = fsysOpenFileAnyExt(fnameIni);
  if (!inif) { tex->release(); tex = nullptr; Sys_Error("cannot load font '%s' (description not found)", *aname); }
  //GLog.Logf(NAME_Debug, "*** %d %d %d %d", (int)inif->AtEnd(), (int)inif->IsError(), inif->TotalSize(), inif->Tell());

  VStr currSection;

  int cwdt = -1, chgt = -1, kern = 0;
  int xwidth[256];
  memset(xwidth, 0, sizeof(xwidth));

  // parse ini file
  while (!inif->AtEnd()) {
    VStr line = readLine(inif);
    if (inif->IsError()) { delete inif; tex->release(); tex = nullptr; Sys_Error("cannot load font '%s' (error loading description)", *aname); }
    if (line.isEmpty() || line[0] == ';' || line.startsWith("//")) continue;
    if (line[0] == '[') { currSection = line; continue; }
    // fontmap?
    auto key = getKey(line);
    //GLog.Logf(NAME_Debug, "line:<%s>; key:<%s>; intval=%d", *line, *key, getIntValue(line));
    if (currSection.equ1251CI("[FontMap]")) {
      if (key.equ1251CI("CharWidth")) { cwdt = getIntValue(line); continue; }
      if (key.equ1251CI("CharHeight")) { chgt = getIntValue(line); continue; }
      if (key.equ1251CI("Kerning")) { kern = getIntValue(line); continue; }
      continue;
    }
    if (currSection.length() < 2 || VStr::digitInBase(currSection[1]) < 0 || !currSection.endsWith("]")) continue;
    if (!key.equ1251CI("Width")) continue;
    int cidx = 0;
    for (int f = 1; f < currSection.length(); ++f) {
      int d = VStr::digitInBase(currSection[f]);
      if (d < 0) {
        if (f != currSection.length()-1) cidx = -1;
        break;
      }
      cidx = cidx*10+d;
    }
    if (cidx >= 0 && cidx < 256) {
      int w = getIntValue(line);
      //GLog.Logf(NAME_Debug, "cidx=%d; w=%d", cidx, w);
      if (w < 0) w = 0;
      xwidth[cidx] = w;
    }
  }

  delete inif;

  if (cwdt < 1 || chgt < 1) { tex->release(); tex = nullptr; Sys_Error("cannot load font '%s' (invalid description 00)", *aname); }
  int xchars = tex->width/cwdt;
  int ychars = tex->height/chgt;
  if (xchars < 1 || ychars < 1 || xchars*ychars < 128) { tex->release(); tex = nullptr; Sys_Error("cannot load font '%s' (invalid description 01)", *aname); }

  VFont *fnt = new VFont();
  fnt->name = aname;
  fnt->singleTexture = true;
  fnt->tex = tex;

  fnt->chars.setLength(xchars*ychars);

  fnt->fontHeight = chgt;
  int totalWdt = 0, maxWdt = -0x7fffffff, minWdt = 0x7fffffff, totalCount = 0;

  for (int cx = 0; cx < xchars; ++cx) {
    for (int cy = 0; cy < ychars; ++cy) {
      FontChar &fc = fnt->chars[cy*xchars+cx];
      fc.ch = cy*xchars+cx;
      fc.width = cwdt;
      fc.height = chgt;
      //fc.advance = xwidth[fc.ch]+kern;
      fc.leftbear = 0;
      fc.rightbear = xwidth[fc.ch]+kern;
      if (minWdt > cwdt) minWdt = cwdt;
      if (maxWdt < cwdt) maxWdt = cwdt;
      totalWdt += cwdt;
      ++totalCount;
      if (fc.ch == 32) fnt->spaceWidth = fc.leftbear+fc.rightbear;
      fc.topofs = 0;
      fc.tx0 = (float)(cx*cwdt)/(float)tex->getWidth();
      fc.ty0 = (float)(cy*chgt)/(float)tex->getHeight();
      fc.tx1 = (float)(cx*cwdt+cwdt)/(float)tex->getWidth();
      fc.ty1 = (float)(cy*chgt+chgt)/(float)tex->getHeight();
      fc.tex = tex;
    }
  }

  fnt->minWidth = minWdt;
  fnt->maxWidth = maxWdt;
  fnt->avgWidth = (totalCount ? totalWdt/totalCount : 0);

  fnt->next = fontList;
  fontList = fnt;

  return fnt;
}


//==========================================================================
//
//  VFont::LoadPCF
//
//==========================================================================
VFont *VFont::LoadPCF (VName aname, VStr filename) {
/*
  static const vuint16 cp12512Uni[128] = {
    0x0402,0x0403,0x201A,0x0453,0x201E,0x2026,0x2020,0x2021,0x20AC,0x2030,0x0409,0x2039,0x040A,0x040C,0x040B,0x040F,
    0x0452,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0x003F,0x2122,0x0459,0x203A,0x045A,0x045C,0x045B,0x045F,
    0x00A0,0x040E,0x045E,0x0408,0x00A4,0x0490,0x00A6,0x00A7,0x0401,0x00A9,0x0404,0x00AB,0x00AC,0x00AD,0x00AE,0x0407,
    0x00B0,0x00B1,0x0406,0x0456,0x0491,0x00B5,0x00B6,0x00B7,0x0451,0x2116,0x0454,0x00BB,0x0458,0x0405,0x0455,0x0457,
    0x0410,0x0411,0x0412,0x0413,0x0414,0x0415,0x0416,0x0417,0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E,0x041F,
    0x0420,0x0421,0x0422,0x0423,0x0424,0x0425,0x0426,0x0427,0x0428,0x0429,0x042A,0x042B,0x042C,0x042D,0x042E,0x042F,
    0x0430,0x0431,0x0432,0x0433,0x0434,0x0435,0x0436,0x0437,0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E,0x043F,
    0x0440,0x0441,0x0442,0x0443,0x0444,0x0445,0x0446,0x0447,0x0448,0x0449,0x044A,0x044B,0x044C,0x044D,0x044E,0x044F,
  };
*/

  auto fl = fsysOpenFile(filename);
  if (!fl) { Sys_Error("cannot load font '%s'", *aname); }

  PcfFont pcf;
  if (!pcf.load(*fl)) { Sys_Error("invalid PCF font '%s'", *aname); }

  VFont *fnt = new VFont();
  fnt->name = aname;
  fnt->singleTexture = false;
  fnt->tex = nullptr;

  fnt->chars.setLength(256);
  for (int f = 0; f < fnt->chars.length(); ++f) fnt->chars[f].ch = -1;

  fnt->fontHeight = pcf.fntAscent+pcf.fntDescent;
  fnt->spaceWidth = -1;

  int totalWdt = 0, maxWdt = -0x7fffffff, minWdt = 0x7fffffff, totalCount = 0;

  for (int gidx = 0; gidx < pcf.glyphs.length(); ++gidx) {
    PcfFont::Glyph &gl = pcf.glyphs[gidx];
    int chidx = -1;
    if (gl.codepoint < 128) {
      chidx = (int)gl.codepoint;
    } else {
      //for (unsigned f = 0; f < 128; ++f) if (cp12512Uni[f] == gl.codepoint) { chidx = (int)f+128; break; }
      if (gl.codepoint > 255) continue;
      chidx = (int)gl.codepoint;
    }
    if (chidx < 0) continue; // unused glyph
    //GLog.Logf(NAME_Debug, "gidx=%d; codepoint=%u; chidx=%d", gidx, gl.codepoint, chidx);
    FontChar &fc = fnt->chars[chidx];
    if (fc.ch >= 0) continue; // duplicate glyph

    fc.tex = pcf.createGlyphTexture(gidx);
    if (!fc.tex) continue;

    fc.ch = chidx;
    fc.tx0 = fc.ty0 = 0;
    fc.tx1 = fc.ty1 = 1;

    fc.width = fc.tex->img->width;
    fc.height = fc.tex->img->height;
    //fc.advance = gl.metrics.leftSidedBearing+gl.metrics.width+gl.metrics.rightSideBearing;
    fc.topofs = pcf.fntAscent-gl.metrics.ascent;
    fc.leftbear = gl.metrics.leftSidedBearing;
    fc.rightbear = gl.metrics.rightSideBearing;
    fc.ascent = gl.metrics.ascent;
    fc.descent = gl.metrics.descent;

    if (fc.ch == 32) fnt->spaceWidth = fc.leftbear+fc.rightbear;

    if (minWdt > gl.metrics.width) minWdt = gl.metrics.width;
    if (maxWdt < gl.metrics.width) maxWdt = gl.metrics.width;
    totalWdt += gl.metrics.width;
    ++totalCount;

    /*
    VGLTexture *ifile = SpawnWithReplace<VGLTexture>();
    ifile->tex = fc.tex;
    ifile->id = vcGLAllocId(ifile);
    */

    fc.tex->update();
  }

  fnt->minWidth = minWdt;
  fnt->maxWidth = maxWdt;
  fnt->avgWidth = (totalCount ? totalWdt/totalCount : 0);

  if (fnt->spaceWidth < 0) fnt->spaceWidth = fnt->avgWidth;

  fnt->next = fontList;
  fontList = fnt;

  return fnt;
}


//==========================================================================
//
//  VFont::GetChar
//
//==========================================================================
const VFont::FontChar *VFont::getChar (int ch) const noexcept {
  //GLog.Logf(NAME_Debug, "GET CHAR #%d (%d); internal:%d", ch, chars.length(), chars[ch].ch);
  if (ch < 0) return nullptr;
  if (ch < 0 || ch >= chars.length()) {
    ch = VStr::upcase1251(ch);
    if (ch < 0 || ch >= chars.length()) return nullptr;
  }
  return (chars[ch].ch >= 0 ? &chars[ch] : nullptr);
}


//==========================================================================
//
//  VFont::charWidth
//
//==========================================================================
int VFont::charWidth (int ch) const noexcept {
  auto fc = getChar(ch);
  return (fc ? fc->width : 0);
}


//==========================================================================
//
//  VFont::textWidth
//
//==========================================================================
int VFont::textWidth (VStr s) const noexcept {
  int res = 0, curwdt = 0;
  for (int f = 0; f < s.length(); ++f) {
    vuint8 ch = vuint8(s[f]);
    if (ch == '\n') {
      if (res < curwdt) res = curwdt;
      curwdt = 0;
      continue;
    }
    auto fc = getChar(ch);
    if (fc) curwdt += fc->leftbear+fc->rightbear;
  }
  if (res < curwdt) res = curwdt;
  return res;
}


//==========================================================================
//
//  VFont::textHeight
//
//==========================================================================
int VFont::textHeight (VStr s) const noexcept {
  int res = fontHeight;
  for (int f = 0; f < s.length(); ++f) if (s[f] == '\n') res += fontHeight;
  return res;
}


#endif

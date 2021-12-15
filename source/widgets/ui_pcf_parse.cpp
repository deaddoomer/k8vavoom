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
//** PCF font parser, directly included in "ui_font.cpp"
//**************************************************************************
class PcfFont {
public:
  static constexpr unsigned PCF_PROPERTIES = 1u<<0;
  static constexpr unsigned PCF_ACCELERATORS = 1u<<1;
  static constexpr unsigned PCF_METRICS = 1u<<2;
  static constexpr unsigned PCF_BITMAPS = 1u<<3;
  static constexpr unsigned PCF_INK_METRICS = 1u<<4;
  static constexpr unsigned PCF_BDF_ENCODINGS = 1u<<5;
  static constexpr unsigned PCF_SWIDTHS = 1u<<6;
  static constexpr unsigned PCF_GLYPH_NAMES = 1u<<7;
  static constexpr unsigned PCF_BDF_ACCELERATORS = 1u<<8;

  static constexpr unsigned PCF_DEFAULT_FORMAT = 0x00000000u;
  static constexpr unsigned PCF_INKBOUNDS = 0x00000200u;
  static constexpr unsigned PCF_ACCEL_W_INKBOUNDS = 0x00000100u;
  static constexpr unsigned PCF_COMPRESSED_METRICS = 0x00000100u;

  static constexpr unsigned PCF_GLYPH_PAD_MASK = 3u<<0; // See the bitmap table for explanation
  static constexpr unsigned PCF_BYTE_MASK = 1u<<2; // If set then Most Sig Byte First
  static constexpr unsigned PCF_BIT_MASK = 1u<<3; // If set then Most Sig Bit First
  static constexpr unsigned PCF_SCAN_UNIT_MASK = 3u<<4; // See the bitmap table for explanation

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
  };

  struct TocEntry {
    vuint32 type; // See below, indicates which table
    vuint32 format; // See below, indicates how the data are formatted in the table
    vuint32 size; // In bytes
    vuint32 offset; // from start of file

    inline bool isTypeCorrect () const noexcept {
      if (type == 0) return false;
      if (type&((~1u)<<8)) return false;
      bool was1 = false;
      for (unsigned shift = 0; shift < 9; ++shift) {
        if (type&(1u<<shift)) {
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
    vuint8* bitmap; // may be greater than necessary; shared between all glyphs; don't destroy
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

    bool getPixel (int x, int y) const noexcept {
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

  vuint8 *createGlyphByteTexture (int gidx, vuint8 clrEmpty, vuint8 clrFull, int *wdt, int *hgt) noexcept;

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

  inline void clear () noexcept { clearInternal(); }

  bool load (VStream &fl);
};


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
//  PcfFont::createGlyphByteTexture
//
//==========================================================================
vuint8 *PcfFont::createGlyphByteTexture (int gidx, vuint8 clrEmpty, vuint8 clrFull, int *wdt, int *hgt) noexcept {
  Glyph &gl = glyphs[gidx];
  //!fprintf(stderr, "glyph #%d (ch=%u); wdt=%d; hgt=%d\n", gidx, gl.codepoint, gl.bmpWidth(), gl.bmpHeight());
  if (gl.bmpWidth() < 1 || gl.bmpHeight() < 1) {
    if (wdt) *wdt = 0;
    if (hgt) *hgt = 0;
    return nullptr;
  }

  if (wdt) *wdt = gl.bmpWidth();
  if (hgt) *hgt = gl.bmpHeight();
  vuint8 *bmp = (vuint8 *)Z_Malloc(gl.bmpWidth()*gl.bmpHeight());
  vuint8 *dst = bmp;
  for (int dy = 0; dy < gl.bmpHeight(); ++dy) {
    for (int dx = 0; dx < gl.bmpWidth(); ++dx) {
      bool pix = gl.getPixel(dx, dy);
      *dst++ = (pix ? clrFull : clrEmpty);
    }
  }

  return bmp;
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
bool PcfFont::load (VStream &fl) {
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

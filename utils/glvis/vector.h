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

// ////////////////////////////////////////////////////////////////////////// //
// 2d vector
class TVec {
public:
  double x;
  double y;

  TVec () {}
  TVec (double Ax, double Ay) { x = Ax; y = Ay; }
  TVec (const double f[2]) { x = f[0]; y = f[1]; }

  inline const double &operator[] (int i) const { return (&x)[i]; }
  inline double &operator[] (int i) { return (&x)[i]; }

  inline TVec &operator += (const TVec &v) { x += v.x; y += v.y; return *this; }
  inline TVec &operator -= (const TVec &v) { x -= v.x; y -= v.y; return *this; }
  inline TVec &operator *= (double scale) { x *= scale; y *= scale; return *this; }
  inline TVec &operator /= (double scale) { x /= scale; y /= scale; return *this; }
  inline TVec operator + () const { return *this; }
  inline TVec operator - () const { return TVec(-x, -y); }
  inline double Length () const { return sqrt(x*x+y*y); }
};

inline TVec operator + (const TVec &v1, const TVec &v2) { return TVec(v1.x+v2.x, v1.y+v2.y); }
inline TVec operator - (const TVec &v1, const TVec &v2) { return TVec(v1.x-v2.x, v1.y-v2.y); }
inline TVec operator * (const TVec &v, double s) { return TVec(s*v.x, s*v.y); }
inline TVec operator * (double s, const TVec &v) { return TVec(s*v.x, s*v.y); }
inline TVec operator / (const TVec &v, double s) { return TVec(v.x/s, v.y/s); }
inline bool operator == (const TVec &v1, const TVec &v2) { return (v1.x == v2.x && v1.y == v2.y); }
inline bool operator != (const TVec &v1, const TVec &v2) { return (v1.x != v2.x || v1.y != v2.y); }
inline double Length (const TVec &v) { return sqrt(v.x*v.x+v.y*v.y); }
inline TVec Normalise (const TVec &v) { return v/v.Length(); }
inline double DotProduct (const TVec &v1, const TVec &v2) { return v1.x*v2.x+v1.y*v2.y; }


// ////////////////////////////////////////////////////////////////////////// //
// 2d plane (lol)
class TPlane {
public:
  TVec normal;
  double dist;

  inline void Set (const TVec &Anormal, double Adist) { normal = Anormal; dist = Adist; }

  // Initialises vertical plane from point and direction
  inline void SetPointDir (const TVec &point, const TVec &dir) {
    normal = Normalise(TVec(dir.y, -dir.x));
    dist = DotProduct(point, normal);
  }

  // Initialises vertical plane from 2 points
  inline void Set2Points (const TVec &v1, const TVec &v2) {
    SetPointDir(v1, v2-v1);
  }
};

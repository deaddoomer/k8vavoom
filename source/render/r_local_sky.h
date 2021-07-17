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
#ifndef VAVOOM_R_LOCAL_HEADER_SKY
#define VAVOOM_R_LOCAL_HEADER_SKY


// ////////////////////////////////////////////////////////////////////////// //
class VSky {
public:
  enum {
    VDIVS = 8,
    HDIVS = 16
  };

  sky_t sky[HDIVS*VDIVS];
  int NumSkySurfs;
  int SideTex;
  bool bIsSkyBox;
  bool SideFlip;
  bool WasOldSky;
  bool WasOldFlip;

private:
  void ClearSkyT (const bool keepOffset) noexcept;

public:
  void InitOldSky (int Sky1Texture, int Sky2Texture,
                   float Sky1ScrollDelta, float Sky2ScrollDelta,
                   bool DoubleSky, bool ForceNoSkyStretch, bool Flip,
                   bool keepOffset);
  void InitSkyBox (VName Name1, VName Name2, bool keepOffset);
  void Init (int Sky1Texture, int Sky2Texture,
             float Sky1ScrollDelta, float Sky2ScrollDelta,
             bool DoubleSky, bool ForceNoSkyStretch, bool Flip, bool Lightning,
             bool keepOffset);
  void Draw (int ColorMap);
};


// ////////////////////////////////////////////////////////////////////////// //
class VSkyPortal : public VPortal {
public:
  VSky *Sky;

public:
  inline VSkyPortal (VRenderLevelShared *ARLev, VSky *ASky) noexcept : VPortal(ARLev), Sky(ASky) {}

  virtual bool NeedsDepthBuffer () const noexcept override;
  virtual bool IsSky () const noexcept override;

  virtual bool MatchSky (VSky *) const noexcept override;

  virtual void DrawContents () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VSkyBoxPortal : public VPortal {
public:
  VEntity *Viewport;

public:
  inline VSkyBoxPortal (VRenderLevelShared *ARLev, VEntity *AViewport) noexcept : VPortal(ARLev), Viewport(AViewport) {}

  virtual bool IsSky () const noexcept override;
  virtual bool IsSkyBox () const noexcept override;

  virtual bool MatchSkyBox (VEntity *) const noexcept override;

  virtual void DrawContents () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VSectorStackPortal : public VPortal {
public:
  VEntity *Viewport;

public:
  // no bbox for now
  inline VSectorStackPortal (VRenderLevelShared *ARLev, VEntity *AViewport) noexcept : VPortal(ARLev), Viewport(AViewport) { needBBox = false; }

  virtual bool IsStack () const noexcept override;

  virtual bool MatchSkyBox (VEntity *) const noexcept override;

  virtual void DrawContents () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VMirrorPortal : public VPortal {
public:
  TPlane Plane;

public:
  // no bbox for now
  inline VMirrorPortal (VRenderLevelShared *ARLev, const TPlane *APlane) : VPortal(ARLev), Plane(*APlane) { needBBox = false; }

  virtual bool IsMirror () const noexcept override;

  virtual bool MatchMirror (const TPlane *) const noexcept override;

  virtual void DrawContents () override;
};


#endif

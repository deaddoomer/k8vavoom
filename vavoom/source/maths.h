//**************************************************************************
//**
//**	##   ##    ##    ##   ##   ####     ####   ###     ###
//**	##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**	 ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**	 ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**	  ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**	   #    ##    ##    #      ####     ####   ##       ##
//**
//**	Copyright (C) 1999-2001 J�nis Legzdi��
//**
//**	This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**	This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**	
//**************************************************************************

#undef MIN
#undef MAX
#undef MID
#define MIN(x, y)		((x) < (y) ? (x) : (y))
#define MAX(x, y)		((x) > (y) ? (x) : (y))
#define MID(min, val, max)	MAX(min, MIN(val, max))

//==========================================================================
//
//	Fixed point, 32bit as 16.16.
//
//==========================================================================

#define FRACBITS		16
#define FRACUNIT		(1<<FRACBITS)

typedef int fixed_t;

#define FL(x)	((float)(x) / (float)FRACUNIT)
#define FX(x)	(fixed_t)((x) * FRACUNIT)

//==========================================================================
//
//	Angles
//
//==========================================================================

// Binary Angle Measument, BAM.
#define ANG1			(ANG45 / 45)
#define ANG45			0x20000000
#define ANG90			0x40000000
#define ANG180			0x80000000
#define ANG270			0xc0000000

#ifndef M_PI
#define M_PI			3.14159265358979323846
#endif

#define _DEG2RAD		0.017453292519943296
#define _RAD2DEG		57.2957795130823209
#define _BAM2DEG		8.38190317153931e-08
#define _DEG2BAM		11930464.711111111
#define _BAM2RAD		1.46291807926716e-09
#define _RAD2BAM		683565275.57643159

#define DEG2RAD(a)		((a) * _DEG2RAD)
#define RAD2DEG(a)		((a) * _RAD2DEG)
#define BAM2DEG(a)		((double)(a) * _BAM2DEG)
#define DEG2BAM(a)		((angle_t)((a) * _DEG2BAM))
#define BAM2RAD(a)		((double)(a) * _BAM2RAD)
#define RAD2BAM(a)		((angle_t)((a) * _RAD2BAM))

typedef unsigned angle_t;

class TAVec
{
 public:
	angle_t		pitch;
	angle_t		yaw;
	angle_t		roll;
};

// HEADER FILES ------------------------------------------------------------

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int mlog2(int val);

void AngleVectors(const TAVec &angles, TVec &forward, TVec &right, TVec &up);
void AngleVector(const TAVec &angles, TVec &forward);
void VectorAngles(const TVec &vec, TAVec &angles);

// PUBLIC DATA DECLARATIONS ------------------------------------------------

inline float msin(angle_t angle)
{
	return sin(BAM2RAD(angle));
}

inline float mcos(angle_t angle)
{
	return cos(BAM2RAD(angle));
}

inline float mtan(angle_t angle)
{
	return tan(BAM2RAD(angle));
}

inline angle_t matan(float y, float x)
{
	return RAD2BAM(atan2(y, x));
}

//==========================================================================
//
//								PLANES
//
//==========================================================================

class TPlane
{
 public:
	TVec		normal;
	float		dist;

	void Set(const TVec &Anormal, float Adist)
	{
		normal = Anormal;
		dist = Adist;
	}

	//	Initializes vertical plane from point and direction
	void SetPointDir(const TVec &point, const TVec &dir)
	{
		normal = Normalize(TVec(dir.y, -dir.x, 0));
		dist = DotProduct(point, normal);
	}

	//	Initializes vertical plane from 2 points
	void Set2Points(const TVec &v1, const TVec &v2)
	{
		SetPointDir(v1, v2 - v1);
	}

	//	Get z of point with given x and y coords
	// Don't try to use it on a vertical plane
	float GetPointZ(float x, float y) const
	{
		return (dist - normal.x * x - normal.y * y) / normal.z;
	}

	float GetPointZ(const TVec &v) const
	{
		return GetPointZ(v.x, v.y);
	}

	//	Returns side 0 (front) or 1 (back).
	int PointOnSide(const TVec &point)
	{
		return DotProduct(point, normal) - dist < 0;
	}
};


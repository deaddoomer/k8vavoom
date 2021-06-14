#ifndef __WORKDATA_H__
#define __WORKDATA_H__

#ifdef _MSC_VER
#pragma once
#endif

namespace ZDBSP {

struct vertex_t
{
  fixed_t x, y;
};

struct node_t
{
  fixed_t x, y, dx, dy;
  fixed_t bbox[2][4];
  unsigned int intchildren[2];
};

struct subsector_t
{
  DWORD numlines;
  DWORD firstline;
};

} // namespace

#endif //__WORKDATA_H__

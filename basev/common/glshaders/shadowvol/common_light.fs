//#define SHADOWVOL_COMMON_LIGHT_NEW
// strange thing: it seems that checking shadowmaps before
// calculating the attenuation is very slightly faster...


#ifdef SHADOWVOL_COMMON_LIGHT_NEW
  $include "shadowvol/common_light.new.fs"
#else
  $include "shadowvol/common_light.old.fs"
#endif

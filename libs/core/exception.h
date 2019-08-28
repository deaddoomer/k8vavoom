//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
//WARNING! turning assertions off WILL break the engine!
//#define VAVOOM_DISABLE_ASSERTS


//==========================================================================
//
//  Exceptions
//
//==========================================================================
class VException : VInterface {
public:
  virtual const char *What () const = 0;
};


#define MAX_ERROR_TEXT_SIZE   (1024)

class VavoomError : public VException {
public:
  char message[MAX_ERROR_TEXT_SIZE];

  explicit VavoomError (const char *text);
  virtual const char *What () const override;
};


class RecoverableError : public VavoomError {
public:
  explicit RecoverableError (const char *text) : VavoomError(text) {}
};


//==========================================================================
//
//  Guard macros
//
//==========================================================================

// turn on usage of context in guard macros on platforms where it's not
// safe to throw an exception in signal handler
#ifdef USE_SIGNAL_HANDLER
# if DO_GUARD && defined(__linux__)
#  define USE_GUARD_SIGNAL_CONTEXT
# elif defined(USE_GUARD_SIGNAL_CONTEXT)
#  undef USE_GUARD_SIGNAL_CONTEXT
# endif
#elif defined(USE_GUARD_SIGNAL_CONTEXT)
# undef USE_GUARD_SIGNAL_CONTEXT
#endif


#ifdef USE_GUARD_SIGNAL_CONTEXT
#include <setjmp.h>
// stack control
class __Context {
public:
  __Context () { memcpy(&Last, &Env, sizeof(jmp_buf)); }
  ~__Context () { memcpy(&Env, &Last, sizeof(jmp_buf)); }
  static jmp_buf Env;
  static const char *ErrToThrow;

protected:
  jmp_buf Last;
};
#endif


void Host_CoreDump (const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void Sys_Error (const char *, ...) __attribute__((noreturn, format(printf, 1, 2)));

// call `abort()` or `exit()` there to stop standard processing
extern void (*SysErrorCB) (const char *msg);

//const char *SkipPathPartCStr (const char *s);
constexpr inline __attribute__((pure)) const char *SkipPathPartCStr (const char *s) {
  const char *lastSlash = nullptr;
  for (const char *t = s; *t; ++t) {
    if (*t == '/') lastSlash = t+1;
#ifdef _WIN32
    if (*t == '\\') lastSlash = t+1;
#endif
  }
  return (lastSlash ? lastSlash : s);
}


//==========================================================================
//
//  Assertion macros
//
//==========================================================================

#if !defined(VAVOOM_DISABLE_ASSERTS)
//# define vassert(e)  if (!(e)) throw VavoomError("Assertion failed: " #e)
# define vassert(e)  if (!(e)) do { Sys_Error("%s:%d: Assertion failed: %s", SkipPathPartCStr(__FILE__), __LINE__, #e); } while (0)
#else
# warning "WARNING! WARNING! WARNING! you'd better NEVER turn off assertion checking in k8vavoom code!"
# define vassert(e)
#endif
//#define vensure(e)  if (!(e)) throw VavoomError("Assertion failed: " #e)
#define vensure(e)  if (!(e)) do { Sys_Error("%s:%d: Assertion failed: %s", SkipPathPartCStr(__FILE__), __LINE__, #e); } while (0)

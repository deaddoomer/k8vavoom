//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2019-2021 Ketmar Dark
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libs/core.h"


//==========================================================================
//
//  usage
//
//==========================================================================
static __attribute__((noreturn)) void usage () {
  GLog.Write("%s",
    "USAGE:\n"
    "  dunno\n"
    "");
  Z_ShuttingDown();
  Z_Exit(1);
}


//==========================================================================
//
//  main
//
//==========================================================================
int main (int argc, char **argv) {
  GLog.Logf("JAMTEST build date: %s  %s", __DATE__, __TIME__);

  if (argc != 1) usage();

  Z_ShuttingDown();
  return 0;
}

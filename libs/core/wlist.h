//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
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
/*
** A weighted list template class
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
*/
#ifndef VAVOOM_CORE_LIB_WLIST
#define VAVOOM_CORE_LIB_WLIST


// taken from GZDoom
template<class T> class TWeightedList {
public:
  template<class U> struct Choice {
    Choice<U> *Next;
    vuint16 Weight;
    vuint8 RandomVal;  // 0 (never) - 255 (always)
    T Value;

    Choice(vuint16 w, U v) : Next(nullptr), Weight(w), RandomVal(0), Value(v) {}
  };

public:
  Choice<T> *Choices;
  //FRandom &RandomClass;

private:
  void RecalcRandomVals ();

  //TWeightedList &operator= (const TWeightedList &) { return *this; }

public:
  inline TWeightedList (/*FRandom &pr*/) noexcept : Choices(nullptr)/*, RandomClass(pr)*/ {}

  TWeightedList &operator= (const TWeightedList &) = delete;

  ~TWeightedList () {
    Choice<T> *choice = Choices;
    while (choice != nullptr) {
      Choice<T> *next = choice->Next;
      delete choice;
      choice = next;
    }
  }

  void AddEntry (T value, vuint16 weight);
  T PickEntry () const;
  void ReplaceValues (T oldval, T newval);
  TWeightedList<T> Clone ();
};


template<class T> TWeightedList<T> TWeightedList<T>::Clone () {
  TWeightedList<T> *res = new TWeightedList<T>();
  for (const Choice<T> *c = Choices; c; c = c->Next) {
    res->AddEntry(c->Weight, c->Value);
  }
  return res;
}


template<class T> void TWeightedList<T>::AddEntry (T value, vuint16 weight) {
  if (weight == 0) return; // if the weight is 0, don't bother adding it, since it will never be chosen

  Choice<T> **insAfter = &Choices, *insBefore = Choices;
  Choice<T> *theNewOne;

  while (insBefore != nullptr && insBefore->Weight < weight) {
    insAfter = &insBefore->Next;
    insBefore = insBefore->Next;
  }

  theNewOne = new Choice<T>(weight, value);
  *insAfter = theNewOne;
  theNewOne->Next = insBefore;
  RecalcRandomVals();
}


template<class T> T TWeightedList<T>::PickEntry () const {
  //vuint8 randomnum = (vuint8)(Random()*256.0f);
  vuint8 randomnum = (vuint8)((((float)(rand()&0x7fff)/(float)0x8000))*256.0f);
  Choice<T> *choice = Choices;
  while (choice != nullptr && randomnum > choice->RandomVal) choice = choice->Next;
  return (choice != nullptr ? choice->Value : nullptr);
}


template<class T> void TWeightedList<T>::RecalcRandomVals () {
  // redistribute the RandomVals so that they form the correct
  // distribution (as determined by the range of weights)

  int numChoices, weightSums;
  Choice<T> *choice;
  double randVal, weightDenom;

  if (Choices == nullptr) return; // no choices, so nothing to do

  numChoices = 1;
  weightSums = 0;

  for (choice = Choices; choice->Next != nullptr; choice = choice->Next) {
    ++numChoices;
    weightSums += choice->Weight;
  }

  weightSums += choice->Weight;
  choice->RandomVal = 255;  // The last choice is always randomval 255

  randVal = 0.0;
  weightDenom = 1.0/(double)weightSums;

  for (choice = Choices; choice->Next != nullptr; choice = choice->Next) {
    randVal += (double)choice->Weight*weightDenom;
    choice->RandomVal = (vuint8)(randVal*255.0);
  }
}


// replace all values that match oldval with newval
template<class T> void TWeightedList<T>::ReplaceValues (T oldval, T newval) {
  Choice<T> *choice;
  for (choice = Choices; choice != nullptr; choice = choice->Next) {
    if (choice->Value == oldval) choice->Value = newval;
  }
}


#endif

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Logging_h
#define Logging_h

#include "mozilla/Likely.h"

class Logging
{
public:
  static bool isVerbose()
  {
    return Singleton.verbose;
  }

private:
  bool verbose;

public:
  static void Init()
  {
    const char *env = getenv("MOZ_DEBUG_LINKER");
    if (env && *env == '1')
      Singleton.verbose = true;
  }

private:
  static Logging Singleton;
};

#define DEBUG_LOG(...)   \
  do {                   \
    if (MOZ_UNLIKELY(Logging::isVerbose())) {  \
      LOG(__VA_ARGS__);  \
    }                    \
  } while(0)

#if defined(__LP64__)
#  define PRIxAddr "lx"
#  define PRIxSize "lx"
#  define PRIdSize "ld"
#  define PRIuSize "lu"
#else
#  define PRIxAddr "x"
#  define PRIxSize "x"
#  define PRIdSize "d"
#  define PRIuSize "u"
#endif

#endif /* Logging_h */

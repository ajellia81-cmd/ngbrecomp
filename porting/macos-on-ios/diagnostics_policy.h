#pragma once
// Only conversion shims include this policy; original Mac logging is untouched.
#import <Foundation/Foundation.h>
#include <stdio.h>
#ifndef NGB_CONVERSION_DIAGNOSTICS
#define NGB_CONVERSION_DIAGNOSTICS 0
#endif
#if NGB_CONVERSION_DIAGNOSTICS
#define NGB_CONVERSION_DIAG(...) fprintf(stderr, __VA_ARGS__)
#else
#define NGB_CONVERSION_DIAG(...) ((void)0)
// Compile out shim-only messages and their formatting arguments.
#define NSLog(...) ((void)0)
#endif

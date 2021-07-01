/******************************************************************************
**  MODULE:        legacytoolkit.h
**  PROJECT:       EPANET-MSX
**  DESCRIPTION:   C/C++ header file for EPANET Multi-Species Extension Toolkit
**  COPYRIGHT:     Copyright (C) 2007 Feng Shang, Lewis Rossman, and James Uber.
**                 All Rights Reserved. See license information in LICENSE.TXT.
**  AUTHORS:       L. Rossman, US EPA - NRMRL
**                 F. Shang, University of Cincinnati
**                 J. Uber, University of Cincinnati
**  VERSION:       1.1 
**  LAST UPDATE:   11/01/10
*******************************************************************************/

#ifndef EPANETMSX_H
#define EPANETMSX_H

// Define msxtpyes.h
#ifndef MAGICNUMBER
#include "msxtypes.h"
#endif

// --- define WINDOWS

#undef WINDOWS
#ifdef _WIN32
  #define WINDOWS
#endif
#ifdef __WIN32__
  #define WINDOWS
#endif

// --- define DLLEXPORT

#ifndef DLLEXPORT                                                              // ttaxon - 9/7/10
  #ifdef WINDOWS
    #ifdef __cplusplus
    #define DLLEXPORT extern "C" __declspec(dllexport) __stdcall
    #else
    #define DLLEXPORT __declspec(dllexport) __stdcall
    #endif
  #else
    #ifdef __cplusplus
    #define DLLEXPORT extern "C"
    #else
    #define DLLEXPORT
    #endif
  #endif  
#endif

// --- declare MSX functions

int  DLLEXPORT MSXopen(MSXproject *MSX, char*argv[]);
int  DLLEXPORT MSXsolveH(MSXproject *MSX);
int  DLLEXPORT MSXusehydfile(MSXproject *MSX);
int  DLLEXPORT MSXsolveQ(MSXproject *MSX);
int  DLLEXPORT MSXinit(MSXproject *MSX, int saveFlag);
int  DLLEXPORT MSXstep(MSXproject *MSX, long *t, long *tleft);
int  DLLEXPORT MSXsaveoutfile(MSXproject *MSX, char *fname);
int  DLLEXPORT MSXsavemsxfile(MSXproject *MSX, char *fname);
int  DLLEXPORT MSXreport(MSXproject *MSX);
int  DLLEXPORT MSXclose(MSXproject *MSX);

int  DLLEXPORT MSXgeterror(MSXproject *MSX, int code, char *msg, int len);

int DLLEXPORT runLegacy(MSXproject *MSX, int argc, char *argv[]);


#endif
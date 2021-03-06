/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

#ifndef _VISIBILITY_H
#define _VISIBILITY_H

int32_t PixelTranspType(int16_t nTexture, int16_t nOrient, int16_t nFrame, fix u, fix v); //-1: supertransp., 0: opaque,
                                                                                          //1: transparent

int32_t CanSeePoint(
    CObject *pObj,
    CFixVector *vSource,
    CFixVector *vDest,
    int16_t nSegment,
    fix xRad = 0,
    float fov = -2.0f,
    int32_t nThread = MAX_THREADS);

int32_t CanSeeObject(int32_t nObject, int32_t bCheckObjs, int32_t nThread = MAX_THREADS);

int32_t ObjectToObjectVisibility(
    CObject *pObj1,
    CObject *pObj2,
    int32_t transType,
    float fov = -2.0f,
    int32_t nThread = MAX_THREADS);

int32_t TargetInLineOfFire(void);

#endif //_VISIBILITY_H

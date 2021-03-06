/*
 * $Revision: 1.1 $
 * $Author: bradleyb $
 * $Date: 2001/01/28 16:10:57 $
 *
 * Event header file
 *
 * $Log: event.h,v $
 * Revision 1.1  2001/01/28 16:10:57  bradleyb
 * unified input headers.
 *
 *
 */

#ifndef _EVENT_H
#define _EVENT_H

#ifdef GII_XWIN
#include <ggi/input/xwin.h>
#endif

void event_poll();

#ifdef GII_XWIN
void init_gii_xwin(Display *disp, Window win);
#endif

extern int32_t bLegacyInput;

#endif

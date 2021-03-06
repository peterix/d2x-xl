/* $Id: joystick.c,v 1.2 2003/01/01 00:55:03 btb Exp $ */
/*
 *
 * Linux joystick support
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "timer.h"
#include "pstypes.h"
#include "mono.h"
#include "joy.h"

#include "joystick.h"

char bJoyInstalled = 0;
char bJoyPresent = 0;

joystick_device j_joystick[4];
joystick_axis j_axis[MAX_AXES];
joystick_button j_button[MAX_BUTTONS];

int j_num_axes = 0, j_num_buttons = 0;
int timer_rate;

int j_axes_in_sticks[4]; /* number of axes in the first [x] sticks */
int j_buttons_in_sticks[4]; /* number of buttons in the first [x] sticks */

int joyDeadzone = 0;

int j_Get_joydev_axis_number(int all_axis_number) {
    int i, joy_axis_number = all_axis_number;

    for (i = 0; i < j_axis[all_axis_number].joydev; i++) {
        joy_axis_number -= j_joystick[i].num_axes;
    }

    return joy_axis_number;
}

int j_Get_joydev_button_number(int all_button_number) {
    int i, joy_button_number = all_button_number;

    for (i = 0; i < j_button[all_button_number].joydev; i++) {
        joy_button_number -= j_joystick[i].num_buttons;
    }

    return joy_button_number;
}

int j_UpdateState() {
    int num_processed = 0, i;
    struct js_event current_event;
    struct JS_DATA_TYPE joy_data;

    for (i = 0; i < j_num_buttons; i++) {
        // changed 6/24/1999 to finally squish the timedown bug - Owen Evans
        if (j_button[i].state != j_button[i].lastState) {
            if (j_button[i].state) {
                j_button[i].downcount++;
                j_button[i].timedown = TimerGetFixedSeconds();
            }
        }
        // end changed - OE
        j_button[i].lastState = j_button[i].state;
    }

    for (i = 0; i < 4; i++) {
        if (j_joystick[i].buffer >= 0) {
            if (j_joystick[i].version) {
                while (read(j_joystick[i].buffer, &current_event, sizeof(struct js_event)) > 0) {
                    num_processed++;
                    switch (current_event.nType & ~JS_EVENT_INIT) {
                    case JS_EVENT_AXIS:
                        j_axis[j_axes_in_sticks[i] + current_event.number].value = current_event.value;
                        break;
                    case JS_EVENT_BUTTON:
                        j_button[j_buttons_in_sticks[i] + current_event.number].state = current_event.value;
                        break;
                    }
                }
            } else {
                read(j_joystick[i].buffer, &joy_data, JS_RETURN);
                j_axis[j_axes_in_sticks[i] + 0].value = joy_data.x;
                j_axis[j_axes_in_sticks[i] + 1].value = joy_data.y;
                j_button[j_buttons_in_sticks[i] + 0].state = (joy_data.buttons & 0x01);
                j_button[j_buttons_in_sticks[i] + 1].state = (joy_data.buttons & 0x02) >> 1;
            }
        }
    }

    return num_processed;
}

void JoySetCalVals(int *axis_min, int *axis_center, int *axis_max) {
    /* stpohle - this is already done in the "JoyInit" function, so we don't need it in here.
        int i;

        for (i = 0; i < 4; i++) {
            j_axis[i].center_val = axis_center[i];
            j_axis[i].min_val = axis_min[i];
            j_axis[i].max_val = axis_max[i];
        }

    */
}

void JoyGetCalVals(int *axis_min, int *axis_center, int *axis_max) {
    int i;

    // edited 05/18/99 Matt Mueller - we should return all axes instead of j_num_axes, since they are all given to us in
    // JoySetCalVals ( and because checker complains :)
    for (i = 0; i < 4; i++) {
        // end edit -MM
        axis_center[i] = j_axis[i].center_val;
        axis_min[i] = j_axis[i].min_val;
        axis_max[i] = j_axis[i].max_val;
    }
}

void joy_set_min(int axis_number, int value) { j_axis[axis_number].min_val = value; }

void joy_set_center(int axis_number, int value) { j_axis[axis_number].center_val = value; }

void joy_set_max(int axis_number, int value) { j_axis[axis_number].max_val = value; }

ubyte JoyGetPresentMask() { return 1; }

void JoySetTimerRate(int maxValue) { timer_rate = maxValue; }

int JoyGetTimerRate() { return timer_rate; }

void JoyFlush() {
    int i;

    if (!bJoyInstalled)
        return;

    for (i = 0; i < j_num_buttons; i++) {
        j_button[i].timedown = 0;
        j_button[i].downcount = 0;
    }
}

ubyte JoyReadRawAxis(ubyte mask, int *axes) {
    int i;

    j_UpdateState();

    for (i = 0; i < j_num_axes; i++) {
        axes[i] = j_axis[i].value;
    }

    return 0;
}

/* JoyInit () is pretty huge, a bit klunky, and by no means pretty.  But who cares?  It does the job and it's only run
 * once. */

int JoyInit() {
    int i, j;

    if (bJoyInstalled)
        return 0;
    JoyFlush();

    if (!bJoyInstalled) {

        // printf ("Initializing joystick... ");

        j_joystick[0].buffer = open("/dev/js0", O_NONBLOCK);
        j_joystick[1].buffer = open("/dev/js1", O_NONBLOCK);
        j_joystick[2].buffer = open("/dev/js2", O_NONBLOCK);
        j_joystick[3].buffer = open("/dev/js3", O_NONBLOCK);

        if (j_joystick[0].buffer >= 0 || j_joystick[1].buffer >= 0 || j_joystick[2].buffer >= 0 ||
            j_joystick[3].buffer >= 0) {
            // printf ("found: ");
            for (i = 0; i < 4; i++) {
                if (j_joystick[i].buffer >= 0) {
                    ioctl(j_joystick[i].buffer, JSIOCGAXES, &j_joystick[i].num_axes);
                    ioctl(j_joystick[i].buffer, JSIOCGBUTTONS, &j_joystick[i].num_buttons);
                    ioctl(j_joystick[i].buffer, JSIOCGVERSION, &j_joystick[i].version);
                    if (!j_joystick[i].version) {
                        j_joystick[i].num_axes = 2;
                        j_joystick[i].num_buttons = 2;
                        // printf ("js%d (v0.x)  " , i);
                    } else {
                        // printf ("js%d (v%d.%d.%d)  ",
                            i, 
							(j_joystick[i].version & 0xff0000) >> 16,
							(j_joystick[i].version & 0xff00) >> 8,
							j_joystick[i].version & 0xff);
                    }

                    for (j = j_num_axes; j < (j_num_axes + j_joystick[i].num_axes); j++) {
                        j_axis[j].joydev = i;
                        if (j_joystick[i].version) {
                            j_axis[j].center_val = 0;
                            j_axis[j].max_val = 32767;
                            j_axis[j].min_val = -32767;
                        }
                    }
                    for (j = j_num_buttons; j < (j_num_buttons + j_joystick[i].num_buttons); j++) {
                        j_button[j].joydev = i;
                    }

                    j_num_axes += j_joystick[i].num_axes;
                    j_num_buttons += j_joystick[i].num_buttons;

                } else {
                    j_joystick[i].num_buttons = 0;
                    j_joystick[i].num_axes = 0;
                }

                for (j = 0; j < i; j++) {
                    j_axes_in_sticks[i] += j_joystick[j].num_axes;
                    j_buttons_in_sticks[i] += j_joystick[j].num_buttons;
                }
            }
        } else {
            // printf ("no joysticks found\n");
            return 0;
        }

        // printf ("\n");

        if (j_num_axes > MAX_AXES)
            j_num_axes = MAX_AXES;
        if (j_num_buttons > MAX_BUTTONS)
            j_num_buttons = MAX_BUTTONS;

        bJoyPresent = 1;
        bJoyInstalled = 1;
        return 1;
    }

    return 1;
}

void joy_close() {
    int i;

    if (!bJoyInstalled)
        return;

    for (i = 0; i < 4; i++) {
        if (j_joystick[i].buffer >= 0) {
            // printf ("closing js%d\n", i);
            close(j_joystick[i].buffer);
        }
        j_joystick[i].buffer = -1;
    }

    bJoyPresent = 0;
    bJoyInstalled = 0;
}

void joy_set_cen() {}

int JoyGetscaledReading(int raw, int axis_num) {
    int d, x;

    raw -= j_axis[axis_num].center_val;

    if (raw < 0)
        d = j_axis[axis_num].center_val - j_axis[axis_num].min_val;
    else if (raw > 0)
        d = j_axis[axis_num].max_val - j_axis[axis_num].center_val;
    else
        d = 0;

    if (d)
        x = ((raw << 7) / d);
    else
        x = 0;

    if (x < -128)
        x = -128;
    if (x > 127)
        x = 127;

    // added on 4/13/99 by Victor Rachels to add deadzone control
    d = (joyDeadzone)*6;
    if ((x > (-1 * d)) && (x < d))
        x = 0;
    // end this section addition -VR

    return x;
}

void JoyGetpos(int *x, int *y) {
    int axis[MAX_AXES];

    if ((!bJoyInstalled) || (!bJoyPresent)) {
        *x = *y = 0;
        return;
    }

    JoyReadRawAxis(JOY_ALL_AXIS, axis);

    *x = JoyGetscaledReading(axis[0], 0);
    *y = JoyGetscaledReading(axis[1], 1);
}

int JoyGetbuttonState(int btn) {
    if (btn >= j_num_buttons)
        return 0;
    j_UpdateState();

    return j_button[btn].state;
}

int JoyGetButtonDownCnt(int btn) {
    int downcount;

    j_UpdateState();

    downcount = j_button[btn].downcount;
    j_button[btn].downcount = 0;

    return downcount;
}

// changed 6/24/99 to finally squish the timedown bug - Owen Evans
fix JoyGetButtonDownTime(int btn) {
    fix downtime;
    j_UpdateState();

    if (j_button[btn].state) {
        downtime = TimerGetFixedSeconds() - j_button[btn].timedown;
        j_button[btn].timedown = TimerGetFixedSeconds();
    } else {
        downtime = 0;
    }

    return downtime;
}
// end changed - OE

void joy_poll() {}

void JoySetSlowReading(int flag) {}

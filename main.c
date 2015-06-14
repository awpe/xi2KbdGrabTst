/*gcc -lX11 -lXi*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/extensions/XInput2.h>
#define KBD_DEV_ID 3

Bool
chckXI2Ext (Display * dpy,
            int     * xi_opcode)
{
    int event, error, major, minor, rc;

    if (XQueryExtension (dpy, "XInputExtension", xi_opcode, &event, &error) == 0)
    {
        printf ("X Input extension not available.\n");
        return False;
    }

    major = 2;
    minor = 0;

    if ((rc = XIQueryVersion (dpy, &major, &minor)) == BadRequest)
    {
        printf ("XI2 not available. Server supports %d.%d\n", major, minor);
        return False;
    }
    else if (rc != Success)
    {
        printf ("Xlib internal error!\n");
        return False;
    }

    return True;
}

void
grab_key (Display * dpy,
          Window    win,
          int     * xi_opcode)
{
    XIGrabModifiers       modifiers[4], failed_modifiers[4];
    XEvent                ev;
    XIEventMask           evmask;
    XGenericEventCookie * cookie;
    XIDeviceEvent       * xide;
    KeySym              * keysym;
    KeyCode               TransCtrlKC, ExitCtrlKC;
    int                   keysym_count, kc, nmodifiers, failed_nmodifiers;
    unsigned char         mask[(XI_LASTEVENT + 7) / 8];

    TransCtrlKC = XKeysymToKeycode (dpy, XStringToKeysym ("u"));
    ExitCtrlKC  = XKeysymToKeycode (dpy, XStringToKeysym ("g"));

    memset (mask, 0, sizeof (mask));
    XISetMask (mask, XI_KeyRelease);
    XISetMask (mask, XI_KeyPress);

    evmask.mask_len        = sizeof (mask);
    evmask.mask            = mask;

    nmodifiers             = 4;

    modifiers[0].modifiers = ShiftMask | ControlMask;
    modifiers[1].modifiers = ShiftMask | ControlMask | LockMask;
    modifiers[2].modifiers = ShiftMask | ControlMask | Mod2Mask;
    modifiers[3].modifiers = ShiftMask | ControlMask | Mod2Mask  | LockMask;

    memcpy (failed_modifiers, modifiers, sizeof (modifiers));

    printf ("Grabbing keycode %d (usually %s) with CTRL + SHIFT modifiers\n",
            TransCtrlKC, "u");

    failed_nmodifiers = XIGrabKeycode (dpy, KBD_DEV_ID, TransCtrlKC, win,
                                       GrabModeAsync, GrabModeAsync, False,
                                       &evmask, nmodifiers, failed_modifiers);

    if (failed_nmodifiers != 0)
    {
        int i;
        for (i = 0; i < failed_nmodifiers; i++)
        {
            printf ("Modifier %x failed with error %d\n",
                    failed_modifiers[i].modifiers, failed_modifiers[i].status);
        }
        return ;
    }

    printf ("Grabbing keycode %d (usually %s) with CTRL + SHIFT modifiers\n",
            TransCtrlKC, "g");

    failed_nmodifiers = XIGrabKeycode (dpy, KBD_DEV_ID, ExitCtrlKC, win,
                                       GrabModeAsync, GrabModeAsync, False,
                                       &evmask, nmodifiers, failed_modifiers);

    if (failed_nmodifiers != 0)
    {
        int i;
        for (i = 0; i < failed_nmodifiers; i++)
        {
            printf ("Modifier %x failed with error %d\n",
                    failed_modifiers[i].modifiers, failed_modifiers[i].status);
        }
        return ;
    }

    printf ("Waiting for grab to activate now. Press a key.\n");

    while (1)
    {
        cookie = &ev.xcookie;
        XNextEvent (dpy, &ev);

        if (   cookie->type                != GenericEvent
            || cookie->extension           != *xi_opcode
            || XGetEventData (dpy, cookie) == 0)
        {
            continue;
        }

        xide = (XIDeviceEvent*) ev.xcookie.data;

        keysym = XGetKeyboardMapping (dpy, xide->detail, 1, &keysym_count);
        kc = XKeysymToKeycode (dpy, *keysym);
        XFree (keysym);

        if (kc == 30 && cookie->evtype == XI_KeyRelease)
        {
            printf ("CTRL + SHIFT + U\n");
        }

        if (kc == 42 && cookie->evtype == XI_KeyRelease)
        {
            printf ("CTRL + SHIFT + G\n");
        }

        XFreeEventData (dpy, cookie);
    }

    XIUngrabKeycode (dpy, KBD_DEV_ID, TransCtrlKC, win, nmodifiers, modifiers);
    XIUngrabKeycode (dpy, KBD_DEV_ID, ExitCtrlKC, win, nmodifiers, modifiers);
}

int
main (void)
{
    Display         * dpy;
    Window            root;
    int               xi_opcode;

    dpy        = XOpenDisplay (NULL);
    root       = DefaultRootWindow (dpy);

    if (chckXI2Ext (dpy, &xi_opcode) == False)
    {
        return EXIT_FAILURE;
    }

    grab_key (dpy, root, &xi_opcode);

    XCloseDisplay (dpy);


    return EXIT_SUCCESS;
}


/*gcc -lX11 -lXi*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/extensions/XInput2.h>

Bool
chckXI2Ext (Display * dpy,
            int     * xiOp)
{
    int event, error, major, minor, rc;

    if (XQueryExtension (dpy, "XInputExtension", xiOp, &event, &error) == 0)
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

Bool
grabKey (Display         * xDisplay,
         Window            w,
         KeyCode           xKCode,
         int               nDevs,
         int             * devs,
         int               nMods,
         XIGrabModifiers * mods)
{
    XIGrabModifiers * failMods;
    XIEventMask       evmask;
    int               nfailMods, i;
    unsigned char     mask[(XI_LASTEVENT + 7) / 8];

    failMods = (XIGrabModifiers*) malloc (sizeof (XIGrabModifiers) * nMods);

    memcpy (failMods, mods, sizeof (XIGrabModifiers) * nMods);

    memset (mask, 0, sizeof (mask));
    XISetMask (mask, XI_KeyRelease);
    XISetMask (mask, XI_KeyPress);

    memset (&evmask, 0, sizeof (evmask));
    evmask.mask_len = sizeof (mask);
    evmask.mask     = mask;

    nfailMods = 0;

    for (i = 0; i < nDevs && nfailMods == 0; ++i)
    {
        nfailMods = XIGrabKeycode (xDisplay, devs[i], xKCode, w, GrabModeAsync,
                                   GrabModeAsync, False, &evmask, nMods,
                                   failMods);
    }

    if (nfailMods != 0)
    {
        for (i = 0; i < nfailMods; ++i)
        {
            printf ("Modifier %x failed with error %d\n", failMods[i].modifiers,
                    failMods[i].status);
        }

        free (failMods);

        return False;
    }

    printf ("\tsuccess!\n\n");

    free (failMods);

    return True;
}

void
ungrabKey (Display         * dpy,
           Window            win,
           KeyCode           xKCode,
           int               nDevs,
           int             * devs,
           int               nMods,
           XIGrabModifiers * mods)
{
    for (int i = 0; i < nDevs; ++i)
    {
        XIUngrabKeycode (dpy, devs[i], xKCode, win, nMods, mods);
    }
}

int
main (void)
{
    Display             * dpy;
    Window                root;
    int                   xiOp, stop, keysym_count, kc, nMods, * kbds, nDevs;
    int                   nKbds;
    KeyCode               TransCtrlKC, ExitCtrlKC;
    KeySym              * keysym;
    XGenericEventCookie * cookie;
    XIDeviceEvent       * xide;
    XEvent                ev;
    XIDeviceInfo        * device_info;


    /**************************************************************************/
    /*start connection with x server*/
    /**************************************************************************/
    dpy        = XOpenDisplay (NULL);
    if (dpy == NULL)
    {
        printf ("Cannot open default display\n!");
        return EXIT_FAILURE;
    }
    root       = DefaultRootWindow (dpy);
    /**************************************************************************/


    /**************************************************************************/
    /*check XInput2 extension availability and its version*/
    /**************************************************************************/
    if (chckXI2Ext (dpy, &xiOp) == False)
    {
        XCloseDisplay (dpy);
        return EXIT_FAILURE;
    }
    /**************************************************************************/


    /**************************************************************************/
    /*get master keyboard count and IDs*/
    /**************************************************************************/
    nDevs       = 0;
    nKbds       = 0;
    kbds        = NULL;
    device_info = XIQueryDevice (dpy, XIAllMasterDevices, &nDevs);

    if (device_info != NULL)
    {
        kbds = (int*) malloc (sizeof (int) * nDevs);

        for (int i = 0; i < nDevs; ++i)
        {
            if (device_info[i].use == XIMasterKeyboard)
            {
                kbds[nKbds] = device_info[i].deviceid;
                ++nKbds;
            }
        }

        XIFreeDeviceInfo (device_info);
    }
    else
    {
        printf ("Cannot get master device list!\n");
        XCloseDisplay (dpy);
        return EXIT_FAILURE;
    }
    /**************************************************************************/


    /**************************************************************************/
    /*prepare modifiers*/
    /**************************************************************************/
    nMods = 4;

    XIGrabModifiers modifiers[4];

    modifiers[0].modifiers = ShiftMask | ControlMask;
    modifiers[1].modifiers = ShiftMask | ControlMask | LockMask;
    modifiers[2].modifiers = ShiftMask | ControlMask | Mod2Mask;
    modifiers[3].modifiers = ShiftMask | ControlMask | Mod2Mask  | LockMask;
    /**************************************************************************/


    /**************************************************************************/
    /*get keycodes*/
    /**************************************************************************/
    TransCtrlKC = XKeysymToKeycode (dpy, XStringToKeysym ("u"));
    ExitCtrlKC  = XKeysymToKeycode (dpy, XStringToKeysym ("g"));
    /**************************************************************************/


    /**************************************************************************/
    /*Grab translation control key*/
    /**************************************************************************/
    printf ("Grabbing keycode %d (usually %s) with CTRL + SHIFT modifiers\n",
            TransCtrlKC, "u");

    if (grabKey (dpy, root, TransCtrlKC, nKbds, kbds, nMods, modifiers)
        == False)
    {
        free (kbds);
        XCloseDisplay (dpy);
        return EXIT_FAILURE;
    }
    /**************************************************************************/


    /**************************************************************************/
    /*Grab exit key*/
    /**************************************************************************/
    printf ("Grabbing keycode %d (usually %s) with CTRL + SHIFT modifiers\n",
            ExitCtrlKC, "g");

    if (grabKey (dpy, root, ExitCtrlKC, nKbds, kbds, nMods, modifiers)
        == False)
    {
        ungrabKey (dpy, root, TransCtrlKC, nKbds, kbds, nMods, modifiers);
        free (kbds);
        XCloseDisplay (dpy);
        return EXIT_FAILURE;
    }
    /**************************************************************************/


    /**************************************************************************/
    /*Start event processing*/
    /**************************************************************************/
    printf ("Waiting for input. Press CTRL + SHIFT + G to quit.\n\n");

    stop = 0;

    while (stop == 0)
    {
        cookie = &ev.xcookie;
        XNextEvent (dpy, &ev);

        if (   cookie->type                != GenericEvent
            || cookie->extension           != xiOp
            || XGetEventData (dpy, cookie) == 0)
        {
            continue;
        }

        xide = (XIDeviceEvent*) ev.xcookie.data;

        keysym = XGetKeyboardMapping (dpy, xide->detail, 1, &keysym_count);
        kc = XKeysymToKeycode (dpy, *keysym);
        XFree (keysym);

        if (kc == TransCtrlKC && cookie->evtype == XI_KeyRelease)
        {
            printf ("CTRL + SHIFT + U\n");
        }

        if (kc == ExitCtrlKC && cookie->evtype == XI_KeyRelease)
        {
            printf ("CTRL + SHIFT + G\n");
            stop = 1;
        }

        XFreeEventData (dpy, cookie);
    }
    /**************************************************************************/


    ungrabKey (dpy, root, TransCtrlKC, nKbds, kbds, nMods, modifiers);
    ungrabKey (dpy, root, ExitCtrlKC, nKbds, kbds, nMods, modifiers);

    XCloseDisplay (dpy);

    free (kbds);

    return EXIT_SUCCESS;
}


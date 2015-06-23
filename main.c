/*gcc -std=c11 -lX11 -lXi*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/extensions/XInput2.h>
#define LOG_LVL_NO 0
#define LOG_LVL_2 2

struct context_
{
    Display * xDpy;
    int       xiOp;
} ;

typedef struct context_ XWCContext;

struct DevList_
{
    int * devs;
    int   nDevs;
} ;

typedef struct DevList_ DevList;

void
logCtr (const char * buf, int log_lvl, Bool l)
{
    if (! buf)
    {
        return;
    }
    if (log_lvl)
    {
        return;
    }
    if (! l)
    {
        return;
    }
}

Bool
chckXI2Ext (XWCContext * ctx)
{
    int event, error, major, minor, rc;
    char buf[1024];

    if (ctx == NULL)
    {
        logCtr ("Cannot check XInput 2 extension: NULL pointer to XWC context"
                " received.", LOG_LVL_NO, False);
        return False;
    }

    if (XQueryExtension (ctx->xDpy, "XInputExtension", &ctx->xiOp, &event,
                         &error) == 0)
    {
        logCtr ("X Input extension not available.", LOG_LVL_NO, False);
        return False;
    }

    major = 2;
    minor = 0;

    if ((rc = XIQueryVersion (ctx->xDpy, &major, &minor)) == BadRequest)
    {
        snprintf (buf, sizeof (buf), "XI2 not available. Server supports %d.%d",
                  major, minor);
        logCtr (buf,  LOG_LVL_NO, False);
        return False;
    }
    else if (rc != Success)
    {
        logCtr ("Xlib internal error!", LOG_LVL_NO, False);
        return False;
    }

    return True;
}

Bool
grabKeyCtrl (XWCContext      * ctx,
             Window            w,
             KeyCode           xKCode,
             DevList         * kbds,
             int               nMods,
             XIGrabModifiers * mods,
             Bool              grab)
{
    XIGrabModifiers * failMods;
    XIEventMask       evmask;
    int               nfailMods, i;
    unsigned char     mask[(XI_LASTEVENT + 7) / 8];
    char              buf[1024];

    if (ctx == NULL)
    {
        logCtr ("Cannot grab key: NULL pointer to XWC context received.",
                LOG_LVL_NO, False);
        return False;
    }

    if (kbds == NULL)
    {
        logCtr ("Cannot grab key: NULL pointer to keyboard device list"
                " received.", LOG_LVL_NO, False);
        return False;
    }

    if (kbds->nDevs < 0 || kbds->nDevs > 127)
    {
        logCtr ("Cannot grab key: bad device count.", LOG_LVL_NO, False);
        return False;
    }

    if (kbds->devs == NULL)
    {
        logCtr ("Cannot grab key: NULL pointer to device array received.",
                LOG_LVL_NO, False);
        return False;
    }

    if (nMods < 0 || nMods > 127)
    {
        logCtr ("Cannot grab key: bad mods count.", LOG_LVL_NO, False);
        return False;
    }



    if (mods == NULL)
    {
        logCtr ("Cannot grab key: NULL pointer to modifiers array received.",
                LOG_LVL_NO, False);
        return False;
    }

    if (w == None)
    {
        logCtr ("Cannot grab key: No window specified.", LOG_LVL_NO, False);
        return False;
    }

    if (grab == False)
    {
        for (int i = 0; i < kbds->nDevs; ++ i)
        {
            XIUngrabKeycode (ctx->xDpy, kbds->devs[i], xKCode, w, nMods, mods);
        }
        return True;
    }

    failMods = (XIGrabModifiers*) malloc (sizeof (XIGrabModifiers) * nMods);

    if (failMods == NULL)
    {
        logCtr ("Cannot grab key: cannot allocate array for failed mods.",
                LOG_LVL_NO, False);
        return False;
    }

    memcpy (failMods, mods, sizeof (XIGrabModifiers) * nMods);

    memset (mask, 0, sizeof (mask));
    XISetMask (mask, XI_KeyRelease);
    XISetMask (mask, XI_KeyPress);

    memset (&evmask, 0, sizeof (evmask));
    evmask.mask_len = sizeof (mask);
    evmask.mask     = mask;

    nfailMods       = 0;

    for (i = 0; i < kbds->nDevs && nfailMods == 0; ++ i)
    {
        nfailMods = XIGrabKeycode (ctx->xDpy, kbds->devs[i], xKCode, w,
                                   GrabModeAsync, GrabModeAsync, False, &evmask,
                                   nMods, failMods);
    }

    if (nfailMods != 0)
    {
        for (i = 0; i < nfailMods; ++ i)
        {

            snprintf (buf, sizeof (buf), "Modifier %x failed with error %d\n",
                      failMods[i].modifiers, failMods[i].status);
            logCtr (buf, LOG_LVL_NO, False);
        }

        free (failMods);

        return False;
    }

    snprintf (buf, sizeof (buf), "success!");
    logCtr (buf, LOG_LVL_2, True);

    free (failMods);

    return True;
}

Bool
getMasterDevsList (XWCContext *  ctx,
                   DevList    ** devList,
                   int           devType)
{
    int          * devs, nResDevs, nAllDevs;
    XIDeviceInfo * device_info;

    if (ctx == NULL)
    {
        logCtr ("Cannot get list of master devices: NULL pointer to program"
                " context received!", LOG_LVL_NO, False);
        return False;
    }

    nAllDevs    = 0;
    nResDevs    = 0;
    devs        = NULL;
    device_info = XIQueryDevice (ctx->xDpy, XIAllMasterDevices, &nAllDevs);

    if (device_info != NULL)
    {
        devs = (int*) malloc (sizeof (int) * nAllDevs);

        if (devs == NULL)
        {
            logCtr ("Cannot get list of master devices: cannot allocate memory"
                    " for device's id array!", LOG_LVL_NO, False);
            XIFreeDeviceInfo (device_info);
            return False;
        }

        for (int i = 0; i < nAllDevs; ++ i)
        {
            if (device_info[i].use == devType)
            {
                devs[nResDevs] = device_info[i].deviceid;
                ++ nResDevs;
            }
        }

        XIFreeDeviceInfo (device_info);

        *devList = (DevList *) malloc (sizeof (DevList));

        if (*devList == NULL)
        {
            logCtr ("Cannot get list of master devices: Cannot allocate memory"
                    " for device list!", LOG_LVL_NO, False);
            free (devs);
            return False;
        }

        (*devList)->devs  = devs;
        (*devList)->nDevs = nResDevs;
    }
    else
    {
        logCtr ("Cannot get list of master devices: XIQueryDevice error!",
                LOG_LVL_NO, False);
        return False;
    }

    return True;
}

int
main (void)
{
    Window                root;
    int                   stop, keysym_count, kc, nMods;
    KeyCode               TransCtrlKC, ExitCtrlKC;
    KeySym              * keysym;
    XGenericEventCookie * cookie;
    XIDeviceEvent       * xide;
    XEvent                ev;
    XWCContext            ctx;
    DevList             * kbds;


    /**************************************************************************/
    /*start connection with x server*/
    /**************************************************************************/
    ctx.xDpy        = XOpenDisplay (NULL);
    if (ctx.xDpy == NULL)
    {
        printf ("Cannot open default display\n!");
        return EXIT_FAILURE;
    }
    root       = DefaultRootWindow (ctx.xDpy);
    /**************************************************************************/


    /**************************************************************************/
    /*check XInput2 extension availability and its version*/
    /**************************************************************************/
    if (chckXI2Ext (&ctx) == False)
    {
        XCloseDisplay (ctx.xDpy);
        return EXIT_FAILURE;
    }
    /**************************************************************************/


    /**************************************************************************/
    /*get master keyboard count and IDs*/
    /**************************************************************************/
    if (getMasterDevsList (&ctx, &kbds, XIMasterKeyboard) == False)
    {
        XCloseDisplay (ctx.xDpy);
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
    TransCtrlKC = XKeysymToKeycode (ctx.xDpy, XStringToKeysym ("u"));
    ExitCtrlKC  = XKeysymToKeycode (ctx.xDpy, XStringToKeysym ("g"));
    /**************************************************************************/


    /**************************************************************************/
    /*Grab translation control key*/
    /**************************************************************************/
    printf ("Grabbing keycode %d (usually %s) with CTRL + SHIFT modifiers\n",
            TransCtrlKC, "u");

    if (grabKeyCtrl (&ctx, root, TransCtrlKC, kbds, nMods, modifiers, True)
        == False)
    {
        free (kbds->devs);
        free (kbds);
        XCloseDisplay (ctx.xDpy);
        return EXIT_FAILURE;
    }
    /**************************************************************************/


    /**************************************************************************/
    /*Grab exit key*/
    /**************************************************************************/
    printf ("Grabbing keycode %d (usually %s) with CTRL + SHIFT modifiers\n",
            ExitCtrlKC, "g");

    if (grabKeyCtrl (&ctx, root, ExitCtrlKC, kbds, nMods, modifiers, True)
        == False)
    {
        grabKeyCtrl (&ctx, root, TransCtrlKC, kbds, nMods, modifiers, False);
        free (kbds->devs);
        free (kbds);
        XCloseDisplay (ctx.xDpy);
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
        cookie = & ev.xcookie;
        XNextEvent (ctx.xDpy, &ev);

        if (   cookie->type                     != GenericEvent
            || cookie->extension                != ctx.xiOp
            || XGetEventData (ctx.xDpy, cookie) == 0)
        {
            continue;
        }

        xide = (XIDeviceEvent*) ev.xcookie.data;

        keysym = XGetKeyboardMapping (ctx.xDpy, xide->detail, 1, &keysym_count);
        kc = XKeysymToKeycode (ctx.xDpy, *keysym);
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

        XFreeEventData (ctx.xDpy, cookie);
    }
    /**************************************************************************/


    grabKeyCtrl (&ctx, root, TransCtrlKC, kbds, nMods, modifiers, False);
    grabKeyCtrl (&ctx, root, ExitCtrlKC, kbds, nMods, modifiers, False);

    XCloseDisplay (ctx.xDpy);

    free (kbds->devs);
    free (kbds);

    return EXIT_SUCCESS;
}

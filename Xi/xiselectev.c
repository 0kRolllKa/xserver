/*
 * Copyright 2008 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Peter Hutterer
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif


#include "dixstruct.h"
#include "windowstr.h"
#include "exglobals.h"
#include "exevents.h"
#include <X11/extensions/XI2proto.h>

#include "xiselectev.h"

int
SProcXISelectEvent(ClientPtr client)
{
    char n;
    int i;
    xXIEventMask* evmask;

    REQUEST(xXISelectEventsReq);
    swaps(&stuff->length, n);
    REQUEST_SIZE_MATCH(xXISelectEventsReq);
    swapl(&stuff->window, n);
    swaps(&stuff->num_masks, n);

    evmask = (xXIEventMask*)&stuff[1];
    for (i = 0; i < stuff->num_masks; i++)
    {
        swaps(&evmask->deviceid, n);
        swaps(&evmask->mask_len, n);
        evmask = (xXIEventMask*)(((char*)evmask) + evmask->mask_len * 4);
    }

    return (ProcXISelectEvent(client));
}

int
ProcXISelectEvent(ClientPtr client)
{
    int rc, num_masks, i;
    WindowPtr win;
    DeviceIntPtr dev;
    DeviceIntRec dummy;
    xXIEventMask *evmask;
    int *types = NULL;

    REQUEST(xXISelectEventsReq);
    REQUEST_AT_LEAST_SIZE(xXISelectEventsReq);

    rc = dixLookupWindow(&win, stuff->window, client, DixReceiveAccess);
    if (rc != Success)
        return rc;

    /* check request validity */
    evmask = (xXIEventMask*)&stuff[1];
    num_masks = stuff->num_masks;
    while(num_masks--)
    {
        if (evmask->deviceid != XIAllDevices &&
            evmask->deviceid != XIAllMasterDevices)
            rc = dixLookupDevice(&dev, evmask->deviceid, client, DixReadAccess);
        else {
            /* XXX: XACE here? */
        }
        if (rc != Success)
            return rc;


        if ((evmask->mask_len * 4) > XI_LASTEVENT)
        {
            unsigned char *bits = (unsigned char*)&evmask[1];
            for (i = XI_LASTEVENT; i < evmask->mask_len * 4; i++)
            {
                if (BitIsOn(bits, i))
                    return BadValue;
            }
        }

        evmask = (xXIEventMask*)(((unsigned char*)evmask) + evmask->mask_len * 4);
        evmask++;
    }

    /* Set masks on window */
    evmask = (xXIEventMask*)&stuff[1];
    num_masks = stuff->num_masks;
    while(num_masks--)
    {
        if (evmask->deviceid == XIAllDevices ||
            evmask->deviceid == XIAllMasterDevices)
        {
            dummy.id = evmask->deviceid;
            dev = &dummy;
        } else
            dixLookupDevice(&dev, evmask->deviceid, client, DixReadAccess);
        XISetEventMask(dev, win, client, evmask->mask_len * 4, (unsigned char*)&evmask[1]);
        evmask = (xXIEventMask*)(((unsigned char*)evmask) + evmask->mask_len * 4);
        evmask++;
    }

    RecalculateDeliverableEvents(win);

    xfree(types);
    return Success;
}


int
SProcXIGetSelectedEvents(ClientPtr client)
{
    char n;

    REQUEST(xXIGetSelectedEventsReq);
    swaps(&stuff->length, n);
    REQUEST_SIZE_MATCH(xXIGetSelectedEventsReq);
    swapl(&stuff->window, n);

    return (ProcXIGetSelectedEvents(client));
}

int
ProcXIGetSelectedEvents(ClientPtr client)
{
    int rc, i;
    WindowPtr win;
    char n;
    char *buffer = NULL;
    xXIGetSelectedEventsReply reply;
    OtherInputMasks *masks;
    InputClientsPtr others = NULL;
    xXIEventMask *evmask = NULL;

    REQUEST(xXIGetSelectedEventsReq);
    REQUEST_SIZE_MATCH(xXIGetSelectedEventsReq);

    rc = dixLookupWindow(&win, stuff->window, client, DixReceiveAccess);
    if (rc != Success)
        return rc;

    reply.repType = X_Reply;
    reply.RepType = X_XIGetSelectedEvents;
    reply.length = 0;
    reply.sequenceNumber = client->sequence;
    reply.num_masks = 0;

    masks = wOtherInputMasks(win);
    if (masks)
    {
	for (others = wOtherInputMasks(win)->inputClients; others;
	     others = others->next) {
	    if (SameClient(others, client)) {
                break;
            }
        }
    }

    if (!others)
    {
        WriteReplyToClient(client, sizeof(xXIGetSelectedEventsReply), &reply);
        return Success;
    }

    buffer = xcalloc(MAXDEVICES, sizeof(xXIEventMask) + XI2MASKSIZE);
    if (!buffer)
        return BadAlloc;

    evmask = (xXIEventMask*)buffer;
    for (i = 0; i < MAXDEVICES; i++)
    {
        int j;
        unsigned char *devmask = others->xi2mask[i];

        for (j = XI2MASKSIZE - 1; j >= 0; j--)
        {
            if (devmask[j] != 0)
            {
                evmask->deviceid = i;
                evmask->mask_len = (j + 4)/4; /* j is an index, hence + 4,
                                                 not + 3 */

                reply.num_masks++;
                reply.length += sizeof(xXIEventMask)/4 + evmask->mask_len;

                if (client->swapped)
                {
                    swaps(&evmask->deviceid, n);
                    swaps(&evmask->mask_len, n);
                }

                memcpy(&evmask[1], devmask, j + 1);
                evmask = (xXIEventMask*)((char*)evmask +
                           sizeof(xXIEventMask) + evmask->mask_len * 4);
                break;
            }
        }
    }

    WriteReplyToClient(client, sizeof(xXIGetSelectedEventsReply), &reply);

    if (reply.num_masks)
    {
        WriteSwappedDataToClient(client, reply.length * 4, buffer);
    }


    xfree(buffer);
    return Success;
}

void SRepXIGetSelectedEvents(ClientPtr client,
                            int len, xXIGetSelectedEventsReply *rep)
{
    char n;

    swaps(&rep->sequenceNumber, n);
    swapl(&rep->length, n);
    swaps(&rep->num_masks, n);
    WriteToClient(client, len, (char *)rep);
}


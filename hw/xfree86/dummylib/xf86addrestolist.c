
#ifdef HAVE_XORG_CONFIG_H
#include <xorg-config.h>
#endif

#include "xf86.h"

resPtr
xf86AddResToList(resPtr rlist, resRange *Range, int entityIndex)
{
    return rlist;
}

void
xf86FreeResList(resPtr rlist)
{
    return;
}

resPtr
xf86DupResList(const resPtr rlist)
{
    return rlist;
}

// **********************************************************************
//
// Copyright (c) 2002
// MutableRealms, Inc.
// Huntsville, AL, USA
//
// All Rights Reserved
//
// **********************************************************************

#ifndef ICE_SKELETON_H
#define ICE_SKELETON_H

#include <Ice/Shared.h>
#include <Ice/Handle.h>

#include <Ice/Shared.h>
#include <Ice/Handle.h>

namespace IceServant { namespace Ice { class ServantS; } }

namespace Ice
{

void ICE_API _incRef(::IceServant::Ice::ServantS*);
void ICE_API _decRef(::IceServant::Ice::ServantS*);

typedef Handle< ::IceServant::Ice::ServantS > Servant;

}

namespace IceServant { namespace Ice
{

class ICE_API ServantS : virtual public ::Ice::SimpleShared
{
public:

protected:

    ServantS();
    virtual ~ServantS();

private:

    ServantS(const ServantS&);
    void operator=(const ServantS&);
};

} }

#endif

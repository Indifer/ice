// **********************************************************************
//
// Copyright (c) 2003-2012 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#include <Ice/Incoming.h>
#include <Ice/IncomingAsync.h>
#include <Ice/IncomingRequest.h>
#include <Ice/ObjectAdapter.h>
#include <Ice/ServantLocator.h>
#include <Ice/ServantManager.h>
#include <Ice/Object.h>
#include <Ice/ConnectionI.h>
#include <Ice/LocalException.h>
#include <Ice/Instance.h>
#include <Ice/Properties.h>
#include <Ice/LoggerUtil.h>
#include <Ice/Protocol.h>
#include <Ice/ReplyStatus.h>
#include <IceUtil/StringUtil.h>

using namespace std;
using namespace Ice;
using namespace IceInternal;

namespace IceUtilInternal
{

extern bool ICE_DECLSPEC_IMPORT printStackTraces;

}

IceInternal::IncomingBase::IncomingBase(Instance* instance, ConnectionI* connection,
                                        const ObjectAdapterPtr& adapter,
                                        bool response, Byte compress, Int requestId) :
    _response(response),
    _compress(compress),
    _os(instance, Ice::currentProtocolEncoding),
    _connection(connection)
{
    _current.adapter = adapter;
    _current.con = _connection;
    _current.requestId = requestId;
}

IceInternal::IncomingBase::IncomingBase(IncomingBase& in) :
    _current(in._current), // copy
    _os(in._os.instance(), Ice::currentProtocolEncoding),
    _interceptorAsyncCallbackQueue(in._interceptorAsyncCallbackQueue) // copy
{
    __adopt(in); // adopt everything else
}

void
IceInternal::IncomingBase::__adopt(IncomingBase& other)
{
    _servant = other._servant;
    other._servant = 0;

    _locator = other._locator;
    other._locator = 0;

    _cookie = other._cookie;
    other._cookie = 0;

    _response = other._response;
    other._response = false;

    _compress = other._compress;
    other._compress = 0;

    _os.swap(other._os);

    _connection = other._connection;
    other._connection = 0;
}

BasicStream* 
IncomingBase::__startWriteParams()
{
    if(_response)
    {
        assert(_os.b.size() == headerSize + 4); // Reply status position.
        assert(_current.encoding >= Ice::Encoding_1_0); // Encoding for reply is known.
        _os.write(static_cast<Ice::Byte>(0));
        _os.startWriteEncaps(_current.encoding);
    }
    
    //
    // We still return the stream even if no response is expected. The
    // servant code might still write some out parameters if for
    // example a method with out parameters somehow and erroneously
    // invoked as oneway (or if the invocation is invoked on a 
    // blobject and the blobject erroneously writes a response).
    //
    return &_os;
}

void 
IncomingBase::__endWriteParams(bool ok)
{
    if(_response)
    {
        *(_os.b.begin() + headerSize + 4) = ok ? replyOK : replyUserException; // Reply status position.
        _os.endWriteEncaps();
    }
}

void 
IncomingBase::__writeEmptyParams()
{
    if(_response)
    {
        assert(_os.b.size() == headerSize + 4); // Reply status position.
        assert(_current.encoding >= Ice::Encoding_1_0); // Encoding for reply is known.
        _os.write(replyOK);
        _os.writeEmptyEncaps(_current.encoding);
    }
}

void 
IncomingBase::__writeParamEncaps(const Byte* v, Ice::Int sz, bool ok)
{
    if(_response)
    {
        assert(_os.b.size() == headerSize + 4); // Reply status position.
        assert(_current.encoding >= Ice::Encoding_1_0); // Encoding for reply is known.
        _os.write(ok ? replyOK : replyUserException);
        if(sz == 0)
        {
            _os.writeEmptyEncaps(_current.encoding);
        }
        else
        {
            _os.writeEncaps(v, sz);
        }
    }
}

void
IceInternal::IncomingBase::__warning(const Exception& ex) const
{
    Warning out(_os.instance()->initializationData().logger);

    out << "dispatch exception: " << ex;
    out << "\nidentity: " << _os.instance()->identityToString(_current.id);
    out << "\nfacet: " << IceUtilInternal::escapeString(_current.facet, "");
    out << "\noperation: " << _current.operation;

    if(_connection)
    {
        Ice::ConnectionInfoPtr connInfo = _connection->getInfo();
        Ice::IPConnectionInfoPtr ipConnInfo = Ice::IPConnectionInfoPtr::dynamicCast(connInfo);
        if(ipConnInfo)
        {
            out << "\nremote host: " << ipConnInfo->remoteAddress + " remote port: " << ipConnInfo->remotePort;
        }
    }
}

void
IceInternal::IncomingBase::__warning(const string& msg) const
{
    Warning out(_os.instance()->initializationData().logger);

    out << "dispatch exception: " << msg;
    out << "\nidentity: " << _os.instance()->identityToString(_current.id);
    out << "\nfacet: " << IceUtilInternal::escapeString(_current.facet, "");
    out << "\noperation: " << _current.operation;

    if(_connection)
    {
        Ice::ConnectionInfoPtr connInfo = _connection->getInfo();
        Ice::IPConnectionInfoPtr ipConnInfo = Ice::IPConnectionInfoPtr::dynamicCast(connInfo);
        if(ipConnInfo)
        {
            out << "\nremote host: " << ipConnInfo->remoteAddress + " remote port: " << ipConnInfo->remotePort;
        }
    }
}

bool
IceInternal::IncomingBase::__servantLocatorFinished()
{
    assert(_locator && _servant);
    try
    {
        _locator->finished(_current, _servant, _cookie);
        return true;
    }
    catch(const UserException& ex)
    {
        assert(_connection);

        //
        // The operation may have already marshaled a reply; we must overwrite that reply.
        //
        if(_response)
        {
            _os.b.resize(headerSize + 4); // Reply status position.
            _os.write(replyUserException);
            _os.startWriteEncaps(_current.encoding);
            _os.write(ex);
            _os.endWriteEncaps();
            _connection->sendResponse(&_os, _compress);
        }
        else
        {
            _connection->sendNoResponse();
        }

        _connection = 0;
    }
    catch(const std::exception& ex)
    {
        __handleException(ex);
    }
    catch(...)
    {
        __handleException();
    }
    return false;
}

void
IceInternal::IncomingBase::__handleException(const std::exception& exc)
{
    assert(_connection);

    if(dynamic_cast<const RequestFailedException*>(&exc))
    {
        RequestFailedException* rfe =
            const_cast<RequestFailedException*>(dynamic_cast<const RequestFailedException*>(&exc));

        if(rfe->id.name.empty())
        {
            rfe->id = _current.id;
        }

        if(rfe->facet.empty() && !_current.facet.empty())
        {
            rfe->facet = _current.facet;
        }

        if(rfe->operation.empty() && !_current.operation.empty())
        {
            rfe->operation = _current.operation;
        }

        if(_os.instance()->initializationData().properties->getPropertyAsIntWithDefault("Ice.Warn.Dispatch", 1) > 1)
        {
            __warning(*rfe);
        }

        if(_response)
        {
            _os.b.resize(headerSize + 4); // Reply status position.
            if(dynamic_cast<ObjectNotExistException*>(rfe))
            {
                _os.write(replyObjectNotExist);
            }
            else if(dynamic_cast<FacetNotExistException*>(rfe))
            {
                _os.write(replyFacetNotExist);
            }
            else if(dynamic_cast<OperationNotExistException*>(rfe))
            {
                _os.write(replyOperationNotExist);
            }
            else
            {
                assert(false);
            }

            rfe->id.__write(&_os);

            //
            // For compatibility with the old FacetPath.
            //
            if(rfe->facet.empty())
            {
                _os.write(static_cast<string*>(0), static_cast<string*>(0));
            }
            else
            {
                _os.write(&rfe->facet, &rfe->facet + 1);
            }

            _os.write(rfe->operation, false);

            _connection->sendResponse(&_os, _compress);
        }
        else
        {
            _connection->sendNoResponse();
        }
    }
    else if(const Exception* ex = dynamic_cast<const Exception*>(&exc))
    {

        if(_os.instance()->initializationData().properties->getPropertyAsIntWithDefault("Ice.Warn.Dispatch", 1) > 0)
        {
            __warning(*ex);
        }

        if(_response)
        {
            _os.b.resize(headerSize + 4); // Reply status position.
            if(const UnknownLocalException* ule = dynamic_cast<const UnknownLocalException*>(&exc))
            {
                _os.write(replyUnknownLocalException);
                _os.write(ule->unknown, false);
            }
            else if(const UnknownUserException* uue = dynamic_cast<const UnknownUserException*>(&exc))
            {
                _os.write(replyUnknownUserException);
                _os.write(uue->unknown, false);
            }
            else if(const UnknownException* ue = dynamic_cast<const UnknownException*>(&exc))
            {
                _os.write(replyUnknownException);
                _os.write(ue->unknown, false);
            }
            else if(const LocalException* le = dynamic_cast<const LocalException*>(&exc))
            {
                _os.write(replyUnknownLocalException);
                ostringstream str;
                str << *le;
#ifdef __GNUC__
                if(IceUtilInternal::printStackTraces)
                {
                    str <<  '\n' << ex->ice_stackTrace();
                }
#endif
                _os.write(str.str(), false);
            }
            else if(const UserException* ue = dynamic_cast<const UserException*>(&exc))
            {
                _os.write(replyUnknownUserException);
                ostringstream str;
                str << *ue;
#ifdef __GNUC__
                if(IceUtilInternal::printStackTraces)
                {
                    str <<  '\n' << ex->ice_stackTrace();
                }
#endif
                _os.write(str.str(), false);
            }
            else
            {
                _os.write(replyUnknownException);
                ostringstream str;
                str << *ex;
                _os.write(str.str(), false);
            }
            _connection->sendResponse(&_os, _compress);
        }
        else
        {
            _connection->sendNoResponse();
        }
    }
    else
    {
        if(_os.instance()->initializationData().properties->getPropertyAsIntWithDefault("Ice.Warn.Dispatch", 1) > 0)
        {
            __warning(string("std::exception: ") + exc.what());
        }

        if(_response)
        {
            _os.b.resize(headerSize + 4); // Reply status position.
            _os.write(replyUnknownException);
            ostringstream str;
            str << "std::exception: " << exc.what();
            _os.write(str.str(), false);
            _connection->sendResponse(&_os, _compress);
        }
        else
        {
            _connection->sendNoResponse();
        }
    }

    _connection = 0;
}

void
IceInternal::IncomingBase::__handleException()
{
    if(_os.instance()->initializationData().properties->getPropertyAsIntWithDefault("Ice.Warn.Dispatch", 1) > 0)
    {
        __warning("unknown c++ exception");
    }

    assert(_connection);

    if(_response)
    {
        _os.b.resize(headerSize + 4); // Reply status position.
        _os.write(replyUnknownException);
        string reason = "unknown c++ exception";
        _os.write(reason, false);
        _connection->sendResponse(&_os, _compress);
    }
    else
    {
        _connection->sendNoResponse();
    }

    _connection = 0;
}


IceInternal::Incoming::Incoming(Instance* instance, ConnectionI* connection, const ObjectAdapterPtr& adapter,
                                bool response, Byte compress, Int requestId) :
    IncomingBase(instance, connection, adapter, response, compress, requestId),
    _inParamPos(0)
{
    //
    // Prepare the response if necessary.
    //
    if(response)
    {
        _os.writeBlob(replyHdr, sizeof(replyHdr));
        
        //
        // Add the request ID.
        //
        _os.write(requestId);
    }
}


void
IceInternal::Incoming::push(const Ice::DispatchInterceptorAsyncCallbackPtr& cb)
{
    _interceptorAsyncCallbackQueue.push_front(cb);
}

void
IceInternal::Incoming::pop()
{
    _interceptorAsyncCallbackQueue.pop_front();
}

void
IceInternal::Incoming::startOver()
{
    if(_inParamPos == 0)
    {
        //
        // That's the first startOver, so almost nothing to do
        //
        _inParamPos = _is->i;
    }
    else
    {
        killAsync();

        //
        // Let's rewind _is and clean-up _os
        //
        _is->i = _inParamPos;
        _os.b.resize(headerSize + 4); // Reply status position.
    }
}

void
IceInternal::Incoming::killAsync()
{
    //
    // Always runs in the dispatch thread
    //
    if(_cb != 0)
    {
        //
        // May raise ResponseSentException
        //
        _cb->__deactivate(*this);
        _cb = 0;
    }
}

void
IceInternal::Incoming::setActive(IncomingAsync& cb)
{
    assert(_cb == 0);
    _cb = &cb; // acquires a ref-count
}

void
IceInternal::Incoming::invoke(const ServantManagerPtr& servantManager, BasicStream* stream)
{
    _is = stream;

    //
    // Read the current.
    //
    _current.id.__read(_is);

    //
    // For compatibility with the old FacetPath.
    //
    string facet;
    {
        vector<string> facetPath;
        _is->read(facetPath);
        if(!facetPath.empty())
        {
            if(facetPath.size() > 1)
            {
                throw MarshalException(__FILE__, __LINE__);
            }
            facet.swap(facetPath[0]);
        }
    }
    _current.facet.swap(facet);

    _is->read(_current.operation, false);

    Byte b;
    _is->read(b);
    _current.mode = static_cast<OperationMode>(b);

    Int sz = _is->readSize();
    while(sz--)
    {
        pair<const string, string> pr;
        _is->read(const_cast<string&>(pr.first));
        _is->read(pr.second);
        _current.ctx.insert(_current.ctx.end(), pr);
    }

    //
    // Don't put the code above into the try block below. Exceptions
    // in the code above are considered fatal, and must propagate to
    // the caller of this operation.
    //

    if(servantManager)
    {
        _servant = servantManager->findServant(_current.id, _current.facet);
        if(!_servant)
        {
            _locator = servantManager->findServantLocator(_current.id.category);
            if(!_locator && !_current.id.category.empty())
            {
                _locator = servantManager->findServantLocator("");
            }

            if(_locator)
            {
                try
                {
                    _servant = _locator->locate(_current, _cookie);
                }
                catch(const UserException& ex)
                {
                    Ice::EncodingVersion encoding = _is->skipEncaps(); // Required for batch requests.

                    if(_response)
                    {
                        _os.write(replyUserException);
                        _os.startWriteEncaps(encoding);
                        _os.write(ex);
                        _os.endWriteEncaps();
                        _connection->sendResponse(&_os, _compress);
                    }
                    else
                    {
                        _connection->sendNoResponse();
                    }

                    _connection = 0;
                    return;
                }
                catch(const std::exception& ex)
                {
                    _is->skipEncaps(); // Required for batch requests.
                    __handleException(ex);
                    return;
                }
                catch(...)
                {
                    _is->skipEncaps(); // Required for batch requests.
                    __handleException();
                    return;
                }
            }
        }
    }

    try
    {
        if(_servant)
        {
            //
            // DispatchAsync is a "pseudo dispatch status", used internally only
            // to indicate async dispatch.
            //
            if(_servant->__dispatch(*this, _current) == DispatchAsync)
            {            
                //
                // If this was an asynchronous dispatch, we're done here.
                //
                return;
            }

            if(_locator && !__servantLocatorFinished())
            {
                return;
            }
        }
        else
        {
            //
            // Skip the input parameters, this is required for reading
            // the next batch request if dispatching batch requests.
            //
            _is->skipEncaps(); 

            if(servantManager && servantManager->hasServant(_current.id))
            {
                throw FacetNotExistException(__FILE__, __LINE__, _current.id, _current.facet, _current.operation);
            }
            else
            {
                throw ObjectNotExistException(__FILE__, __LINE__, _current.id, _current.facet, _current.operation);
            }
        }
    }
    catch(const std::exception& ex)
    {
        if(_servant && _locator && !__servantLocatorFinished())
        {
            return;
        }
        __handleException(ex);
        return;
    }
    catch(...)
    {
        if(_servant && _locator && !__servantLocatorFinished())
        {
            return;
        }
        __handleException();
        return;
    }

    //
    // Don't put the code below into the try block above. Exceptions
    // in the code below are considered fatal, and must propagate to
    // the caller of this operation.
    //

    if(_response)
    {
        _connection->sendResponse(&_os, _compress);
    }
    else
    {
        _connection->sendNoResponse();
    }

    _connection = 0;
}

bool
IceInternal::IncomingRequest::isCollocated()
{
    return false;
}

const Current&
IceInternal::IncomingRequest::getCurrent()
{
    return _in.getCurrent();
}

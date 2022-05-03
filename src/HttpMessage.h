#ifndef _HTTPMESSAGE_H_
#define _HTTPMESSAGE_H_

#include "ProtocolMessage.h"

namespace protocol
{

class HttpMessage : public ProtocolMessage {

};

class HttpRequest : public HttpMessage {

};

class HttpResponse : public HttpMessage {

};


}


#endif


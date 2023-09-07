#pragma once

#include <string>

namespace opticloud
{

struct ConnectionParameters
{    
    std::string  url;
    std::string  hostAlias;
    std::string  accessToken;
    bool         doVerifySslCertificates{true};
    bool         doReuseExistingConnection{true};
    unsigned int progressTimeout_s{300};
    unsigned int heartbeatInterval_s{60};
};

}

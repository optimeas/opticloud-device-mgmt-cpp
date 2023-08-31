#pragma once

#include <string>

namespace opticloud
{

struct ConnectionParameters
{    
    std::string  url;
    std::string  hostAlias;
    std::string  accessToken;
    bool         disableSslVerification{false};
    unsigned int progressTimeout_s{300};
};

}

#ifndef  MCD_APP_UTILS_H
#define  MCD_APP_UTILS_H

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <sstream>
#include <string>
#include <stdlib.h>

namespace app {

inline uint32_t string_to_ip(const std::string& str_ip) {
    struct in_addr addr;
    inet_pton(AF_INET, str_ip.c_str(), &addr);
    return addr.s_addr;
}

inline std::string ip_to_string(uint32_t ip) {
    struct in_addr addr;
    char buf[32] = {0};
    addr.s_addr = ip;
    inet_ntop(AF_INET, &addr, buf, sizeof(buf));
    return std::string(buf);
}

inline std::string NumberToString(int n) {
    std::stringstream ss;
    ss << n;
    return ss.str();
}

inline int StringToInt(const std::string& s) {
    return atoi(s.c_str());
}

}  // namespace app

#endif  // MCD_APP_UTILS_H

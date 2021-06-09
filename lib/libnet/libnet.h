#ifndef __libnet__
#define __libnet__

#include <string>

void	netGetIP(std::string &dev, std::string &ip, std::string &mask, std::string &brdcast);
void	netGetDefaultRoute(std::string &ip);
void	netGetHostname(std::string &host);
void	netSetHostname(std::string &host);
void	netSetNameserver(std::string &ip);
void	netGetNameserver(std::string &ip);
void	netGetMacAddr(std::string &ifname, unsigned char *mac);

#endif

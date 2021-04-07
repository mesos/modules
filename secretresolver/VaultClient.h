#ifndef LIBVAULT_VAULTCLIENT_H
#define LIBVAULT_VAULTCLIENT_H

#include <unordered_map>
#include "HttpClient.h"

void autoRenewToken(HttpClient httpClient, std::string addr, std::string token, int delaySeconds);

class VaultClient {
private:
    std::string addr;
    std::string token;
    std::string role;
    std::string secret;
    int refreshDelaySeconds;
    std::string prefix;
    bool debug;
    HttpClient httpClient = (bool)nullptr;
    std::string vaultUrl(std::string path);
    std::string getToken();
public:
    VaultClient(std::string addr, std::string prefix, std::string token, std::string cacert);
    VaultClient(std::string addr, std::string prefix, std::string token, std::string cacert, bool debug);
    VaultClient(std::string addr, std::string prefix, std::string role, std::string secret, int refreshDelaySeconds, std::string cacert);
    VaultClient(std::string addr, std::string prefix, std::string role, std::string secret, int refreshDelaySeconds, std::string cacert, bool debug);
    std::string get(std::string path);
    std::string put(std::string path, std::unordered_map<std::string, std::string> map);
};

#endif //LIBVAULT_VAULTCLIENT_H

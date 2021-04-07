#ifndef LIBVAULT_HTTPCLIENT_H
#define LIBVAULT_HTTPCLIENT_H

class HttpClient {
private:
    std::string cacert;
    bool debug;
public:
    HttpClient();
    HttpClient(bool debug);
    std::string get(std::string url, std::string string);
    std::string post(std::string url, std::string token);
    std::string post(std::string url, std::string token, std::string value);
    void setCacert(std::string cacert);
};

#endif //LIBVAULT_HTTPCLIENT_H

#include <string>
#include <iostream>
#include <chrono>
#include <thread>

#include "VaultClient.h"
#include "HttpClient.h"
#include "json.hpp"
#include <process/process.hpp>
using process::Failure;

using json = nlohmann::json;

VaultClient::VaultClient(std::string addr, std::string prefix, std::string token, std::string cacert) : VaultClient(addr, prefix, token, cacert, false){}

VaultClient::VaultClient(std::string addr, std::string prefix, std::string token, std::string cacert, bool debug) {
	this->addr = addr;
	this->token = token;
	this->prefix = prefix;
	this->httpClient = HttpClient(debug);
	this->httpClient.setCacert(cacert);
}

VaultClient::VaultClient(std::string addr, std::string prefix, std::string role, std::string secret, int refreshDelaySeconds, std::string cacert) : VaultClient(addr, prefix, role, secret, refreshDelaySeconds, cacert, false){}

VaultClient::VaultClient(std::string addr, std::string prefix, std::string role, std::string secret, int refreshDelaySeconds, std::string cacert, bool debug) {
	VLOG(2) << "VaultClient init by role " + role ;
	this->addr = addr;
	this->role = role;
	this->secret = secret;
	this->refreshDelaySeconds = refreshDelaySeconds;
	this->prefix = prefix;
	this->httpClient = HttpClient(debug);
	this->httpClient.setCacert(cacert);
}

std::string VaultClient::vaultUrl(std::string path) {
	return addr + "/v1/" + prefix + path;
}

void autoRenewToken(HttpClient httpClient,std::string addr, std::string token, int delaySeconds){
	while(true) {
		std::this_thread::sleep_for(std::chrono::seconds(delaySeconds));
		httpClient.post(addr + "/v1/auth/token/renew-self", token);
	}
}

std::string VaultClient::getToken() {
	if(token.empty()) {
		std::string b = "{\"role_id\":\"" + role + "\",\"secret_id\":\"" + secret + "\"}";
		VLOG(2) << "login body " + b;
		std::string res = httpClient.post(addr + "/v1/auth/approle/login", "", b);
		VLOG(2) << "login result " + res;
		json j = json::parse(res);
		if(j.count("auth") == 0) {
			return "";	
		}
		token = j["auth"]["client_token"];
		if ( ! token.empty()) {
			std::thread t(autoRenewToken, httpClient, addr, token, refreshDelaySeconds); 
			t.detach();
		}

	}
	return token;
}

std::string VaultClient::get(std::string path) {
	std::string t = getToken();
	VLOG(2) << "token " + t;
	if(t.empty()) {
		return "";
	}
	return httpClient.get(vaultUrl(path), t);
}

std::string VaultClient::put(std::string path, std::unordered_map<std::string, std::string> map) {
	json j;
	j["data"] = json::object();
	std::for_each(map.begin(), map.end(), [&](std::pair<std::string, std::string> pair) {
			j["data"][pair.first] = pair.second;
			});

	return httpClient.post(vaultUrl(path), getToken(), j.dump());
}




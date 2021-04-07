#include <curl/curl.h>
#include <iostream>

#include "HttpClient.h"

static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	((std::string *) userp)->append((char *) contents, size * nmemb);
	return size * nmemb;
}

HttpClient::HttpClient() : HttpClient(false) {}

HttpClient::HttpClient(bool debug) {
	this->debug = debug;
}

void HttpClient::setCacert(std::string cacert) {
	this->cacert = cacert;
}

std::string HttpClient::get(std::string url, std::string token) {

	CURL *curl;
	std::string readBuffer;

	curl = curl_easy_init();
	if (curl) {
		struct curl_slist *chunk = nullptr;
		chunk = curl_slist_append(chunk, ("X-Vault-Token: " + token).c_str());

		// TODO: SSL verify host and peer
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
		curl_easy_setopt(curl, CURLOPT_CAINFO, cacert.c_str());

		if (debug) {
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		}

		CURLcode res = curl_easy_perform(curl);
		if (res != CURLE_OK) {
			std::cout << "GET " << url << " failed: " << curl_easy_strerror(res) << std::endl;
		}

		curl_easy_cleanup(curl);
		curl_slist_free_all(chunk);
	}

	return readBuffer;
}

std::string HttpClient::post(std::string url, std::string token) {
	return post(url,token,"");
}

std::string HttpClient::post(std::string url, std::string token, std::string value) {

	CURL *curl;
	CURLcode res = CURLE_SEND_ERROR;
	std::string readBuffer;

	curl = curl_easy_init();
	if (curl) {
		struct curl_slist *chunk = nullptr;
		if(!token.empty()) {
			chunk = curl_slist_append(chunk, ("X-Vault-Token: " + token).c_str());
		}
		chunk = curl_slist_append(chunk, "Accept: application/json");
		chunk = curl_slist_append(chunk, "Content-Type: application/json");

		// TODO: SSL verify host and peer
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, value.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
		curl_easy_setopt(curl, CURLOPT_CAINFO, cacert.c_str());

		if (debug) {
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		}

		res = curl_easy_perform(curl);

		if (res != CURLE_OK) {
			std::cout << "POST " << url << " failed: " << curl_easy_strerror(res) << std::endl;
		}

		curl_easy_cleanup(curl);
		curl_slist_free_all(chunk);
	}

	return readBuffer;
}

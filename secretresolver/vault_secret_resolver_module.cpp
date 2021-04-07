/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <mesos/mesos.hpp>
#include <mesos/module.hpp>

#include <mesos/module/secret_resolver.hpp>

#include <process/future.hpp>
#include <process/process.hpp>
#include <process/protobuf.hpp>

#include "VaultClient.h"
#include "json.hpp"
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>

using namespace mesos;
using process::Failure;
using json = nlohmann::json;

class VaultSecretsResolver : public SecretResolver {
private:
    hashmap <std::string, std::string> flags;
    VaultClient *vaultClient;
public:
    Try <Nothing> initialize(const mesos::Parameters &parameters) {
        foreach(
        const mesos::Parameter &parameter, parameters.parameter()) {
            if (parameter.has_key() && parameter.has_value()) {
                flags[parameter.key()] = parameter.value();
            } else {
                return Error("Invalid key-value parameters");
            }
        }
        if (!flags.contains("vault_addr")) {
            return Error("vault_addr is required");
        }
        if (!flags.contains("vault_cacert")) {
            return Error("vault_cacert is required");
        }
        if (!flags.contains("vault_token") && !flags.contains("vault_role_id")) {
            return Error("vault_token or vault_role_id (prefered) is required");
        }
        if (flags.contains("vault_role_id") && !flags.contains("vault_role_secret")) {
            return Error("vault_role_secret is required");
        }
        std::string prefix = "secret/";

        if (flags.contains("vault_kv_prefix")) {
            prefix = flags["vault_kv_prefix"];
        }

        if (flags.contains("vault_kv_version") && flags["vault_kv_version"] == "2") {
	    prefix.append("data/");
        }

	// TODO add config var for token refresh delay
	int refreshDelaySeconds = 600;

        bool debug = (flags.contains("debug") && flags["debug"] == "true");

        VLOG(1) << "vault_addr: " + flags["vault_addr"] + ",prefix: " + prefix + ",token: " + flags["vault_token"] +
		   ",role: " + flags["vault_role_id"] +
                   ",cacert: " + flags["vault_cacert"] + ",debug: " << std::boolalpha << debug << std::endl;
	if(flags.contains("vault_token") && ! flags["vault_token"].empty()) {
        	vaultClient = new VaultClient(flags["vault_addr"], prefix, flags["vault_token"], flags["vault_cacert"], debug);
	} else {
        	vaultClient = new VaultClient(flags["vault_addr"], prefix, flags["vault_role_id"], flags["vault_role_secret"], refreshDelaySeconds, flags["vault_cacert"], debug);
	}
        return Nothing();
    }

    ~VaultSecretsResolver() override {}

    process::Future <Secret::Value> resolve(const Secret &secret) const override {
        if (secret.has_value()) {
            VLOG(2) << "[vault secret module] value: " + secret.value().data();
            return secret.value();
        }
        if (!secret.has_reference()) {
            return Failure("[vault secret module] Secret has no reference");
        }
        std::string name = secret.reference().name();
        std::string key = secret.reference().key();
        VLOG(2) << "[vault secret module] reference name: " + name;
        VLOG(2) << "[vault secret module] reference key: " + key;

        std::string result = vaultClient->get(name);
        if (result.empty()) {
            std::ostringstream stringStream;
            stringStream << "[vault secret module] Empty result when fetching secret from reference [" + name + "]" ;
            LOG(WARNING) << stringStream.str();
            return Failure(stringStream.str());
	}

        VLOG(2) << "[vault secret module] backend result for reference [" + name + "] is [" + result + "]";

        auto j = json::parse(result);
        VLOG(2) << "[vault secret module] json parsed data: " + j.dump();

        if (j.count("data") == 0) {
	    std::vector<std::string> parts;
	    boost::algorithm::split(parts, name, [](char c){return c == '/';});
	    parts.erase(parts.end()-2);
	    name = boost::algorithm::join(parts, "/");

	    VLOG(2) << "[vault secret module] Cannot find secret, try parent: " + name;

            result = vaultClient->get(name);

            if (result.empty()) {
                std::ostringstream stringStream;
                stringStream << "[vault secret module] Cannot get secret from parent reference [" + secret.reference().name() + "] or [" + name + "]" ;
                LOG(WARNING) << stringStream.str();
                return Failure(stringStream.str());
	    }
        
	    VLOG(2) << "[vault secret module] backend result for parent reference [" + name + "] is [" + result + "]";

            j = json::parse(result);
            VLOG(2) << "[vault secret module] json parsed parent data: " + j.dump();
        }

        Secret::Value value;

        if (j["data"]["data"].count(key) > 0 ) {
		// kv v2
       		value.set_data(j["data"]["data"][key].get<std::string>());
	} else if (j["data"].count(key) > 0) {
		// kv v1
       		value.set_data(j["data"][key].get<std::string>());
	} else {
            std::ostringstream stringStream;
            stringStream << "[vault secret module] Empty secret key from vault reference " + name + "@" + key;
            LOG(WARNING) << stringStream.str();
            return Failure(stringStream.str());
        }

        return value;
    }
};


static SecretResolver *createSecretResolver(const Parameters &parameters) {
    VaultSecretsResolver *vaultModule = new VaultSecretsResolver();
    Try <Nothing> result = vaultModule->initialize(parameters);
    if (result.isError()) {
        delete vaultModule;
        return nullptr;
    }
    return vaultModule;
}


// Declares a Hook module named 'org_apache_mesos_TestHook'.
mesos::modules::Module <SecretResolver> org_apache_mesos_VaultSecretsResolver(
        MESOS_MODULE_API_VERSION,
        MESOS_VERSION,
        "Apache Mesos",
        "modules@mesos.apache.org",
        "Vault secret resolver module.",
        NULL,
        createSecretResolver);

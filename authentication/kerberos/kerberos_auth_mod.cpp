/**
 * Copyright 2014-present Mesosphere Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include <mesos/mesos.hpp>
#include <mesos/module.hpp>

#include <mesos/authentication/authenticatee.hpp>
#include <mesos/authentication/authenticator.hpp>

#include <mesos/module/authenticatee.hpp>
#include <mesos/module/authenticator.hpp>

#include <stout/os.hpp>

#include "authenticatee.hpp"
#include "authenticator.hpp"

using namespace mesos;

using mesos::Authenticatee;
using mesos::Authenticator;

using std::string;

static bool compatible()
{
  return true;
}


std::string getEnvironment(const std::string& key)
{
  const Option<std::string> path = os::getenv(key);
  return path.isSome() ? path.get() : std::string();
}


static Authenticatee* createGSSAPIAuthenticatee(const Parameters& parameters)
{
  mesos::internal::gssapi::GSSAPIAuthenticatee* authenticatee(
      new mesos::internal::gssapi::GSSAPIAuthenticatee());

  // Get user configuration overrides from the environment for
  // backwards compatibility.
  string service = getEnvironment("SASL_SERVICE_NAME");
  string serverPrefix = getEnvironment("SASL_SERVER_PREFIX");

  // Get user configuration overrides from the module parameters.
  foreach (const mesos::Parameter& parameter, parameters.parameter()) {
    if (parameter.has_key() && parameter.has_value()) {
      if (parameter.key() == "service_name") {
        service = parameter.value();
      } else if (parameter.key() == "server_prefix") {
        serverPrefix = parameter.value();
      } else {
        LOG(WARNING) << "com_mesosphere_mesos_GSSAPIAuthenticatee does not "
                     << "support a parameter named '" << parameter.key() << "'";
      }
    }
  }

  authenticatee->prepare(service, serverPrefix);

  return authenticatee;
}


mesos::modules::Module<Authenticatee> com_mesosphere_mesos_GSSAPIAuthenticatee(
    MESOS_MODULE_API_VERSION,
    MESOS_VERSION,
    "Mesosphere",
    "till@mesosphere.io",
    "Kerberos (GSSAPI) SASL authenticatee module.",
    compatible,
    createGSSAPIAuthenticatee);


static Authenticator* createGSSAPIAuthenticator(const Parameters& parameters)
{
  mesos::internal::gssapi::GSSAPIAuthenticator* authenticator(
      new mesos::internal::gssapi::GSSAPIAuthenticator());

  // Get user configuration overrides from the environment for
  // backwards compatibility.
  string service = getEnvironment("SASL_SERVICE_NAME");
  string serverPrefix = getEnvironment("SASL_SERVER_PREFIX");
  string realm = getEnvironment("SASL_REALM");

  // Get user configuration overrides from the module parameters.
  foreach (const mesos::Parameter& parameter, parameters.parameter()) {
    if (parameter.has_key() && parameter.has_value()) {
      if (parameter.key() == "service_name") {
        service = parameter.value();
      } else if (parameter.key() == "server_prefix") {
        serverPrefix = parameter.value();
      } else if (parameter.key() == "realm") {
        realm = parameter.value();
      } else {
        LOG(WARNING) << "com_mesosphere_mesos_GSSAPIAuthenticator does not "
                     << "support a parameter named '" << parameter.key() << "'";
      }
    }
  }

  authenticator->prepare(service, serverPrefix, realm);

  return authenticator;
}


mesos::modules::Module<Authenticator> com_mesosphere_mesos_GSSAPIAuthenticator(
    MESOS_MODULE_API_VERSION,
    MESOS_VERSION,
    "Mesosphere",
    "till@mesosphere.io",
    "Kerberos (GSSAPI) SASL authenticator module.",
    compatible,
    createGSSAPIAuthenticator);

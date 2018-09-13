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


#ifndef __AUTHENTICATION_GSSAPI_AUTHENTICATOR_HPP__
#define __AUTHENTICATION_GSSAPI_AUTHENTICATOR_HPP__

#include <string>

#include <mesos/mesos.hpp>

#include <mesos/module/authenticator.hpp>

#include <process/future.hpp>
#include <process/id.hpp>
#include <process/process.hpp>

#include <stout/option.hpp>
#include <stout/try.hpp>

namespace mesos {
namespace internal {
namespace gssapi {

// Forward declarations.
class GSSAPIAuthenticatorProcess;

class GSSAPIAuthenticator : public Authenticator
{
public:
  GSSAPIAuthenticator();

  virtual ~GSSAPIAuthenticator();

  void prepare(const std::string& service_,
               const std::string& serverPrefix_,
               const std::string& realm_);

  virtual Try<Nothing> initialize(const Option<Credentials>& credentials);

  virtual process::Future<Option<std::string>> authenticate(
      const process::UPID& pid);

private:
  GSSAPIAuthenticatorProcess* process;

  std::string service;
  std::string serverPrefix;
  std::string realm;
};

} // namespace cram_md5 {
} // namespace internal {
} // namespace mesos {

#endif // __AUTHENTICATION_GSSAPI_AUTHENTICATOR_HPP__
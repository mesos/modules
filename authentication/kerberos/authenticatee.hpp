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


#ifndef __AUTHENTICATION_GSSAPI_AUTHENTICATEE_HPP__
#define __AUTHENTICATION_GSSAPI_AUTHENTICATEE_HPP__

#include <string>

#include <mesos/authentication/authenticatee.hpp>

#include <process/future.hpp>
#include <process/id.hpp>
#include <process/process.hpp>

namespace mesos {
namespace internal {
namespace gssapi {

// Forward declaration.
class GSSAPIAuthenticateeProcess;


class GSSAPIAuthenticatee : public Authenticatee
{
public:
  GSSAPIAuthenticatee();

  virtual ~GSSAPIAuthenticatee();

  void prepare(const std::string& service,
               const std::string& serverPrefix);

  virtual process::Future<bool> authenticate(
    const process::UPID& pid,
    const process::UPID& clientPid,
    const mesos::Credential& credential);

private:
  GSSAPIAuthenticateeProcess* process;

  std::string principal;
  std::string service;
  std::string serverPrefix;
};

} // namespace gssapi {
} // namespace internal {
} // namespace mesos {

#endif //__AUTHENTICATION_GSSAPI_AUTHENTICATEE_HPP__

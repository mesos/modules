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


#include <stddef.h>   // For size_t needed by sasl.h.

#include <string>
#include <vector>

#include <sasl/sasl.h>
#include <sasl/saslplug.h>

#include <mesos/mesos.hpp>

#include <mesos/module/authenticator.hpp>

#include <process/defer.hpp>
#include <process/future.hpp>
#include <process/id.hpp>
#include <process/once.hpp>
#include <process/process.hpp>
#include <process/protobuf.hpp>

#include <stout/check.hpp>
#include <stout/net.hpp>

#include "authenticator.hpp"

// We need to disable the deprecation warnings as Apple has decided
// to deprecate all of CyrusSASL's functions with OS 10.11
// (see MESOS-3030). We are using GCC pragmas also for covering clang.
#ifdef __APPLE__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace mesos {
namespace internal {
namespace gssapi {

using namespace process;

using std::string;

class GSSAPIAuthenticatorSessionProcess
  : public ProtobufProcess<GSSAPIAuthenticatorSessionProcess>
{
public:
  explicit GSSAPIAuthenticatorSessionProcess(
      const UPID& pid_,
      const string& service_,
      const string& serverPrefix_,
      const string& realm_)
      : ProcessBase(ID::generate("gssapi_authenticator_session")),
        status(READY),
        pid(pid_),
        service(service_),
        serverPrefix(serverPrefix_),
        realm(realm_),
        connection(NULL) {}

  virtual ~GSSAPIAuthenticatorSessionProcess()
  {
    if (connection != NULL) {
      sasl_dispose(&connection);
    }
  }

  virtual void finalize()
  {
    discarded(); // Fail the promise.
  }

  Future<Option<string>> authenticate()
  {
    if (status != READY) {
      return promise.future();
    }

    // 'service', 'serverPrefix' as well as 'realm' may be supplied
    // as overrides.
    if (!service.empty()) {
      LOG(INFO) << "SASL service name: " << service;
    }
    const char* service_ = service.empty() ? "mesos" : service.c_str();

    string server = "";
    if (!serverPrefix.empty()) {
      Try<string> hostname = net::hostname();
      if (hostname.isError()) {
        status = ERROR;
        promise.fail("Failed to resolve hostname: " + hostname.error());
        return promise.future();
      }
      server = serverPrefix + hostname.get();
      LOG(INFO) << "SASL connecting to server: " << server;
    }
    const char* server_ = server.empty() ? NULL : server.c_str();

    LOG(INFO) << "SASL using realm: " << realm;
    const char* realm_ = realm.empty() ? NULL : realm.c_str();

    LOG(INFO) << "Creating new server SASL connection";

    int result = sasl_server_new(
        service_,   // Registered name of service.
        server_,    // Server's FQDN; NULL uses gethostname().
        realm_,     // The user realm used for password lookups;
                    // NULL means default to FQDN.
        NULL, NULL, // IP address information strings.
        NULL,       // Callbacks supported only for this connection.
        0,          // Security flags (security layers are enabled
                    // using security properties, separately).
        &connection);

    if (result != SASL_OK) {
      string error = "Failed to create server SASL connection: ";
      error += sasl_errstring(result, NULL, NULL);
      LOG(ERROR) << error;
      AuthenticationErrorMessage message;
      message.set_error(error);
      send(pid, message);
      status = ERROR;
      promise.fail(error);
      return promise.future();
    }

    // Get the list of mechanisms.
    const char* output = NULL;
    unsigned length = 0;
    int count = 0;

    result = sasl_listmech(
        connection,  // The context for this connection.
        NULL,        // Not supported.
        "",          // What to prepend to the output string.
        ",",         // What to separate mechanisms with.
        "",          // What to append to the output string.
        &output,     // The output string.
        &length,     // The length of the output string.
        &count);     // The count of the mechanisms in output.

    if (result != SASL_OK || output == NULL) {
      string error = "Failed to get list of mechanisms: ";
      LOG(WARNING) << error << sasl_errstring(result, NULL, NULL);
      AuthenticationErrorMessage message;
      error += sasl_errdetail(connection);
      message.set_error(error);
      send(pid, message);
      status = ERROR;
      promise.fail(error);
      return promise.future();
    }

    std::vector<string> mechanisms = strings::tokenize(output, ",");
    LOG(INFO) << "Available mechanisms: " << output;

    // Send authentication mechanisms.
    AuthenticationMechanismsMessage message;
    foreach (const string& mechanism, mechanisms) {
      message.add_mechanisms(mechanism);
    }

    send(pid, message);

    status = STARTING;

    // Stop authenticating if nobody cares.
    promise.future().onDiscard(defer(self(), &Self::discarded));

    return promise.future();
  }

protected:
  virtual void initialize()
  {
    link(pid); // Don't bother waiting for a lost authenticatee.

    // Anticipate start and steps messages from the client.
    install<AuthenticationStartMessage>(
        &GSSAPIAuthenticatorSessionProcess::start,
        &AuthenticationStartMessage::mechanism,
        &AuthenticationStartMessage::data);

    install<AuthenticationStepMessage>(
        &GSSAPIAuthenticatorSessionProcess::step,
        &AuthenticationStepMessage::data);
  }

  virtual void exited(const UPID& _pid)
  {
    if (pid == _pid) {
      status = ERROR;
      promise.fail("Failed to communicate with authenticatee");
    }
  }

  void start(const string& mechanism, const string& data)
  {
    if (status != STARTING) {
      AuthenticationErrorMessage message;
      message.set_error("Unexpected authentication 'start' received");
      send(pid, message);
      status = ERROR;
      promise.fail(message.error());
      return;
    }

    LOG(INFO) << "Received SASL authentication start with "
              << mechanism.c_str() << " mechanism";

    // Start the server.
    const char* output = NULL;
    unsigned length = 0;

    int result = sasl_server_start(
        connection,
        mechanism.c_str(),
        data.length() == 0 ? NULL : data.data(),
        data.length(),
        &output,
        &length);

    handle(result, output, length);
  }

  void step(const string& data)
  {
    if (status != STEPPING) {
      AuthenticationErrorMessage message;
      message.set_error("Unexpected authentication 'step' received");
      send(pid, message);
      status = ERROR;
      promise.fail(message.error());
      return;
    }

    LOG(INFO) << "Received SASL authentication step";

    const char* output = NULL;
    unsigned length = 0;

    int result = sasl_server_step(
        connection,
        data.length() == 0 ? NULL : data.data(),
        data.length(),
        &output,
        &length);

    handle(result, output, length);
  }

  void discarded()
  {
    status = DISCARDED;
    promise.fail("Authentication discarded");
  }

private:
  // Helper for handling result of server start and step.
  void handle(int result, const char* output, unsigned length)
  {
    if (result == SASL_OK) {
      char *name;

      result = sasl_getprop(connection, SASL_USERNAME, (const void **)&name);

      if (result != SASL_OK) {
        LOG(ERROR) << "Failed to retrieve principal after successful "
                   << "authentication: " << sasl_errstring(result, NULL, NULL);
        AuthenticationErrorMessage message;
        std::string error(sasl_errdetail(connection));
        message.set_error(error);
        send(pid, message);
        status = ERROR;
        promise.fail(error);
        return;
      } else {
        principal = name;
      }

      LOG(INFO) << "Authentication success";
      // Note that we're not using SASL_SUCCESS_DATA which means that
      // we should not have any data to send when we get a SASL_OK.
      CHECK(output == NULL);
      send(pid, AuthenticationCompletedMessage());
      status = COMPLETED;
      promise.set(principal);
    } else if (result == SASL_CONTINUE) {
      LOG(INFO) << "Authentication requires more steps";
      AuthenticationStepMessage message;
      message.set_data(CHECK_NOTNULL(output), length);
      send(pid, message);
      status = STEPPING;
    } else if (result == SASL_NOUSER || result == SASL_BADAUTH) {
      LOG(WARNING) << "Authentication failure: "
                   << sasl_errstring(result, NULL, NULL);
      send(pid, AuthenticationFailedMessage());
      status = FAILED;
      promise.set(Option<string>::none());
    } else {
      LOG(ERROR) << "Authentication error: "
                 << sasl_errstring(result, NULL, NULL);
      AuthenticationErrorMessage message;
      string error(sasl_errdetail(connection));
      message.set_error(error);
      send(pid, message);
      status = ERROR;
      promise.fail(message.error());
    }
  }

  enum {
    READY,
    STARTING,
    STEPPING,
    COMPLETED,
    FAILED,
    ERROR,
    DISCARDED
  } status;

  const UPID pid;

  const string service;
  const string serverPrefix;
  const string realm;

  sasl_conn_t* connection;

  Promise<Option<string>> promise;

  Option<string> principal;
};


class GSSAPIAuthenticatorSession
{
public:
  GSSAPIAuthenticatorSession(const UPID& pid,
                             const string& service,
                             const string& serverPrefix,
                             const string& realm)
  {
    process =
      new GSSAPIAuthenticatorSessionProcess(pid, service, serverPrefix, realm);
    spawn(process);
  }

  virtual ~GSSAPIAuthenticatorSession()
  {
    // TODO(vinod): As a short term fix for the race condition #1 in
    // MESOS-1866, we inject the 'terminate' event at the end of the
    // CRAMMD5AuthenticatorProcess queue instead of at the front.
    // The long term fix for this is https://reviews.apache.org/r/25945/.
    terminate(process, false);
    wait(process);
    delete process;
  }

  virtual Future<Option<string>> authenticate()
  {
    return dispatch(process, &GSSAPIAuthenticatorSessionProcess::authenticate);
  }

private:
  GSSAPIAuthenticatorSessionProcess* process;
};


class GSSAPIAuthenticatorProcess :
  public Process<GSSAPIAuthenticatorProcess>
{
public:
  GSSAPIAuthenticatorProcess() :
    ProcessBase(ID::generate("gssapi_authenticator")) {}

  virtual ~GSSAPIAuthenticatorProcess() {}

  Future<Option<string>> authenticate(const UPID& pid,
                                      const string& service,
                                      const string& serverPrefix,
                                      const string& realm)
  {
    VLOG(1) << "Starting authentication session for " << pid;

    if (sessions.contains(pid)) {
      return Failure("Authentication session already active for " +
                     string(pid));
    }

    Owned<GSSAPIAuthenticatorSession> session(
        new GSSAPIAuthenticatorSession(pid, service, serverPrefix, realm));

    sessions.put(pid, session);

    return session->authenticate()
      .onAny(defer(self(), &Self::_authenticate, pid));
  }

  virtual void _authenticate(const UPID& pid)
  {
    VLOG(1) << "Authentication session cleanup for " << pid;

    CHECK(sessions.contains(pid));

    sessions.erase(pid);
  }

private:
  hashmap <UPID, Owned<GSSAPIAuthenticatorSession>> sessions;
};


GSSAPIAuthenticator::GSSAPIAuthenticator() : process(NULL) {}


GSSAPIAuthenticator::~GSSAPIAuthenticator()
{
  if (process != NULL) {
    terminate(process);
    wait(process);
    delete process;
  }
}


Try<Nothing> GSSAPIAuthenticator::initialize(
  const Option<Credentials>& credentials)
{
  static Once* initialize = new Once();
  static Option<Error>* error = new Option<Error>();

  if (process != NULL) {
    return Error("Authenticator initialized already");
  }

  // Thechnically, this guard is not needed as sasl_server_init itself
  // makes sure it only gets initialized once.
  if (!initialize->once()) {
    LOG(INFO) << "Initializing server SASL";

    int result = sasl_server_init(NULL, "mesos");

    if (result != SASL_OK) {
      *error = Error(
          string("Failed to initialize SASL: ") +
          sasl_errstring(result, NULL, NULL));
    }

    initialize->done();
  }

  if (error->isSome()) {
    return error->get();
  }

  process = new GSSAPIAuthenticatorProcess();
  spawn(process);

  return Nothing();
}


void GSSAPIAuthenticator::prepare(const string& service_,
                                  const string& serverPrefix_,
                                  const string& realm_)
{
  service = service_;
  serverPrefix = serverPrefix_;
  realm = realm_;
}


Future<Option<string>> GSSAPIAuthenticator::authenticate(
    const UPID& pid)
{
  if (process == NULL) {
    return Failure("Authenticator not initialized");
  }
  return dispatch(
      process,
      &GSSAPIAuthenticatorProcess::authenticate,
      pid,
      service,
      serverPrefix,
      realm);
}

} // namespace gssapi {
} // namespace internal {
} // namespace mesos {

#ifdef __APPLE__
#pragma GCC diagnostic pop
#endif

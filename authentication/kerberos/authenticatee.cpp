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


#include <stddef.h>

#include <sasl/sasl.h>

#include <mesos/mesos.hpp>

#include <process/defer.hpp>
#include <process/once.hpp>
#include <process/protobuf.hpp>

#include <stout/net.hpp>
#include <stout/os.hpp>
#include <stout/strings.hpp>

#include "authenticatee.hpp"

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

class GSSAPIAuthenticateeProcess
  : public ProtobufProcess<GSSAPIAuthenticateeProcess>
{
public:
  GSSAPIAuthenticateeProcess(const UPID& client_,
                             const string& principal_,
                             const string& service_,
                             const string& serverPrefix_)
    : ProcessBase(ID::generate("authenticatee")),
      principal(principal_),
      service(service_),
      serverPrefix(serverPrefix_),
      client(client_),
      status(READY),
      connection(NULL) {}

  virtual ~GSSAPIAuthenticateeProcess()
  {
    if (connection != NULL) {
      sasl_dispose(&connection);
    }
  }

  virtual void finalize()
  {
    discarded(); // Fail the promise.
  }

  Future<bool> authenticate(const UPID& pid)
  {
    static Once* initialize = new Once();
    static bool initialized = false;

    if (!initialize->once()) {
      LOG(INFO) << "Initializing client SASL";
      int result = sasl_client_init(NULL);
      if (result != SASL_OK) {
        status = ERROR;
        string error(sasl_errstring(result, NULL, NULL));
        promise.fail("Failed to initialize SASL: " + error);
        initialize->done();
        return promise.future();
      }

      initialized = true;

      initialize->done();
    }

    if (!initialized) {
      promise.fail("Failed to initialize SASL");
      return promise.future();
    }

    if (status != READY) {
      return promise.future();
    }

    LOG(INFO) << "Creating new client SASL connection";

    callbacks[0].id = SASL_CB_GETREALM;
    callbacks[0].proc = NULL;
    callbacks[0].context = NULL;

    callbacks[1].id = SASL_CB_USER;
    callbacks[1].proc = (int(*)()) &user;
    callbacks[1].context = (void*) principal.c_str();

    // NOTE: Some SASL mechanisms do not allow/enable "proxying",
    // i.e., authorization. Therefore, some mechanisms send _only_ the
    // authorization name rather than both the user (authentication
    // name) and authorization name. Thus, for now, we assume
    // authorization is handled out-of-band. Consider the
    // SASL_NEED_PROXY flag if we want to reconsider this in the
    // future.
    callbacks[2].id = SASL_CB_AUTHNAME;
    callbacks[2].proc = (int(*)()) &user;
    callbacks[2].context = (void*) principal.c_str();

    callbacks[3].id = SASL_CB_PASS;
    callbacks[3].proc = (int(*)()) &pass;
    callbacks[3].context = (void*) NULL;

    callbacks[4].id = SASL_CB_LIST_END;
    callbacks[4].proc = NULL;
    callbacks[4].context = NULL;

    if (!service.empty()) {
      LOG(INFO) << "SASL service name: " << service;
    }
    const char* service_ = service.empty() ? "mesos" : service.c_str();

    // Resolve server's hostname from pid ip.
    Try<string> hostname = net::getHostname(pid.address.ip);
    if (hostname.isError()) {
      status = ERROR;
      promise.fail("Failed to resolve hostname: " + hostname.error());
      return promise.future();
    }
    string server = hostname.get();

    if (!serverPrefix.empty()) {
      server = serverPrefix + server;
    }
    LOG(INFO) << "SASL connecting to server: " << server;
    const char* server_ = server.c_str();

    int result = sasl_client_new(
        service_,   // Registered name of service.
        server_,    // Server's FQDN.
        NULL, NULL, // IP Address information strings.
        callbacks,  // Callbacks supported only for this connection.
        0,          // Security flags (security layers are enabled
                    // using security properties, separately).
        &connection);

    if (result != SASL_OK) {
      status = ERROR;
      string error(sasl_errstring(result, NULL, NULL));
      promise.fail("Failed to create client SASL connection: " + error);
      return promise.future();
    }

    AuthenticateMessage message;
    message.set_pid(client);
    send(pid, message);

    status = STARTING;

    // Stop authenticating if nobody cares.
    promise.future().onDiscard(defer(self(), &Self::discarded));

    return promise.future();
  }

protected:
  virtual void initialize()
  {
    // Anticipate mechanisms and steps from the server.
    install<AuthenticationMechanismsMessage>(
        &GSSAPIAuthenticateeProcess::mechanisms,
        &AuthenticationMechanismsMessage::mechanisms);

    install<AuthenticationStepMessage>(
        &GSSAPIAuthenticateeProcess::step,
        &AuthenticationStepMessage::data);

    install<AuthenticationCompletedMessage>(
        &GSSAPIAuthenticateeProcess::completed);

    install<AuthenticationFailedMessage>(
        &GSSAPIAuthenticateeProcess::failed);

    install<AuthenticationErrorMessage>(
        &GSSAPIAuthenticateeProcess::error,
        &AuthenticationErrorMessage::error);
  }

  void mechanisms(const std::vector<string>& mechanisms)
  {
    if (status != STARTING) {
      status = ERROR;
      promise.fail("Unexpected authentication 'mechanisms' received");
      return;
    }

    // TODO(benh): Store 'from' in order to ensure we only communicate
    // with the same Authenticator.

    LOG(INFO) << "Received SASL authentication mechanisms: "
              << strings::join(",", mechanisms);

    sasl_interact_t* interact = NULL;
    const char* output = NULL;
    unsigned length = 0;
    const char* mechanism = NULL;

    int result = sasl_client_start(
        connection,
        strings::join(" ", mechanisms).c_str(),
        &interact,     // Set if an interaction is needed.
        &output,       // The output string (to send to server).
        &length,       // The length of the output string.
        &mechanism);   // The chosen mechanism.

    CHECK_NE(SASL_INTERACT, result)
      << "Not expecting an interaction (ID: " << interact->id << ")";

    if (result != SASL_OK && result != SASL_CONTINUE) {
      string error(sasl_errdetail(connection));
      status = ERROR;
      promise.fail("Failed to start the SASL client: " + error);
      return;
    }

    LOG(INFO) << "Attempting to authenticate with mechanism '"
              << mechanism << "'";

    AuthenticationStartMessage message;
    message.set_mechanism(mechanism);
    message.set_data(output, length);

    reply(message);

    status = STEPPING;
  }

  void step(const string& data)
  {
    if (status != STEPPING) {
      status = ERROR;
      promise.fail("Unexpected authentication 'step' received");
      return;
    }

    LOG(INFO) << "Received SASL authentication step";

    sasl_interact_t* interact = NULL;
    const char* output = NULL;
    unsigned length = 0;

    int result = sasl_client_step(
        connection,
        data.length() == 0 ? NULL : data.data(),
        data.length(),
        &interact,
        &output,
        &length);

    CHECK_NE(SASL_INTERACT, result)
      << "Not expecting an interaction (ID: " << interact->id << ")";

    if (result == SASL_OK || result == SASL_CONTINUE) {
      // We don't start the client with SASL_SUCCESS_DATA so we may
      // need to send one more "empty" message to the server.
      AuthenticationStepMessage message;
      if (output != NULL && length > 0) {
        message.set_data(output, length);
      } else {
        message.set_data(NULL, 0);
      }
      reply(message);
    } else {
      status = ERROR;
      string error(sasl_errdetail(connection));
      promise.fail("Failed to perform authentication step: " + error);
    }
  }

  void completed()
  {
    if (status != STEPPING) {
      status = ERROR;
      promise.fail("Unexpected authentication 'completed' received");
      return;
    }

    LOG(INFO) << "Authentication success";

    status = COMPLETED;
    promise.set(true);
  }

  void failed()
  {
    status = FAILED;
    promise.set(false);
  }

  void error(const string& error)
  {
    status = ERROR;
    promise.fail("Authentication error: " + error);
  }

  void discarded()
  {
    status = DISCARDED;
    promise.fail("Authentication discarded");
  }

private:
  static int user(
      void* context,
      int id,
      const char** result,
      unsigned* length)
  {
    CHECK(SASL_CB_USER == id || SASL_CB_AUTHNAME == id);
    *result = static_cast<const char*>(context);
    if (length != NULL) {
      *length = strlen(*result);
    }
    return SASL_OK;
  }

  static int pass(
      sasl_conn_t* connection,
      void* context,
      int id,
      sasl_secret_t** secret)
  {
    CHECK_EQ(SASL_CB_PASS, id);
    if (context == NULL) {
      return SASL_BADAUTH;
    }
    *secret = static_cast<sasl_secret_t*>(context);
    return SASL_OK;
  }

  const string principal;
  const string service;
  const string serverPrefix;

  // PID of the client that needs to be authenticated.
  const UPID client;

  sasl_callback_t callbacks[5];

  enum {
    READY,
    STARTING,
    STEPPING,
    COMPLETED,
    FAILED,
    ERROR,
    DISCARDED
  } status;

  sasl_conn_t* connection;

  Promise<bool> promise;
};


GSSAPIAuthenticatee::GSSAPIAuthenticatee() : process(NULL) {}


GSSAPIAuthenticatee::~GSSAPIAuthenticatee()
{
  if (process != NULL) {
    terminate(process);
    wait(process);
    delete process;
  }
}


void GSSAPIAuthenticatee::prepare(const string& service_,
                                  const string& serverPrefix_)
{
  service = service_;
  serverPrefix = serverPrefix_;
}


Future<bool> GSSAPIAuthenticatee::authenticate(
  const UPID& pid,
  const UPID& client,
  const mesos::Credential& credential)
{
  CHECK(process == NULL);

  CHECK(credential.has_principal());

  process = new GSSAPIAuthenticateeProcess(
      client, credential.principal(), service, serverPrefix);
  spawn(process);

  return dispatch(
      process, &GSSAPIAuthenticateeProcess::authenticate, pid);
}

} // namespace gssapi {
} // namespace internal {
} // namespace mesos {

#ifdef __APPLE__
#pragma GCC diagnostic pop
#endif

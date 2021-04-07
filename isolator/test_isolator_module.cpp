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

#include <string>

#include <mesos/mesos.hpp>

#include <mesos/module/isolator.hpp>

#include <mesos/slave/isolator.hpp>

#include <process/future.hpp>
#include <process/owned.hpp>
#include <process/process.hpp>

#include <stout/try.hpp>
#include <stout/option.hpp>

#include "test_isolator_module.hpp"

using namespace mesos;
using namespace mesos::slave;

// A basic Isolator that keeps track of the pid but doesn't do any resource
// isolation. Subclasses must implement usage() for their appropriate
// resource(s).

Try<mesos::slave::Isolator*> TestIsolatorProcess::create(
    const Parameters& parameters)
{
  return new TestIsolator(process::Owned<TestIsolatorProcess>(
      new TestIsolatorProcess(parameters)));
}

process::Future<Nothing> TestIsolatorProcess::recover(
    const std::vector<ContainerState>& states,
    const hashset<ContainerID>& orphans)
{
  foreach (const ContainerState& run, states) {
    // This should (almost) never occur: see comment in
    // PosixLauncher::recover().
    if (pids.contains(run.container_id())) {
      return process::Failure("Container already recovered");
    }

    pids.put(run.container_id(), run.pid());

    process::Owned<process::Promise<ContainerLimitation>> promise(
        new process::Promise<ContainerLimitation>());
    promises.put(run.container_id(), promise);
  }

  return Nothing();
}

process::Future<Option<mesos::slave::ContainerLaunchInfo>>
  TestIsolatorProcess::prepare(
      const ContainerID& containerId,
      const mesos::slave::ContainerConfig& containerConfig)
{
  if (promises.contains(containerId)) {
    return process::Failure("Container " + stringify(containerId) +
                            " has already been prepared");
  }

  process::Owned<process::Promise<ContainerLimitation>> promise(
      new process::Promise<ContainerLimitation>());
  promises.put(containerId, promise);

  return None();
}

process::Future<Nothing> TestIsolatorProcess::isolate(
    const ContainerID& containerId,
    pid_t pid)
{
  if (!promises.contains(containerId)) {
    return process::Failure("Unknown container: " + stringify(containerId));
  }

  pids.put(containerId, pid);

  return Nothing();
}

process::Future<ContainerLimitation> TestIsolatorProcess::watch(
    const ContainerID& containerId)
{
  if (!promises.contains(containerId)) {
    return process::Failure("Unknown container: " + stringify(containerId));
  }

  return promises[containerId]->future();
}

process::Future<Nothing> TestIsolatorProcess::update(
    const ContainerID& containerId,
    const Resources& resourceRequests,
    const google::protobuf::Map<std::string, Value::Scalar>& resourceLimits = {})
{
  if (!promises.contains(containerId)) {
    return process::Failure("Unknown container: " + stringify(containerId));
  }

  // No resources are actually isolated so nothing to do.
  return Nothing();
}

process::Future<ResourceStatistics> TestIsolatorProcess::usage(
    const ContainerID& containerId)
{
  if (!pids.contains(containerId)) {
    LOG(WARNING) << "No resource usage for unknown container '"
                 << containerId << "'";
  }
  return ResourceStatistics();
}

process::Future<Nothing> TestIsolatorProcess::cleanup(
    const ContainerID& containerId)
{
  if (!promises.contains(containerId)) {
    return process::Failure("Unknown container: " + stringify(containerId));
  }

  // TODO(idownes): We should discard the container's promise here to signal
  // to anyone that holds the future from watch().
  promises.erase(containerId);

  pids.erase(containerId);

  return Nothing();
}


// The sole purpose of this function is just to exercise the
// compatibility logic.
static bool compatible()
{
  return true;
}


static Isolator* createTestIsolator(const Parameters& parameters)
{
  Try<Isolator*> result = TestIsolatorProcess::create(parameters);
  if (result.isError()) {
    return NULL;
  }
  return result.get();
}


// Declares a CPU Isolator module named 'testCpuIsolator'.
mesos::modules::Module<Isolator> org_apache_mesos_TestIsolator(
    MESOS_MODULE_API_VERSION,
    MESOS_VERSION,
    "Apache Mesos",
    "modules@mesos.apache.org",
    "Test Isolator module.",
    compatible,
    createTestIsolator);

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
#include <mesos/module/isolator.hpp>

#include <stout/try.hpp>

using namespace mesos;

using mesos::Isolator;
using mesos::internal::slave::state::RunState;

// A basic IsolatorProcess that keeps track of the pid but doesn't do any
// resource isolation. Subclasses must implement usage() for their appropriate
// resource(s).
class PosixIsolatorProcess : public IsolatorProcess
{
public:
  virtual process::Future<Nothing> recover(
      const std::list<RunState>& state)
  {
    foreach (const RunState& run, state) {
      if (!run.id.isSome()) {
        return process::Failure("ContainerID is required to recover");
      }

      if (!run.forkedPid.isSome()) {
        return process::Failure("Executor pid is required to recover");
      }

      // This should (almost) never occur: see comment in
      // PosixLauncher::recover().
      if (pids.contains(run.id.get())) {
        return process::Failure("Container already recovered");
      }

      pids.put(run.id.get(), run.forkedPid.get());

      process::Owned<process::Promise<Limitation> > promise(
          new process::Promise<Limitation>());
      promises.put(run.id.get(), promise);
    }

    return Nothing();
  }

  virtual process::Future<Option<CommandInfo> > prepare(
      const ContainerID& containerId,
      const ExecutorInfo& executorInfo,
      const std::string& directory,
      const Option<std::string>& user)
  {
    if (promises.contains(containerId)) {
      return process::Failure("Container " + stringify(containerId) +
                              " has already been prepared");
    }

    process::Owned<process::Promise<Limitation> > promise(
        new process::Promise<Limitation>());
    promises.put(containerId, promise);

    return None();
  }

  virtual process::Future<Nothing> isolate(
      const ContainerID& containerId,
      pid_t pid)
  {
    if (!promises.contains(containerId)) {
      return process::Failure("Unknown container: " + stringify(containerId));
    }

    pids.put(containerId, pid);

    return Nothing();
  }

  virtual process::Future<Limitation> watch(
      const ContainerID& containerId)
  {
    if (!promises.contains(containerId)) {
      return process::Failure("Unknown container: " + stringify(containerId));
    }

    return promises[containerId]->future();
  }

  virtual process::Future<Nothing> update(
      const ContainerID& containerId,
      const Resources& resources)
  {
    if (!promises.contains(containerId)) {
      return process::Failure("Unknown container: " + stringify(containerId));
    }

    // No resources are actually isolated so nothing to do.
    return Nothing();
  }

  virtual process::Future<Nothing> cleanup(const ContainerID& containerId)
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

protected:
  hashmap<ContainerID, pid_t> pids;
  hashmap<ContainerID,
          process::Owned<process::Promise<Limitation> > > promises;
};


class PosixCpuIsolatorProcess : public PosixIsolatorProcess
{
public:
  static Try<Isolator*> create()
  {
    process::Owned<IsolatorProcess> process(new PosixCpuIsolatorProcess());

    return new Isolator(process);
  }

  virtual process::Future<ResourceStatistics> usage(
      const ContainerID& containerId)
  {
    if (!pids.contains(containerId)) {
      LOG(WARNING) << "No resource usage for unknown container '"
                   << containerId << "'";
      return ResourceStatistics();
    }

    // Use 'mesos-usage' but only request 'cpus_' values.
    ResourceStatistics usage;
    return usage;
//      mesos::internal::usage(pids.get(containerId).get(), false, true);
//    if (usage.isError()) {
//      return process::Failure(usage.error());
//    }
//    return usage.get();
  }

private:
  PosixCpuIsolatorProcess() {}
};


class PosixMemIsolatorProcess : public PosixIsolatorProcess
{
public:
  static Try<Isolator*> create()
  {
    process::Owned<IsolatorProcess> process(new PosixMemIsolatorProcess());

    return new Isolator(process);
  }

  virtual process::Future<ResourceStatistics> usage(
      const ContainerID& containerId)
  {
    if (!pids.contains(containerId)) {
      LOG(WARNING) << "No resource usage for unknown container '"
                   << containerId << "'";
      return ResourceStatistics();
    }

    ResourceStatistics usage;
    return usage;
//    // Use 'mesos-usage' but only request 'mem_' values.
//    Try<ResourceStatistics> usage =
//      mesos::internal::usage(pids.get(containerId).get(), true, false);
//    if (usage.isError()) {
//      return process::Failure(usage.error());
//    }
//    return usage.get();
  }

private:
  PosixMemIsolatorProcess() {}
};


// The sole purpose of this function is just to exercise the
// compatibility logic.
static bool compatible()
{
  return true;
}


static Isolator* createCpuIsolator(const Parameters& parameters)
{
  Try<Isolator*> result = PosixCpuIsolatorProcess::create();
  if (result.isError()) {
    return NULL;
  }
  return result.get();
}


static Isolator* createMemIsolator(const Parameters& parameters)
{
  Try<Isolator*> result = PosixMemIsolatorProcess::create();
  if (result.isError()) {
    return NULL;
  }
  return result.get();
}


// Declares a CPU Isolator module named 'testCpuIsolator'.
mesos::modules::Module<Isolator> org_apache_mesos_TestCpuIsolator(
    MESOS_MODULE_API_VERSION,
    MESOS_VERSION,
    "Apache Mesos",
    "modules@mesos.apache.org",
    "Test CPU Isolator module.",
    compatible,
    createCpuIsolator);


// Declares a Memory Isolator module named 'testMemIsolator'.
mesos::modules::Module<Isolator> org_apache_mesos_TestMemIsolator(
    MESOS_MODULE_API_VERSION,
    MESOS_VERSION,
    "Apache Mesos",
    "modules@mesos.apache.org",
    "Test Memory Isolator module.",
    NULL, // Do not perform any compatibility check.
    createMemIsolator);

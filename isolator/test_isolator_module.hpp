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

#ifndef __TEST_ISOLATOR_MODULE_HPP__
#define __TEST_ISOLATOR_MODULE_HPP__

#include <mesos/mesos.hpp>

#include <mesos/slave/isolator.hpp>

#include <process/future.hpp>
#include <process/owned.hpp>
#include <process/process.hpp>

#include <stout/try.hpp>
#include <stout/option.hpp>

namespace mesos {

// A basic Isolator that keeps track of the pid but doesn't do any resource
// isolation. Subclasses must implement usage() for their appropriate
// resource(s).

class TestIsolatorProcess : public process::Process<TestIsolatorProcess>
{
public:
  static Try<mesos::slave::Isolator*> create(
      const Parameters& parameters);

  ~TestIsolatorProcess() {}

  process::Future<Nothing> recover(
      const std::list<mesos::slave::ContainerState>& states,
      const hashset<ContainerID>& orphans);

  process::Future<Option<mesos::slave::ContainerPrepareInfo>> prepare(
      const ContainerID& containerId,
      const ExecutorInfo& executorInfo,
      const std::string& directory,
      const Option<std::string>& user);

  process::Future<Nothing> isolate(
      const ContainerID& containerId,
      pid_t pid);

  process::Future<mesos::slave::ContainerLimitation> watch(
      const ContainerID& containerId);

  process::Future<Nothing> update(
      const ContainerID& containerId,
      const Resources& resources);

  process::Future<ResourceStatistics> usage(
      const ContainerID& containerId);

  process::Future<Nothing> cleanup(
      const ContainerID& containerId);

private:
  TestIsolatorProcess(const Parameters& parameters_)
    : parameters(parameters_) {}

  const Parameters parameters;
  hashmap<ContainerID, pid_t> pids;
  hashmap<ContainerID,
    process::Owned<process::Promise<mesos::slave::ContainerLimitation>>>
      promises;
};


class TestIsolator : public mesos::slave::Isolator
{
public:
  TestIsolator(process::Owned<TestIsolatorProcess> process_)
    : process(process_)
  {
    spawn(CHECK_NOTNULL(process.get()));
  }

  virtual ~TestIsolator()
  {
    terminate(process.get());
    wait(process.get());
  }

  virtual process::Future<Option<int>> namespaces()
  {
    return None();
  }

  virtual process::Future<Nothing> recover(
      const std::list<mesos::slave::ContainerState>& states,
      const hashset<ContainerID>& orphans)
  {
    return dispatch(process.get(),
                    &TestIsolatorProcess::recover,
                    states,
                    orphans);
  }

  virtual process::Future<Option<mesos::slave::ContainerPrepareInfo>> prepare(
      const ContainerID& containerId,
      const ExecutorInfo& executorInfo,
      const std::string& directory,
      const Option<std::string>& user)
  {
    return dispatch(process.get(),
                    &TestIsolatorProcess::prepare,
                    containerId,
                    executorInfo,
                    directory,
                    user);
  }

  virtual process::Future<Nothing> isolate(
      const ContainerID& containerId,
      pid_t pid)
  {
    return dispatch(process.get(),
                    &TestIsolatorProcess::isolate,
                    containerId,
                    pid);
  }

  virtual process::Future<mesos::slave::ContainerLimitation> watch(
      const ContainerID& containerId)
  {
    return dispatch(process.get(),
                    &TestIsolatorProcess::watch,
                    containerId);
  }

  virtual process::Future<Nothing> update(
      const ContainerID& containerId,
      const Resources& resources)
  {
    return dispatch(process.get(),
                    &TestIsolatorProcess::update,
                    containerId,
                    resources);
  }

  virtual process::Future<ResourceStatistics> usage(
      const ContainerID& containerId)
  {
    return dispatch(process.get(),
                    &TestIsolatorProcess::usage,
                    containerId);
  }

  virtual process::Future<Nothing> cleanup(
      const ContainerID& containerId)
  {
    return dispatch(process.get(),
                    &TestIsolatorProcess::cleanup,
                    containerId);
  }

private:
  process::Owned<TestIsolatorProcess> process;
};

}

#endif // #ifndef __TEST_ISOLATOR_MODULE_HPP__

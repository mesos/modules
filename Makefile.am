# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License

AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4

# We want to install modules in mesos directory.
pkglibdir = $(libdir)/mesos

# Initialize variables here so we can use += operator everywhere else.
pkglib_LTLIBRARIES =

# Add compiler and linker flags for pthreads.
AM_CXXFLAGS = $(PTHREAD_CFLAGS)
AM_LIBS = $(PTHREAD_LIBS)

# Setup CPPFLAGS that are used for most source files.
AM_CPPFLAGS = $(MESOS_CPPFLAGS) -Wall -Werror

# Library containing the test CRAM-MD5 authentication modules.
# TODO(tillt): Add cyrus-sasl2 dependency while removing it from libmesos.
# TODO(tillt): Enable optional building of this module library.
# TODO(tillt): Make this module library installable. See MESOS-1940.
pkglib_LTLIBRARIES += libtestauthentication.la
libtestauthentication_la_SOURCES = authentication/test_authentication_modules.cpp \
				   authentication/cram_md5/auxprop.cpp
libtestauthentication_la_LDFLAGS = -release $(PACKAGE_VERSION) -shared $(MESOS_LDFLAGS)

# Library containing test CPU and memory isolator modules.
pkglib_LTLIBRARIES += libtestisolator.la
libtestisolator_la_SOURCES = isolator/test_isolator_module.cpp
libtestisolator_la_LDFLAGS = -release $(PACKAGE_VERSION) -shared $(MESOS_LDFLAGS)

# Library containing test hook module.
pkglib_LTLIBRARIES += libtesthook.la
libtesthook_la_SOURCES = hook/test_hook_module.cpp
libtesthook_la_LDFLAGS = -release $(PACKAGE_VERSION) -shared $(MESOS_LDFLAGS)

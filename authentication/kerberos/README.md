# Mesos Kerberos Authentication Modules

Modules allowing the SASL based authentication of slaves and frameworks against a master using Kerberos, via GSSAPI.

## Build
See [Building the Modules](https://github.com/mesosphere/proprietary-modules).

## Prerequisites

### Kerberos

These modules rely upon a fully setup Kerberos KDC [Key Distribution Center].
- [Manually setting up a KDC on Amazon's AWS](https://github.com/mesosphere/documentation/wiki/Prepare-AWS-for-Kerberos)
- [Setting up a dockerized KDC](https://github.com/tillt/docker-kdc)

## Setup

Create a host specific JSON file (e.g. `master_gssapi.json` or `slave_gssapi.json`):

#### Authenticator module JSON

This module supports parameters. All of those parameters are optional. For backwards compatiblity, the parameters may also get supplied using environment variables.

| key             | default value | description                                    | environment variable |
|-----------------|---------------|------------------------------------------------|----------------------|
| `service_name`  | `mesos`       | The registered name of the service using SASL. | `SASL_SERVICE_NAME`  |
| `server_prefix` |               | Added in front of the hostname.                | `SASL_SERVER_PREFIX` |
| `realm`         |               | The domain of the user agent.                  | `SASL_REALM`         |

```
{
  "libraries": [
    {
      "file": "/path/to/libkerberosauth.so",
      "modules": [
        {
          "name": "com_mesosphere_mesos_GSSAPIAuthenticator",
          "parameters": [
            {
            	"key": "service_name",
            	"value": "registered name"
            },
            {
            	"key": "server_prefix",
            	"value": "hostname prefix"
            },
            {
            	"key": "realm",
            	"value": "kerberos realm"
            }
          ]
        }
      ]
    }
  ]
}
```


#### Authenticatee module JSON

This module supports parameters. All of those parameters are optional. For backwards compatiblity, the parameters may also get supplied using environment variables.

| key             | default value | description                                    | environment variable |
|-----------------|---------------|------------------------------------------------|----------------------|
| `service_name`  | `mesos`       | The registered name of the service using SASL. | `SASL_SERVICE_NAME`  |
| `server_prefix` |               | Added in front of the hostname.                | `SASL_SERVER_PREFIX` |

```
{
  "libraries": [
    {
      "file": "/path/to/libkerberosauth.so",
      "modules": [
        {
          "name": "com_mesosphere_mesos_GSSAPIAuthenticatee",
          "parameters": [
            {
            	"key": "service_name",
            	"value": ""
            },
            {
            	"key": "server_prefix",
            	"value": ""
            }
          ]
        }
      ]
    }
  ]
}
```

#### Authenticatee credential JSON

For selecting the principal that should get authenticated, use the `--credential` flag of the slave (or framework). Note that no password is used / required.

```
{
  "principal": "kerberos principal"
}
```

#### Kerberos specific environment variables

The following environment variables are supported.

| name          | description                                                                                                                  |
|---------------|------------------------------------------------------------------------------------------------------------------------------|
| `KRB5_KTNAME` | Default keytab file name.                                                                                                    |
| `KBB5CCNAME`  | Default name for the credentials cache file.                                                                                 |
| `KRB5_TRACE`  | File name for trace-logging output. For example, `export KRB5_TRACE=/dev/stderr` would send tracing information to `stderr`. __Note__: this is for debugging Kerberos specifics and does not affect the log-output of Mesos or the modules. |

## Use

#### Running a master

##### Relevant flags

| name                  | description
|-----------------------|----------------------------------------------------------|
| `authenticate`        | Only authenticated frameworks are allowed to register.   |
| `authenticate_slaves` | Only authenticated slaves are allowed to register.       |
| `authenticators`      | Authenticator implementation to use when authenticating. |
| `modules`             | List of modules to be loaded.                            |

##### Example

Enforce Kerberos authenticated slaves:

```
./bin/mesos-master.sh --authenticate_slaves \
--authenticators=com_mesosphere_mesos_GSSAPIAuthenticator \
--work_dir=/tmp/mesos \
--modules=file://path/to/authenticator_modules.json
```

#### Running a slave

##### Relevant flags

| name             | description                                              |
|------------------|----------------------------------------------------------|
| `authenticatee`  | Authenticatee implementation to use when authenticating. |
| `credential`     | Information used for selecting a principal.              |
| `modules`        | List of modules to be loaded.                            |

##### Example

Enable Kerberos authentication when registering:

```
./bin/mesos-slave.sh --master=master_ip:port \
--authenticatee=com_mesosphere_mesos_GSSAPIAuthenticatee \
--credential=file://path/to/credential.json \
--modules=file://path/to/authenticatee_modules.json
```

#### Running the test-framework

##### Relevant environment variables

| name                  | description                                              |
|-----------------------|----------------------------------------------------------|
| `DEFAULT_PRINCIPAL`   | Information used for selecting a principal.              |
| `MESOS_AUTHENTICATE`  | Enables authentication.                                  |
| `MESOS_AUTHENTICATEE` | Authenticatee implementation to use when authenticating. |
| `MESOS_MODULES`       | List of modules to be loaded.                            |

##### Example

Enable Kerberos authentication when registering:

```
MESOS_AUTHENTICATE=YES \
DEFAULT_PRINCIPAL="mesos/hostname.domain.name" \
MESOS_MODULES=file://path/to/authenticatee_modules.json \
MESOS_AUTHENTICATEE=com_mesosphere_mesos_GSSAPIAuthenticatee \
./src/test-framework.sh --master=master_ip:port
```

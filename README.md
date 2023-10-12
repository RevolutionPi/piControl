<details>
<summary>We have moved to GitLab! Read this for more information.</summary>

We have recently moved our repositories to GitLab. You can find piControl
here: https://gitlab.com/revolutionpi/piControl  
All repositories on GitHub will stay up-to-date by being synchronised from
GitLab.

We still maintain a presence on GitHub but our work happens over at GitLab. If
you want to contribute to any of our projects we would prefer this contribution
to happen on GitLab, but we also still accept contributions on GitHub if you
prefer that.
</details>

# piControl

piControl is a kernel module for interfacing with RevPi hardware. It provides a common interface to all RevPi related IOs, which a user can consume via `/dev/piControl0`.

## Usage

**NOTE**: Building the master branch requires a Linux kernel with support for the pibridge serdev driver (i.e. [revpi-6.1](https://github.com/RevolutionPi/linux/commits/devel/revpi-6.1)). The branch [`revpi-5.10`](https://github.com/RevolutionPi/linux/commits/revpi-5.10) does not require support for this and can be built against an earlier kernel version of the RevolutionPi kernel.

## Build the module

All the following steps need to be executed on a RevPi device running the official image. It is also possible to compile the module on another system, but the instructions may vary.

Install kernel headers:

```
sudo apt update
sudo apt install raspberrypi-kernel-headers
```

Checkout repository and switch working directory:

```
git clone https://github.com/RevolutionPi/piControl.git
cd piControl
```

Compile module sources for current kernel and architecture:

```
KDIR=/usr/src/linux-headers-$(uname -r)/ make
```

## Load the compiled module

Before the newly compiled module can be loaded, the currently loaded module needs to be unloaded:

```
sudo rmmod piControl
```

> **NOTE**: If the command fails with an error such as `rmmod: ERROR: Module piControl is in use`, applications that are using the module need to be stopped. The command `sudo lsof /dev/piControl0` will show a list of applications that are using the piControl device.

Load the newly compiled module:

```
sudo insmod piControl.ko
```

This will only load the newly compiled module once. On the next reboot, the pre-installed module in `/lib/modules/$(uname -r)/extra/piControl.ko` will be loaded again. If the newly compiled module should be used permanently, then it needs to be copied into the modules folder:

```
sudo cp piControl.ko /lib/modules/$(uname -r)/extra/piControl.ko
```

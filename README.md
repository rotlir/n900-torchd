# torchd
Torchd is a torch control daemon for Nokia N900 running mainline Linux kernels with libgpiod. It allows you to easily communicate with ADP1653 flash controller installed in N900.

## Building
Torchd depends on libgpiod and libi2c. You can install build dependencies on Alpine/postmarketOS with:
```
# apk add build-base libgpiod libgpiod-dev linux-headers i2c-tools i2c-tools-dev
```

Build by running `make` in the project root directory.

You can install built binaries and OpenRC service script by running `make install`, uninstall with `make uninstall`.

## Usage 
Start torchd OpenRC service or run `torchd` as root.

The daemon creates a UNIX domain socket, which is located in '/run/torchd.sock' by default. You can toggle the behavior of the torch by writing different values to the socket:

- `off`  -- turn off torch and release control of ADP1653, so it becomes available to other processes
- `torch` -- turn on flash with the lowest current possible; supplying higher currents may result in permanent hardware damage to the LED
- `red` -- turn on red privacy LED

Torchd comes with a simple program `torch` to make it easier to use the daemon. The usage is intuitive:
```
$ torch torch
$ torch red
$ torch off
```

## License
The project is licensed under the terms of the GNU General Public License v3.0. See `LICENSE` for details.
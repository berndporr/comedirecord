# comedirecord

An oscilloscope program for COMEDI

## Prerequisites

```
sudo apt install libqwt-qt5-dev qtdeclarative5-dev-tools libfftw3-dev iir1-dev
```

## Compilation

```
cmake .
make
```

Test it locally if it runs with:

```
./comedirecord
```

## Installation

```
sudo make install
```

which installs it in `/usr/local/bin`.


## Running it

After the global install type into the terminal:

```
comedirecord
```

## Troubleshooting

Check with
```
sudo dmesg
```

if you see:

```
[ 3506.038554] comedi: version 0.7.76 - http://www.comedi.org
[ 3506.065482] comedi comedi0: ADC_zero = 80011c
[ 3506.066796] comedi comedi0: driver 'usbduxsigma' has successfully auto-configured 'usbduxsigma'.
[ 3506.066849] usbcore: registered new interface driver usbduxsigma
```

and check that you are in the group `iocard`. Open the groups file:

```
sudo nano /etc/group
```

and see if your username is added to the group `iocard`:

```
iocard:x:125:my_user_name
```

If it's not the case add yourself, save the file, close the terminate and re-open it.

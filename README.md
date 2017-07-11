# 3d_led_cube_receiver

# Install

## for CentOS 6.6 Final without the real 3d LED cube

```bash
$ sudo yum install opencv-devel
$ sudo yum install boost-devel
```

# Build

## for Raspberry pi with the real 3d LED cube

```bash
$ cd make
$ make
```

## for Linux without the real 3d LED cube

```bash
$ cd make
$ ENABLE_REAL_3D_LED_CUBE=false make
```

## for another OS without the real 3d LED cube

```bash
$ cd make
$ make
```

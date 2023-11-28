# DragonStub

A generic standalone EFI stub for DragonOS kernel, which based on the Linux EFI stub.

## Requirements

To build the stub, you need to have the following packages installed:

```bash
sudo apt install -y gcc-riscv64-linux-gnu
```

## Building

```bash
ARCH=riscv64 make -j $(nproc)
```

## Run

```bash
make run
```

## Maintainer

- longjin <longjin@dragonos.org>

## License

DragonStub is licensed under the GPLv2 License. See [LICENSE](LICENSE) for details.

## References

- GNU-EFI: DragonStub built with gnu-efi
- Linux-EFIStub: In Linux kernel source tree: firmware/efi/libstub

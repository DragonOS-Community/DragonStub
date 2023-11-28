#pragma once

#if defined(CONFIG_riscv64)
#define efi_bs_call(func, ...) (BS->func(__VA_ARGS__))
#else
#error "Unexpectedly include 'riscv64.h'"
#endif

#define COMMAND_LINE_SIZE 1024

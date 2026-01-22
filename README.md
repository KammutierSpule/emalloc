# emalloc

External memory allocator with separate metadata for embedded systems and resource-constrained environments.

## Overview

**emalloc** is a memory allocator designed for managing external memory pools with metadata stored separately. This design is particularly useful for embedded systems, non memory-mapped devices, and scenarios where you need fine-grained control over memory allocation without polluting the managed memory space with bookkeeping data.

Traditional memory allocation algorithms store metadata (such as allocation tree information) within the memory arena itself. This approach becomes problematic when working on external memory devices that lack or is not possible to have CPU address mapping, such as SPI RAM. In these scenarios, standard allocators algorithms would be hard to adapt or perform poorly due to the need for external metadata fetches. Since memory allocation algorithms are inherently slow, this library optimizes performance by storing metadata in internal CPU RAM while returning only the address offset needed for the external device.

### Key Features

- 🎯 **Separate Metadata**: All allocation metadata is stored externally from the managed memory pool
- 🚀 **First-Fit Algorithm**: Fast allocation with intelligent free block merging
- 💾 **Memory Efficient**: Smart coalescing of adjacent free blocks
- ✅ **Production Ready**: Comprehensive test suite with internal consistency checks

### Use Cases

- **Embedded Systems**: Manage external SRAM, Flash, or non-memory-mapped peripherals
- **Custom Memory Pools**: Create specialized allocators for specific subsystems
- **Memory Constrained Devices**: Efficient memory management with predictable behavior
- **Real-Time Systems**: Deterministic allocation patterns

## Building

In order to use this library in your project you only need two files: [emalloc.h](./emalloc/include/emalloc.h) and [emalloc.c](./emalloc/src/emalloc.c).

You shall not use the CMakeLists.txt present on this project to integrated it on your project. It is only used for development purposes.

### Build tests and development

The project development uses CMake for building:

```bash
mkdir build
cd build
cmake ..
cmake --build .
cd build/emalloc/test
./RunAllTests
```

### Enable Statistics

To enable detailed performance statistics, build with:

```bash
cmake -DEMALLOC_STATISTICS=1 ..
cmake --build .
```

## Quick Start

### Basic Usage

```c
#include <emalloc/emalloc.h>
#include <stdint.h>
#include <stdio.h>

// Metadata storage (2 uint32_t per node = 8 bytes per node)
// Max nodes needed depends on fragmentation, typically 10-20% of blocks
#define N_NODES (128)
uint64_t s_nodes[N_NODES];

int main(void) {
    // Configure the allocator
    sEMALLOC_cfg config = {
        .nodes_poll = s_nodes,
        .nodes_poll_length = N_NODES,
        .external_memory_size_bytes = ... whatever external size is in bytes ...
    };

    // Initialize context
    sEMALLOC_ctx ctx;
    uint32_t result = emalloc_init(&ctx, &config);
    if (result != EMALLOC_OK) {
        printf("Initialization failed: 0x%X\n", result);
        return 1;
    }

    // Allocate memory
    uint32_t offset1 = emalloc_alloc(&ctx, 256);
    if (offset1 & EMALLOC_ERR_MASK) {
        // See EMALLOC_ERR_*
        printf("Allocation failed: error: 0x%X\n", offset1);
        return 1;
    }

    // Use the allocated memory, example:
    SPI_Write(offset1, "Hello World!", 13);

    printf("Allocated 256 bytes at offset: %u\n", offset1);
    printf("Currently allocated: %u bytes\n",
           emalloc_get_allocated_size_bytes(&ctx));

    // Allocate more
    uint32_t offset2 = emalloc_alloc(&ctx, 512);
    printf("Allocated 512 bytes at offset: %u\n", offset2);
    printf("Currently allocated: %u bytes\n",
           emalloc_get_allocated_size_bytes(&ctx));

    // Free memory
    result = emalloc_free(&ctx, offset1);
    if (result != EMALLOC_OK) {
        printf("Free failed: 0x%X\n", result);
        return 1;
    }

    printf("After freeing first block: %u bytes\n",
           emalloc_get_allocated_size_bytes(&ctx));

    // Free second allocation
    emalloc_free(&ctx, offset2);
    printf("After freeing all: %u bytes\n",
           emalloc_get_allocated_size_bytes(&ctx));

    return 0;
}
```

### Embedded System Example

```c
// Example for external SRAM
#include <emalloc/emalloc.h>

#define EXT_SRAM_BASE_ADDR  0x60000000
#define EXT_SRAM_SIZE       (512 * 1024)  // 512 KB

// Metadata in internal RAM
#define N_NODES (1024)
static uint64_t s_sram_nodes[N_NODES];
static sEMALLOC_ctx s_sram_ctx;

void external_sram_init(void) {
    // Configure memory controller for external SRAM
    // ... hardware initialization code ...

    // Initialize allocator
    sEMALLOC_cfg config = {
        .nodes_poll = s_sram_nodes,
        .nodes_poll_length = N_NODES,
        .external_memory_size_bytes = EXT_SRAM_SIZE
    };

    if (emalloc_init(&s_sram_ctx, &config) != EMALLOC_OK) {
        // Handle error
        while(1);
    }
}

void* sram_malloc(size_t size) {
    // You may add multi-thread protection (mutex)
    uint32_t offset = emalloc_alloc(&s_sram_ctx, size);

    if (offset & EMALLOC_ERR_MASK) {
        return NULL;
    }

    // Mind offset 0 is a valid address returned by emalloc_alloc
    return (void*)(EXT_SRAM_BASE_ADDR + offset);
}

void sram_free(void* ptr) {
    // You may add multi-thread protection (mutex)
    if (!ptr) return;

    const uint32_t offset = (uint32_t)((uintptr_t)ptr - EXT_SRAM_BASE_ADDR);
    emalloc_free(&s_sram_ctx, offset);
}
```

## Design Principles

### Memory Layout

```
External Memory Pool (managed):
┌─────────────────────────────────────┐
│  Allocated  │  Free  │  Allocated  │
│  Block 1    │  Space │  Block 2    │
└─────────────────────────────────────┘

Metadata (separate):
┌──────────────────────────────────┐
│ Node 1 │ Node 2 │ Node 3 │ ... │
└──────────────────────────────────┘
```

### Allocation Strategy

1. **First-Fit**: Searches for the first free block large enough
2. **Coalescing**: Automatically merges adjacent free blocks
3. **Node Merging**: Optimizes metadata usage by combining consecutive free nodes
4. **Lazy Sorting**: Only sorts nodes when necessary for efficient search

### Memory Alignment

All allocations are aligned to 16-byte boundaries:

- Requested size < 16 bytes -> allocated 16 bytes
- Requested size = 25 bytes -> allocated 32 bytes (next multiple of 16)

## Performance Characteristics

NOTE: This needs confirmation and validation on real device!

| Operation | Best Case | Average Case | Worst Case |
| - | - | - | - |
| Allocation | O(1) | O(n) | O(n²) |
| Deallocation | O(log n) | O(log n) | O(n) |
| Coalescing | O(1) | O(1) | O(n) |

Where `n` is the number of allocated blocks.

## Testing

The project includes comprehensive unit tests using CppUTest:

```bash
cd build/emalloc/test
./RunAllTests

# Run specific test groups
./RunAllTests -g emalloc_init
./RunAllTests -g emalloc_alloc
./RunAllTests -g emalloc_free
```

### Coverage Report

Generate coverage report:

```bash
./scripts/run_gcovr.sh
```

## Configuration Options

### Compile-Time Options

| Option | Default | Description |
| - | - | - |
| `EMALLOC_STATISTICS` | 0 | Enable performance statistics collection |
| `EMALLOC_USE_ASSERT` | 0 | Enable runtime assertions |
| `EMALLOC_INTERNAL_CHECKS` | 0 | Enable internal consistency checks |

### Example CMake Configuration

```cmake
target_compile_definitions(emalloc PRIVATE
    EMALLOC_STATISTICS=1
    EMALLOC_USE_ASSERT=1
)
```

## Best Practices

### Sizing the Node Pool

The number of nodes needed depends on the number of allocations you are expecting on your application.
You should also consider about 10%...20% of framegmented blocks. i.e: blocks which was not possible to use or to merge.
Worse case scenario: 1 node for every 16 bytes of data.

### Error Handling

Always check return values. Example:

```c
uint32_t offset = emalloc_alloc(&ctx, size);
if (offset & EMALLOC_ERR_MASK) {
    // Handle specific errors
    switch (offset) {
        case EMALLOC_ERR_NO_EXTERNAL_MEMORY:
            // Out of memory
            break;
        case EMALLOC_ERR_NO_MORE_FREE_NODES:
            // Increase node pool size
            break;
        default:
            // Other error
            break;
    }
}
```

Mind that offset 0 is a valid address. So you cannot test for NULL.

### Thread Safety

The allocator is reentrant if:

1. Each thread uses a separate `sEMALLOC_ctx`
2. External synchronization is used for shared contexts
3. Statistics are disabled (`EMALLOC_STATISTICS=0`) to avoid global state

```c
// Thread-safe wrapper example
pthread_mutex_t alloc_mutex = PTHREAD_MUTEX_INITIALIZER;

uint32_t safe_alloc(sEMALLOC_ctx* ctx, uint32_t size) {
    pthread_mutex_lock(&alloc_mutex);
    uint32_t result = emalloc_alloc(ctx, size);
    pthread_mutex_unlock(&alloc_mutex);
    return result;
}
```

## Limitations

- Maximum managed memory: 2 GB (31-bit because of internally use of int32_t)
- Minimum allocation size: 16 bytes
- Maximum nodes: 2147483648 (int32_t limit)
- Alignment: Fixed at 16 bytes (not configurable at runtime)
- Statistics are global when enabled (not per-context)

## Profiling

Raspberry pico RP2040 @125MHz
https://github.com/KammutierSpule/emalloc-rp2040-profile

### #comparison

Test for 2048 max nodes.

emalloc.alloc average 1093 ... 4174 cycles (8.7 ... 33.4us)
emalloc.free average 1754 ... 3891 cycles (14.0 ... 31.1us)

freertos.alloc average ~308 cycles (2.5us)
freertos.free average ~341 cycles (2.7us)
NOTE: freertos.alloc and free is O(1) ? with some jitter.

libc.alloc average ~393 cycles (3.0us)
libc.free average ~350 cycles (2.8us)
NOTE: libc.alloc and free is almost O(1) all the times with very little jitter.

Conclusion: emalloc.alloc and free is 3.5x ... 14x slower in comparison to other allocations.
Worse case measured was about 1ms. See test log for more info.

**NOTE:** This comparison does not directly evaluate the algorithms themselves, as their underlying mechanisms differ significantly. For instance, in conventional memory allocators where memory is address-mapped, the allocation metadata is typically stored adjacent to the allocated data. For example, the metadata for a node might be located at `allocated address - 1`. In contrast, **emalloc** separates metadata from the managed memory pool, which is a fundamental design difference that impacts performance characteristics.

## License

This project is licensed under the **BSD 3-Clause License**. See the [LICENSE](LICENSE) file for details.

```
Copyright (C) 2026 Mario Luzeiro
SPDX-License-Identifier: BSD-3-Clause
```

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Follow the existing code style
4. Add tests for new functionality
5. Ensure all tests pass
6. Commit your changes (`git commit -m 'Add amazing feature'`)
7. Push to the branch (`git push origin feature/amazing-feature`)
8. Open a Pull Request

### Code Style

The project follows Google C++ Style Guide with some modifications. Run cpplint:

```bash
cpplint --recursive emalloc/
```

## Author

**Mario Luzeiro**
Email: <mluzeiro@ua.pt>

## Acknowledgments

- Optimized for embedded systems and real-time applications
- Inspired by classic memory allocator designs
- Tested algorithms: bubble, shell, shell tokuda, insertion, insertion binary, insertion sentinel, heap sort

## Support

For questions, issues, or feature requests, please open an issue on GitHub.

## Development Investment

Open source software doesn't come free. This project represents **75 hours** of development time.

**Time Breakdown:** Core development (24 hrs) • Testing, improvements and bug fixing (24 hrs) • Profiling (15 hrs) • Documentation (6 hrs) • Research (6 hrs)

**Appreciate this work?** ⭐ Star the repo • 🐛 Report issues • 💝 [Sponsor/donate]

**Need consulting?** I'm available for custom software development and consulting services. [Contact me](mailto:mrluzeiro@ua.pt).

---

**Platform Target:** Any (portable C code)
**Version:** See git tags for releases

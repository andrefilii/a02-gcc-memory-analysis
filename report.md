# Internal Memory Management in GCC

## Architectural Overview of Memory Management in the GNU Compiler Collection (GCC)

The GNU Compiler Collection (v13+) employs a specialized, hybrid memory management architecture designed to sustain high compilation throughput while managing the complex lifecycles of Intermediate Representation (IR) artifacts. Modern GCC has transitioned significantly to C++, utilizing templates and strongly typed allocators to optimize the legacy C-style management mechanisms.

### 1. Management of Long-Lived Objects: The GGC Subsystem

The **GGC (GCC Garbage Collector)** serves as the persistence layer for the compiler, managing objects with indeterminate lifetimes or those that must survive across multiple compilation passes.

* **Target Objects:** GGC manages the core IR structures: `tree` nodes (high-level IR), `gimple` statements (middle-end IR), `rtx` (Register Transfer Language), and global symbol table entries.
* **Mechanism:** GGC utilizes a precise mark-and-sweep garbage collection strategy. It eschews reference counting in favor of "roots"—global or static pointers explicitly registered with the collector.
* **GTY and Metaprogramming:** To handle root registration and traversal automatically, GCC uses a custom generator, `gengtype`. Source structures requiring collection are annotated with `GTY(())` markers. During the build, `gengtype` generates the `gt-*.h` files containing the typed traversal functions used by the collector.
* **Allocation Strategy (Modern C++):**
* **Frontend Interface:** Allocation is primarily handled via strongly typed templates defined in `ggc.h`, such as `ggc_alloc<T>()`. These are implemented as inline functions to reduce call overhead.
* **Backend Implementation:** The inline templates delegate to the physical allocator, principally implemented in `ggc-page.cc`. This module organizes the heap into "pages" of specific order sizes (powers of two). Large allocations bypass the page logic and are handled via standard OS primitives (e.g., `mmap`).

### 2. Management of Short-Lived Data: Obstacks and Object Allocators

For data with deterministic scopes (e.g., pass-local analysis), GCC bypasses the GGC to reduce GC pressure, favoring region and pool allocation.

#### A. Obstacks (Object Stacks)

Despite the C++ transition, GCC retains the efficient "Obstack" mechanism (via `libiberty`) for variable-sized, stack-like allocations.

* **Use Case:** Obstacks are the standard backing store for `bitmap` structures (via `bitmap_obstack`), which are ubiquitous in data-flow analysis and SSA renaming. They are also used extensively during the parsing phase for temporary string storage.
* **Lifecycle:** Freeing is region-based; releasing an obstack or a specific point within it (`obstack_free`) invalidates all subsequently allocated objects, making it ideal for phase-based cleanup.
* **Source:** Defined in `include/obstack.h` and heavily utilized in `bitmap.cc`.

#### B. Object Allocators (Pools)

Modern GCC wraps the legacy `alloc_pool` logic in C++ templates to provide type-safe, fixed-size allocation.

* **Use Case:** High-frequency, fixed-size objects required for specific graph algorithms, such as `et_node` in dominance calculations or specific SSA update records.
* **Mechanism:** The `object_allocator<T>` template class (defined in `alloc-pool.h`) maintains a specialized free list for type `T`. This ensures $O(1)$ allocation/deallocation and maximizes cache locality by grouping identical objects.
* **Source:** `alloc-pool.h` and `alloc-pool.cc`.

### 3. Memory Ownership Transition: FE $\to$ ME $\to$ BE

The ownership model shifts as the compilation pipeline progresses, dictating which allocator is dominant.

#### Front End (FE) Phase

* **Activity:** Lexing and Parsing (C++, Fortran, Rust, etc.).
* **Memory Behavior:** The Front End constructs the `tree` IR (GGC-managed). However, the parsing process relies heavily on **Obstacks** for scope tracking and token buffering.
* **Transition:** Upon parse completion, temporary obstacks are released. The resulting `tree` root is preserved in GGC memory, serving as the interface to the Middle End.

#### Middle End (ME) Phase

* **Activity:** GIMPLE Lowering, SSA Construction, Optimization Passes.
* **Memory Behavior:** The `tree` IR is lowered to `gimple` (GGC-managed). The *Pass Manager* orchestrates the lifetime of analysis data. Passes typically instantiate local `object_allocator` pools or `bitmap_obstack`s for dominance frontiers, liveness sets, or loop structures.
* **Transition:** At the end of a pass (e.g., `pass_impl::execute`), local pools are destructed. `ggc_collect()` may be triggered by the `TODO_ggc_collect` flag to reclaim unreachable `gimple` statements (e.g., after Dead Code Elimination).

#### Back End (BE) Phase

* **Activity:** RTL Expansion, Register Allocation, Code Gen.
* **Memory Behavior:** GIMPLE transforms into `rtx` (GGC-managed). The Register Allocator (IRA/LRA) creates significant memory pressure, utilizing specialized allocators within `ira-build.cc` and `lra.cc` (often custom pools) to manage interference graphs.
* **Finalization:** Post-assembly generation, GGC roots are cleared. In JIT contexts (`libgccjit`), the context is torn down, releasing all associated GGC pages and pools.

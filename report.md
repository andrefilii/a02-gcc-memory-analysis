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

## Deep Analysis: Obstack Memory Management in GCC (v13+)

The **Obstack** (Object Stack) is a foundational memory management mechanism in GCC provided by `libiberty`. Despite the codebase's transition to C++, Obstacks remain the critical infrastructure for high-performance, region-based memory allocation where object lifetimes are strictly nested or phase-bound.

### 1. Internal Mechanism: The Bump Pointer Strategy

The Obstack is designed to eliminate the overhead of `malloc`/`free` for individual small objects. It implements a "fast path" allocation strategy often referred to as **Bump Pointer Allocation** or **Linear Allocation**.

* **The Chunk Structure:** An Obstack maintains a linked list of large memory blocks called "chunks" (typically 4KB or larger). It tracks three critical pointers within the current chunk:
1. `chunk_limit`: The end of the currently allocated physical memory.
2. `object_base`: The start of the object currently being constructed.
3. `next_free`: The "bump pointer" indicating the next available byte.


* **Allocation ():**
When a new object of size  is requested:
1. **Check:** The allocator calculates `next_free + N`.
2. **Fast Path:** If the result is  `chunk_limit`, the allocation is a simple pointer increment. The old `next_free` is returned as the object address, and `next_free` is updated. This involves zero search overhead, zero fragmentation checks, and is strictly .
3. **Slow Path:** If the request exceeds the remaining space, the `libiberty` backend allocates a new chunk (via `xmalloc`), links it to the previous chunk, and satisfies the request from the new block.

### 2. Modern Usage & API in the C++ Codebase

While `obstack` is a legacy C structure, it is deeply integrated into the modern C++ files (`.cc`).

* **Direct Macro Usage:**
The standard macros defined in `include/obstack.h`—specifically `obstack_alloc`, `obstack_grow`, and `obstack_finish`—are heavily used directly within C++ source files.
* *Modern C++ Context:* You will frequently see these macros in `cp/parser.cc` (the C++ Front End parser) and `c-family/c-common.cc`. The macros operate on the raw `struct obstack`.


* **The `bitmap_obstack` Wrapper:**
One of the most critical modern applications is within the Data Flow Analysis (DFA) subsystem.
* **Definition:** In `bitmap.h` and `bitmap.cc`, GCC defines `struct bitmap_obstack`. This structure wraps a standard `obstack` specifically for allocating `bitmap_element` nodes (linked list nodes representing bits).
* **Efficiency:** Instead of allocating a `bitmap_element` (approx 24-32 bytes) via `new` or `malloc`, the bitmapping subsystem calls `bitmap_alloc`. This function fetches raw memory from the wrapped obstack. This drastically improves cache locality, as sequential bitmap elements are packed contiguously in memory, which is vital for traversing liveness sets or dominance frontiers.

### 3. Use Case: The Front End (Lexing and Parsing)

The Front End (FE) utilizes Obstacks to solve the "Unknown Size" problem during Lexing.

* **String Interning & Tokenization:**
When the lexer encounters a string literal or identifier, it does not initially know the length. Using `std::string` would imply repeated reallocations and copies.
* **Mechanism:** The FE uses `obstack_1grow` (adds 1 byte) to push characters onto the "current object" as they are read. The `next_free` pointer simply advances.
* **Finalization:** Once the closing quote or delimiter is found, `obstack_finish` is called. This function "seals" the object, returns the pointer to `object_base`, and updates `object_base` to equal `next_free`.
* **Performance:** This allows constructing strings in-place with zero intermediate copying.

### 4. The "Freeing" Logic: Scope-Based Reclaiming

Obstacks enforce a strict LIFO (Last-In, First-Out) reclamation policy, which aligns perfectly with compiler scoping rules.

* **Mechanism (`obstack_free`):**
The function `obstack_free(obstack_ptr, object_ptr)` does not strictly "free" the object pointed to by `object_ptr`. Instead, it resets the `next_free` pointer of the obstack *back* to `object_ptr`.
* **Implication:** This effectively frees `object_ptr` **and every object allocated after it**.
* **Compilation Scope Example:**
1. **Enter Scope:** The compiler records the current state of the obstack (e.g., `void *mark = obstack_alloc(obs, 0)`).
2. **Process Scope:** It allocates dozens of temporary nodes, strings, and lists on the obstack.
3. **Exit Scope:** It calls `obstack_free(obs, mark)`.


* **Result:** The "bump pointer" snaps back to the `mark`. All temporary memory is instantly invalidated without iterating over the objects. This is significantly faster than destructing a `std::vector` of pointers.
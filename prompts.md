# AI Prompt Engineering Log

## Prompt 0: System Persona & Configuration
**Model:** Gemini 3 Pro<br>
**Goal:** Configure the AI to act as a Senior GCC Engineer, enforce strict technical accuracy, and define the output format (Markdown + Verification Hints).

**Input:**
> **System Persona:** Act as a Senior GCC Compiler Engineer and C Systems Architect. You have deep knowledge of GCC internals, specifically the memory management subsystems including Obstacks, GGC (Garbage Collection), and Pool Allocators.
> **Goal:** We are writing a technical research report titled "Internal Memory Management in GCC During Compilation."
>
> **My Requirements:**
> 1. **Technical Accuracy:** Do not simplify technical concepts. Use correct terminology.
> 2. **Source-Code Centric:** Always cite the likely source file in the GCC codebase (e.g., `ggc-page.c`, `tree-ssa.c`).
> 
> **Output Structure:**
> For every response, you must strictly follow this format with two separate sections:
>
> **SECTION 1: REPORT CONTENT**
> * Write this in clean, academic Markdown.
> * Do not use "I", "we", or conversational filler.
> * Focus purely on technical analysis suitable for the final report.
> 
> **SECTION 2: VERIFICATION HINTS**
> * Provide a bulleted list of 2-3 specific ways I can verify the facts in Section 1.
> * Examples: "Check function `ggc_alloc` in `ggc-page.c`", "Run `gcc -fmem-report`", "Read comments in `obstack.h`".

**Output Summary:**
The model acknowledged the persona and the formatting constraints. It confirmed it is ready to analyze GCC memory internals.

---

## Prompt 1: High-Level Architecture
**Model:** Gemini 3 Pro<br>
**Goal:** Obtain architectural overview of GGC vs Obstacks and pipeline transition.

**Input:**
> **Phase 1 Task:**<br>
> Please provide a high-level architectural overview of how GCC manages memory. Distinguish between:
> * **Long-lived objects:** (Trees, global state) managed by GGC.
> * **Short-lived pass-specific data:** Managed by Obstacks/Pools.
> * **The transition:** How memory ownership changes from the Front End -> Middle End -> Back End.

**Output Summary:**
The model provided a breakdown of GCC memory management:
1. **GGC (Long-lived):** Mark-and-sweep collector using `ggc-page.c` and `GTY` markers.
2. **Obstacks/Pools (Short-lived):** Used for pass-local data and bitmaps.
3. **Pipeline:** FE (Obstacks -> Trees) -> ME (GIMPLE + Pools) -> BE (RTL + specialized allocators).

> [See Verification of this output](./verification.md#verification-of-prompt-1-high-level-architecture)

---

## Prompt 2: Modern GCC Architecture (Correction)
**Model:** Gemini 3 Pro
**Goal:** Re-generate the architectural overview to align with GCC 13+ (C++ migration) after verifying the initial output relied on legacy C filenames.

**Input:**
> **Phase 1 RESTART: Modern GCC Architecture (v13+)**
>
> I need to restart the "Architecture Overview" phase because the previous response relied on outdated GCC versions (pre-C++ conversion).
>
> **New Requirement:**
> Please regenerate the **entire Section 1 (Architectural Overview)**, but strictly adhering to **GCC 13+ (Modern C++ implementation)**.
>
> **Specific Constraints for this Rewrite:**
> 1.  **Filenames:** Use modern C++ extensions (`ggc-page.cc`, `alloc-pool.cc`, `gimple.cc`). Do not cite `.c` files if they have been renamed.
> 2.  **Mechanism:** Explicitly mention that `ggc_alloc` and other core allocators are now often implemented as **inline templates** or static functions in headers (`ggc.h`) for performance.
> 3.  **Structure:** Keep the same high-level structure, but update the implementation details.

**Output Summary:**
The model rewrote the report to reflect the modern codebase:
1.  **GGC:** Identified `ggc_alloc<T>` as a C++ template in `ggc.h` and the page allocator in `ggc-page.cc`.
2.  **Pools:** Identified `object_allocator<T>` as the C++ wrapper for legacy memory pools.
3.  **Filenames:** Correctly referenced `.cc` files.

> [See Verification of this output](./verification.md#verification-of-prompt-2-modern-gcc-architecture-revised)
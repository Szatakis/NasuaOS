# NasuaOS — Debug Instructions

## Purpose

These instructions define the rules and procedures for debugging NasuaOS.

The primary goal of debugging is to identify and fix the **actual root cause** of a problem.

Do not hide symptoms.

Do not apply random workarounds.

Do not modify unrelated subsystems.

Every debugging change must have a clear technical reason.

Kernel stability, memory safety, and CPU exception correctness always have higher priority than GUI, applications, or cosmetic issues.

---

# 1. General Debugging Rules

Always diagnose the problem before modifying code.

Preferred procedure:

```text
1. Reproduce the problem.
2. Collect logs.
3. Identify the failing subsystem.
4. Determine what operation failed.
5. Reduce the problem to the smallest reproducible case.
6. Add targeted debugging output.
7. Trace the execution and data flow.
8. Identify the root cause.
9. Make the smallest reasonable fix.
10. Rebuild.
11. Reproduce the original problem.
12. Verify that the fix actually solved the cause.
13. Verify that existing functionality still works.
```

Do not change multiple unrelated systems at once.

For example, if a USB driver crashes because of an invalid pointer, do not simultaneously rewrite:

* the USB driver,
* the memory allocator,
* the scheduler,
* the GUI,
* the filesystem.

First identify where the invalid pointer came from.

---

# 2. Logging

Always check existing logs before making changes.

Logs should contain enough information to reconstruct what happened.

Important information includes:

* subsystem/component,
* initialization stage,
* operation being performed,
* virtual addresses,
* physical addresses,
* register values,
* device addresses,
* PCI identifiers,
* error codes,
* sizes,
* flags,
* return values,
* state transitions.

Use consistent prefixes.

Recommended prefixes:

```text
[BOOT]
[CPU]
[GDT]
[IDT]
[ISR]
[IRQ]
[APIC]
[PIC]

[MEM]
[PMM]
[VMM]
[PAGING]
[HHDM]

[PCI]
[USB]
[XHCI]
[EHCI]
[OHCI]
[UHCI]
[ATA]
[AHCI]
[PS2]
[KBD]
[MOUSE]
[NET]

[ACPI]
[FS]
[RAMDISK]

[GUI]
[FB]
[APP]
```

Example:

```text
[PCI] Device found: 00:03.0
[PCI] Vendor: 0x8086
[PCI] Device: 0x100E
```

Another example:

```text
[VMM] Mapping page
[VMM] Virtual:  0xFFFFFFFF827C0000
[VMM] Physical: 0x00000000004DA000
[VMM] Flags:    PRESENT | WRITE
[VMM] Mapping successful
```

Errors should use:

```text
[COMPONENT] ERROR: <description>
```

Do not remove useful logging simply because the log looks bad.

Fix the problem producing the log.

---

# 3. Page Faults

Page Faults are high-priority kernel errors.

A Page Fault must be treated as a memory-management problem until proven otherwise.

Never assume that:

* PMM is broken,
* VMM is broken,
* the allocator is broken,
* the driver is broken,
* the pointer is correct,
* the page simply needs to be mapped.

Verify the complete memory-management path.

---

## 3.1 Required Page Fault Information

When a Page Fault occurs, log at minimum:

```text
[PAGE FAULT]
CR2:        0xFFFFFFFF827C03FC
RIP:        0xFFFFFFFF80104521
RSP:        0xFFFFFFFF80F00000
CR3:        0x0000000000100000
Error:      0x0000000000000002
```

Also log:

* faulting virtual address,
* instruction address,
* stack pointer,
* active page-table root,
* decoded error code,
* access type,
* privilege level,
* page-table entries,
* physical mapping if present,
* allocation information if applicable.

---

## 3.2 Page Fault Error Code

Decode the x86_64 Page Fault error code.

```text
Bit 0 - P

0 = Non-present page
1 = Protection violation


Bit 1 - W/R

0 = Read
1 = Write


Bit 2 - U/S

0 = Supervisor
1 = User


Bit 3 - RSVD

1 = Reserved-bit violation


Bit 4 - I/D

1 = Instruction fetch


Bit 5 - PK

1 = Protection-key violation


Bit 6 - SS

1 = Shadow-stack access


Bit 7 - HLAT

1 = HLAT-related fault
```

Example:

```text
Error Code: 0x2
```

Means:

```text
P     = 0
W/R   = 1
U/S   = 0
```

Therefore:

```text
Kernel/supervisor attempted to WRITE
to a NON-PRESENT page.
```

Do not immediately map the page.

Determine why the kernel attempted to access that address.

---

## 3.3 Check CR2

`CR2` contains the virtual address that caused the Page Fault.

Example:

```text
[PAGE FAULT]
CR2: 0xFFFFFFFF8270D658
```

Compare CR2 with the fault address recorded by the exception handler.

They should normally describe the same address.

If they differ, investigate:

* exception entry,
* register saving,
* nested exceptions,
* corrupted stack,
* logging code,
* incorrect ISR implementation.

---

## 3.4 Check RIP

Log the instruction pointer:

```text
RIP: 0xFFFFFFFF80104521
```

Determine:

* which function contains RIP,
* which instruction caused the fault,
* whether it performs a read/write,
* which register contains the pointer,
* whether the pointer is valid,
* whether the instruction matches the Page Fault error code.

Do not fix a Page Fault without determining what instruction caused it.

---

## 3.5 Check CR3

Log the current page-table root:

```text
CR3: 0x0000000000100000
```

Verify:

* CR3 points to valid page-table memory,
* the page-table root belongs to the expected address space,
* kernel mappings are present,
* required HHDM mappings are present.

If NasuaOS uses a single kernel address space, verify that the expected kernel mappings are still active.

---

# 4. Page Table Debugging

For every Page Fault, inspect the page-table hierarchy.

On standard x86_64 4-level paging:

```text
PML4
 ↓
PDPT
 ↓
PD
 ↓
PT
 ↓
Physical Page
```

Extract the indexes from the faulting virtual address.

Log:

```text
[VMM] Page fault address: 0xFFFFFFFF827C03FC

[VMM] PML4 index: 511
[VMM] PDPT index: 510
[VMM] PD index:   31
[VMM] PT index:   192
```

Then inspect the entries:

```text
[VMM] PML4E: 0x...
[VMM] PDPTE: 0x...
[VMM] PDE:   0x...
[VMM] PTE:   0x...
```

Check every entry for:

* Present,
* Read/write,
* User/supervisor,
* NX,
* Page Size,
* reserved bits,
* physical address.

A page being present does not automatically mean that the mapping is correct.

---

# 5. Page Permissions

Check the permissions of every relevant mapping.

Important properties:

```text
PRESENT
WRITE
USER
NX
```

Examples:

```text
Read-only page + write access
    → Protection violation

Supervisor page + user access
    → Protection violation

NX page + instruction fetch
    → Execution fault
```

Do not fix permission problems by making all pages:

```text
PRESENT | WRITE | EXECUTABLE
```

Permissions must match the intended use of the memory.

---

# 6. Large Pages

Check whether the mapping uses:

```text
4 KiB
2 MiB
1 GiB
```

If the Page Size bit is set in the relevant entry, stop the normal page-table walk at that level.

Do not interpret a large-page entry as a pointer to a lower-level page table.

Calculate the physical address using the correct page size.

---

# 7. Virtual → Physical Translation

Verify the complete translation.

Example:

```text
Virtual:
0xFFFFFFFF827C03FC

Virtual page:
0xFFFFFFFF827C0000

Offset:
0x03FC

Physical page:
0x00000000004DA000

Physical address:
0x00000000004DA3FC
```

Do not accidentally treat the faulting virtual address as a page base.

For 4 KiB pages:

```text
Virtual page = VA & ~0xFFF
Offset        = VA & 0xFFF
```

Verify that the physical page actually exists.

---

# 8. PMM Debugging

For memory allocated dynamically, log:

```text
[PMM] Allocated page: 0x00000000004DA000
```

For freeing:

```text
[PMM] Freed page: 0x00000000004DA000
```

For every allocation determine:

```text
Owner
Size
Lifetime
Physical address
Purpose
```

Example:

```text
[PMM] Allocation
Physical: 0x00000000004DA000
Size:     4096
Owner:    USB transfer buffer
```

Never use a physical page after it has been returned to the PMM.

---

# 9. VMM Debugging

Every important mapping operation should be traceable.

Example:

```text
[VMM] Mapping page
[VMM] Virtual:  0xFFFFFFFF827C0000
[VMM] Physical: 0x00000000004DA000
[VMM] Flags:    PRESENT | WRITE
[VMM] Result:   SUCCESS
```

For unmapping:

```text
[VMM] Unmapping page
[VMM] Virtual:  0xFFFFFFFF827C0000
[VMM] Physical: 0x00000000004DA000
[VMM] Result:   SUCCESS
```

Verify that:

```text
map()
```

and:

```text
unmap()
```

operate on the expected page tables.

---

# 10. Allocation Boundaries

Verify that every accessed address belongs to the expected allocation.

Example:

```text
Allocation:
Start: 0xFFFFFFFF827C0000
Size:  0x1000
```

Valid range:

```text
0xFFFFFFFF827C0000
-
0xFFFFFFFF827C0FFF
```

Access:

```text
0xFFFFFFFF827D0000
```

is outside the allocation.

Possible causes:

* buffer overflow,
* incorrect pointer arithmetic,
* incorrect allocation size,
* structure-size mismatch,
* corrupted pointer,
* missing bounds check.

Do not simply map the next page.

---

# 11. Use-After-Free

If memory was previously valid and suddenly becomes invalid, check whether it was freed.

Example:

```text
[PMM] Freed page:
       0x00000000004DA000

[VMM] Unmapped:
       VA = 0xFFFFFFFF827C0000
       PA = 0x00000000004DA000
```

If the kernel later accesses the same region, investigate the ownership/lifetime bug.

Never solve use-after-free by keeping arbitrary pages mapped forever.

Fix the owner and lifetime logic.

---

# 12. Pointer Corruption

When an address looks suspicious, trace where it came from.

For example:

```text
0xFFFFFFFFFFFFFFFF
0x0000000000000000
0xCCCCCCCCCCCCCCCC
0xDEADBEEF
```

Do not assume that the pointer itself is the original problem.

Trace:

```text
allocation
→ initialization
→ storage
→ modification
→ function call
→ dereference
```

Look for:

* buffer overflows,
* uninitialized variables,
* incorrect casts,
* structure packing problems,
* wrong pointer arithmetic,
* stack corruption,
* use-after-free,
* incorrect interrupt register restoration.

---

# 13. HHDM and Higher-Half Addresses

NasuaOS uses higher-half/kernel virtual addresses and may use an HHDM mapping.

When accessing an address such as:

```text
0xFFFFFFFF827C03FC
```

verify which virtual-memory region it belongs to.

Do not assume that every high virtual address is valid.

Check:

```text
[HHDM]
Physical base:
Virtual base:
Mapped size:

[KERNEL]
Virtual base:
Physical base:
Mapped size:

[VMM]
Requested virtual:
Mapped physical:
```

Verify that the address is within the intended region.

---

# 14. Page Fault Handler Safety

The Page Fault handler itself must not cause another Page Fault.

Avoid in the early crash path:

* dynamic allocation,
* complex filesystem operations,
* GUI operations,
* accessing potentially unmapped memory,
* complex driver calls,
* recursive logging,
* code that depends on the broken subsystem.

The first debugging messages should use a reliable low-level output mechanism.

Example:

```text
[PAGE FAULT] ENTER
[PAGE FAULT] CR2 = ...
[PAGE FAULT] Error = ...
[PAGE FAULT] RIP = ...
[PAGE FAULT] CR3 = ...
```

If the system resets before producing these messages, investigate:

* IDT,
* GDT,
* exception entry,
* ISR stub,
* kernel stack,
* register saving/restoring,
* nested exceptions.

---

# 15. Triple Faults and Sudden Resets

If the machine suddenly resets, do not assume that the original fault is unknown.

Check:

* GDT,
* IDT,
* exception handlers,
* ISR stubs,
* interrupt stack,
* kernel stack,
* stack alignment,
* register preservation,
* Page Fault handler,
* Double Fault handler,
* interrupt return,
* `iretq`,
* interrupt enable state.

A common sequence may be:

```text
Original fault
    ↓
Page Fault
    ↓
Page Fault handler crashes
    ↓
Double Fault
    ↓
Double Fault handler crashes
    ↓
Triple Fault
    ↓
CPU reset
```

Therefore, a reset may be a secondary failure rather than the original problem.

---

# 16. GDT / IDT / CPU Exceptions

When debugging CPU exceptions verify:

```text
[GDT]
GDT address
GDT limit
Code segment
Data segment
TSS

[IDT]
IDT address
IDT limit
Exception vectors
IRQ vectors
Handler addresses
Gate type
Present bit
```

For every exception verify:

* correct vector,
* correct handler,
* correct stack,
* correct register frame,
* correct return address,
* correct segment state,
* correct `iretq`.

Do not modify exception handlers without checking the actual CPU state.

---

# 17. Interrupt Debugging

When debugging interrupts, check:

* IDT entry,
* IRQ/vector number,
* PIC/APIC configuration,
* IRQ masking,
* interrupt controller state,
* interrupt handler,
* EOI,
* saved registers,
* restored registers,
* stack state,
* interrupt enable state.

Expected structure:

```text
Hardware
    ↓
Interrupt Controller
    ↓
CPU
    ↓
IDT
    ↓
ISR Stub
    ↓
C/C++ Handler
    ↓
Device Acknowledge
    ↓
EOI
    ↓
IRETQ
```

Interrupt handlers should be as short and deterministic as reasonably possible.

Do not perform unnecessary heavy operations inside interrupt handlers.

---

# 18. Drivers

Every driver should have a consistent initialization sequence.

Example:

```text
[DRIVER] Initializing...
[DRIVER] Hardware detected
[DRIVER] Hardware configured
[DRIVER] Driver initialized
[DRIVER] Ready
```

Do not print:

```text
[DRIVER] Ready
```

if an essential initialization step failed.

Use:

```text
[DRIVER] ERROR: <description>
```

When possible, log hardware identification.

For PCI devices:

```text
[PCI] Device found: 00:03.0
[PCI] Vendor: 0x8086
[PCI] Device: 0x100E
[PCI] Class: 0x02
[PCI] Subclass: 0x00
```

---

# 19. PCI Debugging

When debugging PCI:

Check:

* bus,
* device,
* function,
* vendor ID,
* device ID,
* class,
* subclass,
* programming interface,
* BARs,
* command register,
* status register,
* IRQ configuration,
* MSI/MSI-X if used.

For BARs:

```text
[PCI] BAR0: 0x...
[PCI] BAR1: 0x...
```

Determine whether a BAR represents:

```text
Memory space
I/O space
32-bit
64-bit
Prefetchable
Non-prefetchable
```

Do not map a PCI BAR without checking its type and size.

---

# 20. USB / XHCI / EHCI Debugging

For USB host controllers log:

```text
[USB] Controller detected
[USB] Controller type: XHCI
[USB] MMIO base: 0x...
[USB] MMIO size: 0x...
[USB] Initialization started
[USB] Controller reset
[USB] Controller configured
[USB] Controller ready
```

For XHCI verify:

* MMIO mapping,
* capability registers,
* operational registers,
* controller reset,
* DCBAA,
* command ring,
* event ring,
* interrupter,
* device slots,
* port status,
* TRBs,
* physical memory used by controller.

For EHCI verify:

* BAR,
* MMIO mapping,
* capability length,
* operational registers,
* USB command,
* USB status,
* port status,
* periodic schedule,
* asynchronous schedule.

DMA buffers must be physically valid and correctly aligned.

---

# 21. PS/2 Keyboard and Mouse

For PS/2 debugging check:

```text
[PS2] Controller detected
[PS2] Keyboard initialized
[PS2] Mouse initialized
```

For keyboard:

```text
[KBD] Scancode: 0x...
[KBD] Key: ...
[KBD] State: DOWN/UP
```

For mouse:

```text
[MOUSE] Packet:
X: ...
Y: ...
Buttons: ...
```

Verify:

* scancode set,
* key press/release state,
* sign extension,
* X/Y movement,
* button bits,
* packet synchronization.

Do not fix incorrect cursor movement by arbitrarily reversing axes unless the packet decoding has been verified.

---

# 22. ATA / Storage Debugging

For ATA PIO verify:

* I/O ports,
* drive selection,
* status register,
* error register,
* sector count,
* LBA registers,
* command,
* BSY,
* DRQ,
* ERR,
* device presence.

Example:

```text
[ATA] Primary controller detected
[ATA] Drive selected
[ATA] Status: 0x...
[ATA] LBA: ...
[ATA] Sector count: ...
[ATA] Read command issued
[ATA] Read successful
```

Do not assume that a drive is absent merely because one command failed.

---

# 23. Filesystem Debugging

For filesystem failures verify:

* disk read,
* sector/block calculation,
* filesystem header,
* structure sizes,
* offsets,
* entry count,
* allocation table,
* file size,
* bounds,
* buffer size.

Never trust filesystem metadata without validating its bounds.

Before reading:

```text
offset + size
```

must remain inside the valid device/filesystem range.

---

# 24. GUI / Framebuffer Debugging

For graphics problems check:

```text
Framebuffer address
Width
Height
Pitch
Bytes per pixel
Resolution
Backbuffer address
Backbuffer size
```

Example:

```text
[FB] Address: 0x...
[FB] Width: 1280
[FB] Height: 720
[FB] Pitch: 5120
[FB] BPP: 32
```

Calculate framebuffer size correctly.

Do not assume:

```text
width * height * bytes_per_pixel
```

equals the pitch-based framebuffer size.

The pitch may contain padding.

---

# 25. Backbuffer and Flickering

For GUI rendering:

```text
Render
 ↓
Backbuffer
 ↓
Completed frame
 ↓
Framebuffer
```

Prefer rendering the complete frame into the backbuffer and then copying the finished frame to the framebuffer.

Avoid partially updating the visible framebuffer while other GUI elements are still being rendered.

For flickering investigate:

* rendering order,
* cursor drawing,
* framebuffer copy timing,
* backbuffer invalidation,
* double buffering,
* refresh rate,
* concurrent writes.

Do not fix flickering with arbitrary delays.

---

# 26. Timing and Delays

Do not add arbitrary delays to make a race condition or hardware problem disappear.

Bad:

```text
sleep(100);
```

added without understanding why the delay is required.

If timing matters, identify the actual synchronization requirement.

Use:

* hardware status bits,
* interrupts,
* controller state,
* proper polling conditions,
* timeouts.

Every polling loop should have a clear exit condition and preferably a timeout.

---

# 27. Compiler Warnings

Never ignore compiler warnings without checking them.

Warnings can indicate:

* incorrect pointer conversion,
* truncation,
* signed/unsigned bugs,
* uninitialized values,
* invalid casts,
* structure layout problems,
* unreachable code,
* incorrect function declarations.

Do not suppress warnings merely to obtain a successful build.

---

# 28. Undefined Behavior

Treat undefined behavior as a real bug.

Investigate:

* out-of-bounds accesses,
* invalid pointer dereferences,
* uninitialized variables,
* invalid casts,
* strict-aliasing violations,
* incorrect alignment,
* integer overflow,
* signed overflow,
* use-after-free,
* double-free,
* incorrect object lifetime.

A kernel may appear to work in QEMU while still containing undefined behavior.

---

# 29. QEMU vs VirtualBox vs Real Hardware

Do not assume that behavior in QEMU represents real hardware.

Likewise, do not assume that VirtualBox behaves exactly like QEMU.

Differences may exist in:

* ACPI,
* PCI,
* USB,
* APIC,
* timing,
* CPU features,
* firmware,
* framebuffer,
* storage,
* interrupt behavior.

When a bug occurs only on one platform, compare hardware/virtual hardware behavior before modifying the common code.

Prefer testing hardware-dependent code on real hardware when possible.

---

# 30. Reproduction

Every bug should have a reproducible test case.

Record:

```text
Environment:
QEMU / VirtualBox / Real Hardware

CPU:
...

RAM:
...

Boot mode:
UEFI / BIOS

Resolution:
...

Configuration:
...

Steps:
1. ...
2. ...
3. ...
```

A fix should be tested against the original reproduction steps.

---

# 31. Minimal Changes

Keep kernel and driver changes minimal.

Do not rewrite working code without a reason.

If the problem is:

```text
mouse packet decoding
```

do not rewrite:

```text
GUI
VMM
scheduler
filesystem
```

Prefer:

```text
smallest change
→ rebuild
→ test
→ verify
```

If the smallest change does not solve the root cause, investigate further before expanding the scope.

---

# 32. Preserve Existing Functionality

When fixing a bug:

* preserve working APIs,
* preserve working drivers,
* preserve existing behavior,
* avoid unnecessary refactoring,
* avoid changing unrelated interfaces.

If an API must change, explain why.

Example:

```text
Reason for API change:
The existing API cannot represent the required ownership/lifetime information.
```

---

# 33. Do Not Hide Errors

Never consider a bug fixed because:

* logging was removed,
* the error message was disabled,
* the exception was ignored,
* the feature was disabled,
* the crash was caught,
* the invalid operation was skipped,
* an arbitrary delay was added,
* memory was mapped without understanding why,
* permissions were made more permissive.

These are workarounds, not root-cause fixes.

---

# 34. Debugging Priority

Fix problems in this order:

```text
1. Memory corruption
2. Page Faults
3. Use-after-free
4. PMM / VMM / Paging
5. GDT / IDT
6. CPU exceptions
7. Interrupts
8. Kernel stack corruption
9. Hardware drivers
10. Filesystem
11. Kernel services
12. GUI / Graphics
13. Applications
```

A memory corruption problem takes priority over a GUI problem.

A Page Fault takes priority over an application bug.

Kernel stability always takes priority over cosmetic issues.

---

# 35. Bug Report Format

Use this format when reporting a problem:

```text
[BUG]

Component:
<component>

Environment:
<QEMU / VirtualBox / Real Hardware>

Expected:
<expected behavior>

Actual:
<actual behavior>

Logs:
<relevant logs>

Recent Changes:
<recent changes>

Reproduction:
<steps to reproduce>

Suspected Cause:
<known or suspected cause>

Verified Cause:
<confirmed root cause>

Fix:
<implemented fix>

Verification:
<test performed after the fix>
```

Do not fill `Suspected Cause` with a guess and present it as fact.

Use:

```text
Suspected Cause:
<reasoned hypothesis>
```

until it has been verified.

---

# 36. Root Cause Requirement

A bug is not considered solved until the actual cause is understood.

A proper fix should answer:

```text
What failed?
Why did it fail?
Where did the invalid state originate?
Why did the existing code allow it?
What was changed?
Why does the change prevent the problem?
How was the fix verified?
```

For example:

Bad conclusion:

```text
The Page Fault disappeared after mapping another page.
```

Good conclusion:

```text
The USB buffer was allocated as 4096 bytes but the controller wrote beyond
the allocation because the transfer length was calculated from the wrong
descriptor field. This corrupted the next page-table structure and later
caused a Page Fault. The transfer-length calculation was corrected and the
buffer bounds were validated.
```

The second explanation identifies the root cause.

---

# 37. AI Debugging Rules

When an AI is asked to debug NasuaOS, it must follow these rules.

Before modifying code:

```text
1. Read the relevant source files.
2. Read the relevant logs.
3. Identify the subsystem.
4. Identify the exact failing operation.
5. Trace the relevant data flow.
6. Determine what evidence supports the suspected cause.
```

The AI must not invent:

* memory addresses,
* hardware register values,
* PCI IDs,
* device capabilities,
* page mappings,
* logs,
* test results.

If information is missing, explicitly state what information is required.

Do not claim that code was tested if it was not actually tested.

Do not claim that hardware supports something without evidence.

---

# 38. AI Code Modification Rules

When modifying NasuaOS code:

* make the smallest reasonable change,
* preserve existing functionality,
* preserve existing APIs unless necessary,
* preserve useful debugging logs,
* do not introduce unrelated refactoring,
* do not disable functionality to hide a bug,
* do not add arbitrary delays,
* do not make all memory writable/executable,
* do not ignore compiler warnings,
* do not catch and ignore exceptions,
* do not silently ignore hardware errors.

Every modification should have a purpose connected to the diagnosed root cause.

---

# 39. Hardware Documentation

For hardware-dependent problems, consult the appropriate documentation/specification when available.

Examples:

```text
Intel SDM
AMD Architecture Programmer's Manual
PCI specification
USB specification
xHCI specification
EHCI specification
ACPI specification
ATA/ATAPI specification
Limine documentation
```

Do not guess register meanings or bit layouts.

If a register or bit is uncertain, verify it against documentation.

---

# 40. Final Debugging Checklist

Before considering a kernel bug fixed, verify:

```text
[ ] Problem reproduced
[ ] Logs collected
[ ] Failing subsystem identified
[ ] Root cause identified
[ ] Relevant memory mappings verified
[ ] Relevant registers verified
[ ] Error codes decoded
[ ] Allocation ownership checked
[ ] Page permissions checked
[ ] Interrupt state checked if relevant
[ ] Hardware state checked if relevant
[ ] Minimal fix implemented
[ ] Project rebuilt
[ ] Original reproduction tested
[ ] Existing functionality tested
[ ] No new warnings introduced
[ ] No useful logging removed
[ ] No arbitrary workaround added
[ ] Root cause documented
```

The final objective is not merely:

```text
"No crash."
```

The objective is:

```text
"The invalid state that caused the crash has been identified,
corrected, and verified."
```

---

# 41. Core Principle

> **Debug the cause, not the symptom.**

NasuaOS debugging should always favor:

```text
Evidence
    ↓
Reproduction
    ↓
Isolation
    ↓
Tracing
    ↓
Root Cause
    ↓
Minimal Fix
    ↓
Verification
```

Never replace this process with:

```text
Crash
 ↓
Guess
 ↓
Random change
 ↓
Delay
 ↓
Hope
```

A stable kernel is more important than a kernel that merely appears to work.

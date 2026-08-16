# NasuaOS — Debug Instructions

## Purpose

These instructions define the basic rules for debugging NasuaOS. Always diagnose the actual root cause of a problem instead of hiding its symptoms.

## 1. Logging

Always check logs before making changes.

Logs should include:

* component name,
* initialization stage,
* memory addresses when relevant,
* register values when relevant,
* error codes,
* a clear description of the event.

Example:

```text
[PCI] Device found: 00:03.0
[PCI] Vendor: 0x8086
[PCI] Device: 0x100E
```

Use consistent prefixes:

```text
[BOOT]
[CPU]
[MEM]
[PMM]
[VMM]
[PAGING]
[PCI]
[USB]
[XHCI]
[EHCI]
[ATA]
[PS2]
[KBD]
[MOUSE]
[NET]
[FS]
[GUI]
```

## 2. Page Faults

When a Page Fault occurs, check:

1. Faulting address.
2. Virtual address.
3. Physical address.
4. Relevant page-table entries.
5. Page permissions.
6. Whether the page was allocated.
7. Whether the mapping is correct.
8. Whether the accessed address is within the expected allocation.

Do not assume that the allocator is responsible. Verify the complete memory-management path.

Example:

```text
[PAGE FAULT]
Address: 0xFFFFFFFF827C03FC
Error:   0x0000000000000002
CR2:     0xFFFFFFFF827C03FC
```

## 3. Triple Faults / Kernel Crashes

When the system suddenly resets or causes a Triple Fault, check:

* GDT,
* IDT,
* CPU exception handlers,
* kernel stack,
* interrupt handlers,
* Page Fault handler,
* ISR register saving/restoring,
* interrupt return address,
* stack alignment.

If the system resets without producing a log, add debugging output as early as possible in the crash path.

## 4. Memory Debugging

For memory-related problems, log:

```text
[VMM] Virtual address
[VMM] Physical address
[VMM] Page flags
[VMM] Mapping result
[PMM] Allocated page
[PMM] Freed page
```

Never use memory after it has been freed.

Every memory allocation should have a clearly defined:

* owner,
* size,
* lifetime,
* permissions,
* deallocation responsibility.

Avoid silent memory corruption.

## 5. Drivers

Every driver should have its own logging prefix.

Driver initialization should follow a structure similar to:

```text
[DRIVER] Initializing...
[DRIVER] Hardware detected
[DRIVER] Hardware configured
[DRIVER] Driver initialized
[DRIVER] Ready
```

Errors should use:

```text
[DRIVER] ERROR: <description>
```

Do not report a driver as initialized if an essential initialization step failed.

## 6. Interrupts

When debugging interrupts, check:

* IDT entries,
* IRQ number,
* PIC/APIC configuration,
* IRQ masking,
* interrupt handler,
* EOI handling,
* saved registers,
* stack state,
* interrupt enable/disable state.

Interrupt handlers should be as short and deterministic as reasonably possible.

## 7. GUI / Framebuffer

For graphical problems, check:

* framebuffer address,
* width,
* height,
* pitch,
* bytes per pixel,
* resolution,
* backbuffer,
* rendering order,
* framebuffer copy operation.

For flickering problems, verify that rendering is performed to the backbuffer and that the completed frame is copied to the framebuffer at the appropriate time.

## 8. Debugging Procedure

Do not modify multiple unrelated subsystems at once.

Preferred procedure:

```text
1. Reproduce the problem.
2. Collect logs.
3. Identify the failing subsystem.
4. Reduce the problem to the smallest reproducible case.
5. Add targeted logging.
6. Identify the root cause.
7. Make the smallest reasonable fix.
8. Rebuild.
9. Test again.
10. Verify that the fix did not break another subsystem.
```

Every debugging change should have a clear purpose.

## 9. General Rules

* Do not guess memory addresses.
* Do not ignore compiler warnings.
* Do not remove useful logging just to hide errors.
* Do not change APIs without a reason.
* Do not fix crashes by adding arbitrary delays.
* Do not assume QEMU behaves exactly like real hardware.
* Test hardware-dependent code on real hardware when possible.
* Keep kernel and driver changes minimal.
* Preserve existing working functionality.
* Prefer deterministic fixes over workarounds.
* Check documentation and hardware specifications when debugging hardware-related problems.

## 10. Bug Report Format

Use this format when reporting a problem:

```text
[BUG]
Component: <component>
Environment: <QEMU / VirtualBox / Real Hardware>

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
```

## 11. Debug Priority

Fix issues in the following order:

1. Memory corruption / Page Faults
2. GDT / IDT / CPU exceptions
3. Interrupts
4. PMM / VMM / Paging
5. Hardware drivers
6. Filesystem
7. Kernel services
8. GUI / Graphics
9. Applications

Kernel stability and memory safety always have higher priority than cosmetic or application-level issues.

## 12. Root Cause

The goal of debugging is to find and fix the **actual root cause**.

Do not consider the problem solved if a change only:

* hides the error,
* prevents the log from appearing,
* adds an arbitrary delay,
* disables the affected feature,
* catches the crash without fixing its cause.

A proper fix should explain **why the bug occurred** and prevent it from happening again.

/* Darwin VM operations available through the public SDK of each Apple host.
 * iPhoneOS deliberately rejects <mach/mach_vm.h>. On ARM64 its vm_* API
 * uses pointer-sized addresses; macOS retains the existing mach_vm_* path.
 */
#ifndef XBOX_APPLE_VM_H
#define XBOX_APPLE_VM_H

#include <TargetConditionals.h>
#include <mach/mach.h>

#if TARGET_OS_IPHONE
#include <mach/vm_map.h>
typedef vm_address_t ngb_vm_address_t;
typedef vm_size_t ngb_vm_size_t;
#ifdef __cplusplus
static_assert(sizeof(vm_address_t) == sizeof(void *), "VM address must hold a host pointer");
#else
_Static_assert(sizeof(vm_address_t) == sizeof(void *), "VM address must hold a host pointer");
#endif
#define ngb_vm_map vm_map
#define ngb_vm_read_overwrite vm_read_overwrite
#define ngb_vm_region vm_region_64
#define ngb_vm_protect vm_protect
#define ngb_vm_write vm_write
#else
#include <mach/mach_vm.h>
typedef mach_vm_address_t ngb_vm_address_t;
typedef mach_vm_size_t ngb_vm_size_t;
#define ngb_vm_map mach_vm_map
#define ngb_vm_read_overwrite mach_vm_read_overwrite
#define ngb_vm_region mach_vm_region
#define ngb_vm_protect mach_vm_protect
#define ngb_vm_write mach_vm_write
#endif

#endif

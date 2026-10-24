/* Exercise real allocation/protection helpers, without any game input. */
#include "../src/port.h"
#include "platform/win32_compat.h"
#include <stdio.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "VM check failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

int main(void)
{
    size_t size = (size_t)sysconf(_SC_PAGESIZE);
    DWORD old;
    unsigned char readback = 0, replacement = 0x5A;
    unsigned char *page = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(page != NULL);
    page[16] = 0xA5;
    CHECK(ngb_mem_read(page + 16, &readback, 1) && readback == 0xA5);
    /* A fixed reservation must never replace another live allocation. */
    CHECK(VirtualAlloc(page, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == NULL);
    CHECK(page[16] == 0xA5);
    CHECK(VirtualProtect(page, size, PAGE_READONLY, &old));
    CHECK(ngb_mem_write(page + 16, &replacement, 1));
    CHECK(page[16] == replacement);
#ifdef __APPLE__
    ngb_vm_address_t region = (ngb_vm_address_t)page;
    ngb_vm_size_t span = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    CHECK(ngb_vm_region(mach_task_self(), &region, &span, VM_REGION_BASIC_INFO_64,
                       (vm_region_info_t)&info, &count, &object) == KERN_SUCCESS);
    if (object) mach_port_deallocate(mach_task_self(), object);
    CHECK(!(info.protection & VM_PROT_WRITE));
#endif
    CHECK(VirtualProtect(page, size, PAGE_READWRITE, &old));
    CHECK(VirtualFree(page, 0, MEM_RELEASE));
    /* Exercise the actual backing-file path that failed on the first phone.
     * The views must alias rather than allocate independent RAM copies. */
    HANDLE mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                       0, 64 * 1024 * 1024, NULL);
    CHECK(mapping != NULL);
    unsigned char *first = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
    unsigned char *second = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
    CHECK(first != NULL && second != NULL && first != second);
    first[16] = 0x3C;
    CHECK(second[16] == 0x3C);
    second[32] = 0xA7;
    CHECK(first[32] == 0xA7);
    CHECK(UnmapViewOfFile(first));
    CHECK(UnmapViewOfFile(second));
    CHECK(CloseHandle(mapping));
    puts("VM allocation collision, protected write and protection restore: OK");
    puts("64 MiB file mapping and bidirectional shared aliases: OK");
    return 0;
}

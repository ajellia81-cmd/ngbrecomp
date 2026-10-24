/* iPhoneOS ARM64 assembly compilation smoke test, not a playable game. */
#if !defined(__APPLE__) || !defined(__aarch64__)
#error Apple ARM64 required
#endif
__asm__(
".text\n.p2align 2\n.globl _ngb_ctx_switch_smoke\n_ngb_ctx_switch_smoke:\n"
"sub sp, sp, #176\n"
"stp x19, x20, [sp, #0]\nstp x21, x22, [sp, #16]\n"
"stp x23, x24, [sp, #32]\nstp x25, x26, [sp, #48]\n"
"stp x27, x28, [sp, #64]\nstp x29, x30, [sp, #80]\n"
"stp d8, d9, [sp, #96]\nstp d10, d11, [sp, #112]\n"
"stp d12, d13, [sp, #128]\nstp d14, d15, [sp, #144]\n"
"mrs x9, fpcr\nmrs x10, fpsr\nstp x9, x10, [sp, #160]\n"
"mov x9, sp\nstr x9, [x0]\nmov sp, x1\n"
"ldp x19, x20, [sp, #0]\nldp x21, x22, [sp, #16]\n"
"ldp x23, x24, [sp, #32]\nldp x25, x26, [sp, #48]\n"
"ldp x27, x28, [sp, #64]\nldp x29, x30, [sp, #80]\n"
"ldp d8, d9, [sp, #96]\nldp d10, d11, [sp, #112]\n"
"ldp d12, d13, [sp, #128]\nldp d14, d15, [sp, #144]\n"
"ldp x9, x10, [sp, #160]\nmsr fpcr, x9\nmsr fpsr, x10\n"
"add sp, sp, #176\nret\n");
extern void ngb_ctx_switch_smoke(void **save, void *load);
void (*ngb_smoke_symbol(void))(void **, void *) { return ngb_ctx_switch_smoke; }

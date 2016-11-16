/*
* UAE - The Un*x Amiga Emulator
*
* ucon.resource
*
*/

#include "sysconfig.h"
#include "sysdeps.h"

#include "uae.h"
#include "options.h"
#include "uae/memory.h"
#include "custom.h"
#include "newcpu.h"
#include "traps.h"
#include "autoconf.h"
#include "execlib.h"
#include "uaeresource.h"
#include "native2amiga.h"
#include "ucon.h"

//#define UCON_DEBUG
#ifdef UCON_DEBUG
#define DEBUG_LOG(format, ...) \
    do { \
        printf("ucon: " format, ##__VA_ARGS__); \
    } while(0)
#else
#define DEBUG_LOG(format, ...)
#endif

#define UCON_VERSION_MAJOR      0
#define UCON_VERSION_MINOR      1
#define UCON_VERSION_REV        0

static uaecptr res_init, res_name, res_id, base;

#define UCON_FLAG_QUIT_EMU      1

static int flags;

/* functions */

static uae_u32 REGPARAM2 ucon_init (TrapContext *ctx)
{
    uae_u32 read_sig_mask = m68k_dreg(regs, 0);
    uae_u32 ucon_task = m68k_dreg(regs, 1);
    DEBUG_LOG("init: read_sig_mask=%08x, ucon_task=%08x\n", read_sig_mask, ucon_task);

    int res = ucon_host_init(ucon_task, read_sig_mask);

    DEBUG_LOG("init done. res=%d\n", res);
    return res;
}

static uae_u32 REGPARAM2 ucon_exit (TrapContext *ctx)
{
    DEBUG_LOG("exit\n");

    ucon_host_exit();

    if(flags & UCON_FLAG_QUIT_EMU) {
        DEBUG_LOG("quit emu\n");
        uae_quit();
    }

    DEBUG_LOG("exit done.\n");
    return 0;
}

static uae_u32 REGPARAM2 ucon_open (TrapContext *ctx)
{
    uae_u32 fh = m68k_dreg(regs, 0);
    uae_u32 name_addr = m68k_dreg(regs, 1);
    uae_u32 type = m68k_dreg(regs, 2);
    uae_u8 *name_ptr = get_real_address(name_addr);

    /* check for flags */
    if(strstr((const char *)name_ptr, "QUIT_EMU") != NULL) {
        flags |= UCON_FLAG_QUIT_EMU;
    }

    DEBUG_LOG("open: fh=%08x, name='%s', type=%d, flags=%x\n", fh, name_ptr, type, flags);
    return 0;
}

static uae_u32 REGPARAM2 ucon_close (TrapContext *ctx)
{
    uae_u32 fh = m68k_dreg(regs, 0);

    DEBUG_LOG("close: fh=%08x\n", fh);
    return 0;
}

static uae_u32 REGPARAM2 ucon_read (TrapContext *ctx)
{
    uae_u32 buffer_addr =m68k_dreg(regs, 0);
    uae_u32 length = m68k_dreg(regs, 1);
    uae_u8 *ptr = get_real_address(buffer_addr);
    uae_u32 res_ptr = m68k_dreg(regs, 2);

    DEBUG_LOG("read: buf=%08x, len=%08x, res_ptr=%08x\n", buffer_addr, length, res_ptr);

    ucon_host_read(ptr, length, res_ptr);

    return 0;
}

static uae_u32 REGPARAM2 ucon_write (TrapContext *ctx)
{
    uae_u32 buffer_addr =m68k_dreg(regs, 0);
    uae_u32 length = m68k_dreg(regs, 1);
    uae_u8 *ptr = get_real_address(buffer_addr);

#ifdef UCON_DEBUG
    uae_u8 *clone = (uae_u8 *)malloc(length + 1);
    memcpy(clone, ptr, length);
    clone[length] = 0;

    DEBUG_LOG("write: buf=%08x, len=%08x: '%s'\n", buffer_addr, length, clone);
#else
    ucon_host_write(ptr, length);
#endif

    return length;
}

/* called to setup library base fields (if any) */

static uae_u32 REGPARAM2 ucon_initcode (TrapContext *ctx)
{
    DEBUG_LOG("initcode: context = %p\n", ctx);
    base = m68k_dreg (regs, 0);

#if 0
    /* setup UconResource */
    uaecptr rb = base + SIZEOF_LIBRARY;
    put_word (rb + 0, UAEMAJOR);
    put_long (rb + 8, rtarea_base);
#endif

    return base;
}

uaecptr ucon_startup (uaecptr resaddr)
{
    DEBUG_LOG("startup\n");

    put_word (resaddr + 0x0, 0x4AFC);
    put_long (resaddr + 0x2, resaddr);
    put_long (resaddr + 0x6, resaddr + 0x1A); /* Continue scan here */
    put_word (resaddr + 0xA, 0x8101); /* RTF_AUTOINIT|RTF_COLDSTART; Version 1 */
    put_word (resaddr + 0xC, 0x0878); /* NT_DEVICE; pri 05 */
    put_long (resaddr + 0xE, res_name);
    put_long (resaddr + 0x12, res_id);
    put_long (resaddr + 0x16, res_init);
    resaddr += 0x1A;
    return resaddr;
}

void ucon_install (void)
{
    DEBUG_LOG("install\n");

    /* resource name, id */
    TCHAR tmp[100];
    _stprintf (tmp, _T("ucon resource %d.%d.%d"),
        UCON_VERSION_MAJOR,
        UCON_VERSION_MINOR,
        UCON_VERSION_REV);
    res_name = ds (_T("ucon.resource"));
    res_id = ds (tmp);

    /* init resource function */
    uae_u32 initcode = here ();
    calltrap (deftrap (ucon_initcode)); dw (RTS);

    /* resource functions */
    uae_u32 func_ucon_init = here ();
    calltrap (deftrap (ucon_init)); dw (RTS);
    uae_u32 func_ucon_exit = here ();
    calltrap (deftrap (ucon_exit)); dw (RTS);
    uae_u32 func_ucon_open = here ();
    calltrap (deftrap (ucon_open)); dw (RTS);
    uae_u32 func_ucon_close = here ();
    calltrap (deftrap (ucon_close)); dw (RTS);
    uae_u32 func_ucon_read = here ();
    calltrap (deftrap (ucon_read)); dw (RTS);
    uae_u32 func_ucon_write = here ();
    calltrap (deftrap (ucon_write)); dw (RTS);

    /* FuncTable */
    uae_u32 functable = here ();
    dl (func_ucon_init);
    dl (func_ucon_exit);
    dl (func_ucon_open);
    dl (func_ucon_close);
    dl (func_ucon_read);
    dl (func_ucon_write);
    dl (0xFFFFFFFF); /* end of table */

    /* DataTable */
    uae_u32 datatable = here ();
    dw (0xE000); /* INITBYTE */
    dw (0x0008); /* LN_TYPE */
    dw (0x0800); /* NT_RESOURCE */
    dw (0xC000); /* INITLONG */
    dw (0x000A); /* LN_NAME */
    dl (res_name);
    dw (0xE000); /* INITBYTE */
    dw (0x000E); /* LIB_FLAGS */
    dw (0x0600); /* LIBF_SUMUSED | LIBF_CHANGED */
    dw (0xD000); /* INITWORD */
    dw (0x0014); /* LIB_VERSION */
    dw (UCON_VERSION_MAJOR);
    dw (0xD000); /* INITWORD */
    dw (0x0016); /* LIB_REVISION */
    dw (UCON_VERSION_MINOR);
    dw (0xC000); /* INITLONG */
    dw (0x0018); /* LIB_IDSTRING */
    dl (res_id);
    dw (0x0000); /* end of table */

    res_init = here ();
    dl (SIZEOF_LIBRARY /*+ 12*/); /* size of device base */
    dl (functable);
    dl (datatable);
    dl (initcode);
}

void ucon_cleanup (void)
{
    DEBUG_LOG("cleanup\n");
    ucon_host_cleanup();
}

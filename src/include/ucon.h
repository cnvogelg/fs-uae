#ifndef UAE_UCON_H
#define UAE_UCON_H

#include "uae/types.h"

uaecptr ucon_startup (uaecptr resaddr);
void ucon_install (void);
void ucon_cleanup (void);

int ucon_host_init(uae_u32 task_ptr, uae_u32 read_sig_mask);
void ucon_host_exit();
void ucon_host_read(uae_u8 *ptr, uae_u32 length, uae_u32 res_ptr);
void ucon_host_write(uae_u8 *ptr, uae_u32 length);
void ucon_host_cleanup (void);

#endif /* UAE_UCON_H */

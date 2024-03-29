/* bitmap.c */
BOOL bitmap_decompress(uint8 * output, int width, int height, uint8 * input, int size, int Bpp);
/* cache.c */
HBITMAP cache_get_bitmap(uint8 cache_id, uint16 cache_idx);
void cache_put_bitmap(uint8 cache_id, uint16 cache_idx, HBITMAP bitmap, uint32 stamp);
void cache_save_state(void);
FONTGLYPH *cache_get_font(uint8 font, uint16 character);
void cache_put_font(uint8 font, uint16 character, uint16 offset, uint16 baseline, uint16 width,
		    uint16 height, HGLYPH pixmap);
DATABLOB *cache_get_text(uint8 cache_id);
void cache_put_text(uint8 cache_id, void *data, int length);
uint8 *cache_get_desktop(uint32 offset, int cx, int cy, int bytes_per_pixel);
void cache_put_desktop(uint32 offset, int cx, int cy, int scanline, int bytes_per_pixel,
		       uint8 * data);
HCURSOR cache_get_cursor(uint16 cache_idx);
void cache_put_cursor(uint16 cache_idx, HCURSOR cursor);
/* channels.c */
VCHANNEL *channel_register(char *name, uint32 flags, void (*callback) (STREAM));
STREAM channel_init(VCHANNEL * channel, uint32 length);
void channel_send(STREAM s, VCHANNEL * channel);
void channel_process(STREAM s, uint16 mcs_channel);
/* cliprdr.c */
void cliprdr_send_text_format_announce(void);
void cliprdr_send_blah_format_announce(void);
void cliprdr_send_native_format_announce(uint8 * data, uint32 length);
void cliprdr_send_data_request(uint32 format);
void cliprdr_send_data(uint8 * data, uint32 length);
BOOL cliprdr_init(void);
/* disk.c */
int disk_enum_devices(uint32 * id, char *optarg);
NTSTATUS disk_query_information(NTHANDLE handle, uint32 info_class, STREAM out);
NTSTATUS disk_set_information(NTHANDLE handle, uint32 info_class, STREAM in, STREAM out);
NTSTATUS disk_query_volume_information(NTHANDLE handle, uint32 info_class, STREAM out);
NTSTATUS disk_query_directory(NTHANDLE handle, uint32 info_class, char *pattern, STREAM out);
NTSTATUS disk_create_notify(NTHANDLE handle, uint32 info_class);
NTSTATUS disk_check_notify(NTHANDLE handle);
/* mppc.c */
int mppc_expand(uint8 * data, uint32 clen, uint8 ctype, uint32 * roff, uint32 * rlen);
/* ewmhints.c */
int get_current_workarea(uint32 * x, uint32 * y, uint32 * width, uint32 * height);
/* iso.c */
STREAM iso_init(int length);
void iso_send(STREAM s);
STREAM iso_recv(uint8 * rdpver);
BOOL iso_connect(char *server, char *username);
void iso_disconnect(void);
/* licence.c */
void licence_process(STREAM s);
/* mcs.c */
STREAM mcs_init(int length);
void mcs_send_to_channel(STREAM s, uint16 channel);
void mcs_send(STREAM s);
STREAM mcs_recv(uint16 * channel, uint8 * rdpver);
BOOL mcs_connect(char *server, STREAM mcs_data, char *username);
void mcs_disconnect(void);
/* orders.c */
void process_orders(STREAM s, uint16 num_orders);
void reset_order_state(void);
/* parallel.c */
int parallel_enum_devices(uint32 * id, char *optarg);
/* printer.c */
int printer_enum_devices(uint32 * id, char *optarg);
/* printercache.c */
int printercache_load_blob(char *printer_name, uint8 ** data);
void printercache_process(STREAM s);
/* pstcache.c */
void pstcache_touch_bitmap(uint8 id, uint16 idx, uint32 stamp);
BOOL pstcache_load_bitmap(uint8 id, uint16 idx);
BOOL pstcache_put_bitmap(uint8 id, uint16 idx, uint8 * bmp_id, uint16 wd,
			 uint16 ht, uint16 len, uint8 * data);
int pstcache_enumerate(uint8 id, uint8 * list);
BOOL pstcache_init(uint8 id);
/* rdesktop.c */
int main(int argc, char *argv[]);
void generate_random(uint8 * random);
void *xmalloc(int size);
void *xrealloc(void *oldmem, int size);
void xfree(void *mem);
void error(char *format, ...);
void warning(char *format, ...);
void unimpl(char *format, ...);
void hexdump(unsigned char *p, unsigned int len);
char *next_arg(char *src, char needle);
void toupper_str(char *p);
char *l_to_a(long N, int base);
int load_licence(unsigned char **data);
void save_licence(unsigned char *data, int length);
BOOL rd_pstcache_mkdir(void);
int rd_open_file(char *filename);
void rd_close_file(int fd);
int rd_read_file(int fd, void *ptr, int len);
int rd_write_file(int fd, void *ptr, int len);
int rd_lseek_file(int fd, int offset);
BOOL rd_lock_file(int fd, int start, int len);
/* rdp5.c */
void rdp5_process(STREAM s);
/* rdp.c */
void rdp_out_unistr(STREAM s, char *string, int len);
int rdp_in_unistr(STREAM s, char *string, int uni_len);
void rdp_send_input(uint32 time, uint16 message_type, uint16 device_flags, uint16 param1,
		    uint16 param2);
void process_colour_pointer_pdu(STREAM s);
void process_cached_pointer_pdu(STREAM s);
void process_system_pointer_pdu(STREAM s);
void process_bitmap_updates(STREAM s);
void process_palette(STREAM s);
BOOL rdp_loop(BOOL * deactivated, uint32 * ext_disc_reason);
void rdp_main_loop(BOOL * deactivated, uint32 * ext_disc_reason);
BOOL rdp_connect(char *server, uint32 flags, char *domain, char *password, char *command,
		 char *directory);
void rdp_disconnect(void);
/* rdpdr.c */
int get_device_index(NTHANDLE handle);
void convert_to_unix_filename(char *filename);
BOOL rdpdr_init(void);
void rdpdr_add_fds(int *n, fd_set * rfds, fd_set * wfds, struct timeval *tv, BOOL * timeout);
struct async_iorequest *rdpdr_remove_iorequest(struct async_iorequest *prev,
					       struct async_iorequest *iorq);
void rdpdr_check_fds(fd_set * rfds, fd_set * wfds, BOOL timed_out);
BOOL rdpdr_abort_io(uint32 fd, uint32 major, NTSTATUS status);
/* rdpsnd.c */
void rdpsnd_send_completion(uint16 tick, uint8 packet_index);
BOOL rdpsnd_init(void);
/* rdpsnd_oss.c */
BOOL wave_out_open(void);
void wave_out_close(void);
BOOL wave_out_format_supported(WAVEFORMATEX * pwfx);
BOOL wave_out_set_format(WAVEFORMATEX * pwfx);
void wave_out_volume(uint16 left, uint16 right);
void wave_out_write(STREAM s, uint16 tick, uint8 index);
void wave_out_play(void);
/* secure.c */
void sec_hash_48(uint8 * out, uint8 * in, uint8 * salt1, uint8 * salt2, uint8 salt);
void sec_hash_16(uint8 * out, uint8 * in, uint8 * salt1, uint8 * salt2);
void buf_out_uint32(uint8 * buffer, uint32 value);
void sec_sign(uint8 * signature, int siglen, uint8 * session_key, int keylen, uint8 * data,
	      int datalen);
void sec_decrypt(uint8 * data, int length);
STREAM sec_init(uint32 flags, int maxlen);
void sec_send_to_channel(STREAM s, uint32 flags, uint16 channel);
void sec_send(STREAM s, uint32 flags);
void sec_process_mcs_data(STREAM s);
STREAM sec_recv(uint8 * rdpver);
BOOL sec_connect(char *server, char *username);
void sec_disconnect(void);
/* serial.c */
int serial_enum_devices(uint32 * id, char *optarg);
BOOL serial_get_timeout(NTHANDLE handle, uint32 length, uint32 * timeout, uint32 * itv_timeout);
BOOL serial_get_event(NTHANDLE handle, uint32 * result);
/* tcp.c */
STREAM tcp_init(uint32 maxlen);
void tcp_send(STREAM s);
STREAM tcp_recv(STREAM s, uint32 length);
BOOL tcp_connect(char *server);
void tcp_disconnect(void);
char *tcp_get_address(void);
/* xclip.c */
void ui_clip_format_announce(uint8 * data, uint32 length);
void ui_clip_handle_data(uint8 * data, uint32 length);
void ui_clip_request_data(uint32 format);
void ui_clip_sync(void);
void xclip_init(void);
/* xkeymap.c */
void xkeymap_init(void);
BOOL handle_special_keys(uint32 keysym, unsigned int state, uint32 ev_time, BOOL pressed);
key_translation xkeymap_translate_key(uint32 keysym, unsigned int keycode, unsigned int state);
uint16 xkeymap_translate_button(unsigned int button);
char *get_ksname(uint32 keysym);
void save_remote_modifiers(uint8 scancode);
void restore_remote_modifiers(uint32 ev_time, uint8 scancode);
void ensure_remote_modifiers(uint32 ev_time, key_translation tr);
unsigned int read_keyboard_state(void);
uint16 ui_get_numlock_state(unsigned int state);
void reset_modifier_keys(void);
void rdp_send_scancode(uint32 time, uint16 flags, uint8 scancode);
/* xwin.c */
void ui_begin_update(void);
void ui_end_update(void);
BOOL get_key_state(unsigned int state, uint32 keysym);
BOOL ui_init(void);
void ui_deinit(void);
BOOL ui_create_window(void);
void ui_resize_window(void);
void ui_destroy_window(void);
void xwin_toggle_fullscreen(void);
int ui_select(int rdp_socket);
void ui_move_pointer(int x, int y);
HBITMAP ui_create_bitmap(int width, int height, uint8 * data);
void ui_paint_bitmap(int x, int y, int cx, int cy, int width, int height, uint8 * data);
void ui_destroy_bitmap(HBITMAP bmp);
HGLYPH ui_create_glyph(int width, int height, uint8 * data);
void ui_destroy_glyph(HGLYPH glyph);
HCURSOR ui_create_cursor(unsigned int x, unsigned int y, int width, int height, uint8 * andmask,
			 uint8 * xormask);
void ui_set_cursor(HCURSOR cursor);
void ui_destroy_cursor(HCURSOR cursor);
void ui_set_null_cursor(void);
HCOLOURMAP ui_create_colourmap(COLOURMAP * colours);
void ui_destroy_colourmap(HCOLOURMAP map);
void ui_set_colourmap(HCOLOURMAP map);
void ui_set_clip(int x, int y, int cx, int cy);
void ui_reset_clip(void);
void ui_bell(void);
void ui_destblt(uint8 opcode, int x, int y, int cx, int cy);
void ui_patblt(uint8 opcode, int x, int y, int cx, int cy, BRUSH * brush, int bgcolour,
	       int fgcolour);
void ui_screenblt(uint8 opcode, int x, int y, int cx, int cy, int srcx, int srcy);
void ui_memblt(uint8 opcode, int x, int y, int cx, int cy, HBITMAP src, int srcx, int srcy);
void ui_triblt(uint8 opcode, int x, int y, int cx, int cy, HBITMAP src, int srcx, int srcy,
	       BRUSH * brush, int bgcolour, int fgcolour);
void ui_line(uint8 opcode, int startx, int starty, int endx, int endy, PEN * pen);
void ui_rect(int x, int y, int cx, int cy, int colour);
void ui_draw_glyph(int mixmode, int x, int y, int cx, int cy, HGLYPH glyph, int srcx, int srcy,
		   int bgcolour, int fgcolour);
void ui_draw_text(uint8 font, uint8 flags, int mixmode, int x, int y, int clipx, int clipy,
		  int clipcx, int clipcy, int boxx, int boxy, int boxcx, int boxcy, int bgcolour,
		  int fgcolour, uint8 * text, uint8 length);
void ui_desktop_save(uint32 offset, int x, int y, int cx, int cy);
void ui_desktop_restore(uint32 offset, int x, int y, int cx, int cy);

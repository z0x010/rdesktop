/* bitmap.c */
BOOL bitmap_decompress(unsigned char *output, int width, int height, unsigned char *input, int size);
/* cache.c */
HBITMAP cache_get_bitmap(uint8 cache_id, uint16 cache_idx);
void cache_put_bitmap(uint8 cache_id, uint16 cache_idx, HBITMAP bitmap);
FONTGLYPH *cache_get_font(uint8 font, uint16 character);
void cache_put_font(uint8 font, uint16 character, uint16 offset, uint16 baseline, uint16 width, uint16 height, HGLYPH pixmap);
DATABLOB *cache_get_text(uint8 cache_id);
void cache_put_text(uint8 cache_id, void *data, int length);
uint8 *cache_get_desktop(uint32 offset, int cx, int cy, int bytes_per_pixel);
void cache_put_desktop(uint32 offset, int cx, int cy, int scanline, int bytes_per_pixel, uint8 *data);
HCURSOR cache_get_cursor(uint16 cache_idx);
void cache_put_cursor(uint16 cache_idx, HCURSOR cursor);
/* iso.c */
STREAM iso_init(int length);
void iso_send(STREAM s);
STREAM iso_recv(void);
BOOL iso_connect(char *server);
void iso_disconnect(void);
/* licence.c */
void licence_generate_keys(uint8 *client_key, uint8 *server_key, uint8 *client_rsa);
void licence_process(STREAM s);
/* mcs.c */
STREAM mcs_init(int length);
void mcs_send(STREAM s);
STREAM mcs_recv(void);
BOOL mcs_connect(char *server, STREAM mcs_data);
void mcs_disconnect(void);
/* orders.c */
void process_orders(STREAM s);
void reset_order_state(void);
/* rdesktop.c */
int main(int argc, char *argv[]);
void generate_random(uint8 *random);
void *xmalloc(int size);
void *xrealloc(void *oldmem, int size);
void xfree(void *mem);
void error(char *format, ...);
void unimpl(char *format, ...);
void hexdump(unsigned char *p, unsigned int len);
/* rdp.c */
void rdp_out_unistr(STREAM s, char *string, int len);
void rdp_send_input(uint32 time, uint16 message_type, uint16 device_flags, uint16 param1, uint16 param2);
void rdp_main_loop(void);
BOOL rdp_connect(char *server, uint32 flags, char *domain, char *password, char *command, char *directory);
void rdp_disconnect(void);
/* secure.c */
void sec_hash_48(uint8 *out, uint8 *in, uint8 *salt1, uint8 *salt2, uint8 salt);
void sec_hash_16(uint8 *out, uint8 *in, uint8 *salt1, uint8 *salt2);
void buf_out_uint32(uint8 *buffer, uint32 value);
void sec_sign(uint8 *signature, uint8 *session_key, int length, uint8 *data, int datalen);
STREAM sec_init(uint32 flags, int maxlen);
void sec_send(STREAM s, uint32 flags);
STREAM sec_recv(void);
BOOL sec_connect(char *server);
void sec_disconnect(void);
/* tcp.c */
STREAM tcp_init(int maxlen);
void tcp_send(STREAM s);
STREAM tcp_recv(int length);
BOOL tcp_connect(char *server);
void tcp_disconnect(void);
/* xwin.c */
BOOL ui_create_window(char *title);
void ui_destroy_window(void);
void ui_select(int rdp_socket);
void ui_move_pointer(int x, int y);
HBITMAP ui_create_bitmap(int width, int height, uint8 *data);
void ui_paint_bitmap(int x, int y, int cx, int cy, int width, int height, uint8 *data);
void ui_destroy_bitmap(HBITMAP bmp);
HCURSOR ui_create_cursor(unsigned int x, unsigned int y, int width, int height, uint8 *mask, uint8 *data);
void ui_set_cursor(HCURSOR cursor);
void ui_destroy_cursor(HCURSOR cursor);
HGLYPH ui_create_glyph(int width, int height, uint8 *data);
void ui_destroy_glyph(HGLYPH glyph);
HCOLOURMAP ui_create_colourmap(COLOURMAP *colours);
void ui_destroy_colourmap(HCOLOURMAP map);
void ui_set_colourmap(HCOLOURMAP map);
void ui_set_clip(int x, int y, int cx, int cy);
void ui_reset_clip(void);
void ui_bell(void);
void ui_destblt(uint8 opcode, int x, int y, int cx, int cy);
void ui_patblt(uint8 opcode, int x, int y, int cx, int cy, BRUSH *brush, int bgcolour, int fgcolour);
void ui_screenblt(uint8 opcode, int x, int y, int cx, int cy, int srcx, int srcy);
void ui_memblt(uint8 opcode, int x, int y, int cx, int cy, HBITMAP src, int srcx, int srcy);
void ui_triblt(uint8 opcode, int x, int y, int cx, int cy, HBITMAP src, int srcx, int srcy, BRUSH *brush, int bgcolour, int fgcolour);
void ui_line(uint8 opcode, int startx, int starty, int endx, int endy, PEN *pen);
void ui_rect(int x, int y, int cx, int cy, int colour);
void ui_draw_glyph(int mixmode, int x, int y, int cx, int cy, HGLYPH glyph, int srcx, int srcy, int bgcolour, int fgcolour);
void ui_draw_text(uint8 font, uint8 flags, int mixmode, int x, int y, int clipx, int clipy, int clipcx, int clipcy, int boxx, int boxy, int boxcx, int boxcy, int bgcolour, int fgcolour, uint8 *text, uint8 length);
void ui_desktop_save(uint32 offset, int x, int y, int cx, int cy);
void ui_desktop_restore(uint32 offset, int x, int y, int cx, int cy);
void ui_grab_keyboard(void);
void ui_ungrab_keyboard(void);

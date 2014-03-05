#ifdef CONFIG_AVAHI
void avahi_init(void);
void avahi_done(void);
#else
static inline void avahi_init(void) { }
static inline void avahi_done(void) { }
#endif

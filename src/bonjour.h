#ifdef CONFIG_BONJOUR
void bonjour_init(void);
void bonjour_done(void);
#else
static inline void bonjour_init(void) { }
static inline void bonjour_done(void) { }
#endif

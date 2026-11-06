enum {APP_MODE_OBSERVE=0,APP_MODE_DRIVE=3};
unsigned app_mode_current(void);
int app_mode_allows(unsigned);
int app_mode_change_pending(void);

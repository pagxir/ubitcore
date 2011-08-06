#ifndef _KAD_STORE_H_
#define _KAD_STORE_H_
int kad_get_values(const char *info_hash, char *value, size_t len);
int announce_value(const char *info_hash, const char *value, size_t len);
#endif


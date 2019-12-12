#ifndef lib_vmac_vmac_h
#define lib_vmac_vmac_h

#include<inttypes.h>

#ifdef __cplusplus
extern "C"
{
#endif
	void vmac_register(void *fn);

	void send_vmac(uint16_t type, uint16_t rate, uint16_t seq, char* buf, uint16_t len, char* InterestName, uint16_t name_len);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* lib_vmac_vmac_h */

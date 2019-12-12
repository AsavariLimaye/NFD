#include <stdio.h>
#include <stdlib.h>

#include<inttypes.h>

void vmac_register(void * ptr) {
	 printf("Called vmac_register\n");
	 void (*fn) (uint8_t type, uint64_t enc, char* data, uint16_t len, uint16_t seq, char* interest_name, uint16_t interest_name_len) = ptr;
}

void send_vmac(uint16_t type, uint16_t rate, uint16_t seq, char* buf, uint16_t len, char* InterestName, uint16_t name_len){
	 printf("Called send_vmac\n");
}

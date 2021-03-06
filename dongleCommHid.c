/*
*******************************************************************************    
*   BTChip Bitcoin Hardware Wallet C test interface
*   (c) 2014 BTChip - 1BTChip7VfTnrPra5jqci7ejnMguuHogTn
*   
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*   Unless required by applicable law or agreed to in writing, software
*   distributed under the License is distributed on an "AS IS" BASIS,
*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*   limitations under the License.
********************************************************************************/

#include "dongleCommHid.h"

#define BTCHIP_VID 0x2581
#define BTCHIP_HID_PID 0x2b7c
#define BTCHIP_HID_BOOTLOADER_PID 0x1807

#define TIMEOUT 10000
#define SW1_DATA 0x61
#define MAX_BLOCK 64

int initHid() {
	return libusb_init(NULL);
}

int exitHid() {
	libusb_exit(NULL);
	return 0;
}

int sendApduHid(libusb_device_handle *handle, const unsigned char *apdu, size_t apduLength, unsigned char *out, size_t outLength, int *sw) {
	unsigned char buffer[260];
	unsigned char paddingBuffer[MAX_BLOCK];
	int result;
	int length;
	int swOffset;
	int remaining = apduLength;
	int offset = 0;

	while (remaining > 0) {
		int blockSize = (remaining > MAX_BLOCK ? MAX_BLOCK : remaining);
		memset(paddingBuffer, 0, MAX_BLOCK);
		memcpy(paddingBuffer, apdu + offset, blockSize);
		result = libusb_interrupt_transfer(handle, 0x02, paddingBuffer, blockSize, &length, TIMEOUT);
		if (result < 0) {
			return result;
		}		
		offset += blockSize;
		remaining -= blockSize;
	}
	result = libusb_interrupt_transfer(handle, 0x82, buffer, MAX_BLOCK, &length, TIMEOUT);
	if (result < 0) {
		return result;
	}	
	offset = MAX_BLOCK;
	if (buffer[0] == SW1_DATA) {		
		int dummy;
		length = buffer[1];
		length += 2;
		if (length > (MAX_BLOCK - 2)) {			
			remaining = length - (MAX_BLOCK - 2);
			while (remaining != 0) {
				int blockSize;
				if (remaining > MAX_BLOCK) {
					blockSize = MAX_BLOCK;
				}
				else {
					blockSize = remaining;
				}
				result = libusb_interrupt_transfer(handle, 0x82, buffer + offset, MAX_BLOCK, &dummy, TIMEOUT);
				if (result < 0) {
					return result;
				}
				offset += blockSize;
				remaining -= blockSize;
			}
		}
		length -= 2;
		memcpy(out, buffer + 2, length);
		swOffset = 2 + length;
	}
	else {
		length = 0;
		swOffset = 0;
	}
	if (sw != NULL) {
		*sw = (buffer[swOffset] << 8) | buffer[swOffset + 1];
	}
	return length;
}

libusb_device_handle* getFirstDongleHid() {
	libusb_device_handle *result = libusb_open_device_with_vid_pid(NULL, BTCHIP_VID, BTCHIP_HID_PID);
	if (result == NULL) {
		result = libusb_open_device_with_vid_pid(NULL, BTCHIP_VID, BTCHIP_HID_BOOTLOADER_PID);
		if (result == NULL) {		
			return NULL;
		}
	}
	libusb_detach_kernel_driver(result, 0);
	libusb_claim_interface(result, 0);
	return result;
}

void closeDongleHid(libusb_device_handle *handle) {	
	libusb_release_interface(handle, 0);
	libusb_attach_kernel_driver(handle, 0);
	libusb_close(handle);
}


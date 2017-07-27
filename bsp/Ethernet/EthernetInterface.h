

#ifndef _ETHERNET_INTERFACE_H_
#define _ETHERNET_INTERFACE_H_

#include <common_define.h>

#define IS_BROADCAST(p)     ((p)[0] == 0xff && (p)[1] == 0xff && (p)[2] == 0xff && (p)[3] == 0xff && (p)[4] == 0xff && (p)[5] == 0xff)



typedef struct {
    // The functions hardwareReset and init are called at program start.
    // hardwareReset is called once for all instances.
    void (*hardwareReset)(void);

    // init is called once for each instance.
    // init must return false in case of an error. Then the communication will not be started.
    TBOOL (*init)(void);

    // check whether there a link on the ethernet cable, true meas link, false no link
    TBOOL (*link)(void);

    // sends a packet over the ethernet connection
    // ptTXbuffer pointer to a buffer where the complete ethernet frame is stored, starting with the dest-mac-addr of the eth-header but without the CRC at the end
    // i16uLength is number of bytes in the TXBuffer
    // the TXbuffer can be reused directly after the call of packetSend(). If packetSend works asynchronously is must make a copy of the data inside.
    TBOOL (*packetSend)(INT8U *ptTXbuffer, INT16U i16uLength);

    // paketRead checks whether there is a complete ethernet packet in the receive queue.
    // ptRXbuffer must pointer to buffer where the packet is copied to.
    // pi16uLength_p must pointer to variable. This variable must be set to the size of the ptRXbuffer before packetRead each time before is called.
    // if no packet is available, packetRead returns false and *pi16uLength_p is not changed
    // if the buffer is too small, packetRead returns false and *pi16uLength_p is set to the length of the received packet.
    // otherwise the packet is copied to the RXBuffer and *pi16uLength_p is set to the length of the received packet.
    TBOOL (*packetRead)(INT8U *ptRXbuffer, INT16U *pi16uLength_p);

} ETHERNET_INTERFACE;



#endif // _ETHERNET_INTERFACE_H_

//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
// OlliW @ www.olliw.eu
//*******************************************************
// SX Serial Interface
//********************************************************
#ifndef SX_SERIAL_INTERFACE_TX_H
#define SX_SERIAL_INTERFACE_TX_H
#pragma once

#ifdef TTGO_THINGY53_BLE
#include "ttgo_ble_bridge.h"
#include "ttgo_text_mux.h"
#endif


extern bool connected_and_rx_setup_available(void);
extern tSetup Setup;
extern tGlobalConfig Config;
extern tSerialPorts Serials;
extern tTxMavlink mavlink;
extern tTxMsp msp;


class tTxSxSerial : public tSerialBase
{
  public:
    using tSerialBase::Init; // tTxSxSerial redefines Init(), incompatible with tSerialBase's Init()
    void Init(tSerialBase* const _crsfbridge);

    bool available(void) override;
    char getc(void) override;
    void putbuf(uint8_t* const buf, uint16_t len) override;
    void flush(void) override;

  private:
    tSerialBase* ser;
#ifdef TTGO_THINGY53_BLE
    TtgoTextMux text_mux;
#endif
};


void tTxSxSerial::Init(tSerialBase* const _crsfbridge)
{
    tSerialBase::Init();

    switch (Setup.Tx[Config.ConfigId].SerialPort) {
    case TX_SERIAL_PORT_SERIAL:
    case TX_SERIAL_PORT_SERIAL2:
    case TX_SERIAL_PORT_WIRELESS_BRIDGE:
    case TX_SERIAL_PORT_COM:
        ser = Serials.serial; // already sorted out in serialports.Init()
        break;
    case TX_SERIAL_PORT_CRSF_BRIDGE:
        ser = _crsfbridge;
        break;
    default:
        while(1){} // must not happen
    }
    if (!ser) while(1){} // must not happen
#ifdef TTGO_THINGY53_BLE
    text_mux.reset();
    ttgo_ble_init();
#endif
}


bool tTxSxSerial::available(void)
{
    if (!connected_and_rx_setup_available()) return 0;

    if (SERIAL_LINK_MODE_IS_MAVLINK(Setup.Rx.SerialLinkMode)) {
        return mavlink.available(); // get from serial via MAVLink parser
    }
    if (SERIAL_LINK_MODE_IS_MSP(Setup.Rx.SerialLinkMode)) {
        return msp.available(); // get from serial via MSP parser
    }
#ifdef TTGO_THINGY53_BLE
    return text_mux.available(*ser, ttgo_ble_take);
#else
    return ser->available(); // get from serial
#endif
}


char tTxSxSerial::getc(void)
{
    if (!connected_and_rx_setup_available()) return 0; // not needed as done already by available()

    if (SERIAL_LINK_MODE_IS_MAVLINK(Setup.Rx.SerialLinkMode)) {
        return mavlink.getc(); // get from serial via MAVLink parser
    }
    if (SERIAL_LINK_MODE_IS_MSP(Setup.Rx.SerialLinkMode)) {
        return msp.getc(); // get from serial via MSP parser
    }
#ifdef TTGO_THINGY53_BLE
    return text_mux.getc();
#else
    return ser->getc(); // get from serial
#endif
}


void tTxSxSerial::putbuf(uint8_t* const buf, uint16_t len)
{
    if (!connected_and_rx_setup_available()) return;

    if (SERIAL_LINK_MODE_IS_MAVLINK(Setup.Rx.SerialLinkMode)) { // this has to go via the parser
        for (uint16_t i = 0; i < len; i++) mavlink.putc(buf[i]);
        return;
    }
    if (SERIAL_LINK_MODE_IS_MSP(Setup.Rx.SerialLinkMode)) { // this has to go via the parser
        for (uint16_t i = 0; i < len; i++) msp.putc(buf[i]);
        return;
    }
    ser->putbuf(buf, len);
}


void tTxSxSerial::flush(void)
{
    mavlink.flush(); // we don't distinguish here, can't harm to always flush MAVLink handler
    msp.flush(); // we don't distinguish here, can't harm to always flush MSP handler
    ser->flush();
#ifdef TTGO_THINGY53_BLE
    text_mux.reset();
    ttgo_ble_flush();
#endif
}


#endif // SX_SERIAL_INTERFACE_TX_H

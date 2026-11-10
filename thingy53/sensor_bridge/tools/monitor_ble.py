"""30-second diagnostic, not required for autonomous TTGO operation. MIT license."""
import asyncio
from bleak import BleakScanner, BleakClient
TX = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'

async def main():
    print('Suche Thingy53_mLRS (20 Sekunden) ...')
    device = await BleakScanner.find_device_by_name('Thingy53_mLRS', timeout=20)
    if device is None:
        print('Nicht gefunden. Thingy einschalten und andere BLE-Verbindungen trennen.')
        return
    print('Gefunden:', device.address)
    buffer = bytearray()
    def receive(sender, data):
        buffer.extend(data)
        while b'\n' in buffer:
            line, _, tail = buffer.partition(b'\n')
            buffer[:] = tail
            if line.strip():
                print(line.decode('utf-8', errors='replace'), flush=True)
        if len(buffer) > 4096:
            buffer.clear()
            print('Ueberlanger Teiltext verworfen.')
    async with BleakClient(device) as client:
        await client.start_notify(TX, receive)
        print('Verbunden, Messwerte fuer 30 Sekunden:')
        await asyncio.sleep(30)
    print('Test beendet, BLE-Verbindung freigegeben.')

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
    except Exception as exc:
        print('BLE-Fehler:', exc)
        raise SystemExit(1)

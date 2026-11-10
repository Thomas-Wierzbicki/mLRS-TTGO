# Thingy:53 direkt am TTGO-mLRS-Sender

Basis: mLRS-TTGO Commit 3682fad2. Neues PlatformIO-Ziel:
`tx-ttgo-t3-v16-868-thingy53`.

## Voraussetzungen

- Thingy-Firmware aus Thingy53_Sensor_BLE: Name `Thingy53_mLRS`,
  Nordic UART Service, Messzeilen alle 5 Sekunden.
- PC-Test mit monitor_ble.py beenden, ebenso andere BLE-Clients trennen.
- TX und RX haben funktionierende mLRS-Verbindung, Serial Link Mode = transparent.
- Empfaenger nutzt die vorhandene Textanzeige mit 15-Sekunden-Seitenwechsel.
- BLE wird ueber NimBLE-Arduino **1.4.3** eingebunden; Version festgehalten.

## Bauen und auf TX laden

Im PlatformIO-Terminal im mLRS-Repository:

```powershell
pio run -e tx-ttgo-t3-v16-868-thingy53
pio run -e tx-ttgo-t3-v16-868-thingy53 -t upload --upload-port COM9
```

COM9 ist der bisherige TX-Port; gegebenenfalls anpassen. Serial Monitor und
Python-Seriellprogramme vor dem Upload schliessen. RX und Thingy nicht neu flashen.
Das alte Ziel `tx-ttgo-t3-v16-868` bleibt fuer USB-Texteingabe unveraendert.
Im neuen Ziel senden BLE und USB-Freitext gemeinsam ueber die transparente
TX->RX-Datenstrecke. COM9: 115200 Baud, Zeilenende LF oder CRLF (Enter). RX->TX-Daten werden weiterhin zur seriellen Schnittstelle
ausgegeben. Vollstaendige Zeilen wechseln sich ab; keine Vermischung innerhalb
einer Zeile. Bis zu 255 UTF-8-Bytes pro USB-Zeile; laengere Zeilen werden komplett
verworfen. Vier abgeschlossene USB-Zeilen werden gepuffert; grosse Datenbursts
koennen den begrenzten UART-Puffer ueberlaufen. Angefangene USB-Zeilen warten auf
LF, ohne Sensorwerte aufzuhalten. Umlaute bleiben als UTF-8 erhalten.
Das vorhandene rx.py/tx.py kann weiterverwendet werden. Auf dem RX ersetzt der
naechste Sensorwert gegebenenfalls den Freitext. Bei Funktrennung werden
wartende und angefangene USB-Zeilen verworfen.
MAVLink/MSP werden nicht mit Sensortext vermischt; fuer Sensorbetrieb transparent
verwenden. Der TX-Bildschirm zeigt weiterhin den bisherigen Funkstatus.

## Betrieb und Abnahme

1. Thingy einschalten, dann beide TTGO mit passenden 868-MHz-Antennen einschalten.
2. Bis zu etwa 30 Sekunden fuer BLE-Verbindung und naechste RX-Textseite warten.
3. Erwartet: `T:22.6C H:38% P:1004hPa` (Beispielwerte).
4. Thingy ausschalten: nach BLE-Timeout sollte `BLE:offline` erscheinen.
5. Thingy einschalten: Messwerte muessen ohne TTGO-Neustart wieder erscheinen.
6. RX aus-/einschalten und erneuten Empfang pruefen.
7. Auch mit USB-Netzteilen/Powerbank ohne PC testen; Funkstatus/Reichweite mit
   bisheriger Firmware vergleichen. Gleichzeitiger BLE-/LoRa-Betrieb ist noch
   am echten Aufbau zu testen.

`BLE:searching` ist eine einmalige Startmeldung. Ist die Funkstrecke dabei noch
nicht verbunden, kann sie verworfen werden. Keine BLE-Debugausgaben auf dem
USB-Datenport. Der RX kann bei Ausfall der gesamten Funkstrecke den letzten
Text behalten: Funkstatus beachten; ein alter Messwert ist keine neue Messung.

## Umsetzung

- Separater FreeRTOS-Task auf Core 0 fuer BLE-Scan, Connect und GATT.
- Suche nach exaktem Namen plus NUS-Service, keine fest kodierte BLE-Adresse.
- Subscription auf NUS TX 6e400003-b5a3-f393-e0a9-e50e24dcca9e.
- Maximal 95 Zeichen pro Zeile; Teilpakete zusammensetzen, unvollstaendige oder
  zu lange Zeilen verwerfen. Zeilenabschluss LF, CR wird ignoriert.
- Single-slot Queue: neueste wartende Zeile ersetzt aeltere. Eine bereits
  begonnene Zeile wird vollstaendig gesendet. Wartende Daten aelter als
  15 Sekunden verwerfen; bei LoRa-Trennung puffert mLRS keine Historie.
- Verbindungsabbruch setzt Parser zurueck; erneuter Scan automatisch.
- Nach 20 Sekunden ohne BLE-Notification Verbindung neu aufbauen.
- Namens-/UUID-Filter ist keine Authentifizierung. Passend zur Demo-Firmware
  wird kein Pairing erzwungen.

## Tests

Hosttest (Linux/GCC oder vergleichbarer C++11-Compiler):

```sh
g++ -std=c++11 -Wall -Wextra -Werror tests/thingy53/test_ble_line.cpp -o /tmp/test_ble_line
/tmp/test_ble_line
```

Prueft alle Zweiteilungen einer Messzeile, Leerzeilen, CRLF, ueberlange Zeilen,
ungueltige Zeichen und Parser-Reset bei Verbindungsabbruch.
Parser-Test und kompletter PlatformIO-Build erfolgreich (ESP32 Arduino 2.0.17,
NimBLE 1.4.3). Flash: 744985/1310720 Bytes; statischer RAM:
48068/327680 Bytes. Dynamischer BLE-Heap ist darin nicht vollstaendig erfasst.
Hardwaretest erfolgt auf deinem Aufbau.

## Nach erfolgreichem Hardwaretest sichern

```powershell
git add platformio.ini mLRS/CommonTx/sx_serial_interface_tx.h mLRS/CommonTx/ttgo_ble_bridge.cpp mLRS/CommonTx/ttgo_ble_bridge.h mLRS/CommonTx/ttgo_ble_line.h tests/thingy53/test_ble_line.cpp tests/thingy53/test_text_mux.cpp mLRS/CommonTx/ttgo_text_mux.h thingy53/README_TTGO_BLE.md
git commit -m "Add Thingy53 BLE sensor source for TTGO TX"
git push origin HEAD:main
```

Die Thingy-Anwendungsquellen separat unter `thingy53/sensor_bridge` aufnehmen
(siehe README im Thingy53_Sensor_BLE-Paket), ohne build-Verzeichnis und SDK.

## Zusatztest Freitext + BLE

Bei laufenden Sensorwerten mit tx.py auf COM9 `Hallo vom PC` senden.
Sensorwerte und Text muessen als getrennte Zeilen ankommen. Auch eine langsam
eingegebene Zeile und Umlaute testen. Thingy aus-/einschalten: Freitext soll
weitergehen, Sensoren danach wieder automatisch dazukommen.

Hosttest: `g++ -std=c++11 tests/thingy53/test_text_mux.cpp -o /tmp/test_text_mux`
und `/tmp/test_text_mux`. Getestet: Quellwechsel, unterbrochene Eingabe, UTF-8,
CRLF, 255-/256-Byte-Grenze, Queue-Umlauf und Reset.
Build der Freitext-Version erfolgreich: Flash 745501 Bytes, statischer RAM
49508 Bytes. Hardwaretest der neuen Kombination steht noch aus.

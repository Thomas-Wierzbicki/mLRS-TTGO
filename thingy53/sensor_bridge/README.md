# Thingy:53 → BLE → mLRS (Sensorseite)

Ziel: nRF Connect SDK **3.3.0**, Board **thingy53/nrf5340/cpuapp**, Sysbuild.
Dieses Projekt ersetzt die UART-Beispiellogik durch BME688-Messungen.
Die TTGO-BLE-Client-Erweiterung ist ein nachfolgender Schritt und ist hier noch nicht enthalten.

## Verhalten

- BLE-Name `Thingy53_mLRS`, ein Client gleichzeitig.
- Temperatur, relative Luftfeuchtigkeit und absoluter Luftdruck alle fünf Sekunden.
- Ausgabe z.B. `T:22.4C H:48% P:1013hPa`, mit LF davor und danach.
- NUS Service: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`.
- NUS TX Notifications: `6e400003-b5a3-f393-e0a9-e50e24dcca9e`.
- Client muss Notifications aktivieren. Keine Messwertübertragung ohne Abonnement.
- Bis zu 20 Bytes pro Notification. Der Empfänger muss bis LF zusammensetzen;
  leere Zeilen ignorieren. Kein ATT-MTU-Upgrade erforderlich.
- Advertising wird nach einer Trennung erneut gestartet.
- Kein USB-DTR-Warten, kein angeschlossener PC für den Betrieb nötig.
- Grün: BLE verbunden. Blau: kurzer Messzyklus-Impuls. Rot: Sensorfehler.
- Bei Sensorfehler wird `ERR:BME688:<Fehlercode>` statt alter Messwerte gesendet.
- Diese erste Testversion verlangt keine BLE-Paarung. Ein Client in Reichweite
  kann sich verbinden; keine Zusicherung von Vertraulichkeit.
- BME688 über Zephyr BME680-Treiber; Temperatur kann durch Eigenerwärmung beeinflusst sein.
  Druck ist Stationsdruck, keine Höhen-/Meeresspiegelkorrektur. Kein IAQ/CO2-Wert.

## Installation auf dem bestehenden Entwicklungsprojekt

ZIP nach `C:\Users\thwie\Downloads\Thingy53_Sensor_BLE` entpacken.
Die enthaltenen Dateien aus `thingy53\sensor_bridge` ersetzen die gleichnamigen
Dateien in `C:\ncs\myapps\Thingy53_mLRS`:

```powershell
Copy-Item -Recurse -Force "C:\Users\thwie\Downloads\Thingy53_Sensor_BLE\thingy53\sensor_bridge\*" "C:\ncs\myapps\Thingy53_mLRS\"
```

Es wird nichts automatisch geflasht. Der bestehende Build wird im nächsten Schritt
mit `-p always` neu erzeugt, damit alte Konfigurationen nicht weiterverwendet werden.

## Bauen und Flashen

Im nRF-Connect-Terminal mit SDK/Toolchain 3.3.0:

```powershell
cd C:\ncs\v3.3.0
west build --sysbuild -p always -b thingy53/nrf5340/cpuapp -d C:/ncs/myapps/Thingy53_mLRS/build C:/ncs/myapps/Thingy53_mLRS
```

Nur nach erfolgreichem Build: J-Link Commander schließen, DK J2 an USB,
DK P19 mit Thingy P8 verbinden, beide eingeschaltet. Flash ersetzt die Firmware:

```powershell
west flash -d C:/ncs/myapps/Thingy53_mLRS/build --runner jlink --dev-id 1050098627
```

## BLE-Test vor der TTGO-Integration

Im Python-Terminal mit Bleak (gegebenenfalls `python -m pip install bleak`):

```powershell
python C:\ncs\myapps\Thingy53_mLRS\tools\monitor_ble.py
```

Das Testprogramm verbindet für 30 Sekunden und zeigt zusammengesetzte Textzeilen.
Es kann die neue BLE-Adresse selbst finden. Smartphone-BLE-Verbindungen vorher trennen.
Während dieses Tests kann sich wegen des Ein-Client-Limits kein TTGO verbinden.

## Aufnahme ins mLRS-TTGO-Repository

Repository-Pfad: `C:\Users\thwie\Documents\Wichtige Daten\PlatformIO\Projects\mLRS`.
Nach erfolgreichem Hardwaretest in PowerShell:

```powershell
New-Item -ItemType Directory -Force "C:\Users\thwie\Documents\Wichtige Daten\PlatformIO\Projects\mLRS\thingy53\sensor_bridge"
robocopy "C:\ncs\myapps\Thingy53_mLRS" "C:\Users\thwie\Documents\Wichtige Daten\PlatformIO\Projects\mLRS\thingy53\sensor_bridge" /E /XD build .git __pycache__
```

Robocopy-Codes 0–7 sind keine Kopierfehler. Build-Verzeichnisse anderer Namen ebenfalls
nicht übernehmen. Das enthaltene `.gitignore` schließt `build*` aus.
SDK und Toolchain verbleiben unter C:\ncs und werden nicht eingecheckt.
Quellen mit `git status --short` prüfen und nach dem Test gezielt committen.

## Prüfung und Quellen

- Native C-Tests der Textformatierung: positive/negative Temperatur, -0.x,
  Rundung, kPa→hPa und Puffergrenze erfolgreich.
- SDK-3.3.0-NUS-API, Board-Devicetree und Sensortreiber-Konfiguration geprüft.
- Kein vollständiger Firmware-Build und kein Hardwaretest in der Erstellungsumgebung.
  Diese Prüfungen erfolgen auf dem Windows-PC und dem Thingy.
- https://github.com/nrfconnect/sdk-nrf/tree/v3.3.0/samples/bluetooth/peripheral_uart
- https://github.com/nrfconnect/sdk-nrf/blob/v3.3.0/include/bluetooth/services/nus.h
- https://github.com/nrfconnect/sdk-zephyr/blob/ncs-v3.3.0/boards/nordic/thingy53/thingy53_nrf5340_common.dtsi

CMake, Kconfig und Board-Konfigurationen stammen aus Nordics UART-Beispiel;
Lizenz siehe `LICENSE-Nordic.txt`. Eigene C-Quelldateien und Testprogramm: MIT,
siehe `LICENSE-MIT.txt`. Nordic-Bestandteile sind für den Nordic-Chip bestimmt.

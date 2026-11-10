mLRS – Python-Textterminals für TTGO T3 V1.6

1. ZIP nach C:\Users\thwie\Downloads entpacken.
   Die Dateien sollen in C:\Users\thwie\Downloads\mLRS_Python liegen.
2. PlatformIO Serial Monitor und andere Programme schließen, die COM8/COM9 nutzen.
3. Einmal in PowerShell ausführen:
   python -m pip install pyserial
4. Sender in PowerShell starten:
   python "C:\Users\thwie\Downloads\mLRS_Python\tx.py"
5. Empfänger in einem ZWEITEN PowerShell-Fenster starten:
   python "C:\Users\thwie\Downloads\mLRS_Python\rx.py"

TX: COM9 / RX: COM8 / beide 115200 Baud.
Abweichender Port: python tx.py --port COM12
Abweichende Baudrate: python rx.py --port COM8 --baud 115200
Python 3 mit Tkinter erforderlich (im üblichen Windows-Python enthalten).

Bedienung:
Text eingeben und Enter drücken oder Senden anklicken.
Beide Fenster senden und empfangen. Jede Nachricht endet mit einem Zeilenumbruch.
Beispiel: Im TX-Fenster "Hallo vom Sender" senden. Im RX-Fenster erscheint RX:
Hallo vom Sender. Dort mit "Empfangen!" antworten; im TX-Fenster erscheint die Antwort.
Die vorhandene Empfänger-Firmware zeigt eingehende Texte auf dem OLED an.
Für einen mobilen RX reicht dessen Stromversorgung; der PC am RX wird nur für
das RX-Pythonfenster benötigt. Die Skripte laufen auf dem PC, nicht auf dem TTGO.

USB offen bedeutet nur, dass der PC den seriellen Port geöffnet hat.
TX im Protokoll bestätigt die Übergabe an USB, nicht den Empfang per Funk.
Eine automatische Funk-Empfangsbestätigung wird nicht hinzugefügt.
Beide Geräte müssen mit kompatibler mLRS-Firmware konfiguriert sein.
Die Skripte ändern keine Firmware- oder Funkparameter.

Nach USB-Unterbrechung neuer Verbindungsversuch alle 2 Sekunden am gleichen Port.
Bei geändertem COM-Port das Skript mit dem neuen Port starten.
Alte Sendeaufträge werden verworfen, nicht erneut übertragen.
DTR/RTS werden vor Öffnen deaktiviert; einzelne USB-Treiber können dennoch
beim Öffnen einen Reset auslösen.
UTF-8; maximal 512 Bytes pro Nachricht. Die OLED-Darstellung richtet sich nach
der Firmware und kann kürzen/umschreiben. Ohne Zeilenumbruch werden empfangene
Teiltexte nach 0,8 Sekunden Pause angezeigt. Das Bildschirmprotokoll ist begrenzt
und wird nicht als Datei gespeichert.

Technik: pyserial https://pyserial.readthedocs.io/en/latest/pyserial_api.html
Softwareprüfung mit simuliertem seriellen Anschluss; kein Hardwaretest hier.

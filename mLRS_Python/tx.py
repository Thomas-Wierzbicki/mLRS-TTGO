"""mLRS Textterminal; Python 3 + pyserial. Start: python tx.py --port COM9"""
import argparse
import codecs
import queue
import threading
import time
import tkinter as tk
from tkinter import ttk, scrolledtext

try:
    import serial
except ImportError:
    raise SystemExit('Bitte zuerst ausführen: python -m pip install pyserial')

ROLE = 'TX'
DEFAULT_PORT = 'COM9'


class SerialWorker(threading.Thread):
    def __init__(self, port, baud, events, commands, stop, factory=None):
        super().__init__(daemon=True)
        self.port, self.baud = port, baud
        self.events, self.commands, self.stop = events, commands, stop
        self.factory = factory or serial.serial_for_url

    def run(self):
        generation = 0
        while not self.stop.is_set():
            connection = None
            decoder = codecs.getincrementaldecoder('utf-8')(errors='replace')
            pending = ''
            last_read = time.monotonic()
            try:
                connection = self.factory(self.port, baudrate=self.baud,
                                          timeout=0.1, write_timeout=1,
                                          do_not_open=True)
                connection.dtr = False
                connection.rts = False
                connection.open()
                generation += 1
                self.events.put(('connected', generation))
                while not self.stop.is_set():
                    try:
                        session, message = self.commands.get_nowait()
                    except queue.Empty:
                        pass
                    else:
                        if session != generation:
                            self.events.put(('info', 'Alter Sendeauftrag verworfen.'))
                        else:
                            data = (message + '\n').encode('utf-8')
                            if connection.write(data) != len(data):
                                raise serial.SerialTimeoutException('Nur teilweise geschrieben')
                            self.events.put(('sent', message))
                    data = connection.read(1024)
                    if data:
                        last_read = time.monotonic()
                        pending += decoder.decode(data)
                        while '\n' in pending:
                            line, pending = pending.split('\n', 1)
                            self.events.put(('received', line.rstrip('\r')))
                        if len(pending) >= 4096:
                            self.events.put(('received', pending))
                            pending = ''
                    elif pending and time.monotonic() - last_read > 0.8:
                        self.events.put(('received', pending.rstrip('\r')))
                        pending = ''
            except (serial.SerialException, OSError, ValueError) as exc:
                self.events.put(('disconnected', str(exc)))
            finally:
                if pending:
                    self.events.put(('received', pending))
                if connection is not None:
                    try:
                        connection.close()
                    except (serial.SerialException, OSError):
                        pass
                while True:
                    try:
                        self.commands.get_nowait()
                        self.events.put(('info', 'Nicht gesendeter Text verworfen.'))
                    except queue.Empty:
                        break
            self.stop.wait(2)


class Terminal:
    def __init__(self, root, port, baud):
        self.root, self.session = root, None
        self.events, self.commands = queue.Queue(), queue.Queue(maxsize=20)
        self.stop = threading.Event()
        root.title(f'mLRS {ROLE} – {port}')
        root.geometry('780x520')
        root.minsize(560, 350)
        frame = ttk.Frame(root, padding=12)
        frame.pack(fill='both', expand=True)
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(2, weight=1)
        self.status = tk.StringVar(value=f'Öffne {port} …')
        ttk.Label(frame, textvariable=self.status).grid(row=0, column=0, sticky='w')
        ttk.Label(frame, text='USB offen bestätigt keine Funkverbindung. TX = an USB übergeben, RX = empfangen.',
                  wraplength=500).grid(row=1, column=0, sticky='w', pady=(4, 8))
        self.log = scrolledtext.ScrolledText(frame, wrap='word', state='disabled', font=('Consolas', 11), width=40, height=8)
        self.log.grid(row=2, column=0, sticky='nsew')
        row = ttk.Frame(frame)
        row.grid(row=3, column=0, sticky='ew', pady=(10, 0))
        ttk.Label(row, text='Nachricht:').pack(side='left', padx=(0, 8))
        self.entry = ttk.Entry(row)
        self.entry.pack(side='left', fill='x', expand=True)
        self.entry.bind('<Return>', self.send)
        self.button = ttk.Button(row, text='Senden', command=self.send, state='disabled')
        self.button.pack(side='left', padx=(8, 0))
        ttk.Label(frame, text='Enter sendet eine Zeile (max. 512 UTF-8-Bytes).', wraplength=500).grid(row=4, column=0, sticky='w', pady=(6, 0))
        self.worker = SerialWorker(port, baud, self.events, self.commands, self.stop)
        self.worker.start()
        self.port = port
        self.root.protocol('WM_DELETE_WINDOW', self.close)
        self.root.after(100, self.poll)
        self.entry.focus_set()

    def append(self, label, text):
        self.log.configure(state='normal')
        self.log.insert('end', f'{time.strftime("%H:%M:%S")} {label}: {text}\n')
        if int(self.log.index('end-1c').split('.')[0]) > 2000:
            self.log.delete('1.0', '501.0')
        self.log.see('end')
        self.log.configure(state='disabled')

    def send(self, event=None):
        message = self.entry.get()
        if not message:
            return
        if self.session is None:
            self.append('INFO', 'USB nicht verbunden – Text bleibt im Eingabefeld.')
            return
        if len(message.encode('utf-8')) > 512:
            self.append('INFO', 'Text zu lang: maximal 512 UTF-8-Bytes.')
            return
        try:
            self.commands.put_nowait((self.session, message))
        except queue.Full:
            self.append('INFO', 'Sendewarteschlange voll. Bitte kurz warten.')
            return
        self.entry.delete(0, 'end')

    def poll(self):
        for _ in range(200):
            try:
                kind, value = self.events.get_nowait()
            except queue.Empty:
                break
            if kind == 'connected':
                self.session = value
                self.status.set(f'USB offen: {self.port}')
                self.button.configure(state='normal')
                self.append('INFO', 'USB verbunden.')
            elif kind == 'disconnected':
                self.session = None
                self.status.set(f'USB getrennt: {self.port} – neuer Versuch alle 2 Sekunden')
                self.button.configure(state='disabled')
                if getattr(self, 'last_error', None) != value:
                    self.append('INFO', value)
                    self.last_error = value
            else:
                self.append({'sent': 'TX', 'received': 'RX', 'info': 'INFO'}[kind], value)
        if not self.stop.is_set():
            self.root.after(100, self.poll)

    def close(self):
        self.stop.set()
        self.root.withdraw()
        self.finish_close()

    def finish_close(self):
        if self.worker.is_alive():
            self.root.after(100, self.finish_close)
        else:
            self.root.destroy()


def main():
    parser = argparse.ArgumentParser(description=f'mLRS {ROLE} Textterminal')
    parser.add_argument('--port', default=DEFAULT_PORT, help=f'Serieller Port (Standard {DEFAULT_PORT})')
    parser.add_argument('--baud', type=int, default=115200, help='Baudrate (Standard 115200)')
    args = parser.parse_args()
    root = tk.Tk()
    Terminal(root, args.port, args.baud)
    root.mainloop()


if __name__ == '__main__':
    main()

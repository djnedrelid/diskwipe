#!/usr/bin/env python3

from http.server import BaseHTTPRequestHandler, HTTPServer
from socketserver import ThreadingMixIn
import time, webbrowser, threading, os, fcntl
from pathlib import Path
from datetime import datetime

hostName = "localhost"
serverPort = 8080

class MyServer(BaseHTTPRequestHandler):
    def do_GET(self):
        #
        #   GET håndtering for CSS.
        #
        if self.path == "/css.css":
            self.send_response(200)
            self.send_header("Content-type", "text/css; charset=utf-8")
            self.end_headers()
            htmltemplate = open(Path(__file__).parent / "./webtemplates/css.css", "r")
            self.wfile.write(bytes(htmltemplate.read(), "utf-8"))

        #
        #   GET håndtering for javascript.
        #
        elif self.path == "/js.js":
            self.send_response(200)
            self.send_header("Content-type", "text/javascript; charset=utf-8")
            self.end_headers()
            htmltemplate = open(Path(__file__).parent / "./webtemplates/js.js", "r")
            self.wfile.write(bytes(htmltemplate.read(), "utf-8"))

        #
        #   GET håndtering for disker som blir behandlet.
        #   Henter rapportfil og sender den via AJAX kall.
        #
        elif self.path == "/blirbehandlet":
            self.send_response(200)
            self.send_header("Content-type", "text/plain; charset=utf-8")
            self.end_headers()
            htmltemplate = open(Path(__file__).parent / "./cpphelpers/blirbehandlet/blirbehandlet.txt", "r")
            
            #
            # Lås og les fil. LOCK_EX venter evt. på F_WRLCK som skriver filen.
            # Løsningen bruker fcntl låsing på begge sider.
            #
            fcntl.flock(htmltemplate, fcntl.LOCK_EX)
            tekst = htmltemplate.read()
            fcntl.flock(htmltemplate, fcntl.LOCK_UN)

            tekst = tekst.replace("\n","<br>")
            self.wfile.write(bytes(tekst, "utf-8"))
        
        #
        #   GET håndtering for ferdig behandlede disker.
        #   Henter rapportfiler og slår dem sammen, separert av ----NESTEDISK----.
        #
        elif self.path == "/ferdigbehandlet":
            self.send_response(200)
            self.send_header("Content-type", "text/plain; charset=utf-8")
            self.end_headers()
            filer = []
            for path in os.scandir(Path(__file__).parent / "./cpphelpers/ferdigbehandlet"):
                if path.is_file():
                    filer.append(Path(__file__).parent / "./cpphelpers/ferdigbehandlet" / path.name)

            if len(filer) == 0:
                self.wfile.write(bytes("Ingen resultater funnet.", "utf-8"))
                return

            filer.sort(key=os.path.getmtime, reverse=True)
            
            for ferdigdisk in filer:
                htmltemplate = open(ferdigdisk, "r")

                #
                # Lås og les fil. LOCK_EX venter evt. på F_WRLCK som skriver filen.
                # Løsningen bruker fcntl låsing på begge sider.
                #
                fcntl.flock(htmltemplate, fcntl.LOCK_EX)
                tekst = htmltemplate.read()
                fcntl.flock(htmltemplate, fcntl.LOCK_UN)

                fildato = datetime.fromtimestamp(os.path.getmtime(ferdigdisk)).strftime("%d-%m-%Y %H:%M:%S")

                tekst = tekst.replace("\n", "<br>")
                tekst += "----NESTEDISK----"
                tekst = "<span class=\"fildato\">"+ fildato +"</span><br>"+ tekst
                self.wfile.write(bytes(tekst, "utf-8"))
        
        #
        #   GET håndtering for bildekall.
        #
        elif "/gfx=" in self.path:
            self.send_response(200)
            self.send_header("Content-type", "image/x-png")
            self.end_headers()
            deler = self.path.split("=")
            with open(Path(__file__).parent / "./webtemplates/gfx" / deler[1], "rb") as image:
                f = image.read()
                b = bytearray(f)
                self.wfile.write(b)

        #
        #   GET håndtering for favicon.
        #
        elif self.path == "/favicon.ico":
            self.send_response(200)
            self.send_header("Content-type", "image/x-icon")
            self.end_headers()
            with open(Path(__file__).parent / "./webtemplates/gfx" / self.path.replace("/",""), "rb") as image:
                f = image.read()
                b = bytearray(f)
                self.wfile.write(b)

        #
        #   GET håndtering for direktekall.
        #
        elif self.path == "/":
            self.send_response(200)
            self.send_header("Content-type", "text/html; charset=utf-8")
            self.end_headers()
            htmltemplate = open(Path(__file__).parent / "./webtemplates/index.html", "r")
            self.wfile.write(bytes(htmltemplate.read(), "utf-8"))


class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    # Kjører MyServer i egen tråd.
    pass

if __name__ == "__main__":        
    webServer = ThreadedHTTPServer((hostName, serverPort), MyServer)
    print("Starter webløsning for sletting av disker.")

    try:
        # Åpne webserver i egen tråd, slik at serve_forever() ikke låser denne.
        wserve = threading.Thread(target=webServer.serve_forever, args=())
        wserve.start()
        
        # Åpne nettleser etter at server er startet.
        webbrowser.open('http://localhost:8080/')
        
        # Vent på/join webserver.
        wserve.join()

    except KeyboardInterrupt:
        pass

    webServer.server_close()
    print("Avslutter.")

"""Virtuální tyč v prohlížeči — hýbeš s ní myší, chová se jako tyc.ino.

Spuštění (server.py už musí běžet), pro dvě tyče:
  python virtualni_tyc.py 8001     -> http://localhost:8001
  python virtualni_tyc.py 8002     -> http://localhost:8002

Každý port je jedna tyč (ID tyc-web-8001 atd.).
"""

import os
import sys
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

WEB = Path(__file__).parent / "web"


class Server(ThreadingHTTPServer):
    # na Windows by jinak šel spustit druhý server na stejném portu a ten by tiše "kradl" požadavky
    allow_reuse_address = os.name != "nt"


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(WEB), **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")  # ať prohlížeč vždy načte aktuální verzi stránky
        super().end_headers()

    def do_GET(self):
        url = urlparse(self.path)
        if url.path == "/":
            self.path = "/tyc.html" + ("?" + url.query if url.query else "")
        super().do_GET()

    def log_message(self, fmt, *args):
        pass


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8001
    print(f"Virtuální tyč běží na http://localhost:{port}", flush=True)
    Server(("0.0.0.0", port), Handler).serve_forever()

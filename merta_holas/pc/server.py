"""Rehabilitační LED tyč — PC server (kostra).

Spuštění:  python server.py
Web:       http://localhost:8000

Web API (vše JSON):
  POST /api/registrace   {"id"}                 tyč se ohlásí, dostane roli
  GET  /api/prikaz?id=   -> {"prikaz","role"}   tyč se ptá, co má dělat
  POST /api/data         {"id","role","vzorky"} tyč pošle hotový záznam
  GET  /api/stav                                stav pro webovou stránku
  POST /api/start        {"role"}               web: spustí záznam na tyči
  POST /api/stop         {"role"}               web: zastaví záznam
  POST /api/sezeni       {"pacient","cvik"}     web: nastaví sezení
  POST /api/prohodit                            web: prohodí role tyčí
  GET  /api/historie                            všechna uložená porovnání
"""

import json
import os
import threading
import time
from datetime import datetime
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

PORT = 8000
SLOZKA = Path(__file__).parent
WEB = SLOZKA / "web"
SOUBOR_HISTORIE = SLOZKA / "data" / "historie.json"

zamek = threading.Lock()

# id tyče -> {"role", "posledni_kontakt", "stav"}
tyce = {}
# id tyče -> příkaz, který si tyč vyzvedne při dalším dotazu
prikazy = {}
sezeni = {"pacient": "Neznámý pacient", "cvik": "Náklon", "opakovani": 0}
vzor = None            # poslední záznam mentora
posledni_vysledek = None


def nacti_historii():
    if SOUBOR_HISTORIE.exists():
        return json.loads(SOUBOR_HISTORIE.read_text(encoding="utf-8"))
    return []


def uloz_do_historie(zaznam):
    historie = nacti_historii()
    historie.append(zaznam)
    SOUBOR_HISTORIE.parent.mkdir(exist_ok=True)
    SOUBOR_HISTORIE.write_text(json.dumps(historie, ensure_ascii=False, indent=1), encoding="utf-8")


def tyc_s_roli(role):
    for id_tyce, info in tyce.items():
        if info["role"] == role:
            return id_tyce
    return None


def porovnej(vzor_vzorky, pokus_vzorky):
    """Porovnání vzoru a pokusu. Zatím jen rozsah a tempo, zbytek TODO."""
    def rozsah(vzorky):
        uhly = [v["uhel"] for v in vzorky] or [0]
        return {"min": min(uhly), "max": max(uhly), "rozsah": max(uhly) - min(uhly)}

    def doba(vzorky):
        return vzorky[-1]["t"] / 1000 if vzorky else 0

    rv, rp = rozsah(vzor_vzorky), rozsah(pokus_vzorky)
    dv, dp = doba(vzor_vzorky), doba(pokus_vzorky)
    return {
        "rozsah": {
            "vzor": rv,
            "pokus": rp,
            "procent": round(100 * rp["rozsah"] / rv["rozsah"]) if rv["rozsah"] else None,
        },
        "tempo": {
            "doba_vzor_s": dv,
            "doba_pokus_s": dp,
            "pomer": round(dp / dv, 2) if dv else None,
        },
        "tvar": None,          # TODO: např. DTW vzdálenost křivek
        "plynulost": None,     # TODO: např. počet změn směru / jerk
        "opakovatelnost": None,  # TODO: rozptyl mezi opakováními v sezení
    }


def prijmi_data(data):
    global vzor, posledni_vysledek
    role = tyce.get(data["id"], {}).get("role", data.get("role"))
    vzorky = data.get("vzorky", [])

    if role == "mentor":
        vzor = {"vzorky": vzorky, "cas": datetime.now().isoformat(timespec="seconds")}
        sezeni["opakovani"] = 0
        return {"ok": True, "info": "vzor ulozen"}

    if role == "pacient":
        if vzor is None:
            return {"ok": False, "info": "chybi vzor od mentora"}
        sezeni["opakovani"] += 1
        vysledek = porovnej(vzor["vzorky"], vzorky)
        zaznam = {
            "datum": datetime.now().isoformat(timespec="seconds"),
            "pacient": sezeni["pacient"],
            "cvik": sezeni["cvik"],
            "opakovani": sezeni["opakovani"],
            "vzor": vzor["vzorky"],
            "pokus": vzorky,
            "vysledek": vysledek,
        }
        uloz_do_historie(zaznam)
        posledni_vysledek = zaznam
        return {"ok": True, "info": "porovnano a ulozeno"}

    return {"ok": False, "info": "neznama role"}


class Server(ThreadingHTTPServer):
    # na Windows by jinak šel spustit druhý server na stejném portu a ten by tiše "kradl" požadavky
    allow_reuse_address = os.name != "nt"


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(WEB), **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")  # ať prohlížeč vždy načte aktuální verzi stránky
        super().end_headers()

    def log_message(self, fmt, *args):
        # dotazy na příkaz chodí 2x za sekundu, nespamovat konzoli
        if "/api/prikaz" not in self.path and "/api/stav" not in self.path:
            super().log_message(fmt, *args)

    def posli_json(self, data, kod=200):
        telo = json.dumps(data, ensure_ascii=False).encode("utf-8")
        self.send_response(kod)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(telo)))
        self.send_header("Access-Control-Allow-Origin", "*")  # virtuální tyč běží na jiném portu
        self.end_headers()
        self.wfile.write(telo)

    def do_OPTIONS(self):
        # prohlížeč se před POSTem s JSONem z jiného portu nejdřív zeptá, jestli smí
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def nacti_telo(self):
        delka = int(self.headers.get("Content-Length", 0))
        if delka == 0:
            return {}
        return json.loads(self.rfile.read(delka).decode("utf-8"))

    def do_GET(self):
        url = urlparse(self.path)
        with zamek:
            if url.path == "/api/prikaz":
                id_tyce = parse_qs(url.query).get("id", [""])[0]
                if id_tyce not in tyce:
                    return self.posli_json({"prikaz": "nic", "role": "?"})
                tyce[id_tyce]["posledni_kontakt"] = time.time()
                prikaz = prikazy.pop(id_tyce, "nic")
                return self.posli_json({"prikaz": prikaz, "role": tyce[id_tyce]["role"]})

            if url.path == "/api/stav":
                ted = time.time()
                return self.posli_json({
                    "tyce": [
                        {"id": i, "role": t["role"], "stav": t["stav"],
                         "online": ted - t["posledni_kontakt"] < 3}
                        for i, t in tyce.items()
                    ],
                    "sezeni": sezeni,
                    "vzor": vzor,
                    "posledni_vysledek": posledni_vysledek,
                })

            if url.path == "/api/historie":
                return self.posli_json(nacti_historii())

        super().do_GET()  # statické soubory z web/

    def do_POST(self):
        url = urlparse(self.path)
        data = self.nacti_telo()
        with zamek:
            if url.path == "/api/registrace":
                id_tyce = data["id"]
                if id_tyce not in tyce:
                    role = "mentor" if tyc_s_roli("mentor") is None else "pacient"
                    tyce[id_tyce] = {"role": role, "stav": "ceka"}
                    print(f"Registrována tyč {id_tyce} jako {role}", flush=True)
                tyce[id_tyce]["posledni_kontakt"] = time.time()
                return self.posli_json({"role": tyce[id_tyce]["role"]})

            if url.path == "/api/data":
                if data.get("id") in tyce:
                    tyce[data["id"]]["stav"] = "ceka"
                vysledek = prijmi_data(data)
                print(f"Data od {data.get('id')}: {len(data.get('vzorky', []))} vzorků -> {vysledek['info']}", flush=True)
                return self.posli_json(vysledek)

            if url.path in ("/api/start", "/api/stop"):
                id_tyce = tyc_s_roli(data.get("role"))
                if id_tyce is None:
                    return self.posli_json({"ok": False, "info": "tyč s touto rolí není připojená"}, 404)
                prikaz = url.path.rsplit("/", 1)[1]
                prikazy[id_tyce] = prikaz
                tyce[id_tyce]["stav"] = "zaznam" if prikaz == "start" else "odesila"
                return self.posli_json({"ok": True})

            if url.path == "/api/sezeni":
                sezeni["pacient"] = data.get("pacient", sezeni["pacient"])
                sezeni["cvik"] = data.get("cvik", sezeni["cvik"])
                sezeni["opakovani"] = 0
                return self.posli_json({"ok": True})

            if url.path == "/api/prohodit":
                for t in tyce.values():
                    t["role"] = "pacient" if t["role"] == "mentor" else "mentor"
                return self.posli_json({"ok": True})

        self.posli_json({"ok": False, "info": "neznámá adresa"}, 404)


if __name__ == "__main__":
    try:
        server = Server(("0.0.0.0", PORT), Handler)
    except OSError:
        raise SystemExit(f"Port {PORT} už používá jiný program — nejspíš už běží jiný server.py.")
    print(f"Server běží na http://localhost:{PORT}", flush=True)
    server.serve_forever()

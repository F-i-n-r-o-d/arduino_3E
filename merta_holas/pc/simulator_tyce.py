"""Simulátor tyče — pro zkoušení serveru a webu bez hardwaru.

Spuštění (server už musí běžet), pro dvě tyče ve dvou terminálech:
  python simulator_tyce.py tyc-sim1
  python simulator_tyce.py tyc-sim2

Dělá totéž co tyc.ino: zaregistruje se, ptá se na příkaz a při
záznamu generuje náklon jako sinusovku (mentor 40°, pacient menší).
"""

import json
import math
import random
import sys
import time
import urllib.request

SERVER = "http://127.0.0.1:8000"


def pozadavek(cesta, data=None):
    telo = json.dumps(data).encode() if data is not None else None
    req = urllib.request.Request(SERVER + cesta, data=telo, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=3) as r:
        return json.loads(r.read())


def main():
    id_tyce = sys.argv[1] if len(sys.argv) > 1 else "tyc-sim"
    role = pozadavek("/api/registrace", {"id": id_tyce})["role"]
    print(f"{id_tyce}: role {role}, čekám na start...")

    zaznam, start = False, 0
    vzorky = []
    while True:
        odpoved = pozadavek(f"/api/prikaz?id={id_tyce}")
        role = odpoved["role"]
        if odpoved["prikaz"] == "start":
            zaznam, start, vzorky = True, time.time(), []
            print("START")
        elif odpoved["prikaz"] == "stop" and zaznam:
            zaznam = False
            print(f"STOP, posílám {len(vzorky)} vzorků")
            print(pozadavek("/api/data", {"id": id_tyce, "role": role, "vzorky": vzorky}))

        # 10 vzorků po 50 ms mezi dotazy na příkaz, stejně jako tyč
        for _ in range(10):
            if zaznam:
                t = time.time() - start
                amplituda = 40 if role == "mentor" else 28
                uhel = amplituda * math.sin(t * 2 * math.pi / 4) + random.uniform(-1, 1)
                vzorky.append({"t": int(t * 1000), "uhel": round(uhel, 1)})
            time.sleep(0.05)


if __name__ == "__main__":
    main()

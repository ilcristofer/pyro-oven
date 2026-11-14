#!/usr/bin/env python3
"""Logger telemetria PYRO: campiona /api/status e scrive un CSV."""
import csv, json, time, sys, urllib.request, os

URL = "http://%s/api/status" % (sys.argv[1] if len(sys.argv) > 1 else "pyro.local")
INTERVAL = 2.0
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "sessione_%s.csv" % time.strftime("%Y%m%d_%H%M%S"))

FIELDS = ["t", "iso", "mode", "on", "tc", "tp", "ta", "sc", "sp",
          "epc", "epp", "rc", "rp", "cm", "pm", "bc", "bp", "rpm", "tact", "tm", "ts",
          "stone", "svc", "rec", "eta", "soak", "defc", "defe", "pred", "clb",
          "cool", "ctc", "ctp", "camb"]

def main():
    t0 = time.time()
    with open(OUT, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS, extrasaction="ignore")
        w.writeheader()
        errors = 0
        while True:
            try:
                with urllib.request.urlopen(URL, timeout=4) as r:
                    d = json.loads(r.read().decode())
                d["t"] = round(time.time() - t0, 1)
                d["iso"] = time.strftime("%H:%M:%S")
                w.writerow(d)
                f.flush()
                errors = 0
            except Exception as e:
                errors += 1
                if errors % 12 == 1:
                    print("errore (%d): %s" % (errors, e), file=sys.stderr, flush=True)
            time.sleep(INTERVAL)

main()

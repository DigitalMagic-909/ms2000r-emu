#!/bin/bash
# MS2000R emulator - speed test for older Intel Macs (OS X 10.9 Mavericks and newer).
# Double-click this file (the first time: right-click > Open). flash.bin and boot-362.bin
# must be in the same folder as this file.

cd "$(dirname "$0")"
clear
echo "=============================================="
echo "  MS2000R emulator - snelheidstest"
echo "=============================================="
echo

if [ ! -f flash.bin ] || { [ ! -f boot-362.bin ] && [ ! -f "full FW/boot-362.bin" ] && [ ! -f boot-362.ms2000.bin ] && [ ! -f "full FW/boot-362.ms2000.bin" ]; }; then
    echo "flash.bin en/of boot-362.bin ontbreken."
    echo "Zet ze in deze map:"
    echo "  $(pwd)"
    echo
    read -n 1 -s -r -p "Druk op een toets om te sluiten."
    exit 1
fi

xattr -dr com.apple.quarantine ./ms2000_emulator 2>/dev/null
chmod +x ./ms2000_emulator

echo "Processor: $(sysctl -n machdep.cpu.brand_string)"
echo "macOS:     $(sw_vers -productVersion)"
echo
echo "De emulator start twee keer op en speelt een akkoord (2x 20 seconden geluid)."
echo "Dit duurt een paar minuten - even geduld..."
echo

# Four-note chord from 4 s to 18 s (the firmware boots in about 3 s, muted)
export MS2K_MIDIIN="4.0:903C64;4.0:904064;4.0:904364;4.0:904864;18.0:803C00;18.0:804000;18.0:804300;18.0:804800"

# The real-time factor while the chord plays: from the DSP's once-a-second status lines
# ("[DSP56362] t=<emulated s> s: ... wall=<real s> s"), the first one at t >= 4 s to the last one.
steady() {
    awk '/\[DSP56362\] t=[0-9.]+ s:.*wall=[0-9.]+ s/ {
            match($0, /t=[0-9.]+/);    t = substr($0, RSTART + 2, RLENGTH - 2) + 0
            match($0, /wall=[0-9.]+/); w = substr($0, RSTART + 5, RLENGTH - 5) + 0
            if (t >= 4 && !have) { t1 = t; w1 = w; have = 1 }
            if (have) { t2 = t; w2 = w } }
         END { if (have && w2 > w1) printf "%.2f", (t2 - t1) / (w2 - w1); else print "0" }' "$1"
}

run_test() {   # $1 = 0/1 (DSP on its own thread), $2 = log file
    MS2K_DSPTHREAD=$1 ./ms2000_emulator --headless --quiet-boot --duration 20 --render-test test-geluid.wav > "$2" 2>&1
    local status=$?
    if [ $status -ne 0 ] || ! grep -q "MCU t=" "$2"; then
        echo "  De test is niet gelukt. Stuur het bestand $2 naar Claude."
        tail -15 "$2"
        return 1
    fi
    echo "  Snelheid tijdens het spelen: $(steady "$2")x realtime"
}

echo "Test 1 van 2: alles op een processorkern..."
run_test 0 resultaat.txt
echo
echo "Test 2 van 2: de DSP-chip op een eigen processorkern..."
run_test 1 resultaat-dspthread.txt
echo

BEST=$(printf "%s\n%s\n" "$(steady resultaat.txt)" "$(steady resultaat-dspthread.txt)" | sort -rn | head -1)
echo "----------------------------------------------"
echo "  Beste uitkomst: ${BEST}x realtime"
echo "----------------------------------------------"
echo
awk -v r="$BEST" 'BEGIN {
    if (r >= 1.3)      print "GOED: je Mac is snel genoeg. Een plugin-versie heeft kans van slagen.";
    else if (r >= 1.0) print "KRAP: net snel genoeg. In Cubase met andere plugins erbij gaat het waarschijnlijk haperen.";
    else               print "TE TRAAG: je Mac kan de emulator niet bijhouden. Het geluid zou haperen.";
}'
echo
echo "(De volledige uitvoer staat in resultaat.txt en resultaat-dspthread.txt)"
echo
read -n 1 -s -r -p "Druk op een toets om te sluiten."

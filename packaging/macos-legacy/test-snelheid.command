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
echo "De emulator start op en speelt een akkoord (20 seconden geluid)."
echo "Op een trage Mac kan dit een paar minuten duren - even geduld..."
echo

# Four-note chord from 4 s to 18 s (the firmware boots in about 3 s, muted)
export MS2K_MIDIIN="4.0:903C64;4.0:904064;4.0:904364;4.0:904864;18.0:803C00;18.0:804000;18.0:804300;18.0:804800"
./ms2000_emulator --headless --quiet-boot --duration 20 --render-test test-geluid.wav > resultaat.txt 2>&1
STATUS=$?

LINE=$(grep "MCU t=" resultaat.txt | tail -1)
if [ $STATUS -ne 0 ] || [ -z "$LINE" ]; then
    echo "De test is niet gelukt. Stuur het bestand resultaat.txt naar Claude."
    echo
    tail -20 resultaat.txt
    echo
    read -n 1 -s -r -p "Druk op een toets om te sluiten."
    exit 1
fi

EMU=$(echo "$LINE" | sed -E 's/.*MCU t=([0-9.]+) s.*/\1/')
WALL=$(echo "$LINE" | sed -E 's/.*wall ([0-9.]+) s.*/\1/')
RATIO=$(awk -v e="$EMU" -v w="$WALL" 'BEGIN { if (w > 0) printf "%.2f", e / w; else print "0" }')

echo "----------------------------------------------"
echo "  $EMU seconden MS2000R gerekend in $WALL seconden"
echo "  Snelheid: ${RATIO}x realtime"
echo "----------------------------------------------"
echo
awk -v r="$RATIO" 'BEGIN {
    if (r >= 1.3)      print "GOED: je Mac is snel genoeg. Een plugin-versie heeft kans van slagen.";
    else if (r >= 1.0) print "KRAP: net snel genoeg. In Cubase met andere plugins erbij gaat het waarschijnlijk haperen.";
    else               print "TE TRAAG: je Mac kan de emulator niet bijhouden. Het geluid zou haperen.";
}'
echo
echo "(Volledige uitvoer staat in resultaat.txt, het testgeluid in test-geluid.wav)"
echo
read -n 1 -s -r -p "Druk op een toets om te sluiten."

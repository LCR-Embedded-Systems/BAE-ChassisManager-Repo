#!/bin/bash
phytool write end1/0/22 18;phytool write end1/0/20 0x8002;phytool write end1/0/22 1;phytool write end1/0/0 0x0140 # configure base x with no autoneg

MODE="${1:-base}"

if [ "$MODE" = "f" ] || [ "$MODE" = "all" ]; then
    phytool write end1/0/22 1;phytool write end1/0/16 0x0401 # force good
fi

if [ "$MODE" = "l" ] || [ "$MODE" = "all" ]; then
    phytool write end1/0/22 1;phytool write end1/0/0 0x4140 # loopback
fi

echo "Configuration applied (mode: ${MODE})."
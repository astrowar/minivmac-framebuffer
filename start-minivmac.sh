#!/bin/bash
# Script para iniciar o MinivMac com caminhos absolutos

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DISK_PATH="${SCRIPT_DIR}/System70_boot.dsk"
ROM_PATH="${SCRIPT_DIR}/rom/"

echo "Starting MinivMac..."
echo "Disk: ${DISK_PATH}"
echo "ROM dir: ${ROM_PATH}"

cd "${SCRIPT_DIR}"
./minivmac -d "${SCRIPT_DIR}" "${DISK_PATH}"
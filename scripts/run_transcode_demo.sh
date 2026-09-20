#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${ROOT_DIR}"

POLYXML_BIN="polyxml"
if [ -f "${ROOT_DIR}/../PolyXML/target/release/polyxml" ] && [ -f "${ROOT_DIR}/../PolyXML/target/debug/polyxml" ]; then
    if [ "${ROOT_DIR}/../PolyXML/target/release/polyxml" -nt "${ROOT_DIR}/../PolyXML/target/debug/polyxml" ]; then
        POLYXML_BIN="${ROOT_DIR}/../PolyXML/target/release/polyxml"
    else
        POLYXML_BIN="${ROOT_DIR}/../PolyXML/target/debug/polyxml"
    fi
elif [ -f "${ROOT_DIR}/../PolyXML/target/release/polyxml" ]; then
    POLYXML_BIN="${ROOT_DIR}/../PolyXML/target/release/polyxml"
elif [ -f "${ROOT_DIR}/../PolyXML/target/debug/polyxml" ]; then
    POLYXML_BIN="${ROOT_DIR}/../PolyXML/target/debug/polyxml"
fi

echo "================================================================================"
echo "🛸 PolyXML CLI Streaming & Schema-Directed Transcoder Demo"
echo "================================================================================"

TMP_DIR="$(mktemp -d /tmp/polyxml-transcode-XXXXXX)"
trap 'rm -rf "${TMP_DIR}"' EXIT

echo -e "\n[1] Input: Anduril Lattice SDK Autonomous Entity Telemetry (JSON)"
cat data/lattice_entity.json | head -n 25
echo "..."

echo -e "\n[2] Streaming Pipe: USAF UCI XML -> Canonical JSON via PolyXML CLI:"
cat data/uci_entity.xml | "${POLYXML_BIN}" transcode --to json --pretty > "${TMP_DIR}/uci_pipe.json"
cat "${TMP_DIR}/uci_pipe.json" | head -n 30
echo "..."

echo -e "\n[3] Streaming Pipe: Canonical JSON -> USAF UCI XML via PolyXML CLI:"
cat "${TMP_DIR}/uci_pipe.json" | "${POLYXML_BIN}" transcode --to xml --root EntityMT --pretty > "${TMP_DIR}/uci_pipe.xml"
cat "${TMP_DIR}/uci_pipe.xml" | head -n 30
echo "..."

echo -e "\n[4] Schema-Guided Transcoding: USAF UCI XML -> Strongly-Typed JSON (XSD-Driven):"
"${POLYXML_BIN}" transcode --schema schemas/uci/uci_entity_core.xsd --pretty data/uci_entity.xml -o "${TMP_DIR}/uci_typed.json"
cat "${TMP_DIR}/uci_typed.json" | head -n 30
echo "..."

echo -e "\n[5] Validating Open-Arsenal USAF UCI v2.5 XML Schema (8.3 MB, 5,558 types):"
"${POLYXML_BIN}" validate schemas/uci/UCI_MessageDefinitions_v2_5_0.xsd

echo -e "\n✅ PolyXML CLI Bidirectional Streaming & Schema-Directed Transcoding Complete!"

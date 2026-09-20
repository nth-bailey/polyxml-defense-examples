#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${ROOT_DIR}"

POLYXML_BIN="polyxml"
if [ -f "${ROOT_DIR}/../PolyXML/target/release/polyxml" ]; then
    POLYXML_BIN="${ROOT_DIR}/../PolyXML/target/release/polyxml"
elif [ -f "${ROOT_DIR}/../PolyXML/target/debug/polyxml" ]; then
    POLYXML_BIN="${ROOT_DIR}/../PolyXML/target/debug/polyxml"
fi

echo "================================================================================"
echo "🛸 PolyXML CLI Streaming Transcoder Demo"
echo "================================================================================"

mkdir -p /tmp/polyxml-transcode-demo

echo -e "\n[1] Input: Anduril Lattice SDK Autonomous Entity Telemetry (JSON)"
cat data/lattice_entity.json | head -n 25
echo "..."

echo -e "\n[2] Transcoding USAF UCI XML (data/uci_entity.xml) -> Canonical JSON via PolyXML CLI:"
cat data/uci_entity.xml | "${POLYXML_BIN}" transcode --to json --pretty > /tmp/polyxml-transcode-demo/uci_from_xml.json
cat /tmp/polyxml-transcode-demo/uci_from_xml.json | head -n 30
echo "..."

echo -e "\n[3] Transcoding JSON -> USAF UCI XML via PolyXML CLI:"
cat /tmp/polyxml-transcode-demo/uci_from_xml.json | "${POLYXML_BIN}" transcode --to xml --root EntityMT --pretty > /tmp/polyxml-transcode-demo/uci_roundtrip.xml
cat /tmp/polyxml-transcode-demo/uci_roundtrip.xml | head -n 30
echo "..."

echo -e "\n[4] Validating Open-Arsenal USAF UCI v2.5 XML Schema (8.3 MB, 5,558 types):"
"${POLYXML_BIN}" validate schemas/uci/UCI_MessageDefinitions_v2_5_0.xsd

echo -e "\n✅ PolyXML CLI Bidirectional Streaming Transcoding Demo Complete!"
rm -rf /tmp/polyxml-transcode-demo

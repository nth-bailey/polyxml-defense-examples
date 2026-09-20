#!/usr/bin/env python3
"""Anduril Lattice SDK to USAF UCI v2.5 Telemetry Bridge (Python).

Translates incoming Anduril Lattice entity JSON telemetry into strongly-typed
Open-Arsenal UCI v2.5 XML messages using PolyXML.
"""

from __future__ import annotations

import json
import pathlib
import sys
import time

# Add generated directory to path
repo_root = pathlib.Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(repo_root))

from generated.python.uci_entity_core import (
    ClassificationEnum,
    EntityIdType,
    EntityMdt,
    EntityMt,
    EntityStatusEnum,
    HeaderType,
    KinematicsType,
    ObjectStateEnum,
    SecurityInformationType,
)
import polyxml


def translate_lattice_to_uci(lattice_dict: dict) -> EntityMt:
    """Translate an Anduril Lattice Entity dictionary into a typed UCI EntityMT."""
    loc = lattice_dict.get("location", {})
    kin = lattice_dict.get("kinematics", {})

    return EntityMt(
        security_information=SecurityInformationType(
            classification=ClassificationEnum(lattice_dict.get("classification", "SECRET")),
            owner_producer="USA",
        ),
        message_header=HeaderType(
            message_id=f"MSG-{lattice_dict.get('id', 'UNKNOWN')[:8].upper()}",
            timestamp=lattice_dict.get("timestamp", "2026-09-20T11:00:00Z"),
            originator_id=lattice_dict.get("source_system", "LATTICE_NODE"),
        ),
        object_state=ObjectStateEnum.ACTIVE,
        message_data=EntityMdt(
            entity_id=EntityIdType(
                uuid=lattice_dict["id"],
                callsign=lattice_dict.get("callsign"),
            ),
            creation_timestamp=lattice_dict["timestamp"],
            entity_status=EntityStatusEnum(lattice_dict.get("status", "CONFIRMED")),
            kinematics=KinematicsType(
                latitude=loc.get("latitude", 0.0),
                longitude=loc.get("longitude", 0.0),
                altitude=loc.get("altitude_meters", 0.0),
                heading=kin.get("heading_degrees"),
                ground_speed=kin.get("ground_speed_mps"),
                vertical_speed=kin.get("vertical_speed_mps"),
            ),
            source_system=lattice_dict.get("source_system"),
        ),
    )


def main() -> None:
    data_file = repo_root / "data" / "lattice_entity.json"
    with open(data_file, "r", encoding="utf-8") as f:
        lattice_data = json.load(f)

    print("================================================================================")
    print("🛸 PolyXML: Anduril Lattice SDK ↔ USAF UCI C2 Bridge (Python 3.12+)")
    print("================================================================================")
    print(f"Ingesting Lattice Track: {lattice_data.get('callsign')} (ID: {lattice_data.get('id')})")

    # Measure translation and serialization latency
    start = time.perf_counter()
    uci_entity = translate_lattice_to_uci(lattice_data)
    xml_bytes = uci_entity.to_xml(indent=2)
    elapsed_us = (time.perf_counter() - start) * 1_000_000

    print(f"\n[1] Generated USAF UCI XML Message (latency: {elapsed_us:.2f} μs):")
    xml_str = xml_bytes.decode("utf-8")
    print(xml_str)

    assert "EntityMT" in xml_str or "Entity" in xml_str
    assert lattice_data["id"] in xml_str
    assert str(lattice_data["location"]["latitude"]) in xml_str

    # Direct C/Rust roundtrip transcoding
    print("\n[2] Performing Zero-Copy XML ↔ JSON Roundtrip via polyxml.xml_to_json()...")
    transcoded_json = polyxml.xml_to_json(xml_bytes, indent=2)
    print(transcoded_json.decode("utf-8")[:300] + "\n...")

    print("\n✅ Python Lattice ↔ UCI Bridge executed successfully!")


if __name__ == "__main__":
    main()

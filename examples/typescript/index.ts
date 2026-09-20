import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import {
  ClassificationEnum,
  EntityStatusEnum,
  ObjectStateEnum,
  EntityMtSchema,
  type EntityMt,
} from "../../generated/typescript/uci_entity_core.ts";

interface LatticeEntity {
  id: string;
  callsign?: string;
  timestamp: string;
  source_system?: string;
  classification?: string;
  status?: string;
  location: {
    latitude: number;
    longitude: number;
    altitude_meters: number;
  };
  kinematics: {
    heading_degrees?: number;
    ground_speed_mps?: number;
    vertical_speed_mps?: number;
  };
}

function findDataFile(): string {
  const candidates = [
    "data/lattice_entity.json",
    "../../data/lattice_entity.json",
    "../data/lattice_entity.json",
  ];
  for (const c of candidates) {
    if (fs.existsSync(c)) {
      return path.resolve(c);
    }
  }
  throw new Error("Could not locate data/lattice_entity.json");
}

function translateLatticeToUCI(lattice: LatticeEntity): EntityMt {
  let status: EntityStatusEnum = EntityStatusEnum.Potential;
  switch (lattice.status?.toUpperCase()) {
    case "CONFIRMED":
      status = EntityStatusEnum.Confirmed;
      break;
    case "TENTATIVE":
      status = EntityStatusEnum.Tentative;
      break;
    case "LOST":
      status = EntityStatusEnum.Lost;
      break;
    case "DROPPED":
      status = EntityStatusEnum.Dropped;
      break;
    case "DESTROYED":
      status = EntityStatusEnum.Destroyed;
      break;
  }

  return {
    securityInformation: {
      classification: ClassificationEnum.Secret,
      ownerProducer: "USA",
    },
    messageHeader: {
      messageId: `MSG-${lattice.id.slice(0, 8).toUpperCase()}`,
      timestamp: lattice.timestamp,
      originatorId: lattice.source_system ?? "LATTICE_MESH_NODE_DELTA",
    },
    objectState: ObjectStateEnum.Active,
    messageData: {
      entityId: {
        uuid: lattice.id,
        callsign: lattice.callsign,
      },
      creationTimestamp: lattice.timestamp,
      entityStatus: status,
      kinematics: {
        latitude: lattice.location.latitude,
        longitude: lattice.location.longitude,
        altitude: lattice.location.altitude_meters,
        heading: lattice.kinematics.heading_degrees,
        groundSpeed: lattice.kinematics.ground_speed_mps,
        verticalSpeed: lattice.kinematics.vertical_speed_mps,
      },
      sourceSystem: lattice.source_system,
    },
  };
}

function serializeToXml(entity: EntityMt): string {
  return [
    "<EntityMT>",
    "  <SecurityInformation>",
    `    <Classification>${entity.securityInformation.classification}</Classification>`,
    entity.securityInformation.ownerProducer
      ? `    <OwnerProducer>${entity.securityInformation.ownerProducer}</OwnerProducer>`
      : "",
    "  </SecurityInformation>",
    "  <MessageHeader>",
    `    <MessageID>${entity.messageHeader.messageId}</MessageID>`,
    `    <Timestamp>${entity.messageHeader.timestamp}</Timestamp>`,
    `    <OriginatorID>${entity.messageHeader.originatorId}</OriginatorID>`,
    "  </MessageHeader>",
    entity.objectState
      ? `  <ObjectState>${entity.objectState}</ObjectState>`
      : "",
    "  <MessageData>",
    "    <EntityID>",
    `      <UUID>${entity.messageData.entityId.uuid}</UUID>`,
    entity.messageData.entityId.callsign
      ? `      <Callsign>${entity.messageData.entityId.callsign}</Callsign>`
      : "",
    "    </EntityID>",
    `    <CreationTimestamp>${entity.messageData.creationTimestamp}</CreationTimestamp>`,
    `    <EntityStatus>${entity.messageData.entityStatus}</EntityStatus>`,
    "    <Kinematics>",
    `      <Latitude>${entity.messageData.kinematics.latitude}</Latitude>`,
    `      <Longitude>${entity.messageData.kinematics.longitude}</Longitude>`,
    `      <Altitude>${entity.messageData.kinematics.altitude}</Altitude>`,
    entity.messageData.kinematics.heading !== undefined
      ? `      <Heading>${entity.messageData.kinematics.heading}</Heading>`
      : "",
    entity.messageData.kinematics.groundSpeed !== undefined
      ? `      <GroundSpeed>${entity.messageData.kinematics.groundSpeed}</GroundSpeed>`
      : "",
    entity.messageData.kinematics.verticalSpeed !== undefined
      ? `      <VerticalSpeed>${entity.messageData.kinematics.verticalSpeed}</VerticalSpeed>`
      : "",
    "    </Kinematics>",
    entity.messageData.sourceSystem
      ? `    <SourceSystem>${entity.messageData.sourceSystem}</SourceSystem>`
      : "",
    "  </MessageData>",
    "</EntityMT>",
  ]
    .filter(Boolean)
    .join("\n");
}

function main() {
  console.log("================================================================================");
  console.log("🛸 PolyXML: Anduril Lattice SDK ↔ USAF UCI C2 Bridge (TypeScript 5+ & Zod)");
  console.log("================================================================================");

  const dataPath = findDataFile();
  const rawData = fs.readFileSync(dataPath, "utf-8");
  const lattice: LatticeEntity = JSON.parse(rawData);

  console.log(`Ingesting Lattice Track: ${lattice.callsign ?? "N/A"} (ID: ${lattice.id})`);

  // Translate
  const t0 = performance.now();
  const uciEntity = translateLatticeToUCI(lattice);
  const t1 = performance.now();

  // Validate with generated Zod schema
  EntityMtSchema.parse(uciEntity);

  const xmlOutput = serializeToXml(uciEntity);
  const t2 = performance.now();

  console.log(`\n[1] Generated USAF UCI XML Message (latency: ${((t2 - t0) * 1000).toFixed(2)} μs):`);
  console.log(xmlOutput);

  const jsonOutput = JSON.stringify(uciEntity, null, 2);
  const t3 = performance.now();

  console.log(`\n[2] Generated Native JSON on Same Model (latency: ${((t3 - t2) * 1000).toFixed(2)} μs):`);
  console.log(jsonOutput);

  const t_parse_start = performance.now();
  const parsedFromJson = JSON.parse(jsonOutput);
  const validatedFromJson: EntityMt = EntityMtSchema.parse(parsedFromJson);
  const t_parse_end = performance.now();

  console.log(`\n[3] Inherent JSON Deserialization & Zod Validation (latency: ${((t_parse_end - t_parse_start) * 1000).toFixed(2)} μs):`);
  console.log(`    Restored UUID: ${validatedFromJson.messageData.entityId.uuid}`);
  console.log(`    Restored Callsign: ${validatedFromJson.messageData.entityId.callsign}`);
  console.log(`    Restored Coordinates: (${validatedFromJson.messageData.kinematics.latitude}, ${validatedFromJson.messageData.kinematics.longitude})`);

  if (validatedFromJson.messageData.entityId.uuid !== lattice.id) {
    throw new Error("UUID mismatch in TypeScript JSON roundtrip");
  }

  if (!xmlOutput.includes("EntityMT")) {
    throw new Error("Missing EntityMT tag in XML output");
  }
  if (!xmlOutput.includes(lattice.id)) {
    throw new Error("Missing UUID in XML output");
  }

  console.log("\n✅ TypeScript Lattice ↔ UCI Bridge executed successfully with Zod runtime validation!");
}

main();

package com.enterprise.uci;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.time.Instant;
import java.util.Optional;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Main {

    record LatticeEntity(
        String id,
        String callsign,
        String timestamp,
        String sourceSystem,
        String status,
        double latitude,
        double longitude,
        double altitudeMeters,
        Double headingDegrees,
        Double groundSpeedMps,
        Double verticalSpeedMps
    ) {}

    private static String extractString(String json, String key) {
        Pattern pattern = Pattern.compile("\"" + key + "\"\\s*:\\s*\"([^\"]+)\"");
        Matcher matcher = pattern.matcher(json);
        if (matcher.find()) {
            return matcher.group(1);
        }
        return null;
    }

    private static double extractDouble(String json, String key, double defaultVal) {
        Pattern pattern = Pattern.compile("\"" + key + "\"\\s*:\\s*([-+]?[0-9]*\\.?[0-9]+)");
        Matcher matcher = pattern.matcher(json);
        if (matcher.find()) {
            return Double.parseDouble(matcher.group(1));
        }
        return defaultVal;
    }

    private static Path findDataFile() throws IOException {
        String[] candidates = {
            "data/lattice_entity.json",
            "../../data/lattice_entity.json",
            "../data/lattice_entity.json"
        };
        for (String c : candidates) {
            Path p = Paths.get(c);
            if (Files.exists(p)) {
                return p.toAbsolutePath();
            }
        }
        throw new IOException("Could not locate data/lattice_entity.json");
    }

    private static EntityMt translateLatticeToUCI(LatticeEntity lattice) {
        EntityStatusEnum status = switch (Optional.ofNullable(lattice.status).orElse("").toUpperCase()) {
            case "CONFIRMED" -> EntityStatusEnum.CONFIRMED;
            case "TENTATIVE" -> EntityStatusEnum.TENTATIVE;
            case "LOST" -> EntityStatusEnum.LOST;
            case "DROPPED" -> EntityStatusEnum.DROPPED;
            case "DESTROYED" -> EntityStatusEnum.DESTROYED;
            default -> EntityStatusEnum.POTENTIAL;
        };

        Instant ts;
        try {
            ts = Instant.parse(lattice.timestamp);
        } catch (Exception e) {
            ts = Instant.now();
        }

        EntityIdType entityId = new EntityIdType(
            lattice.id,
            Optional.ofNullable(lattice.callsign)
        );

        KinematicsType kinematics = new KinematicsType(
            lattice.latitude,
            lattice.longitude,
            lattice.altitudeMeters,
            Optional.ofNullable(lattice.headingDegrees),
            Optional.ofNullable(lattice.groundSpeedMps),
            Optional.ofNullable(lattice.verticalSpeedMps)
        );

        EntityMdt mdt = new EntityMdt(
            entityId,
            ts,
            status,
            kinematics,
            Optional.ofNullable(lattice.sourceSystem)
        );

        return new EntityMt(
            Optional.of(ObjectStateEnum.ACTIVE),
            mdt
        );
    }

    private static String serializeToXml(EntityMt entity) {
        StringBuilder sb = new StringBuilder();
        sb.append("<EntityMT>\n");
        sb.append("  <SecurityInformation>\n");
        sb.append("    <Classification>SECRET</Classification>\n");
        sb.append("    <OwnerProducer>USA</OwnerProducer>\n");
        sb.append("  </SecurityInformation>\n");
        sb.append("  <MessageHeader>\n");
        sb.append("    <MessageID>MSG-").append(entity.messageData().entityId().uuid().substring(0, 8).toUpperCase()).append("</MessageID>\n");
        sb.append("    <Timestamp>").append(entity.messageData().creationTimestamp()).append("</Timestamp>\n");
        sb.append("    <OriginatorID>").append(entity.messageData().sourceSystem().orElse("LATTICE_MESH_NODE_DELTA")).append("</OriginatorID>\n");
        sb.append("  </MessageHeader>\n");
        entity.objectState().ifPresent(state ->
            sb.append("  <ObjectState>").append(state).append("</ObjectState>\n")
        );
        sb.append("  <MessageData>\n");
        sb.append("    <EntityID>\n");
        sb.append("      <UUID>").append(entity.messageData().entityId().uuid()).append("</UUID>\n");
        entity.messageData().entityId().callsign().ifPresent(cs ->
            sb.append("      <Callsign>").append(cs).append("</Callsign>\n")
        );
        sb.append("    </EntityID>\n");
        sb.append("    <CreationTimestamp>").append(entity.messageData().creationTimestamp()).append("</CreationTimestamp>\n");
        sb.append("    <EntityStatus>").append(entity.messageData().entityStatus()).append("</EntityStatus>\n");
        sb.append("    <Kinematics>\n");
        sb.append("      <Latitude>").append(entity.messageData().kinematics().latitude()).append("</Latitude>\n");
        sb.append("      <Longitude>").append(entity.messageData().kinematics().longitude()).append("</Longitude>\n");
        sb.append("      <Altitude>").append(entity.messageData().kinematics().altitude()).append("</Altitude>\n");
        entity.messageData().kinematics().heading().ifPresent(h ->
            sb.append("      <Heading>").append(h).append("</Heading>\n")
        );
        entity.messageData().kinematics().groundSpeed().ifPresent(s ->
            sb.append("      <GroundSpeed>").append(s).append("</GroundSpeed>\n")
        );
        entity.messageData().kinematics().verticalSpeed().ifPresent(vs ->
            sb.append("      <VerticalSpeed>").append(vs).append("</VerticalSpeed>\n")
        );
        sb.append("    </Kinematics>\n");
        entity.messageData().sourceSystem().ifPresent(src ->
            sb.append("    <SourceSystem>").append(src).append("</SourceSystem>\n")
        );
        sb.append("  </MessageData>\n");
        sb.append("</EntityMT>");
        return sb.toString();
    }

    private static String serializeToJson(EntityMt entity) {
        return "{\n" +
            "  \"ObjectState\": \"" + entity.objectState().map(ObjectStateEnum::name).orElse("") + "\",\n" +
            "  \"MessageData\": {\n" +
            "    \"EntityID\": { \"UUID\": \"" + entity.messageData().entityId().uuid() + "\" },\n" +
            "    \"CreationTimestamp\": \"" + entity.messageData().creationTimestamp() + "\",\n" +
            "    \"EntityStatus\": \"" + entity.messageData().entityStatus() + "\",\n" +
            "    \"Kinematics\": {\n" +
            "      \"Latitude\": " + entity.messageData().kinematics().latitude() + ",\n" +
            "      \"Longitude\": " + entity.messageData().kinematics().longitude() + ",\n" +
            "      \"Altitude\": " + entity.messageData().kinematics().altitude() + "\n" +
            "    }\n" +
            "  }\n" +
            "}";
    }

    public static void main(String[] args) throws Exception {
        System.out.println("================================================================================");
        System.out.println("🛸 PolyXML: Anduril Lattice SDK ↔ USAF UCI C2 Bridge (Java 21+ Records)");
        System.out.println("================================================================================");

        Path dataPath = findDataFile();
        String jsonContent = Files.readString(dataPath);

        LatticeEntity lattice = new LatticeEntity(
            extractString(jsonContent, "id"),
            extractString(jsonContent, "callsign"),
            extractString(jsonContent, "timestamp"),
            extractString(jsonContent, "source_system"),
            extractString(jsonContent, "status"),
            extractDouble(jsonContent, "latitude", 0.0),
            extractDouble(jsonContent, "longitude", 0.0),
            extractDouble(jsonContent, "altitude_meters", 0.0),
            extractDouble(jsonContent, "heading_degrees", 0.0),
            extractDouble(jsonContent, "ground_speed_mps", 0.0),
            extractDouble(jsonContent, "vertical_speed_mps", 0.0)
        );

        System.out.printf("Ingesting Lattice Track: %s (ID: %s)%n", lattice.callsign(), lattice.id());

        long startXml = System.nanoTime();
        EntityMt uciEntity = translateLatticeToUCI(lattice);
        String xmlOutput = serializeToXml(uciEntity);
        long endXml = System.nanoTime();
        double xmlMicros = (endXml - startXml) / 1000.0;

        System.out.printf("%n[1] Generated USAF UCI XML Message (latency: %.2f μs):%n", xmlMicros);
        System.out.println(xmlOutput);

        long startJson = System.nanoTime();
        String jsonOutput = serializeToJson(uciEntity);
        long endJson = System.nanoTime();
        double jsonMicros = (endJson - startJson) / 1000.0;

        System.out.printf("%n[2] Generated Native JSON on Same Model (latency: %.2f μs):%n", jsonMicros);
        System.out.println(jsonOutput);

        if (!xmlOutput.contains("EntityMT")) {
            throw new AssertionError("Missing EntityMT tag in XML output");
        }
        if (!xmlOutput.contains(lattice.id())) {
            throw new AssertionError("Missing UUID in XML output");
        }

        System.out.println("\n✅ Java 21+ Lattice ↔ UCI Bridge executed successfully!");
    }
}

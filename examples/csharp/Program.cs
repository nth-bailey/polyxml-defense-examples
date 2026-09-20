using System;
using System.Diagnostics;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Xml;
using System.Xml.Serialization;
using Enterprise.Uci;

namespace LatticeUciBridge;

public record LatticeLocation(
    [property: JsonPropertyName("latitude")] double Latitude,
    [property: JsonPropertyName("longitude")] double Longitude,
    [property: JsonPropertyName("altitude_meters")] double AltitudeMeters
);

public record LatticeKinematics(
    [property: JsonPropertyName("heading_degrees")] double? HeadingDegrees,
    [property: JsonPropertyName("ground_speed_mps")] double? GroundSpeedMps,
    [property: JsonPropertyName("vertical_speed_mps")] double? VerticalSpeedMps
);

public record LatticeEntity(
    [property: JsonPropertyName("id")] string Id,
    [property: JsonPropertyName("callsign")] string? Callsign,
    [property: JsonPropertyName("timestamp")] string Timestamp,
    [property: JsonPropertyName("source_system")] string? SourceSystem,
    [property: JsonPropertyName("classification")] string? Classification,
    [property: JsonPropertyName("status")] string? Status,
    [property: JsonPropertyName("location")] LatticeLocation Location,
    [property: JsonPropertyName("kinematics")] LatticeKinematics Kinematics
);

public static class Program
{
    private static string FindDataFile()
    {
        string[] candidates = {
            "data/lattice_entity.json",
            "../../data/lattice_entity.json",
            "../data/lattice_entity.json"
        };
        foreach (var c in candidates)
        {
            if (File.Exists(c))
            {
                return Path.GetFullPath(c);
            }
        }
        throw new FileNotFoundException("Could not locate data/lattice_entity.json");
    }

    private static EntityMt TranslateLatticeToUci(LatticeEntity lattice)
    {
        var ts = DateTimeOffset.TryParse(lattice.Timestamp, out var parsedTs)
            ? parsedTs
            : DateTimeOffset.UtcNow;

        var status = lattice.Status?.ToUpperInvariant() switch
        {
            "CONFIRMED" => EntityStatusEnum.Confirmed,
            "TENTATIVE" => EntityStatusEnum.Tentative,
            "LOST" => EntityStatusEnum.Lost,
            "DROPPED" => EntityStatusEnum.Dropped,
            "DESTROYED" => EntityStatusEnum.Destroyed,
            _ => EntityStatusEnum.Potential
        };

        var entityId = new EntityIdType(lattice.Id, lattice.Callsign);
        var kinematics = new KinematicsType(
            lattice.Location.Latitude,
            lattice.Location.Longitude,
            lattice.Location.AltitudeMeters,
            lattice.Kinematics.HeadingDegrees,
            lattice.Kinematics.GroundSpeedMps,
            lattice.Kinematics.VerticalSpeedMps
        );

        var mdt = new EntityMdt(
            entityId,
            ts,
            status,
            kinematics,
            lattice.SourceSystem
        );

        return new EntityMt(ObjectStateEnum.Active, mdt)
        {
            SecurityInformation = new SecurityInformationType(ClassificationEnum.Secret, "USA"),
            MessageHeader = new HeaderType(
                $"MSG-{lattice.Id.Substring(0, 8).ToUpperInvariant()}",
                ts,
                lattice.SourceSystem ?? "LATTICE_MESH_NODE_DELTA"
            )
        };
    }

    public static void Main(string[] args)
    {
        Console.WriteLine("================================================================================");
        Console.WriteLine("🛸 PolyXML: Anduril Lattice SDK ↔ USAF UCI C2 Bridge (C# 12 / .NET 8)");
        Console.WriteLine("================================================================================");

        var dataPath = FindDataFile();
        var jsonBytes = File.ReadAllBytes(dataPath);
        var lattice = JsonSerializer.Deserialize<LatticeEntity>(jsonBytes)!;

        Console.WriteLine($"Ingesting Lattice Track: {lattice.Callsign ?? "N/A"} (ID: {lattice.Id})");

        // Benchmark XML Serialization
        var swXml = Stopwatch.StartNew();
        var uciEntity = TranslateLatticeToUci(lattice);

        var xmlSerializer = new XmlSerializer(typeof(EntityMt));
        var xmlSettings = new XmlWriterSettings
        {
            Indent = true,
            OmitXmlDeclaration = true
        };

        using var stringWriter = new StringWriter();
        using (var xmlWriter = XmlWriter.Create(stringWriter, xmlSettings))
        {
            var namespaces = new XmlSerializerNamespaces();
            namespaces.Add("uci", "https://www.vdl.afrl.af.mil/programs/oam");
            xmlSerializer.Serialize(xmlWriter, uciEntity, namespaces);
        }
        swXml.Stop();

        var xmlOutput = stringWriter.ToString();
        Console.WriteLine($"\n[1] Generated USAF UCI XML Message (latency: {swXml.Elapsed.TotalMicroseconds:F2} μs):");
        Console.WriteLine(xmlOutput);

        // Benchmark Native JSON Serialization on same record
        var swJson = Stopwatch.StartNew();
        var jsonOptions = new JsonSerializerOptions
        {
            WriteIndented = true
        };
        var jsonOutput = JsonSerializer.Serialize(uciEntity, jsonOptions);
        swJson.Stop();

        Console.WriteLine($"\n[2] Generated Native JSON on Same Model (latency: {swJson.Elapsed.TotalMicroseconds:F2} μs):");
        Console.WriteLine(jsonOutput);

        // Benchmark Native JSON Deserialization back into EntityMt record
        var swFromJson = Stopwatch.StartNew();
        var restoredUci = JsonSerializer.Deserialize<EntityMt>(jsonOutput);
        swFromJson.Stop();

        Console.WriteLine($"\n[3] Inherent JSON Deserialization into EntityMt (latency: {swFromJson.Elapsed.TotalMicroseconds:F2} μs):");
        Console.WriteLine($"    Restored UUID: {restoredUci?.MessageData.EntityId.Uuid}");
        Console.WriteLine($"    Restored Callsign: {restoredUci?.MessageData.EntityId.Callsign}");
        Console.WriteLine($"    Restored Coordinates: ({restoredUci?.MessageData.Kinematics.Latitude}, {restoredUci?.MessageData.Kinematics.Longitude})");

        if (restoredUci?.MessageData.EntityId.Uuid != lattice.Id)
        {
            throw new InvalidOperationException("UUID mismatch in C# JSON roundtrip");
        }

        if (!xmlOutput.Contains("EntityMT"))
        {
            throw new InvalidOperationException("Missing EntityMT in XML output");
        }
        if (!xmlOutput.Contains(lattice.Id))
        {
            throw new InvalidOperationException("Missing UUID in XML output");
        }

        Console.WriteLine("\n✅ C# 12 Lattice ↔ UCI Bridge executed successfully with dual attributes!");
    }
}

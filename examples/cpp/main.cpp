#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <regex>
#include <filesystem>
#include <cassert>
#include "uci_entity_core.hpp"

namespace fs = std::filesystem;
using namespace polyxml::generated;

// Statically verify C++20 XmlModel concept
static_assert(XmlModel<EntityMt>, "EntityMt must satisfy C++20 XmlModel concept");
static_assert(XmlModel<EntityMdt>, "EntityMdt must satisfy C++20 XmlModel concept");
static_assert(XmlModel<KinematicsType>, "KinematicsType must satisfy C++20 XmlModel concept");

struct LatticeEntity {
    std::string id;
    std::optional<std::string> callsign;
    std::string timestamp;
    std::optional<std::string> source_system;
    std::optional<std::string> classification;
    std::string status;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude_meters = 0.0;
    std::optional<double> heading_degrees;
    std::optional<double> ground_speed_mps;
    std::optional<double> vertical_speed_mps;
    std::optional<double> airspeed_mps;
    std::optional<double> pitch_degrees;
    std::optional<double> roll_degrees;
    std::optional<std::string> flight_mode;
    std::optional<std::string> active_waypoint_id;
    std::optional<double> fuel_remaining_percent;
};

std::string extract_string_field(const std::string& json, const std::string& key) {
    std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch match;
    if (std::regex_search(json, match, re) && match.size() > 1) {
        return match[1].str();
    }
    return "";
}

double extract_double_field(const std::string& json, const std::string& key) {
    std::regex re("\"" + key + "\"\\s*:\\s*([-+]?[0-9]*\\.?[0-9]+)");
    std::smatch match;
    if (std::regex_search(json, match, re) && match.size() > 1) {
        return std::stod(match[1].str());
    }
    return 0.0;
}

std::string find_data_file() {
    const std::vector<std::string> candidates = {
        "data/lattice_entity.json",
        "../../data/lattice_entity.json",
        "../data/lattice_entity.json"
    };
    for (const auto& p : candidates) {
        if (fs::exists(p)) {
            return fs::absolute(p).string();
        }
    }
    return "";
}

EntityMt translate_lattice_to_uci(const LatticeEntity& lattice) {
    EntityMt entity;
    entity.object_state = ObjectStateEnum::Active;

    // SecurityInformation & Header
    entity.security_information.classification = ClassificationEnum::Unclassified;
    entity.security_information.owner_producer = "USA";

    entity.message_header.message_id = "MSG-" + lattice.id.substr(0, 8);
    entity.message_header.timestamp = lattice.timestamp;
    entity.message_header.originator_id = lattice.source_system.value_or("LATTICE_MESH_NODE_DELTA");

    // MessageData
    entity.message_data.entity_id.uuid = lattice.id;
    entity.message_data.entity_id.callsign = lattice.callsign;
    entity.message_data.creation_timestamp = lattice.timestamp;

    if (auto status = entity_status_enum_from_string(lattice.status)) {
        entity.message_data.entity_status = *status;
    } else {
        entity.message_data.entity_status = EntityStatusEnum::Potential;
    }

    entity.message_data.kinematics.latitude = lattice.latitude;
    entity.message_data.kinematics.longitude = lattice.longitude;
    entity.message_data.kinematics.altitude = lattice.altitude_meters;
    entity.message_data.kinematics.heading = lattice.heading_degrees;
    entity.message_data.kinematics.ground_speed = lattice.ground_speed_mps;
    entity.message_data.kinematics.vertical_speed = lattice.vertical_speed_mps;
    entity.message_data.kinematics.airspeed = lattice.airspeed_mps;
    entity.message_data.kinematics.pitch = lattice.pitch_degrees;
    entity.message_data.kinematics.roll = lattice.roll_degrees;

    entity.message_data.source_system = lattice.source_system;
    entity.message_data.flight_mode = lattice.flight_mode;
    entity.message_data.active_waypoint = lattice.active_waypoint_id;
    entity.message_data.fuel_percentage = lattice.fuel_remaining_percent;

    return entity;
}

std::string serialize_uci_xml(const EntityMt& entity) {
    std::ostringstream oss;
    oss << "<EntityMT>";
    oss << "<SecurityInformation>";
    oss << "<Classification>" << to_string(entity.security_information.classification) << "</Classification>";
    if (entity.security_information.owner_producer) {
        oss << "<OwnerProducer>" << *entity.security_information.owner_producer << "</OwnerProducer>";
    }
    oss << "</SecurityInformation>";

    oss << "<MessageHeader>";
    oss << "<MessageID>" << entity.message_header.message_id << "</MessageID>";
    oss << "<Timestamp>" << entity.message_header.timestamp << "</Timestamp>";
    oss << "<OriginatorID>" << entity.message_header.originator_id << "</OriginatorID>";
    oss << "</MessageHeader>";

    if (entity.object_state) {
        oss << "<ObjectState>" << to_string(*entity.object_state) << "</ObjectState>";
    }

    oss << "<MessageData>";
    oss << "<EntityID>";
    oss << "<UUID>" << entity.message_data.entity_id.uuid << "</UUID>";
    if (entity.message_data.entity_id.callsign) {
        oss << "<Callsign>" << *entity.message_data.entity_id.callsign << "</Callsign>";
    }
    oss << "</EntityID>";

    oss << "<CreationTimestamp>" << entity.message_data.creation_timestamp << "</CreationTimestamp>";
    oss << "<EntityStatus>" << to_string(entity.message_data.entity_status) << "</EntityStatus>";

    oss << "<Kinematics>";
    oss << "<Latitude>" << entity.message_data.kinematics.latitude << "</Latitude>";
    oss << "<Longitude>" << entity.message_data.kinematics.longitude << "</Longitude>";
    oss << "<Altitude>" << entity.message_data.kinematics.altitude << "</Altitude>";
    if (entity.message_data.kinematics.heading) {
        oss << "<Heading>" << *entity.message_data.kinematics.heading << "</Heading>";
    }
    if (entity.message_data.kinematics.ground_speed) {
        oss << "<GroundSpeed>" << *entity.message_data.kinematics.ground_speed << "</GroundSpeed>";
    }
    if (entity.message_data.kinematics.vertical_speed) {
        oss << "<VerticalSpeed>" << *entity.message_data.kinematics.vertical_speed << "</VerticalSpeed>";
    }
    if (entity.message_data.kinematics.airspeed) {
        oss << "<Airspeed>" << *entity.message_data.kinematics.airspeed << "</Airspeed>";
    }
    if (entity.message_data.kinematics.pitch) {
        oss << "<Pitch>" << *entity.message_data.kinematics.pitch << "</Pitch>";
    }
    if (entity.message_data.kinematics.roll) {
        oss << "<Roll>" << *entity.message_data.kinematics.roll << "</Roll>";
    }
    oss << "</Kinematics>";

    if (entity.message_data.source_system) {
        oss << "<SourceSystem>" << *entity.message_data.source_system << "</SourceSystem>";
    }
    if (entity.message_data.flight_mode) {
        oss << "<FlightMode>" << *entity.message_data.flight_mode << "</FlightMode>";
    }
    if (entity.message_data.active_waypoint) {
        oss << "<ActiveWaypoint>" << *entity.message_data.active_waypoint << "</ActiveWaypoint>";
    }
    if (entity.message_data.fuel_percentage) {
        oss << "<FuelPercentage>" << *entity.message_data.fuel_percentage << "</FuelPercentage>";
    }
    oss << "</MessageData>";
    oss << "</EntityMT>";
    return oss.str();
}

std::string serialize_uci_json(const EntityMt& entity) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"SecurityInformation\":{\"Classification\":\"" << to_string(entity.security_information.classification) << "\"},";
    oss << "\"MessageHeader\":{\"MessageID\":\"" << entity.message_header.message_id << "\",\"Timestamp\":\"" << entity.message_header.timestamp << "\"},";
    if (entity.object_state) {
        oss << "\"ObjectState\":\"" << to_string(*entity.object_state) << "\",";
    }
    oss << "\"MessageData\":{";
    oss << "\"EntityID\":{\"UUID\":\"" << entity.message_data.entity_id.uuid << "\"";
    if (entity.message_data.entity_id.callsign) {
        oss << ",\"Callsign\":\"" << *entity.message_data.entity_id.callsign << "\"";
    }
    oss << "},";
    oss << "\"EntityStatus\":\"" << to_string(entity.message_data.entity_status) << "\",";
    oss << "\"Kinematics\":{";
    oss << "\"Latitude\":" << entity.message_data.kinematics.latitude << ",";
    oss << "\"Longitude\":" << entity.message_data.kinematics.longitude;
    if (entity.message_data.kinematics.airspeed) {
        oss << ",\"Airspeed\":" << *entity.message_data.kinematics.airspeed;
    }
    oss << "}";
    if (entity.message_data.flight_mode) {
        oss << ",\"FlightMode\":\"" << *entity.message_data.flight_mode << "\"";
    }
    oss << "}}";
    return oss.str();
}

int main() {
    std::cout << "================================================================================" << std::endl;
    std::cout << "🛸 PolyXML: Anduril Lattice SDK ↔ USAF UCI C2 Bridge (Modern C++20)" << std::endl;
    std::cout << "   Autonomous Flying Drone Airplane Telemetry (UNCLASSIFIED)" << std::endl;
    std::cout << "================================================================================" << std::endl;

    std::string data_file = find_data_file();
    if (data_file.empty()) {
        std::cerr << "Error: Could not locate data/lattice_entity.json" << std::endl;
        return 1;
    }

    std::ifstream file(data_file);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    LatticeEntity lattice;
    lattice.id = extract_string_field(content, "id");
    lattice.callsign = extract_string_field(content, "callsign");
    lattice.timestamp = extract_string_field(content, "timestamp");
    lattice.source_system = extract_string_field(content, "source_system");
    lattice.classification = extract_string_field(content, "classification");
    lattice.status = extract_string_field(content, "status");

    lattice.latitude = extract_double_field(content, "latitude");
    lattice.longitude = extract_double_field(content, "longitude");
    lattice.altitude_meters = extract_double_field(content, "altitude_meters");

    double hdg = extract_double_field(content, "heading_degrees");
    if (hdg != 0.0) lattice.heading_degrees = hdg;

    double gs = extract_double_field(content, "ground_speed_mps");
    if (gs != 0.0) lattice.ground_speed_mps = gs;

    double vs = extract_double_field(content, "vertical_speed_mps");
    if (vs != 0.0) lattice.vertical_speed_mps = vs;

    double as = extract_double_field(content, "airspeed_mps");
    if (as != 0.0) lattice.airspeed_mps = as;

    double pitch = extract_double_field(content, "pitch_degrees");
    if (pitch != 0.0) lattice.pitch_degrees = pitch;

    double roll = extract_double_field(content, "roll_degrees");
    if (roll != 0.0) lattice.roll_degrees = roll;

    lattice.flight_mode = extract_string_field(content, "flight_mode");
    lattice.active_waypoint_id = extract_string_field(content, "active_waypoint_id");
    double fuel = extract_double_field(content, "fuel_remaining_percent");
    if (fuel != 0.0) lattice.fuel_remaining_percent = fuel;

    std::cout << "Ingesting Autonomous Drone Telemetry: "
              << (lattice.callsign ? *lattice.callsign : "N/A")
              << " (ID: " << lattice.id << ")" << std::endl;

    // 1. Ingest & Serialize to XML
    auto start_xml = std::chrono::high_resolution_clock::now();
    EntityMt uci_entity = translate_lattice_to_uci(lattice);
    std::string xml_output = serialize_uci_xml(uci_entity);
    auto end_xml = std::chrono::high_resolution_clock::now();
    double xml_us = std::chrono::duration<double, std::micro>(end_xml - start_xml).count();

    std::cout << "\n[1] Generated USAF UCI XML Message (latency: " << xml_us << " μs):" << std::endl;
    std::cout << xml_output << std::endl;

    assert(xml_output.find("EntityMT") != std::string::npos);
    assert(xml_output.find("UNCLASSIFIED") != std::string::npos);
    assert(xml_output.find("FURY-UAV-01") != std::string::npos);

    // 2. Inherent JSON Serialization
    auto start_json = std::chrono::high_resolution_clock::now();
    std::string json_output = serialize_uci_json(uci_entity);
    auto end_json = std::chrono::high_resolution_clock::now();
    double json_us = std::chrono::duration<double, std::micro>(end_json - start_json).count();

    std::cout << "\n[2] Generated Native JSON on Same Model (latency: " << json_us << " μs):" << std::endl;
    std::cout << json_output << std::endl;

    // Verify C++20 default equality comparison
    EntityMt copy_entity = uci_entity;
    assert(copy_entity == uci_entity);

    std::cout << "\n✅ C++20 Lattice ↔ UCI Bridge executed successfully!" << std::endl;
    return 0;
}

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

struct LatticeEntity {
    std::string id;
    std::string callsign;
    std::string timestamp;
    std::string source_system;
    std::string classification;
    std::string status;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude_meters = 0.0;
    double heading_degrees = 0.0;
    double ground_speed_mps = 0.0;
    double vertical_speed_mps = 0.0;
};

// Simple zero-dependency JSON field extractor for benchmark demo
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
    entity.security_information.classification = ClassificationEnum::Secret;
    entity.security_information.owner_producer = "USA";

    entity.message_header.message_id = "MSG-" + lattice.id.substr(0, 8);
    entity.message_header.timestamp = lattice.timestamp;
    entity.message_header.originator_id = lattice.source_system;

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

    entity.message_data.source_system = lattice.source_system;

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
    oss << "</Kinematics>";

    if (entity.message_data.source_system) {
        oss << "<SourceSystem>" << *entity.message_data.source_system << "</SourceSystem>";
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
    oss << "\"EntityID\":{\"UUID\":\"" << entity.message_data.entity_id.uuid << "\"},";
    oss << "\"EntityStatus\":\"" << to_string(entity.message_data.entity_status) << "\",";
    oss << "\"Kinematics\":{\"Latitude\":" << entity.message_data.kinematics.latitude << ",\"Longitude\":" << entity.message_data.kinematics.longitude << "}";
    oss << "}}";
    return oss.str();
}

int main() {
    std::cout << "================================================================================" << std::endl;
    std::cout << "🛸 PolyXML: Anduril Lattice SDK ↔ USAF UCI C2 Bridge (Modern C++20)" << std::endl;
    std::cout << "================================================================================" << std::endl;

    std::string data_path = find_data_file();
    if (data_path.empty()) {
        std::cerr << "Error: Could not locate data/lattice_entity.json" << std::endl;
        return 1;
    }

    std::ifstream file(data_path);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << data_path << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json_content = buffer.str();

    LatticeEntity lattice;
    lattice.id = extract_string_field(json_content, "id");
    lattice.callsign = extract_string_field(json_content, "callsign");
    lattice.timestamp = extract_string_field(json_content, "timestamp");
    lattice.source_system = extract_string_field(json_content, "source_system");
    lattice.status = extract_string_field(json_content, "status");
    lattice.latitude = extract_double_field(json_content, "latitude");
    lattice.longitude = extract_double_field(json_content, "longitude");
    lattice.altitude_meters = extract_double_field(json_content, "altitude_meters");
    lattice.heading_degrees = extract_double_field(json_content, "heading_degrees");
    lattice.ground_speed_mps = extract_double_field(json_content, "ground_speed_mps");
    lattice.vertical_speed_mps = extract_double_field(json_content, "vertical_speed_mps");

    std::cout << "Ingesting Lattice Track: " << lattice.callsign << " (ID: " << lattice.id << ")" << std::endl;

    auto start_xml = std::chrono::high_resolution_clock::now();
    EntityMt uci_entity = translate_lattice_to_uci(lattice);
    std::string xml_output = serialize_uci_xml(uci_entity);
    auto end_xml = std::chrono::high_resolution_clock::now();
    auto xml_us = std::chrono::duration_cast<std::chrono::nanoseconds>(end_xml - start_xml).count() / 1000.0;

    std::cout << "\n[1] Generated USAF UCI XML Message (latency: " << xml_us << " μs):" << std::endl;
    std::cout << xml_output << std::endl;

    auto start_json = std::chrono::high_resolution_clock::now();
    std::string json_output = serialize_uci_json(uci_entity);
    auto end_json = std::chrono::high_resolution_clock::now();
    auto json_us = std::chrono::duration_cast<std::chrono::nanoseconds>(end_json - start_json).count() / 1000.0;

    std::cout << "\n[2] Generated Native JSON on Same Model (latency: " << json_us << " μs):" << std::endl;
    std::cout << json_output << std::endl;

    assert(xml_output.find("EntityMT") != std::string::npos);
    assert(xml_output.find(lattice.id) != std::string::npos);
    assert(xml_output.find("CONFIRMED") != std::string::npos);
    assert(uci_entity.validate());

    std::cout << "\n✅ C++20 Lattice ↔ UCI Bridge executed successfully!" << std::endl;
    return 0;
}

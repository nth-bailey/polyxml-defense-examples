package main

import (
	"encoding/json"
	"encoding/xml"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"uci"
)

type LatticeEntity struct {
	ID             string            `json:"id"`
	Callsign       *string           `json:"callsign,omitempty"`
	Timestamp      string            `json:"timestamp"`
	SourceSystem   *string           `json:"source_system,omitempty"`
	Classification *string           `json:"classification,omitempty"`
	Status         *string           `json:"status,omitempty"`
	Location       LatticeLocation   `json:"location"`
	Kinematics     LatticeKinematics `json:"kinematics"`
}

type LatticeLocation struct {
	Latitude       float64 `json:"latitude"`
	Longitude      float64 `json:"longitude"`
	AltitudeMeters float64 `json:"altitude_meters"`
}

type LatticeKinematics struct {
	HeadingDegrees   *float64 `json:"heading_degrees,omitempty"`
	GroundSpeedMps   *float64 `json:"ground_speed_mps,omitempty"`
	VerticalSpeedMps *float64 `json:"vertical_speed_mps,omitempty"`
}

func translateLatticeToUCI(lattice *LatticeEntity) (*uci.EntityMt, error) {
	ts, err := time.Parse(time.RFC3339, lattice.Timestamp)
	if err != nil {
		ts = time.Now().UTC()
	}

	state := uci.ObjectStateEnumActive
	ownerProducer := "USA"
	msgID := "MSG-" + strings.ToUpper(lattice.ID[:8])
	origID := "LATTICE_MESH_NODE_DELTA"
	if lattice.SourceSystem != nil {
		origID = *lattice.SourceSystem
	}

	var status uci.EntityStatusEnum
	switch strings.ToUpper(derefString(lattice.Status)) {
	case "CONFIRMED":
		status = uci.EntityStatusEnumConfirmed
	case "TENTATIVE":
		status = uci.EntityStatusEnumTentative
	case "LOST":
		status = uci.EntityStatusEnumLost
	case "DROPPED":
		status = uci.EntityStatusEnumDropped
	case "DESTROYED":
		status = uci.EntityStatusEnumDestroyed
	default:
		status = uci.EntityStatusEnumPotential
	}

	entity := &uci.EntityMt{
		XMLName: xml.Name{Local: "EntityMT"},
		MessageType: uci.MessageType{
			SecurityInformation: uci.SecurityInformationType{
				Classification: uci.ClassificationEnumSecret,
				OwnerProducer:  &ownerProducer,
			},
			MessageHeader: uci.HeaderType{
				MessageID:    msgID,
				Timestamp:    ts,
				OriginatorID: origID,
			},
		},
		ObjectState: &state,
		MessageData: uci.EntityMdt{
			EntityID: uci.EntityIdType{
				UUID:     lattice.ID,
				Callsign: lattice.Callsign,
			},
			CreationTimestamp: ts,
			EntityStatus:      status,
			Kinematics: uci.KinematicsType{
				Latitude:      lattice.Location.Latitude,
				Longitude:     lattice.Location.Longitude,
				Altitude:      lattice.Location.AltitudeMeters,
				Heading:       lattice.Kinematics.HeadingDegrees,
				GroundSpeed:   lattice.Kinematics.GroundSpeedMps,
				VerticalSpeed: lattice.Kinematics.VerticalSpeedMps,
			},
			SourceSystem: lattice.SourceSystem,
		},
	}

	return entity, nil
}

func derefString(s *string) string {
	if s == nil {
		return ""
	}
	return *s
}

func findDataFile() (string, error) {
	candidates := []string{
		"data/lattice_entity.json",
		"../../data/lattice_entity.json",
		"../data/lattice_entity.json",
	}
	for _, path := range candidates {
		if _, err := os.Stat(path); err == nil {
			return filepath.Abs(path)
		}
	}
	return "", fmt.Errorf("could not find data/lattice_entity.json")
}

func main() {
	fmt.Println("================================================================================")
	fmt.Println("🛸 PolyXML: Anduril Lattice SDK ↔ USAF UCI C2 Bridge (Go 1.22+)")
	fmt.Println("================================================================================")

	dataPath, err := findDataFile()
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error locating data: %v\n", err)
		os.Exit(1)
	}

	dataBytes, err := os.ReadFile(dataPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error reading file: %v\n", err)
		os.Exit(1)
	}

	var lattice LatticeEntity
	if err := json.Unmarshal(dataBytes, &lattice); err != nil {
		fmt.Fprintf(os.Stderr, "Error unmarshaling Lattice JSON: %v\n", err)
		os.Exit(1)
	}

	callsign := "N/A"
	if lattice.Callsign != nil {
		callsign = *lattice.Callsign
	}
	fmt.Printf("Ingesting Lattice Track: %s (ID: %s)\n", callsign, lattice.ID)

	startXML := time.Now()
	uciEntity, err := translateLatticeToUCI(&lattice)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Translation error: %v\n", err)
		os.Exit(1)
	}

	xmlBytes, err := xml.MarshalIndent(uciEntity, "", "  ")
	if err != nil {
		fmt.Fprintf(os.Stderr, "XML Serialization error: %v\n", err)
		os.Exit(1)
	}
	xmlElapsed := time.Since(startXML)

	fmt.Printf("\n[1] Generated USAF UCI XML Message (latency: %v):\n", xmlElapsed)
	fmt.Println(string(xmlBytes))

	startJSON := time.Now()
	jsonBytes, err := json.MarshalIndent(uciEntity, "", "  ")
	if err != nil {
		fmt.Fprintf(os.Stderr, "JSON Serialization error: %v\n", err)
		os.Exit(1)
	}
	jsonElapsed := time.Since(startJSON)

	fmt.Printf("\n[2] Generated Native JSON on Same Model (latency: %v):\n", jsonElapsed)
	fmt.Println(string(jsonBytes))

	if !strings.Contains(string(xmlBytes), "EntityMT") {
		fmt.Fprintf(os.Stderr, "Assertion failed: missing EntityMT tag in XML output\n")
		os.Exit(1)
	}
	if !strings.Contains(string(xmlBytes), lattice.ID) {
		fmt.Fprintf(os.Stderr, "Assertion failed: missing ID in XML output\n")
		os.Exit(1)
	}

	fmt.Println("\n✅ Go Lattice ↔ UCI Bridge executed successfully!")
}

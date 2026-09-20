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
	ID             string             `json:"id"`
	Callsign       *string            `json:"callsign,omitempty"`
	Timestamp      string             `json:"timestamp"`
	SourceSystem   *string            `json:"source_system,omitempty"`
	Classification *string            `json:"classification,omitempty"`
	Status         *string            `json:"status,omitempty"`
	Location       LatticeLocation    `json:"location"`
	Kinematics     LatticeKinematics  `json:"kinematics"`
	FlightPlan     *LatticeFlightPlan `json:"flight_plan,omitempty"`
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
	AirspeedMps      *float64 `json:"airspeed_mps,omitempty"`
	PitchDegrees     *float64 `json:"pitch_degrees,omitempty"`
	RollDegrees      *float64 `json:"roll_degrees,omitempty"`
}

type LatticeFlightPlan struct {
	FlightMode           *string  `json:"flight_mode,omitempty"`
	ActiveWaypointID     *string  `json:"active_waypoint_id,omitempty"`
	FuelRemainingPercent *float64 `json:"fuel_remaining_percent,omitempty"`
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

	var flightMode *string
	var activeWaypoint *string
	var fuelPct *float64
	if lattice.FlightPlan != nil {
		flightMode = lattice.FlightPlan.FlightMode
		activeWaypoint = lattice.FlightPlan.ActiveWaypointID
		fuelPct = lattice.FlightPlan.FuelRemainingPercent
	}

	entity := &uci.EntityMt{
		XMLName: xml.Name{Local: "EntityMT"},
		MessageType: uci.MessageType{
			SecurityInformation: uci.SecurityInformationType{
				Classification: uci.ClassificationEnumUnclassified,
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
				Airspeed:      lattice.Kinematics.AirspeedMps,
				Pitch:         lattice.Kinematics.PitchDegrees,
				Roll:          lattice.Kinematics.RollDegrees,
			},
			SourceSystem:   lattice.SourceSystem,
			FlightMode:     flightMode,
			ActiveWaypoint: activeWaypoint,
			FuelPercentage: fuelPct,
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

func derefFloat(f *float64) float64 {
	if f == nil {
		return 0.0
	}
	return *f
}

func findDataFile() (string, error) {
	candidates := []string{
		"data/lattice_entity.json",
		"../../data/lattice_entity.json",
		"../data/lattice_entity.json",
	}
	for _, c := range candidates {
		if _, err := os.Stat(c); err == nil {
			return filepath.Abs(c)
		}
	}
	return "", fmt.Errorf("could not locate data/lattice_entity.json")
}

func main() {
	fmt.Println("================================================================================")
	fmt.Println("🛸 PolyXML: Anduril Lattice SDK ↔ USAF UCI C2 Bridge (Go 1.22+)")
	fmt.Println("   Autonomous Flying Drone Airplane Telemetry (UNCLASSIFIED)")
	fmt.Println("================================================================================")

	dataPath, err := findDataFile()
	if err != nil {
		panic(err)
	}

	jsonBytes, err := os.ReadFile(dataPath)
	if err != nil {
		panic(err)
	}

	var lattice LatticeEntity
	if err := json.Unmarshal(jsonBytes, &lattice); err != nil {
		panic(err)
	}

	callsign := "N/A"
	if lattice.Callsign != nil {
		callsign = *lattice.Callsign
	}
	fmt.Printf("Ingesting Autonomous Drone Telemetry: %s (ID: %s)\n", callsign, lattice.ID)

	// 1. Ingest & Serialize to XML
	startXml := time.Now()
	uciEntity, err := translateLatticeToUCI(&lattice)
	if err != nil {
		panic(err)
	}
	xmlBytes, err := xml.MarshalIndent(uciEntity, "", "  ")
	if err != nil {
		panic(err)
	}
	xmlDuration := time.Since(startXml)

	fmt.Printf("\n[1] Generated USAF UCI XML Message (latency: %v):\n", xmlDuration)
	fmt.Println(string(xmlBytes))

	if !strings.Contains(string(xmlBytes), "EntityMT") {
		panic("Generated XML does not contain EntityMT")
	}
	if !strings.Contains(string(xmlBytes), "UNCLASSIFIED") {
		panic("Generated XML is not UNCLASSIFIED")
	}

	// 2. Inherent JSON Serialization on the exact same model
	startJson := time.Now()
	jsonWireBytes, err := json.MarshalIndent(uciEntity, "", "  ")
	if err != nil {
		panic(err)
	}
	jsonDuration := time.Since(startJson)

	fmt.Printf("\n[2] Generated Native JSON on Same Model (latency: %v):\n", jsonDuration)
	fmt.Println(string(jsonWireBytes))

	// 3. Inherent JSON Deserialization back into typed Go struct
	startFromJson := time.Now()
	var restoredEntity uci.EntityMt
	if err := json.Unmarshal(jsonWireBytes, &restoredEntity); err != nil {
		panic(err)
	}
	fromJsonDuration := time.Since(startFromJson)

	fmt.Printf("\n[3] Inherent JSON Deserialization into EntityMt (latency: %v):\n", fromJsonDuration)
	fmt.Printf("    Restored UUID: %s\n", restoredEntity.MessageData.EntityID.UUID)
	fmt.Printf("    Restored Callsign: %s\n", derefString(restoredEntity.MessageData.EntityID.Callsign))
	fmt.Printf("    Restored Coordinates: (%v, %v)\n",
		restoredEntity.MessageData.Kinematics.Latitude,
		restoredEntity.MessageData.Kinematics.Longitude)
	fmt.Printf("    Restored Airspeed: %.1f m/s | Pitch: %.1f° | Roll: %.1f°\n",
		derefFloat(restoredEntity.MessageData.Kinematics.Airspeed),
		derefFloat(restoredEntity.MessageData.Kinematics.Pitch),
		derefFloat(restoredEntity.MessageData.Kinematics.Roll))
	fmt.Printf("    Restored Flight Mode: %s\n", derefString(restoredEntity.MessageData.FlightMode))
	fmt.Printf("    Restored Classification: %s\n", restoredEntity.SecurityInformation.Classification)

	if restoredEntity.MessageData.EntityID.UUID != lattice.ID {
		panic("Restored UUID mismatch")
	}
	if restoredEntity.SecurityInformation.Classification != uci.ClassificationEnumUnclassified {
		panic("Restored classification is not UNCLASSIFIED")
	}

	fmt.Println("\n✅ Go Lattice ↔ UCI Bridge executed successfully with dual struct tags!")
}

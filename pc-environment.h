#ifndef PC_ENVIRONMENT_H
#define PC_ENVIRONMENT_H

// Base class providing the MobComEnv interface (nodes, mobility, LTE helper,
// OpenGym step loop). PowerControlMobComEnv overrides its virtual methods
// to implement the power-control-specific RL logic.
#include "base-environment.h"

// DeviceManager: provides GetBsDevices(), GetUtDevices(), GetBsNodes(), etc.
#include "device-manager.h"

// ns3-ai module: OpenGymSpace, OpenGymDataContainer, OpenGymBoxSpace, etc.
// Handles the socket-based communication with the Python RL agent.
#include "ns3/ai-module.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/mobility-helper.h"
#include "ns3/point-to-point-module.h"
#include <string>

namespace ns3
{

/**
 * @ingroup defiance
 *
 * @brief Single-agent RL environment for LTE downlink power control.
 *
 * The RL agent observes the SINR of every (BS, UE) pair and chooses
 * a TX power level (dBm) for each BS. The reward encourages minimising
 * total transmitted power while keeping every UE above a fixed SINR threshold.
 *
 * Inherits from MobComEnv, which owns the ns-3 node/device/channel setup
 * and drives the OpenGym step loop (observe → act → reward cycle).
 */
class PowerControlMobComEnv : public MobComEnv
{
  public:
    // Inherit all MobComEnv constructors unchanged (duration, step interval, delay).
    using MobComEnv::MobComEnv;

    // -----------------------------------------------------------------------
    // OpenGym interface — called by the base class each RL step
    // -----------------------------------------------------------------------

    /** Returns the action space: one float TX power per BS, range [0, 46] dBm. */
    Ptr<OpenGymSpace> GetActionSpace() override;

    /** Returns the observation space: flat float vector of SINR values, range [-100, 200] dB. */
    Ptr<OpenGymSpace> GetObservationSpace() override;

    /** Packs the current sinrs[][] matrix into an OpenGymBoxContainer and sends it to the agent. */
    Ptr<OpenGymDataContainer> GetObservation() override;

    /**
     * Computes the scalar reward for this step.
     * Priority: coverage penalty (negative, returned immediately if any UE is below
     * sinrThreshold) > efficiency reward (1 - normalised total power).
     */
    float GetReward() override;

    /**
     * Applies the agent's chosen TX powers to the ns-3 LTE PHY of each eNB.
     * Also caches the applied values in powers[] for reward computation.
     */
    bool ExecuteActions(Ptr<OpenGymDataContainer> action) override;

    /**
     * Returns diagnostic key-value pairs sent alongside each RL step
     * (SINR per BS/UE, UE distances, measured powers, sim time, cbCalls).
     * Not used by the agent for training — for logging and analysis only.
     */
    std::map<std::string, std::string> GetExtraInfo() override;

    // -----------------------------------------------------------------------
    // ns-3 scenario construction — called once before Simulator::Run()
    // -----------------------------------------------------------------------

    /**
     * Entry point for scenario setup. Calls MobComEnv::SetupScenario() first,
     * then sizes sinrs[][], powers[], and powersMeasured[] to match the
     * actual number of BSs and UEs, and sets sinrThreshold.
     */
    void SetupScenario() override;

    /**
     * Configures LTE attributes that depend on CLI args:
     * pathloss model, handover algorithm, UE noise figure.
     * Called before nodes are created so Config::SetDefault() is safe.
     */
    void SetScenarioAttributes() override;

    /**
     * Selects and builds the spatial topology based on arg_topology.
     * Delegates to one of: CreateSimpleTopology, CreateBsLine,
     * CreateRandomFixedTopology, or CreateMultiBsTopology.
     */
    void CreateTopology() override;

    /**
     * Installs application-layer traffic sources/sinks on the nodes.
     * Called after devices and IP addresses have been assigned.
     */
    void AddTraffic() override;

    /**
     * Connects updateSinr() to the "ReportCurrentCellRsrpSinr" trace
     * on every component carrier of every UE PHY.
     * Must be called after devices are installed.
     */
    void RegisterCallbacks() override;

    // -----------------------------------------------------------------------
    // Topology factory methods
    // -----------------------------------------------------------------------

    /**
     * Places numBSs eNBs in a straight line of total length lineLength (metres).
     * UEs are distributed uniformly along the same line.
     */
    void CreateBsLine(uint16_t numBSs, uint64_t lineLength);

    /**
     * Places numBSs eNBs randomly within a lineLength × lineLength area
     * and positions numUEs UEs randomly around them.
     * The layout is fixed (not changing during the simulation).
     */
    void CreateRandomFixedTopology(uint16_t numUEs, uint16_t numBSs, uint64_t lineLength);

    /**
     * Creates a single-BS, single-UE topology.
     * The UE starts at `position` and moves with constant `velocity`.
     * Used for quick sanity checks and single-cell experiments.
     */
    void CreateSimpleTopology(Vector position, Vector velocity);

    /**
     * Creates a fixed multi-BS topology with a predefined set of eNB and UE
     * positions. Hardcoded geometry for reproducible multi-cell experiments.
     */
    void CreateMultiBsTopology();

    /**
     * Parses command-line arguments and populates arg_* member variables.
     * Also sets the ns-3 RNG seed (seed + parallel) and the ns3-ai trial name.
     */
    void ParseCliArgs(int argc, char* argv[]);

    // -----------------------------------------------------------------------
    // Helper methods
    // -----------------------------------------------------------------------

    /**
     * Returns the flat shape {nUE * nBS} of the observation vector.
     * Used by both GetObservationSpace() and GetObservation().
     */
    std::vector<uint32_t> GetObservationShape();

    /**
     * Serialises sinrs[][] to a string map with keys "sinr_bs{i}ue{j}",
     * then resets every entry to 0 to prevent stale values bleeding into
     * the next RL step.
     */
    std::map<std::string, std::string> sinrVecToMap();

    // -----------------------------------------------------------------------
    // Shared state — accessed by the updateSinr() free callback function
    // -----------------------------------------------------------------------

    /**
     * 2-D SINR matrix [bs_index][ue_index] in dB.
     * Written by updateSinr() on every PHY trace event.
     * Read by GetObservation() and GetReward() once per RL step.
     * Reset to 0 by sinrVecToMap() after each step.
     */
    std::vector<std::vector<float>> sinrs;

    /**
     * TX power values (dBm) written by ExecuteActions() and used by
     * GetReward() to compute the efficiency term.
     * One entry per BS, indexed 0-based.
     */
    std::vector<float> powers;

    /**
     * TX power values (dBm) read back from the actual ns-3 PHY layer
     * inside updateSinr(). Reported in GetExtraInfo() for diagnostics.
     * May differ from powers[] if ns-3 clamps the value internally.
     */
    std::vector<float> powersMeasured;

    /**
     * Counter incremented on every updateSinr() invocation.
     * Reported in GetExtraInfo() to verify that SINR callbacks are
     * firing at the expected rate (should be nUE × nCC per TTI).
     */
    int cbCalls = 0;

    // -----------------------------------------------------------------------
    // CLI arguments — populated by ParseCliArgs()
    // -----------------------------------------------------------------------

    /** UE movement speed in m/s. 0 = stationary. Default: 0. */
    double arg_speed = 0;

    /**
     * Topology selector string. Passed to CreateTopology() to pick the
     * factory method. Valid values: "simple", "line", "random", "multi".
     * Default: "simple".
     */
    std::string arg_topology = "simple";

    /**
     * UE receiver noise figure in dB. Applied via
     * Config::SetDefault("ns3::LteUePhy::NoiseFigure", ...).
     * Higher values lower the effective SINR. Default: 9 dB.
     */
    double arg_noiseFigure = 9;

    /**
     * ns-3 TypeId string of the path-loss model to install on the channel.
     * Example: "ns3::FriisPropagationLossModel" (free-space, default) or
     * "ns3::ThreeLogDistancePropagationLossModel" (urban macro).
     */
    std::string arg_propagationModel = "ns3::FriisPropagationLossModel";

    /**
     * ns-3 TypeId string of the handover algorithm installed on each eNB.
     * "ns3::NoOpHandoverAlgorithm" disables automatic handover (default).
     * "ns3::A3RsrpHandoverAlgorithm" enables RSRP-based handover.
     */
    std::string arg_handoverAlgorithm = "ns3::NoOpHandoverAlgorithm";

  private:
    /**
     * Minimum acceptable SINR in dB for a UE to be considered "covered".
     * Set to 10 dB in SetupScenario(). UEs below this value contribute
     * a negative penalty to the reward in GetReward().
     */
    float sinrThreshold;
};

} // namespace ns3

#endif /* PC_ENVIRONMENT_H */

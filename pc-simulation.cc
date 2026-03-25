#include "pc-environment.h"
#include "ns3/lte-module.h"
#include <numeric>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("PcSimulation");

/**
 * Trace callback fired by ns-3 every time a UE PHY layer reports a new
 * SINR measurement (TraceSource "ReportCurrentCellRsrpSinr").
 * Bound per-UE via MakeBoundCallback so ueId is pre-filled.
 *
 * @param env     pointer to the RL environment (shared state)
 * @param ueId    index of the UE (0-based, bound at registration time)
 * @param cellId  LTE cell ID (1-based, comes from the trace)
 * @param rnti    Radio Network Temporary Identifier (unused here)
 * @param rsrp    Reference Signal Received Power (unused here)
 * @param sinr    Raw linear SINR value from the PHY layer
 * @param ccId    Component Carrier ID (unused here)
 */
void
updateSinr(PowerControlMobComEnv* env,
           uint16_t ueId,
           uint16_t cellId,
           uint16_t rnti,
           double rsrp,
           double sinr,
           uint8_t ccId)
{
    // Convert linear SINR to dB and store in the 2-D matrix [bs][ue].
    // cellId is 1-based in ns-3, so subtract 1 for 0-based indexing.
    env->sinrs[cellId - 1][ueId] = 10 * std::log10((float)sinr);

    // Track how many times this callback was called (used in GetExtraInfo).
    env->cbCalls++;

    std::cout << "[DEBUG] updateSinr cellId=" << cellId << " ueId=" << ueId
              << " sinr=" << env->sinrs[cellId-1][ueId] << std::endl;

    // Snapshot the current TX power of every eNB so GetExtraInfo can
    // report what power was actually set (not just what the agent requested).
    int i = 0;
    auto& bsDevices = env->GetDeviceManager()->GetBsDevices();
    for (auto it = bsDevices.Begin(); it != bsDevices.End(); it++)
    {
        env->powersMeasured[i] = DynamicCast<LteEnbNetDevice>(*it)->GetPhy()->GetTxPower();
        i++;
    }
}

// ---------------------------------------------------------------------------
// PowerControlMobComEnv — RL environment for LTE downlink power control.
// Inherits MobComEnv which sets up nodes, mobility, and the LTE helper.
// Implements the OpenGym interface: observation, action, reward, extra info.
// ---------------------------------------------------------------------------

/**
 * Called once after the ns-3 topology has been built by the base class.
 * Sizes all per-episode data structures to match the actual number of
 * BSs and UEs in the scenario.
 */
void
PowerControlMobComEnv::SetupScenario()
{
    NS_LOG_FUNCTION(this);

    // Let the base class create nodes, install mobility, configure LTE.
    MobComEnv::SetupScenario();

    // Allocate the SINR matrix: sinrs[bs_index][ue_index].
    // One row per BS, one column per UE.
    sinrs.resize(m_deviceManager->GetBsNodes().GetN());
    for (auto& ues : sinrs)
        ues.resize(m_deviceManager->GetUtNodes().GetN());

    // Minimum acceptable SINR in dB. UEs below this threshold contribute
    // a negative penalty in GetReward().
    sinrThreshold = 10;

    // powers[]        — TX power values sent to BSs by ExecuteActions().
    // powersMeasured[]— TX power values read back from BSs in updateSinr().
    powers.resize(m_deviceManager->GetBsDevices().GetN());
    powersMeasured.resize(m_deviceManager->GetBsDevices().GetN());

    std::cout << "[DEBUG] SetupScenario: BSs=" << m_deviceManager->GetBsNodes().GetN()
              << " UEs=" << m_deviceManager->GetUtNodes().GetN()
              << " sinrThreshold=" << sinrThreshold << std::endl;
}

/**
 * Configures ns-3 LTE attributes that depend on CLI arguments.
 * Called by the base class during scenario construction, before nodes
 * are created, so TypeId lookups and Config::SetDefault are safe here.
 */
void
PowerControlMobComEnv::SetScenarioAttributes()
{
    std::cout << "[DEBUG] SetScenarioAttributes: propagationModel=" << arg_propagationModel << std::endl;

    // Set the path-loss model (e.g. FriisPropagationLossModel,
    // ThreeLogDistancePropagationLossModel, etc.) from the CLI argument.
    GetLteHelper()->SetPathlossModelType(TypeId::LookupByName(arg_propagationModel));

    // Set the handover algorithm (e.g. A3RsrpHandoverAlgorithm).
    GetLteHelper()->SetHandoverAlgorithmType(arg_handoverAlgorithm);

    // Set the UE receiver noise figure in dB (affects SINR calculations).
    Config::SetDefault("ns3::LteUePhy::NoiseFigure", DoubleValue(arg_noiseFigure));
}

/**
 * Connects the updateSinr callback to every component carrier of every UE.
 * Must be called after devices are installed (i.e. after SetupScenario).
 *
 * MakeBoundCallback pre-binds the env pointer and the ueId so that when
 * ns-3 fires the trace it only passes the remaining arguments (cellId,
 * rnti, rsrp, sinr, ccId).
 */
void
PowerControlMobComEnv::RegisterCallbacks()
{
    NS_LOG_FUNCTION(this);
    auto& utDevices = GetDeviceManager()->GetUtDevices();
    uint16_t ueId = 0;

    for (auto it = utDevices.Begin(); it != utDevices.End(); it++)
    {
        // A UE may have multiple component carriers (carrier aggregation).
        // Register the callback on each carrier's PHY layer.
        auto map = DynamicCast<LteUeNetDevice>(*it)->GetCcMap();
        for (auto& mapIt : map)
        {
            DynamicCast<ComponentCarrierUe>(mapIt.second)
                ->GetPhy()
                ->TraceConnectWithoutContext(
                    "ReportCurrentCellRsrpSinr",          // trace source name
                    MakeBoundCallback(&updateSinr, this, ueId)); // pre-bind env + ueId
        }
        std::cout << "[DEBUG] RegisterCallbacks: registered ueId=" << ueId << std::endl;
        ueId++;
    }
}

// ---------------------------------------------------------------------------
// OpenGym interface — observation space
// ---------------------------------------------------------------------------

/**
 * Returns the flat shape of the observation vector.
 * Observation = flattened SINR matrix → size = nUE * nBS.
 */
std::vector<uint32_t>
PowerControlMobComEnv::GetObservationShape()
{
    NS_LOG_FUNCTION(this);
    auto size = GetDeviceManager()->GetUtDevices().GetN() *
                GetDeviceManager()->GetBsDevices().GetN();
    std::cout << "[DEBUG] GetObservationShape size=" << size << std::endl;
    return {static_cast<unsigned int>(size)};
}

/**
 * Defines the observation space bounds for the RL agent.
 * Each element is a float SINR in dB, bounded [-100, 200] dB (wide range
 * to cover all realistic and edge-case values).
 */
Ptr<OpenGymSpace>
PowerControlMobComEnv::GetObservationSpace()
{
    NS_LOG_FUNCTION(this);
    std::string dtype = TypeNameGet<float>();
    auto box = CreateObject<OpenGymBoxSpace>(-100, 200.0, GetObservationShape(), dtype);
    std::cout << "[DEBUG] GetObservationSpace called" << std::endl;
    return box;
}

/**
 * Packages the current SINR matrix into a flat OpenGymBoxContainer
 * and sends it to the RL agent as the observation for this step.
 * Iterates BSs in the outer loop, UEs in the inner loop (row-major order).
 */
Ptr<OpenGymDataContainer>
PowerControlMobComEnv::GetObservation()
{
    NS_LOG_FUNCTION(this);
    auto box = CreateObject<OpenGymBoxContainer<float>>(GetObservationShape());
    for (std::vector<std::vector<float>>::size_type bs = 0; bs < sinrs.size(); bs++)
        for (std::vector<float>::size_type ue = 0; ue < sinrs[bs].size(); ue++)
        {
            std::cout << "[DEBUG] GetObservation bs=" << bs << " ue=" << ue
                      << " sinr=" << sinrs[bs][ue] << std::endl;
            box->AddValue(sinrs[bs][ue]);
        }
    return box;
}

// ---------------------------------------------------------------------------
// OpenGym interface — action space
// ---------------------------------------------------------------------------

/**
 * Defines the action space for the RL agent.
 * Action = one TX power value per BS, in dBm, bounded [0, 46] dBm.
 * 46 dBm is the typical 3GPP maximum for macro eNBs.
 */
Ptr<OpenGymSpace>
PowerControlMobComEnv::GetActionSpace()
{
    NS_LOG_FUNCTION(this);
    std::vector<uint32_t> shape = {
        static_cast<unsigned int>(GetDeviceManager()->GetBsDevices().GetN())};
    std::string dtype = TypeNameGet<float>();
    auto box = CreateObject<OpenGymBoxSpace>(0.0, 46.0, shape, dtype);
    std::cout << "[DEBUG] GetActionSpace: nBS=" << shape[0] << std::endl;
    return box;
}

/**
 * Applies the RL agent's chosen action to the simulation.
 * Reads one TX power value per BS from the action container and calls
 * SetTxPower() on the corresponding LteEnbNetDevice PHY.
 * Also saves the applied powers to powers[] for use in GetReward().
 */
bool
PowerControlMobComEnv::ExecuteActions(Ptr<OpenGymDataContainer> action)
{
    NS_LOG_FUNCTION(this << action);
    auto box = action->GetObject<OpenGymBoxContainer<float>>();
    int bsId = 0;
    auto& bsDevices = m_deviceManager->GetBsDevices();
    for (auto it = bsDevices.Begin(); it != bsDevices.End(); it++)
    {
        float powerdbm = box->GetValue(bsId);

        // Apply the power to the actual ns-3 PHY model.
        DynamicCast<LteEnbNetDevice>(*it)->GetPhy()->SetTxPower(powerdbm);

        // Cache for reward computation.
        powers[bsId] = (float)powerdbm;

        std::cout << "[DEBUG] ExecuteActions bs=" << bsId << " power=" << powerdbm << " dBm" << std::endl;
        bsId++;
    }
    return true;
}

// ---------------------------------------------------------------------------
// OpenGym interface — reward
// ---------------------------------------------------------------------------

/**
 * Computes the scalar reward for the current simulation step.
 *
 * Priority logic (higher priority first):
 *
 * 1. COVERAGE PENALTY: if any UE's SINR is below sinrThreshold (10 dB),
 *    accumulate (sinr - threshold) for each violating UE. This sum is
 *    always negative. Return it immediately — coverage always beats efficiency.
 *
 * 2. EDGE CASE: if total power is ~0 W (agent sent 0 to all BSs), return 1.0
 *    to avoid division by zero. In practice the agent should never do this
 *    while maintaining coverage, so it is unreachable in normal training.
 *
 * 3. EFFICIENCY REWARD: if all UEs are covered, reward = 1 - (powerSum / maxPowerSum).
 *    This incentivises the agent to use as little power as possible while
 *    keeping every UE above the SINR threshold.
 *    Range: (0, 1] — closer to 1 means lower total power used.
 */
float
PowerControlMobComEnv::GetReward()
{
    NS_LOG_FUNCTION(this);
    float negativeReward = 0;

    // Sum of all BS TX powers (dBm) — used for the efficiency term.
    float powerSum = std::accumulate(powers.begin(), powers.end(), 0.0f);

    std::cout << "[DEBUG] GetReward start powerSum=" << powerSum
              << " sinrThreshold=" << sinrThreshold << std::endl;

    // --- Step 1: check coverage ---
    for (std::vector<std::vector<float>>::size_type bs = 0; bs < sinrs.size(); bs++)
    {
        for (std::vector<float>::size_type ue = 0; ue < sinrs[bs].size(); ue++)
        {
            float ueSinr = sinrs[bs][ue];
            std::cout << "[DEBUG] GetReward bs=" << bs << " ue=" << ue
                      << " sinr=" << ueSinr << std::endl;

            if (ueSinr < sinrThreshold)
            {
                // Penalty proportional to how far below threshold the UE is.
                // e.g. SINR=7 dB, threshold=10 dB → penalty = -3
                float penalty = ueSinr - sinrThreshold;
                negativeReward += penalty;
                std::cout << "[DEBUG] GetReward penalty bs=" << bs << " ue=" << ue
                          << " penalty=" << penalty
                          << " negativeReward=" << negativeReward << std::endl;
            }
        }
    }

    // If any UE is uncovered, return the negative penalty immediately.
    if (negativeReward < 0.0)
    {
        std::cout << "[DEBUG] GetReward returning negativeReward=" << negativeReward << std::endl;
        return negativeReward;
    }

    // --- Step 2: edge case — zero power ---
    if (powerSum < 0.001f)
    {
        std::cout << "[DEBUG] GetReward powerSum~0 returning 1.0" << std::endl;
        return 1.0f;
    }

    // --- Step 3: efficiency reward ---
    // maxPowerSum = nBS * 46 dBm (all BSs at maximum power).
    float maxPowerSum = (float)powers.size() * 46.0f;
    float powerNorm   = powerSum / maxPowerSum;   // ∈ (0, 1]
    float reward      = 1.0f - powerNorm;         // ∈ [0, 1)
    std::cout << "[DEBUG] GetReward maxPowerSum=" << maxPowerSum
              << " powerNorm=" << powerNorm
              << " reward=" << reward << std::endl;
    return reward;
}

// ---------------------------------------------------------------------------
// OpenGym interface — extra info (diagnostic / logging)
// ---------------------------------------------------------------------------

/**
 * Returns a string→string map of diagnostic values sent alongside each
 * RL step. Used for logging and analysis in Python — not used by the agent
 * for training. Includes:
 *   - per-(bs,ue) SINR values (via sinrVecToMap, which also resets sinrs to 0)
 *   - UE distances to the first BS
 *   - measured TX powers per BS
 *   - number of SINR callback invocations since last step
 *   - current simulation time in ms
 */
std::map<std::string, std::string>
PowerControlMobComEnv::GetExtraInfo()
{
    NS_LOG_FUNCTION(this);
    auto& utDevices = GetDeviceManager()->GetUtDevices();

    // sinrVecToMap() serialises sinrs[][] to strings AND resets the matrix
    // to 0 so stale values don't bleed into the next step.
    std::map<std::string, std::string> extraInfo = sinrVecToMap();

    // Compute and store the distance of each UE from BS 0.
    for (uint32_t i = 0; i < utDevices.GetN(); i++)
    {
        double distance = 0.0;
        auto utDevice  = utDevices.Get(i);

        // Only BS 0 is used as the reference point for distance reporting.
        auto enbDevice = DynamicCast<LteEnbNetDevice>(GetDeviceManager()->GetBsDevices().Get(0));
        if (enbDevice)
        {
            auto targetNode = enbDevice->GetNode();
            auto mm = utDevice->GetNode()->GetObject<MobilityModel>();
            distance = mm->GetDistanceFrom(targetNode->GetObject<MobilityModel>());
        }
        else
            distance = -1.0; // BS not found — should not happen in a valid scenario.

        extraInfo["distance_" + std::to_string(i)] = std::to_string(distance);
        std::cout << "[DEBUG] GetExtraInfo ue=" << i << " distance=" << distance << std::endl;
    }

    // Total number of SINR callbacks fired since simulation start.
    extraInfo["cbCalls"] = std::to_string(cbCalls);

    // TX power actually measured from each BS PHY (read in updateSinr).
    for (std::vector<float>::size_type i = 0; i < powersMeasured.size(); i++)
    {
        extraInfo["power_bs_" + std::to_string(i)] = std::to_string(powersMeasured[i]);
        std::cout << "[DEBUG] GetExtraInfo bs=" << i << " powerMeasured=" << powersMeasured[i] << std::endl;
    }

    // Simulation wall-clock time at this step.
    extraInfo["time"] = std::to_string(Simulator::Now().GetMilliSeconds());
    std::cout << "[DEBUG] GetExtraInfo time=" << Simulator::Now().GetMilliSeconds()
              << "ms cbCalls=" << cbCalls << std::endl;

    return extraInfo;
}

/**
 * Converts the 2-D sinrs[][] matrix into a flat string map with keys like
 * "sinr_bs0ue1", then resets every entry to 0.
 *
 * Resetting to 0 here is important: the SINR callback fires asynchronously
 * and may write new values between steps. By zeroing after serialisation
 * we ensure each RL step only sees measurements from the current interval.
 */
std::map<std::string, std::string>
PowerControlMobComEnv::sinrVecToMap()
{
    std::map<std::string, std::string> m;
    for (std::vector<std::vector<float>>::size_type bs = 0; bs < sinrs.size(); bs++)
        for (std::vector<float>::size_type ue = 0; ue < sinrs[bs].size(); ue++)
        {
            m["sinr_bs" + std::to_string(bs) + "ue" + std::to_string(ue)] =
                std::to_string(sinrs[bs][ue]);
            std::cout << "[DEBUG] sinrVecToMap bs=" << bs << " ue=" << ue
                      << " sinr=" << sinrs[bs][ue] << std::endl;

            // Reset so stale values don't persist into the next RL step.
            sinrs[bs][ue] = 0;
        }
    return m;
}

// ---------------------------------------------------------------------------
// CLI argument parsing
// ---------------------------------------------------------------------------

/**
 * Parses command-line arguments for the simulation run.
 * Configures:
 *   - Scenario parameters: UE speed, topology, noise figure,
 *     propagation model, handover algorithm.
 *   - Reproducibility: RNG seed, run ID, parallel worker ID.
 *   - ns3-ai trial name (used to match the Python-side experiment name).
 *
 * The effective seed is (seed + parallel) so that parallel workers
 * launched with different --parallel IDs produce independent random streams.
 */
void
PowerControlMobComEnv::ParseCliArgs(int argc, char* argv[])
{
    NS_LOG_FUNCTION(this);
    CommandLine cmd(__FILE__);
    cmd.AddValue("speed",            "Speed of the UE",              this->arg_speed);
    cmd.AddValue("topology",         "Topology to use",              this->arg_topology);
    cmd.AddValue("noiseFigure",      "Noise figure of the UE",       this->arg_noiseFigure);
    cmd.AddValue("propagationModel", "Propagation model to use",     this->arg_propagationModel);
    cmd.AddValue("handoverAlgorithm","Handover algorithm to use",    this->arg_handoverAlgorithm);

    uint32_t seed      = 1;
    uint32_t runId     = 1;
    uint32_t parallel  = 0;
    std::string trialName = "single_trial";
    cmd.AddValue("seed",       "Seed for random number generator", seed);
    cmd.AddValue("runId",      "Run ID",                           runId);
    cmd.AddValue("parallel",   "Parallel ID",                      parallel);
    cmd.AddValue("trial_name", "Name of the trial",                trialName);

    cmd.Parse(argc, argv);

    std::cout << "[DEBUG] ParseCliArgs: topology=" << arg_topology
              << " speed=" << arg_speed
              << " seed=" << seed
              << " runId=" << runId << std::endl;

    // Each parallel worker gets a unique seed offset to avoid correlated runs.
    RngSeedManager::SetSeed(seed + parallel);
    RngSeedManager::SetRun(runId);

    // Tell ns3-ai which Python-side trial this run belongs to.
    Ns3AiMsgInterface::Get()->SetTrialName(trialName);
}

// ---------------------------------------------------------------------------
// Handover trace callbacks (logging only — no RL logic)
// ---------------------------------------------------------------------------

/** Logs the start of a UE-side handover procedure. */
void
NotifyHandoverStartUe(std::string context, uint64_t imsi, uint16_t cellId,
                      uint16_t rnti, uint16_t targetCellId)
{
    std::cout << Simulator::Now().GetSeconds() << " " << context
              << " UE IMSI " << imsi
              << ": previously connected to CellId " << cellId << " with RNTI " << rnti
              << ", doing handover to CellId " << targetCellId << std::endl;
}

/** Logs the successful completion of a UE-side handover. */
void
NotifyHandoverEndOkUe(std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    std::cout << Simulator::Now().GetSeconds() << " " << context
              << " UE IMSI " << imsi
              << ": successful handover to CellId " << cellId << " with RNTI " << rnti << std::endl;
}

/** Logs the start of an eNB-side handover procedure. */
void
NotifyHandoverStartEnb(std::string context, uint64_t imsi, uint16_t cellId,
                       uint16_t rnti, uint16_t targetCellId)
{
    std::cout << Simulator::Now().GetSeconds() << " " << context
              << " eNB CellId " << cellId
              << ": start handover of UE with IMSI " << imsi << " RNTI " << rnti
              << " to CellId " << targetCellId << std::endl;
}

/** Logs the successful completion of an eNB-side handover. */
void
NotifyHandoverEndOkEnb(std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
    std::cout << Simulator::Now().GetSeconds() << " " << context
              << " eNB CellId " << cellId
              << ": completed handover of UE with IMSI " << imsi << " RNTI " << rnti << std::endl;
}

} // namespace ns3

// ---------------------------------------------------------------------------
// main() — simulation entry point
// ---------------------------------------------------------------------------

int
main(int argc, char* argv[])
{
    using namespace ns3;

    std::cout << "[DEBUG] main: starting" << std::endl;

    // Create the RL environment object.
    // Arguments: simulation duration = 20 s, step interval = 5 ms, initial delay = 0 ms.
    Ptr<PowerControlMobComEnv> environment =
        CreateObject<PowerControlMobComEnv>(Seconds(20), MilliSeconds(5), MilliSeconds(0));

    // Parse --speed, --topology, --seed, etc. from the command line.
    environment->ParseCliArgs(argc, argv);

    // Build the ns-3 LTE scenario (nodes, channels, mobility, devices).
    environment->SetupScenario();

    // Add X2 interface between all eNBs to enable handover signalling.
    environment->GetLteHelper()->AddX2Interface(environment->GetDeviceManager()->GetBsNodes());

    // Connect handover logging callbacks to all eNB and UE RRC state machines.
    // These are purely for logging — they do not affect the RL environment.
    Config::Connect("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverStart",
                    MakeCallback(&NotifyHandoverStartEnb));
    Config::Connect("/NodeList/*/DeviceList/*/LteUeRrc/HandoverStart",
                    MakeCallback(&NotifyHandoverStartUe));
    Config::Connect("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverEndOk",
                    MakeCallback(&NotifyHandoverEndOkEnb));
    Config::Connect("/NodeList/*/DeviceList/*/LteUeRrc/HandoverEndOk",
                    MakeCallback(&NotifyHandoverEndOkUe));

    std::cout << "[DEBUG] main: starting simulator" << std::endl;

    // Schedule the simulation to stop after the configured duration (20 s).
    Simulator::Stop(environment->GetSimulationDuration());

    // Run the discrete-event simulation loop.
    // The RL agent interacts via the OpenGym socket every 5 ms (step interval).
    Simulator::Run();

    // Notify ns3-ai that the episode is finished (sends terminal flag to Python).
    environment->NotifySimulationEnd();

    std::cout << "[DEBUG] main: simulation ended" << std::endl;

    return 0;
}

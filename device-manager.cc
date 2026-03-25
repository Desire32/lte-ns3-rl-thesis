#include "device-manager.h"
#include "ns3/epc-helper.h"
#include <algorithm>  // std::find, std::distance

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("DeviceManager");

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

// NodeContainer and NetDeviceContainer default-construct to empty — nothing
// to initialise explicitly.
DeviceManager::DeviceManager()
{
}

// All members are ns-3 smart-pointer containers — they clean up automatically.
DeviceManager::~DeviceManager()
{
}

// ---------------------------------------------------------------------------
// Setters — replace the internal container with the one provided by the caller.
// Called once during scenario construction (SetupScenario / CreateTopology).
// ---------------------------------------------------------------------------

/** Stores the complete set of UE (user terminal) nodes. */
void
DeviceManager::SetUtNodes(NodeContainer nodes)
{
    m_UtNodes = nodes;
}

/** Stores the complete set of BS (base station / eNB) nodes. */
void
DeviceManager::SetBsNodes(NodeContainer nodes)
{
    m_BsNodes = nodes;
}

/** Stores the LTE UE net devices (returned by LteHelper::InstallUeDevice). */
void
DeviceManager::SetUtDevices(NetDeviceContainer devices)
{
    m_UtDevices = devices;
}

/** Stores the LTE eNB net devices (returned by LteHelper::InstallEnbDevice). */
void
DeviceManager::SetBsDevices(NetDeviceContainer devices)
{
    m_BsDevices = devices;
}

// ---------------------------------------------------------------------------
// Getters — return non-const references so callers can iterate, index, and
// pass the containers directly to ns-3 helpers without copying.
// ---------------------------------------------------------------------------

/** Returns a reference to the full UE node container. */
NodeContainer&
DeviceManager::GetUtNodes()
{
    return m_UtNodes;
}

/** Returns a reference to the full BS node container. */
NodeContainer&
DeviceManager::GetBsNodes()
{
    return m_BsNodes;
}

/** Returns a reference to the full UE device container. */
NetDeviceContainer&
DeviceManager::GetUtDevices()
{
    return m_UtDevices;
}

/** Returns a reference to the full BS device container. */
NetDeviceContainer&
DeviceManager::GetBsDevices()
{
    return m_BsDevices;
}

// ---------------------------------------------------------------------------
// UE subset management
// ---------------------------------------------------------------------------

/**
 * Registers a named UE subset directly from a pre-built index vector.
 * This is the canonical implementation — the NodeContainer overload below
 * resolves to this after deriving the indices.
 *
 * Overwrites any existing subset with the same name.
 */
void
DeviceManager::AddUtSubset(std::string subsetName, std::vector<uint32_t> nodeIndices)
{
    m_UtSubsets[subsetName] = nodeIndices;
}

/**
 * Registers a named UE subset from a NodeContainer.
 * For each node in `nodes`, finds its position in m_UtNodes using
 * std::find + std::distance to derive the 0-based index, then delegates
 * to the index-vector overload.
 *
 * NS_ASSERT_MSG fires if a node is not found in m_UtNodes (index would
 * equal GetN(), i.e. past-the-end). Note: the assert message says "found"
 * but the intent is to catch the "NOT found" case — this is a bug in the
 * original message text, not in the logic.
 */
void
DeviceManager::AddUtSubset(std::string subsetName, NodeContainer nodes)
{
    std::vector<uint32_t> indices;
    for (auto it = nodes.Begin(); it != nodes.End(); it++)
    {
        // Search for the node pointer in the full UE container.
        auto element = std::find(m_UtNodes.Begin(), m_UtNodes.End(), *it);

        // std::distance gives a 0-based index; equals GetN() if not found.
        size_t index = std::distance(m_UtNodes.Begin(), element);

        // Guard: every node in the subset must exist in the full UT container.
        NS_ASSERT_MSG(index != m_UtNodes.GetN(), "Node " << *it << " found in manager.");

        indices.emplace_back(index);
    }
    AddUtSubset(subsetName, indices);
}

/**
 * Returns the raw index vector for the named UE subset.
 * Useful for cross-referencing with other index-based structures
 * such as sinrs[ueIndex] in the RL environment.
 */
std::vector<uint32_t>
DeviceManager::GetUtSubsetIndices(std::string subsetName)
{
    return m_UtSubsets[subsetName];
}

/**
 * Builds and returns a NodeContainer containing only the UE nodes
 * that belong to the named subset.
 * Iterates the stored index list and fetches each node from m_UtNodes.
 */
NodeContainer
DeviceManager::GetUtSubsetNodes(std::string subsetName)
{
    NodeContainer nodes;
    for (auto idx : m_UtSubsets[subsetName])
    {
        nodes.Add(m_UtNodes.Get(idx));
    }
    return nodes;
}

/**
 * Builds and returns a NetDeviceContainer containing only the UE devices
 * that belong to the named subset.
 * Uses the same index list as GetUtSubsetNodes — valid because of the
 * one-device-per-node assumption.
 */
NetDeviceContainer
DeviceManager::GetUtSubsetDevices(std::string subsetName)
{
    NetDeviceContainer devices;
    for (auto idx : m_UtSubsets[subsetName])
    {
        devices.Add(m_UtDevices.Get(idx));
    }
    return devices;
}

// ---------------------------------------------------------------------------
// BS subset management — mirrors the UE subset methods above
// ---------------------------------------------------------------------------

/**
 * Registers a named BS subset directly from a pre-built index vector.
 * Overwrites any existing subset with the same name.
 */
void
DeviceManager::AddBsSubset(std::string subsetName, std::vector<uint32_t> nodeIndices)
{
    m_BsSubsets[subsetName] = nodeIndices;
}

/**
 * Registers a named BS subset from a NodeContainer.
 * Derives 0-based indices by searching m_BsNodes, then delegates to the
 * index-vector overload. Same guard as the UE version.
 */
void
DeviceManager::AddBsSubset(std::string subsetName, NodeContainer nodes)
{
    std::vector<uint32_t> indices;
    for (auto it = nodes.Begin(); it != nodes.End(); it++)
    {
        // Search for the node pointer in the full BS container.
        auto element = std::find(m_BsNodes.Begin(), m_BsNodes.End(), *it);

        // Past-the-end distance means the node was not found.
        size_t index = std::distance(m_BsNodes.Begin(), element);

        // Guard: every node in the subset must exist in the full BS container.
        NS_ASSERT_MSG(index != m_BsNodes.GetN(), "Node " << *it << " found in manager.");

        indices.emplace_back(index);
    }
    AddBsSubset(subsetName, indices);
}

/** Returns the raw index vector for the named BS subset. */
std::vector<uint32_t>
DeviceManager::GetBsSubsetIndices(std::string subsetName)
{
    return m_BsSubsets[subsetName];
}

/**
 * Builds and returns a NodeContainer with only the BS nodes
 * in the named subset.
 */
NodeContainer
DeviceManager::GetBsSubsetNodes(std::string subsetName)
{
    NodeContainer nodes;
    for (auto idx : m_BsSubsets[subsetName])
    {
        nodes.Add(m_BsNodes.Get(idx));
    }
    return nodes;
}

/**
 * Builds and returns a NetDeviceContainer with only the BS devices
 * in the named subset.
 * Valid under the one-device-per-node assumption.
 */
NetDeviceContainer
DeviceManager::GetBsSubsetDevices(std::string subsetName)
{
    NetDeviceContainer devices;
    for (auto idx : m_BsSubsets[subsetName])
    {
        devices.Add(m_BsDevices.Get(idx));
    }
    return devices;
}

} // namespace ns3

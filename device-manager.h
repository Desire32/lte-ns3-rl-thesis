#ifndef DEFIANCE_DEVICE_MANAGER_H
#define DEFIANCE_DEVICE_MANAGER_H

// NodeContainer: ns-3 container that holds a collection of Ptr<Node>.
// Used to group BS and UE nodes for bulk operations (mobility install,
// device install, IP stack install, etc.).
#include "ns3/net-device-container.h"

// NetDeviceContainer: ns-3 container that holds a collection of Ptr<NetDevice>.
// Used to group LTE eNB/UE devices returned by the LTE helper after installation.
#include "ns3/node-container.h"

#include <map>

namespace ns3
{

// Forward declaration — DeviceManager is used inside MobComEnv but does not
// need to include the full MobComEnv header here (breaks circular dependency).
class MobComEnv;

/**
 * @ingroup defiance
 * @class DeviceManager
 *
 * @brief Owns and organises all ns-3 nodes and net devices in the simulation.
 *
 * Maintains two top-level groups:
 *   - UT (User Terminal / UE): mobile stations
 *   - BS (Base Station / eNB): fixed infrastructure nodes
 *
 * Additionally supports named subsets of each group so that different parts
 * of a multi-cell scenario (e.g. "cell0_ues", "edge_ues") can be addressed
 * independently without duplicating node/device containers.
 *
 * Assumption throughout: each node contains exactly one net device.
 * Subset indices therefore index both the node container and the device
 * container simultaneously.
 */
class DeviceManager
{
  public:
    DeviceManager();
    ~DeviceManager();

    // -----------------------------------------------------------------------
    // Setters — called once during scenario construction
    // -----------------------------------------------------------------------

    /** Stores the full set of UE (user terminal) nodes. */
    void SetUtNodes(NodeContainer nodes);

    /** Stores the full set of BS (base station / eNB) nodes. */
    void SetBsNodes(NodeContainer nodes);

    /**
     * Stores the LTE UE net devices returned by
     * LteHelper::InstallUeDevice(). Must match m_UtNodes order and count.
     */
    void SetUtDevices(NetDeviceContainer devices);

    /**
     * Stores the LTE eNB net devices returned by
     * LteHelper::InstallEnbDevice(). Must match m_BsNodes order and count.
     */
    void SetBsDevices(NetDeviceContainer devices);

    // -----------------------------------------------------------------------
    // Getters — return non-const references so callers can iterate and modify
    // -----------------------------------------------------------------------

    /**
     * Returns a reference to the full UE node container.
     * Callers can use .Get(i), .GetN(), .Begin(), .End() directly.
     */
    NodeContainer& GetUtNodes();

    /** Returns a reference to the full BS node container. */
    NodeContainer& GetBsNodes();

    /**
     * Returns a reference to the full UE device container.
     * Used to install IP stacks, assign EPC addresses, read PHY state, etc.
     */
    NetDeviceContainer& GetUtDevices();

    /** Returns a reference to the full BS device container. */
    NetDeviceContainer& GetBsDevices();

    // -----------------------------------------------------------------------
    // Named subset management — UE side
    // -----------------------------------------------------------------------

    /**
     * Registers a named subset of UEs identified by their 0-based indices
     * into m_UtNodes / m_UtDevices.
     *
     * Example: AddUtSubset("cell0_ues", {0, 1, 2}) groups the first 3 UEs.
     *
     * @param subsetName   unique string key for this subset
     * @param nodeIndices  indices into the full UT node/device containers
     */
    void AddUtSubset(std::string subsetName, std::vector<uint32_t> nodeIndices);

    /**
     * Convenience overload: registers a named UE subset by matching the
     * given NodeContainer against m_UtNodes to derive the index list.
     */
    void AddUtSubset(std::string subsetName, NodeContainer nodes);

    /**
     * Returns the raw index vector for the named UE subset.
     * Useful when you need to cross-reference with other indexed structures
     * (e.g. sinrs[ueIndex]).
     */
    std::vector<uint32_t> GetUtSubsetIndices(std::string subsetName);

    /**
     * Returns a NodeContainer holding only the nodes in the named UE subset.
     * Useful for bulk ns-3 operations (e.g. installing apps on a sub-group).
     */
    NodeContainer GetUtSubsetNodes(std::string subsetName);

    /**
     * Returns a NetDeviceContainer holding only the devices in the named
     * UE subset. Useful for EPC address assignment or PHY-level operations
     * on a specific group of UEs.
     */
    NetDeviceContainer GetUtSubsetDevices(std::string subsetName);

    // -----------------------------------------------------------------------
    // Named subset management — BS side
    // -----------------------------------------------------------------------

    /**
     * Registers a named subset of BSs by 0-based index into m_BsNodes /
     * m_BsDevices.
     *
     * Example: AddBsSubset("sector_a", {0, 2, 4}) for every other eNB.
     */
    void AddBsSubset(std::string subsetName, std::vector<uint32_t> nodeIndices);

    /** Convenience overload: derives indices by matching nodes in m_BsNodes. */
    void AddBsSubset(std::string subsetName, NodeContainer nodes);

    /** Returns the raw index vector for the named BS subset. */
    std::vector<uint32_t> GetBsSubsetIndices(std::string subsetName);

    /** Returns a NodeContainer for the named BS subset. */
    NodeContainer GetBsSubsetNodes(std::string subsetName);

    /** Returns a NetDeviceContainer for the named BS subset. */
    NetDeviceContainer GetBsSubsetDevices(std::string subsetName);

  private:
    // -----------------------------------------------------------------------
    // Full node and device containers
    // -----------------------------------------------------------------------

    /** All UE nodes in the simulation (indexed 0 … N_ue-1). */
    NodeContainer m_UtNodes;

    /** All BS / eNB nodes in the simulation (indexed 0 … N_bs-1). */
    NodeContainer m_BsNodes;

    /** LTE UE net devices — parallel to m_UtNodes (same order and count). */
    NetDeviceContainer m_UtDevices;

    /** LTE eNB net devices — parallel to m_BsNodes (same order and count). */
    NetDeviceContainer m_BsDevices;

    // -----------------------------------------------------------------------
    // Named subset index maps
    // -----------------------------------------------------------------------

    /**
     * Named subsets of UE indices.
     * Key:   user-defined subset name (e.g. "cell0_ues", "edge_ues")
     * Value: vector of 0-based indices into m_UtNodes / m_UtDevices.
     *
     * Assumption: one device per node, so the same index addresses both
     * m_UtNodes.Get(i) and m_UtDevices.Get(i).
     */
    std::map<std::string, std::vector<uint32_t>> m_UtSubsets;

    /**
     * Named subsets of BS indices.
     * Key:   user-defined subset name (e.g. "sector_a", "macro_cells")
     * Value: vector of 0-based indices into m_BsNodes / m_BsDevices.
     *
     * Same one-device-per-node assumption applies.
     */
    std::map<std::string, std::vector<uint32_t>> m_BsSubsets;

}; // class DeviceManager

} // namespace ns3

#endif /* DEFIANCE__DEVICE_MANAGER_H */

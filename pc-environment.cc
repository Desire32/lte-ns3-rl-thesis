#include "pc-environment.h"

#include "ns3/constant-velocity-mobility-model.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("PcMobComEnv");

// ===========================================================================
// Topology construction methods
// ===========================================================================

/**
 * Top-level topology dispatcher. Reads arg_topology and delegates to the
 * appropriate factory method. Also attaches a RangePropagationLossModel
 * (hard cutoff at 250 m) to the downlink spectrum channel after the
 * topology is built — applied to ALL topologies.
 *
 * Supported values of arg_topology:
 *   "simple"   — 1 BS at origin, 1 moving UE (CreateSimpleTopology)
 *   "random"   — 1 BS, 1 UE placed randomly in a 1000 m area (CreateRandomFixedTopology)
 *   "multi_bs" — 2 BSs, 4 UEs at fixed positions (CreateMultiBsTopology)
 */
void
PowerControlMobComEnv::CreateTopology()
{
    NS_LOG_FUNCTION(this);

    if (arg_topology == "simple")
    {
        // Single BS at origin, UE starts at (0,0,1) and moves with arg_speed along x-axis.
        CreateSimpleTopology(Vector(0, 0, 1), Vector(arg_speed, 0, 0));
    }
    else if (arg_topology == "random")
    {
        // 1 BS, 1 UE, random UE placement within a 1000 m line.
        CreateRandomFixedTopology(1, 1, 1000);
    }
    else if (arg_topology == "multi_bs") // 2 eNB-UE pairs
    {
        CreateMultiBsTopology();
    }

    // Hard range limit: any UE farther than 250 m from a BS receives no signal.
    // This prevents unrealistic long-range interference in small scenarios.
    GetLteHelper()->GetDownlinkSpectrumChannel()->AddPropagationLossModel(
        CreateObjectWithAttributes<RangePropagationLossModel>(
            "MaxRange", DoubleValue(250)));
}

// ---------------------------------------------------------------------------

/**
 * Places numBSs eNBs in a straight line along the x-axis.
 * BSs are equally spaced so that they partition [0, lineLength] uniformly.
 *
 * Visual layout (e.g. 3 BSs, lineLength=1000):
 *   (0,0) ---[BS0 @250]--- [BS1 @500] --- [BS2 @750]--- (1000,0)
 *
 * All BSs use ConstantPositionMobilityModel (stationary).
 * Installs LTE eNB devices via the LTE helper and registers them
 * in the DeviceManager.
 *
 * @param numBSs      number of base stations to create
 * @param lineLength  total length of the deployment area in metres
 */
void
PowerControlMobComEnv::CreateBsLine(uint16_t numBSs, uint64_t lineLength)
{
    NS_LOG_FUNCTION(this);
    auto deviceManager = GetDeviceManager();

    NodeContainer enbNodes;
    enbNodes.Create(numBSs);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    auto bsPositionAlloc = CreateObject<ListPositionAllocator>();

    // Distribute BSs so that spacing = lineLength / (numBSs + 1).
    // e.g. 2 BSs in 1000 m → positions 333 m and 667 m.
    int currentPosition = 0;
    for (int i = 0; i < numBSs; i++)
    {
        currentPosition += float(lineLength) / (numBSs + 1);
        bsPositionAlloc->Add(Vector(currentPosition, 0, 1)); // z=1 m (antenna height)
        std::cout << "BS placed at: " << Vector(currentPosition, 0, 1) << std::endl;
    }

    mobility.SetPositionAllocator(bsPositionAlloc);
    mobility.Install(enbNodes);

    // Register nodes and install LTE eNB devices.
    deviceManager->SetBsNodes(enbNodes);
    deviceManager->SetBsDevices(m_lteHelper->InstallEnbDevice(deviceManager->GetBsNodes()));
}

// ---------------------------------------------------------------------------

/**
 * Builds a scenario with BSs along the x-axis and UEs placed randomly
 * in a rectangular area of lineLength × lineLength metres.
 *
 * BS layout: delegates to CreateBsLine (equally spaced along x-axis).
 * UE layout: uniform random x ∈ [0, lineLength], y ∈ [-lineLength/2, lineLength/2].
 * All nodes are stationary (ConstantPositionMobilityModel).
 *
 * @param numUEs      number of user equipment nodes to create
 * @param numBSs      number of base stations (passed to CreateBsLine)
 * @param lineLength  side length of the deployment area in metres
 */
void
PowerControlMobComEnv::CreateRandomFixedTopology(uint16_t numUEs,
                                                 uint16_t numBSs,
                                                 uint64_t lineLength)
{
    NS_LOG_FUNCTION(this);
    auto deviceManager = GetDeviceManager();

    // Create and position BSs first.
    CreateBsLine(numBSs, lineLength);

    NodeContainer ueNodes;
    ueNodes.Create(numUEs);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // Independent uniform random variables for x and y coordinates.
    auto x = CreateObject<UniformRandomVariable>();
    x->SetAttribute("Min", DoubleValue(0));
    x->SetAttribute("Max", DoubleValue(lineLength));

    auto y = CreateObject<UniformRandomVariable>();
    y->SetAttribute("Min", DoubleValue(-lineLength / 2));
    y->SetAttribute("Max", DoubleValue(lineLength / 2));

    auto uePositionAlloc = CreateObject<ListPositionAllocator>();
    for (uint16_t i = 0; i < numUEs; i++)
    {
        auto xCoordinate = x->GetValue();
        auto yCoordinate = y->GetValue();
        uePositionAlloc->Add(Vector(xCoordinate, yCoordinate, 1)); // z=1 m
        std::cout << "UE placed at: " << Vector(xCoordinate, yCoordinate, 1) << std::endl;
    }

    mobility.SetPositionAllocator(uePositionAlloc);
    mobility.Install(ueNodes);

    // Register nodes and install LTE UE devices.
    deviceManager->SetUtNodes(ueNodes);
    deviceManager->SetUtDevices(m_lteHelper->InstallUeDevice(deviceManager->GetUtNodes()));
}

// ---------------------------------------------------------------------------

/**
 * Minimal single-cell scenario: 1 BS at the origin, 1 UE at `position`
 * moving with constant `velocity`.
 *
 * BS: ConstantPositionMobilityModel at (0, 0, 1).
 * UE: ConstantVelocityMobilityModel — set velocity to (0,0,0) for a static UE.
 *
 * Used for single-agent baseline experiments and quick debugging.
 *
 * @param position  initial 3-D position of the UE (metres)
 * @param velocity  constant velocity vector of the UE (m/s)
 */
void
PowerControlMobComEnv::CreateSimpleTopology(Vector position, Vector velocity)
{
    NS_LOG_FUNCTION(this);
    auto deviceManager = GetDeviceManager();

    // --- Base station ---
    NodeContainer enbNodes;
    enbNodes.Create(1);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    auto bsPositionAlloc = CreateObject<ListPositionAllocator>();
    bsPositionAlloc->Add(Vector(0, 0, 1));
    mobility.SetPositionAllocator(bsPositionAlloc);
    mobility.Install(enbNodes);

    deviceManager->SetBsNodes(enbNodes);
    deviceManager->SetBsDevices(m_lteHelper->InstallEnbDevice(deviceManager->GetBsNodes()));

    // --- User equipment ---
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Switch to ConstantVelocityMobilityModel so we can set a non-zero velocity.
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    auto uePositionAlloc = CreateObject<ListPositionAllocator>();
    uePositionAlloc->Add(position);
    mobility.SetPositionAllocator(uePositionAlloc);
    mobility.Install(ueNodes);

    // Apply the velocity vector to the UE after mobility is installed.
    ueNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(velocity);

    deviceManager->SetUtNodes(ueNodes);
    deviceManager->SetUtDevices(m_lteHelper->InstallUeDevice(deviceManager->GetUtNodes()));
}

// ---------------------------------------------------------------------------

/**
 * Two-cell static scenario for multi-BS power control experiments.
 *
 * Layout (all z=1 m):
 *   BS0 @ (0, 0)       BS1 @ (1000, 0)
 *   UE0 @ (0, 0)    — moves at 100 m/s along x-axis (crosses cell boundary)
 *   UE1 @ (0, 250)  — stationary, near BS0
 *   UE2 @ (1000, 500) — stationary, near BS1
 *   UE3 @ (1000, 250) — stationary, near BS1
 *
 * UE0 is the only mobile node; its trajectory takes it from BS0 toward BS1,
 * making it useful for testing inter-cell interference and handover.
 */
void
PowerControlMobComEnv::CreateMultiBsTopology()
{
    NS_LOG_FUNCTION(this);
    auto deviceManager = GetDeviceManager();

    // --- Base stations ---
    NodeContainer enbNodes;
    enbNodes.Create(2);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    auto bsPositionAlloc = CreateObject<ListPositionAllocator>();
    bsPositionAlloc->Add(Vector(0,    0, 1)); // BS0: left cell
    bsPositionAlloc->Add(Vector(1000, 0, 1)); // BS1: right cell, 1 km away
    mobility.SetPositionAllocator(bsPositionAlloc);
    mobility.Install(enbNodes);

    deviceManager->SetBsNodes(enbNodes);
    deviceManager->SetBsDevices(m_lteHelper->InstallEnbDevice(deviceManager->GetBsNodes()));

    // --- User equipment ---
    NodeContainer ueNodes;
    ueNodes.Create(4);

    // All UEs use ConstantVelocityMobilityModel; stationary ones get (0,0,0).
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    auto uePositionAlloc = CreateObject<ListPositionAllocator>();
    uePositionAlloc->Add(Vector(0,    0,   1)); // UE0: starts at BS0
    uePositionAlloc->Add(Vector(0,    250, 1)); // UE1: near BS0, offset in y
    uePositionAlloc->Add(Vector(1000, 500, 1)); // UE2: near BS1, farther in y
    uePositionAlloc->Add(Vector(1000, 250, 1)); // UE3: near BS1
    mobility.SetPositionAllocator(uePositionAlloc);
    mobility.Install(ueNodes);

    // UE0 moves toward BS1 at 100 m/s — will cross the cell boundary mid-sim.
    ueNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(100, 0, 0));
    // UE1–3 are stationary.
    ueNodes.Get(1)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(0, 0, 0));
    ueNodes.Get(2)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(0, 0, 0));
    ueNodes.Get(3)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(0, 0, 0));

    deviceManager->SetUtNodes(ueNodes);
    deviceManager->SetUtDevices(m_lteHelper->InstallUeDevice(deviceManager->GetUtNodes()));
}

// ===========================================================================
// Traffic installation
// ===========================================================================
void
PowerControlMobComEnv::AddTraffic()
{
    NS_LOG_FUNCTION(this);
    auto device_manager = GetDeviceManager();

    // PGW is the EPC gateway — all UE traffic is routed through it.
    Ptr<Node> pgw = m_epcHelper->GetPgwNode();

    // --- Remote host (traffic source/sink outside the LTE network) ---
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // High-capacity point-to-point link between PGW and remote host.
    // 100 Gb/s data rate and 1500-byte MTU ensure the link is never the bottleneck.
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu",      UintegerValue(1500));
    p2ph.SetChannelAttribute("Delay",   TimeValue(MilliSeconds(1)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    // Assign IP addresses on the P2P link (1.0.0.x/8).
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1); // remoteHost = index 1

    // Add a static route on the remote host so it knows how to reach UEs (7.0.0.0/8)
    // via interface 1 (the P2P link toward the PGW).
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(
        Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    NodeContainer&    ueNodes   = device_manager->GetUtNodes();
    NetDeviceContainer& ueDevices = device_manager->GetUtDevices();

    // Install IP stack on all UEs.
    internet.Install(ueNodes);

    // Assign UE IP addresses from the EPC address pool (7.0.0.0/8).
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = m_epcHelper->AssignUeIpv4Address(ueDevices);

    // Set the default gateway on each UE to the EPC's UE-side gateway address
    // so all UE traffic is routed through the EPC toward the remote host.
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Node> ueNode = ueNodes.Get(u);
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(m_epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Attach each UE to its nearest eNB (initial RRC connection + EPS bearer setup).
    AttachUEsToEnbs();

    // --- Install UDP applications on UEs and remote host ---
    uint16_t dlPort = 1100; // shared downlink port (one sink per UE node)
    uint16_t ulPort = 2000; // uplink ports incremented per UE to avoid collisions

    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        // 1 packet every 1 ms = ~8 kbps per flow at 1000-byte packets.
        Time interPacketInterval = MilliSeconds(1);

        // --- Downlink: remoteHost → UE ---
        // Sink on the UE listens on dlPort.
        PacketSinkHelper dlPacketSinkHelper(
            "ns3::UdpSocketFactory",
            InetSocketAddress(Ipv4Address::GetAny(), dlPort));
        serverApps.Add(dlPacketSinkHelper.Install(ueNodes.Get(u)));

        // Source on the remote host sends to the UE's IP on dlPort.
        UdpClientHelper dlClient(ueIpIface.GetAddress(u), dlPort);
        dlClient.SetAttribute("Interval",   TimeValue(interPacketInterval));
        dlClient.SetAttribute("MaxPackets", UintegerValue(1000000));
        clientApps.Add(dlClient.Install(remoteHost));

        // --- Uplink: UE → remoteHost ---
        ++ulPort; // each UE uses a unique uplink port to separate flows
        PacketSinkHelper ulPacketSinkHelper(
            "ns3::UdpSocketFactory",
            InetSocketAddress(Ipv4Address::GetAny(), ulPort));
        serverApps.Add(ulPacketSinkHelper.Install(remoteHost));

        UdpClientHelper ulClient(remoteHostAddr, ulPort);
        ulClient.SetAttribute("Interval",   TimeValue(interPacketInterval));
        ulClient.SetAttribute("MaxPackets", UintegerValue(1000000));
        clientApps.Add(ulClient.Install(ueNodes.Get(u)));
    }

    // Start all sinks and sources at t=100 ms.
    // The 100 ms head-start gives LTE RRC connection and bearer setup time to complete
    // before the first data packet is sent.
    serverApps.Start(MilliSeconds(100));
    clientApps.Start(MilliSeconds(100));
}

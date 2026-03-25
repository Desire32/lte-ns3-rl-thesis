// base-environment.cc
// A foundation class of everything.

// MobComEnv is the constructor which initialises DeviceManager(which is container for UE and eNB nodes),
// also is the bridge between the simulation and the AI Gym interface.

/*
 * LteHelper + EpcHelper are being created
        ↓
 SetScenarioAttributes()  ← parameters (propagation model, noise)
        ↓
 ScheduleNotificationEvents()  ← a schedule when to send observations to Python
        ↓
 CreateTopology()  ← we place eNB and UE nodes on the map
        ↓
 AddTraffic()  ← UDP traffic between UE and remote host
        ↓
 RegisterCallbacks()  ← we subscribe to SINR traces
 *
 */


#include "base-environment.h"

#include "ns3/ns3-ai-gym-interface.h"
#include "ns3/point-to-point-epc-helper.h"

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("lte-base-environment");

TypeId
MobComEnv::GetTypeId()
{
    static TypeId tid = TypeId("ns3::MobComEnv").SetParent<OpenGymEnv>().SetGroupName("OpenGym");
    return tid;
}

MobComEnv::MobComEnv()
    : MobComEnv(Seconds(5), MilliSeconds(5), MilliSeconds(0))
{
}

MobComEnv::MobComEnv(Time simulationDuration, Time notificationRate, Time notificationStart)
    : m_simulationDuration(simulationDuration),
      m_notificationRate(notificationRate),
      m_notificationStart(notificationStart)
{
    m_deviceManager = new DeviceManager();
    SetOpenGymInterface(OpenGymInterface::Get());
}

void
MobComEnv::SetupScenario()
{
    NS_LOG_FUNCTION(this);
    // create the helper objects required for every lte simulation
    m_lteHelper = CreateObject<LteHelper>();
    m_epcHelper = CreateObject<PointToPointEpcHelper>();
    m_lteHelper->SetEpcHelper(m_epcHelper);
    SetScenarioAttributes();

    // set scheduling logic for communication between c++ and python
    ScheduleNotificationEvents();

    // sets the overall network topology
    CreateTopology();
    AddTraffic();
    RegisterCallbacks();
}


////////////////////////////////////////////////////////////////////////////////////////////////
/// [ScheduleNotificationEvents]
// The most important function in the class - schedules notification events to send observations to Python.
// every m_notificationRate, the Notify() function is called to send the current state to Python.
//
// Notify() is the method of OpenGymEnv which calls GetObservation() -> waits an action -> calls ExecuteActions() -> applies tx power.


/* This is our RL Loop right there
 * ns-3 time tick
     → Notify()
         → GetObservation() → Python sees SINR
         → Python counts action (TX power)
         → ExecuteActions() → eNB changes tx power
     → next tick in 5ms
 */
////////////////////////////////////////////////////////////////////////////////////////////////
void
MobComEnv::ScheduleNotificationEvents()
{
    NS_LOG_FUNCTION(this);
    Time cur = m_notificationStart;
    while (cur < m_simulationDuration)
    {
        Simulator::Schedule(cur, &MobComEnv::Notify, this);
        cur += m_notificationRate;
    }
}

void
MobComEnv::AttachUEsToEnbs()
{
    NS_LOG_FUNCTION(this);
    GetLteHelper()->AttachToClosestEnb(m_deviceManager->GetUtDevices(),
                                       m_deviceManager->GetBsDevices());
}

Ptr<LteHelper>
MobComEnv::GetLteHelper()
{
    return m_lteHelper;
}

Ptr<EpcHelper>
MobComEnv::GetEpcHelper()
{
    return m_epcHelper;
}

DeviceManager*
MobComEnv::GetDeviceManager()
{
    return m_deviceManager;
}

Time
MobComEnv::GetSimulationDuration()
{
    return m_simulationDuration;
}

} // namespace ns3

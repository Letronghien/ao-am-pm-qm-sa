/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Ad hoc network, random waypoint, CBR flows from every node to node 0.
// ./ns3 run "manet-routing-example --protocol=QMAODV --nNodes=20"
// protocol: AODV | AOMDV | PMAODV | QMAODV | SA-QMAODV

#include "ns3/aodv-module.h"
#include "ns3/aomdv-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mpaodv-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    std::string protocol = "QMAODV";
    uint32_t nNodes = 20;
    uint32_t maxPaths = 3;
    double area = 1000.0;
    double range = 250.0;
    double rate = 4.0;
    double simTime = 200.0;
    uint32_t run = 1;

    CommandLine cmd(__FILE__);
    cmd.AddValue("protocol", "AODV, AOMDV, PMAODV, QMAODV or SA-QMAODV", protocol);
    cmd.AddValue("nNodes", "number of nodes", nNodes);
    cmd.AddValue("maxPaths", "PMAODV: maximum number of routes (2, 3, 4)", maxPaths);
    cmd.AddValue("area", "side of the square area (m)", area);
    cmd.AddValue("range", "radio range (m)", range);
    cmd.AddValue("rate", "packets/s per node", rate);
    cmd.AddValue("simTime", "simulation time (s)", simTime);
    cmd.AddValue("run", "RNG run number", run);
    cmd.Parse(argc, argv);
    RngSeedManager::SetRun(run);

    NodeContainer nodes;
    nodes.Create(nNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("DsssRate2Mbps"),
                                 "ControlMode",
                                 StringValue("DsssRate1Mbps"));
    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(range));
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    ObjectFactory pos;
    pos.SetTypeId("ns3::RandomRectanglePositionAllocator");
    std::ostringstream u;
    u << "ns3::UniformRandomVariable[Min=0.0|Max=" << area << "]";
    pos.Set("X", StringValue(u.str()));
    pos.Set("Y", StringValue(u.str()));
    Ptr<PositionAllocator> alloc = pos.Create()->GetObject<PositionAllocator>();
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed",
                              StringValue("ns3::UniformRandomVariable[Min=5.0|Max=20.0]"),
                              "Pause",
                              StringValue("ns3::ConstantRandomVariable[Constant=0.0]"),
                              "PositionAllocator",
                              PointerValue(alloc));
    mobility.SetPositionAllocator(alloc);
    mobility.Install(nodes);

    InternetStackHelper stack;
    AodvHelper aodv;
    AomdvHelper aomdv;
    PmaodvHelper pmaodv(maxPaths);
    QmaodvHelper qmaodv;
    SaQmaodvHelper saqmaodv;
    if (protocol == "AODV")
    {
        stack.SetRoutingHelper(aodv);
    }
    else if (protocol == "AOMDV")
    {
        stack.SetRoutingHelper(aomdv);
    }
    else if (protocol == "PMAODV")
    {
        stack.SetRoutingHelper(pmaodv);
    }
    else if (protocol == "QMAODV")
    {
        stack.SetRoutingHelper(qmaodv);
    }
    else if (protocol == "SA-QMAODV")
    {
        stack.SetRoutingHelper(saqmaodv);
    }
    else
    {
        NS_FATAL_ERROR("Unknown protocol " << protocol);
    }
    stack.Install(nodes);
    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer ifs = address.Assign(devices);

    const uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer sink = server.Install(nodes.Get(0));
    sink.Start(Seconds(0.0));
    sink.Stop(Seconds(simTime));
    for (uint32_t i = 1; i < nNodes; ++i)
    {
        UdpClientHelper client(ifs.GetAddress(0), port);
        client.SetAttribute("Interval", TimeValue(Seconds(1.0 / rate)));
        client.SetAttribute("PacketSize", UintegerValue(512));
        client.SetAttribute("MaxPackets", UintegerValue(0));
        ApplicationContainer app = client.Install(nodes.Get(i));
        app.Start(Seconds(1.0 + 0.01 * i));
        app.Stop(Seconds(simTime));
    }

    FlowMonitorHelper fmh;
    Ptr<FlowMonitor> fm = fmh.InstallAll();
    Simulator::Stop(Seconds(simTime + 1.0));
    Simulator::Run();

    fm->CheckForLostPackets();
    uint64_t tx = 0;
    uint64_t rx = 0;
    double delay = 0.0;
    auto classifier = DynamicCast<Ipv4FlowClassifier>(fmh.GetClassifier());
    for (const auto& [id, st] : fm->GetFlowStats())
    {
        if (classifier->FindFlow(id).destinationPort != port)
        {
            continue;
        }
        tx += st.txPackets;
        rx += st.rxPackets;
        delay += st.delaySum.GetSeconds();
    }
    std::cout << "protocol=" << protocol << " nodes=" << nNodes << " tx=" << tx << " rx=" << rx
              << " pdr=" << (tx ? 100.0 * rx / tx : 0.0)
              << "% delayMs=" << (rx ? 1000.0 * delay / rx : 0.0) << std::endl;
    Simulator::Destroy();
    return 0;
}

/*******************************************************************************
 ** BonnMotion - a mobility scenario generation and analysis tool             **
 ** ns3-Example								      **
 ** Copyright (C) 2002-2010 University of Bonn                                **
 ** Code: R. Ernst		                                              **
 **                                                                           **
 ** This program is free software; you can redistribute it and/or modify      **
 ** it under the terms of the GNU General Public License as published by      **
 ** the Free Software Foundation; either version 2 of the License, or         **
 ** (at your option) any later version.                                       **
 **                                                                           **
 ** This program is distributed in the hope that it will be useful,           **
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of            **
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             **
 ** GNU General Public License for more details.                              **
 **                                                                           **
 ** You should have received a copy of the GNU General Public License         **
 ** along with this program; if not, write to the Free Software               **
 ** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA **
 *******************************************************************************/

/*
 * This example demonstrates the use of BonnMotion generated traces within ns3.
 * 
 * 1. Generate movement, e.g. 'bm -f example RandomWaypoint -i 0 -n 1 -d 100'
 * 2. Convert into ns2 file format bm 'NSFile -f example'
 * 3. Run ns-3 './waf --run "scratch/ns3movement --traceFile=/path/to/example/example.ns_movements" 
 * 
 * The script is based on M. Giachino's example from ns-3.10 which can be found
 * in the ns-3.10 source examples/mobility/ns2-mobility-trace.cc
 *
*/

#include "ns3/core-module.h"
#include "ns3/ns2-mobility-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"

#include "ns3/ndnSIM-module.h"

using namespace ns3;
using namespace ns3::ndn;

static void
SetPosition (Ptr<Node> node, Vector position)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  mobility->SetPosition (position);
}

static Vector
GetPosition (Ptr<Node> node)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  return mobility->GetPosition ();
}

static void
GridPosition (int n, int spacing, int offset, NodeContainer nodes)
{
    Vector pos;
    Ptr<Node> node;
    int x, y, i, rank;
    rank = sqrt(n);
    for (i=0; i < n; i++){
        x = i / rank;
        y = i % rank;
        node = nodes.Get (i);
        pos.x = x * spacing + offset;
        pos.y = y * spacing + offset;
        SetPosition (node, pos);
    }
}

int main(int argc, char* argv[]) {
	std::string traceFile;

	int nodeNum = 2;
    int i;
	double duration = 100;
    NodeContainer::Iterator nodei;

	CommandLine cmd;
	cmd.AddValue("traceFile","NS2 movement trace file",traceFile);
	cmd.Parse(argc,argv);

	if(traceFile.empty()) {
		std::cout << "Usage of " << argv[0] << ":\n" << "./waf --run \"script --traceFile=/path/to/tracefile\"\n";
	}

    WifiHelper wifi = WifiHelper::Default ();
    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());
    Ssid ssid = Ssid ("wifi-default");
    wifi.SetRemoteStationManager ("ns3::ArfWifiManager");

    // Install mobile stations
    MobilityHelper mobility;
	Ns2MobilityHelper ns2mobility = Ns2MobilityHelper(traceFile);

	NodeContainer nodes;
	nodes.Create(nodeNum);

	ns2mobility.Install();
    //mobility.Install(nodes);

    for (i = 0; i < nodeNum; i++)
    {
        Names::Add("car-" + boost::lexical_cast<std::string> (i), nodes.Get (i));
    }
	
    wifiMac.SetType ("ns3::StaWifiMac",
                     "Ssid", SsidValue (ssid),
                     "ActiveProbing", BooleanValue (false));
    wifiPhy.Set ("TxPowerStart", DoubleValue (0.0));
    wifiPhy.Set ("TxPowerEnd", DoubleValue (0.0));
    wifi.Install (wifiPhy, wifiMac, nodes);

    // Install APs
    int nAps = 11 * 11;
    NodeContainer aps;
    aps.Create (nAps);
    mobility.Install(aps);

    GridPosition(nAps, 100, 0, aps);

    wifiMac.SetType ("ns3::ApWifiMac",
                     "Ssid", SsidValue (ssid));
    wifiPhy.Set ("TxPowerStart", DoubleValue (0.0));
    wifiPhy.Set ("TxPowerEnd", DoubleValue (0.0));
    wifi.Install (wifiPhy, wifiMac, aps);

    // Install routers
    int nRouters = 10 * 10;
    NodeContainer routers;
    routers.Create (nRouters);
    mobility.Install(routers);

    GridPosition(nRouters, 100, 50, routers);

    // Install producers
    PointToPointHelper pt;
    pt.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    pt.SetChannelAttribute("Delay", StringValue("2ms"));

    NodeContainer producers;
    producers.Create (1);
    mobility.Install(producers);
    Names::Add("producer-0", producers.Get (0));
    Vector pos;
    pos.x = 500;
    pos.y = 500;
    SetPosition(producers.Get (0), pos);

    for (nodei = routers.Begin (); nodei != routers.End (); ++nodei)
    {
        pt.Install(*nodei, producers.Get (0));
    }

    Ptr<Node> router;
    for (i = 0; i < nRouters; i++)
    {
        router = routers.Get(i);
        pt.Install (router, aps.Get (i + i / 10));
        pt.Install (router, aps.Get (i + i / 10 + 1));
        pt.Install (router, aps.Get (i + i / 10 + 11));
        pt.Install (router, aps.Get (i + i / 10 + 11 + 1));
    }

    // NDN setup
    ns3::ndn::StackHelper ndnHelper;
    ndnHelper.SetOldContentStore("ns3::ndn::cs::Lru", "MaxSize", "1000");
    ndnHelper.SetDefaultRoutes(true);
    ndnHelper.Install(nodes);
    ndnHelper.Install(aps);
    ndnHelper.Install(routers);
    ndnHelper.Install(producers);
  
    ns3::ndn::StrategyChoiceHelper::Install(routers, "/", "/localhost/nfd/strategy/best-route");
  
    ns3::ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
    consumerHelper.SetPrefix("/test/prefix");
    consumerHelper.SetAttribute("Frequency", DoubleValue (10));
    consumerHelper.Install(nodes);
  
    ns3::ndn::AppHelper producerHelper("ns3::ndn::Producer");
    producerHelper.SetPrefix("/");
    producerHelper.SetAttribute("PayloadSize", StringValue ("1200"));
    producerHelper.Install(producers);

	Simulator::Stop(Seconds(duration));

    L3RateTracer::Install(NodeContainer(nodes, producers), "nodes-trace.txt", Seconds (0.5));
    AppDelayTracer::Install(NodeContainer(nodes), "app-trace.txt");

	Simulator::Run();
	Simulator::Destroy();

	return 0;
}

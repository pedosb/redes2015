#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ns2-mobility-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/wifi-module.h"

#include "model/ndn-net-device-face.hpp"

using namespace ns3;
using namespace ns3::ndn;

    static void
SetPosition (Ptr<Node> node, Vector position)
{
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
    mobility->SetPosition (position);
}

class NodeMat: public NodeContainer
{
    public:
        int x;
        int y;
        NodeMat(int x, int y) : NodeContainer()
    {
        this->x = x;
        this->y = y;
        this->Create(x * y);
    }
        Ptr<Node> Get(int x, int y)
        {
            return NodeContainer::Get(x * this->x + y);
        }
        void GridPositioning(int spacing, int offset)
        {
            int i, j;
            for (i = 0; i < this->x; i++)
            {
                for (j = 0; j < this->y; j++)
                {
                    SetPosition(this->Get(i, j),
                            Vector(i * spacing + offset,
                                j * spacing + offset,
                                0));
                }
            }
        }
};

int main (int argc, char *argv[])
{
    std::string traceFilePrefix;
    std::string cacheSize;

    CommandLine cmd;
    cmd.AddValue("tracePrefix", "Simulation trace files prefix", traceFilePrefix);
    cmd.AddValue("cacheSize", "Cache size of access point nodes", cacheSize);
    cmd.Parse (argc, argv);

    if (traceFilePrefix.empty () || cacheSize.empty ())
        cmd.PrintHelp(std::cout);

    int i, j;
    int nStreets = 11;
    int apSpacing = 100;
    int nCars = 2;

    NodeContainer cars;
    cars.Create(nCars);
    Ns2MobilityHelper ns2mobility = Ns2MobilityHelper(
            "../bonnmotion-2.1.3/man.ns_movements");
    //ns2mobility.Install();

    for (i = 0; i < nCars; i++)
    {
        Names::Add("car-" + boost::lexical_cast<std::string> (i), cars.Get (i));
    }

    NodeMat aps = NodeMat (nStreets, nStreets);
    NodeContainer producers;
    producers.Create (1);
    Ptr<Node> producer = producers.Get (0);
    Names::Add ("producer-0", producer);

    MobilityHelper mobility;
    mobility.Install (aps);
    mobility.Install (producers);
    mobility.Install (cars);

    SetPosition (cars.Get (0), Vector (0, 980, 0));

    SetPosition (producer,
        Vector ((nStreets / 2) * apSpacing + (apSpacing / 2), -100, 0));
    aps.GridPositioning(apSpacing, 0);

    PointToPointHelper p2p;

    p2p.SetChannelAttribute("Delay", TimeValue(Seconds(80e-3)));
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.Install(aps.Get(nStreets / 2, nStreets / 2), producer);

    p2p.SetChannelAttribute("Delay", TimeValue(Seconds(3.3e-9 * apSpacing)));
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    Ptr<Node> router;

    WifiHelper wifi = WifiHelper::Default();
    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    Ssid ssid = Ssid("wifi-default");

    wifiPhy.SetChannel (wifiChannel.Create ());
    wifiPhy.Set ("TxPowerStart", DoubleValue (0));
    wifiPhy.Set ("TxPowerEnd", DoubleValue (0));
    wifiPhy.Set ("TxPowerLevels", UintegerValue (1));

    wifiMac.SetType ("ns3::ApWifiMac",
                     "Ssid", SsidValue (ssid));
    wifi.Install (wifiPhy, wifiMac, aps);

    wifiMac.SetType ("ns3::StaWifiMac",
                     "Ssid", SsidValue (ssid));
    wifi.Install (wifiPhy, wifiMac, cars);

    StackHelper ndnHelper;

    for (i = 0; i < 11; i++)
    {
        for (j = 0; j < 11; j++)
        {
            router = aps.Get (i, j);
            Names::Add("ap-" + boost::lexical_cast<std::string> (i) + "-" + boost::lexical_cast<std::string> (j), router);
            // Install left and up links on center (even) nodes
            if (i % 2 != 0 && j % 2 != 0)
            {
                p2p.Install (router, aps.Get (i - 1, j));
                p2p.Install (router, aps.Get (i, j - 1));
                // If nStreets is even install right and down links on the last
                // router
                if (nStreets % 2 != 0)
                {
                    if (i == nStreets - 2)
                        p2p.Install (router, aps.Get (i + 1, j));
                    if (j == nStreets - 2)
                        p2p.Install (router, aps.Get (i, j + 1));
                }
            } else if (i % 2 == 0 && j % 2 != 0)
            {
                p2p.Install (router, aps.Get (i, j - 1));
                if (j == nStreets - 2)
                    p2p.Install (router, aps.Get (i, j + 1));
                if (i != 0 && i != nStreets - 1)
                    p2p.Install (router, aps.Get (i - 1, j));
            } else if (i % 2 != 0 && j % 2 == 0 && j != 0 && j != nStreets - 1)
            {
                p2p.Install (router, aps.Get (i, j - 1));
            }
        }
    }

    ndnHelper.SetOldContentStore("ns3::ndn::cs::Lru", "MaxSize", "10000");
    ndnHelper.Install (producers);

    if (cacheSize.compare ("0") == 0)
        ndnHelper.SetOldContentStore("ns3::ndn::cs::Nocache");
    else
        ndnHelper.SetOldContentStore("ns3::ndn::cs::Lru", "MaxSize", cacheSize);
    ndnHelper.Install (aps);

    ndnHelper.SetOldContentStore("ns3::ndn::cs::Nocache");
    ndnHelper.Install (cars);

    StrategyChoiceHelper::InstallAll ("/prefix", "/localhost/nfd/strategy/best-route");

    AppHelper consumerHelper = AppHelper ("ns3::ndn::ConsumerCbr");
    consumerHelper.SetPrefix ("/prefix");
    consumerHelper.SetAttribute ("Frequency", StringValue ("10"));
    consumerHelper.Install (cars);

    AppHelper producerHelper = AppHelper ("ns3::ndn::Producer");
    producerHelper.SetPrefix ("/prefix");
    producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));
    producerHelper.Install (producer);

    FibHelper::AddRoute(aps.Get (nStreets / 2, nStreets / 2), "/prefix", producer, 1);
    for (i = 0; i < 11; i++)
    {
        for (j = 0; j < 11; j++)
        {
            if (j % 2 == 0 && j < nStreets / 2)
            {
                FibHelper::AddRoute(aps.Get (i, j), "/prefix", aps.Get (i, j + 1), 1);
            } else if (j % 2 != 0 && i % 2 != 0 && j < nStreets / 2)
            {
                FibHelper::AddRoute(aps.Get (i, j), "/prefix", aps.Get (i, j + 1), 1);
            } else if (j % 2 != 0 && i < nStreets / 2)
            {
                FibHelper::AddRoute(aps.Get (i, j), "/prefix", aps.Get (i + 1, j), 1);
            } else if (j % 2 != 0 && i > nStreets / 2)
            {
                FibHelper::AddRoute(aps.Get (i, j), "/prefix", aps.Get (i - 1, j), 1);
            } else if (j % 2 == 0 && j > nStreets / 2 && i % 2 == 0 && j < nStreets - 1)
            {
                cout << Names::FindName (aps.Get (i, j)) << " ";
                cout << Names::FindName (aps.Get (i, j + 1)) << "\n";
                FibHelper::AddRoute(aps.Get (i, j), "/prefix", aps.Get (i, j + 1), 1);
            } else if (j == nStreets - 1)
            {
                FibHelper::AddRoute(aps.Get (i, j), "/prefix", aps.Get (i, j - 1), 1);
            } else if (j > nStreets / 2 && i % 2 != 0)
            {
                FibHelper::AddRoute(aps.Get (i, j), "/prefix", aps.Get (i, j - 1), 1);
            }
            if (0){
            router = aps.Get (i, j);
            // Install left and up links on center (even) nodes
            if (i % 2 != 0 && j % 2 != 0)
            {
                FibHelper::AddRoute(aps.Get (i - 1, j), "/prefix", router, 1);
                FibHelper::AddRoute(aps.Get (i, j - 1), "/prefix", router, 1);
                // If nStreets is even install right and down links
                if (nStreets % 2 != 0)
                {
                    if (i == nStreets - 2)
                        FibHelper::AddRoute(aps.Get (i + 1, j), "/prefix", router, 1);
                    if (j == nStreets - 2)
                        FibHelper::AddRoute(aps.Get (i, j + 1), "/prefix", router, 1);
                }
            } else if (i % 2 == 0 && j % 2 != 0)
            {
                FibHelper::AddRoute(aps.Get (i, j - 1), "/prefix", router, 1);
                if (j == nStreets - 2)
                    FibHelper::AddRoute(aps.Get (i, j + 1), "/prefix", router, 1);
                if (i != 0 && i != nStreets - 1)
                    FibHelper::AddRoute(aps.Get (i - 1, j), "/prefix", router, 1);
            } else if (i % 2 != 0 && j % 2 == 0 && j != 0 && j != nStreets - 1)
            {
                FibHelper::AddRoute(aps.Get (i, j - 1), "/prefix", router, 1);
            }
            }
        }
    }

    AppDelayTracer::Install (cars, traceFilePrefix + "app-trace.txt");
    L3RateTracer::Install (NodeContainer(producers, cars), traceFilePrefix + "l3-trace");

    cout << cars.Get (0)->GetNDevices () << "\n";
    for (i=0; i < cars.Get (0)->GetNDevices(); i++)
    {
        cout << cars.Get (0)->GetDevice (i)->GetTypeId () << "\n";
        Ptr<L3Protocol> ndn = cars.Get (0)->GetObject<L3Protocol> ();
        FibHelper::AddRoute(cars.Get (0), "/prefix", ndn->getFaceByNetDevice(cars.Get (0)->GetDevice (i)), 1);
    }

    for (const auto& entry : aps.Get (9, 9)->
            GetObject<ns3::ndn::L3Protocol> ()->getForwarder ()->getFib ())
    {
        cout << entry.getPrefix () << " (";
        for (auto& nextHop : entry.getNextHops ())
        {
            cout << *nextHop.getFace () << ", ";
            auto ndFace = dynamic_pointer_cast<NetDeviceFace>(nextHop.getFace ());
            if (ndFace == nullptr)
                continue;

            cout << " towards ";

            //cout << *nextHop.getFace ();
            cout << Names::FindName(ndFace->GetNetDevice ()->GetChannel ()->GetDevice (1)->GetNode ()) << " ";
        }
        cout << ")\n";
        
    }

    Simulator::Stop (Seconds (44.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}

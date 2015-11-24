## -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-
# pylint: disable=missing-docstring
# pylint: disable=import-error
# pylint: disable=wildcard-import
# pylint: disable=invalid-name
#
# Copyright (c) 2011-2015  Regents of the University of California.
#
# This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
# contributors.
#
# ndnSIM is free software: you can redistribute it and/or modify it under the terms
# of the GNU General Public License as published by the Free Software Foundation,
# either version 3 of the License, or (at your option) any later version.
#
# ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE.  See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
#

# ndn-simple.py

from ns.core import Vector, Config, StringValue, CommandLine, Simulator, \
        Seconds
from ns.network import NodeContainer, Node
from ns.point_to_point import PointToPointHelper
from ns.ndnSIM import ndn
from ns.wifi import YansWifiPhyHelper, YansWifiChannelHelper, \
    NqosWifiMacHelper, WifiHelper, Ssid, SsidValue
from ns.mobility import MobilityModel, MobilityHelper

#
# This scenario simulates a very simple network topology:
#
#
#      +----------+     1Mbps      +--------+     1Mbps      +----------+
#      | consumer | <------------> | router | <------------> | producer |
#      +----------+         10ms   +--------+          10ms  +----------+
#
#
# Consumer requests data from producer with frequency 10 interests per second
# (interests contain constantly increasing sequence number).
#
# For every received interest, producer replies with a data packet, containing
# 1024 bytes of virtual payload.
#
# To run scenario and see what is happening, use the following command:
#
#     NS_LOG=ndn.Consumer:ndn.Producer ./waf --pyrun=src/ndnSIM/examples/ndn-simple.py
#

def SetPosition(node, dpos):
    mob = node.GetObject(MobilityModel.GetTypeId())
    mob.SetPosition(dpos)

class NodeMat():

    def __init__(self, x, y):
        self.container = NodeContainer()
        self.container.Create(x * y)
        self.size = (x, y)

    def Get(self, x, y):
        return self.container.Get(x * self.size[0] + y)

def GridPositioning(spacing, nodeMat, offset=0):
    pos = Vector()
    for i in range(nodeMat.size[0]):
        for j in range(nodeMat.size[1]):
            pos.x = i * spacing + offset
            pos.y = j * spacing + offset
            SetPosition(nodeMat.Get(i, j), pos)

def main():
    # Set default parameters for PointToPoint links and channels
    Config.SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("10Mbps"))
    Config.SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"))
    Config.SetDefault("ns3::DropTailQueue::MaxPackets", StringValue("20"))

    # Read optional command-line parameters (e.g., enable visualizer with ./waf --pyrun=<> --visualize
    import sys
    cmd = CommandLine()
    cmd.Parse(sys.argv)

    nSteets = 11
    apSpacing = 100
    nCars = 1

    # Creating nodes
    aps = NodeMat(nSteets, nSteets)

    cars = NodeContainer()
    cars.Create(nCars)

    producer = Node()

    mobility = MobilityHelper()
    mobility.InstallAll()

    SetPosition(producer, Vector((nSteets / 2) * apSpacing + (apSpacing / 2), -100, 0))

    GridPositioning(apSpacing, aps)

    SetPosition(cars.Get(0), Vector(0, 20, 0))

    # Connecting nodes using two links
    p2p = PointToPointHelper()

    p2p.Install(aps.Get(nSteets / 2, nSteets / 2), producer)

    for i in range(nSteets):
        for j in range(nSteets):
            router = aps.Get(i, j)
            # Install left and up links on center (even) nodes
            if i % 2 != 0 and j % 2 != 0:
                p2p.Install(router, aps.Get(i - 1, j))
                p2p.Install(router, aps.Get(i, j - 1))
                # If nStreets is even install right and down links too
                if nSteets % 2 != 0:
                    if i == nSteets - 2:
                        p2p.Install(router, aps.Get(i + 1, j))
                    if j == nSteets - 2:
                        p2p.Install(router, aps.Get(i, j + 1))
            elif i % 2 == 0 and j % 2 != 0:
                p2p.Install(router, aps.Get(i, j - 1))
                if j == nSteets - 2:
                    p2p.Install(router, aps.Get(i, j + 1))
                if i != 0 and i != nSteets - 1:
                    p2p.Install(router, aps.Get(i - 1, j))
            elif i % 2 != 0 and j % 2 == 0 and j != 0 and j != nSteets - 1:
                p2p.Install(router, aps.Get(i, j - 1))

    wifi = WifiHelper.Default()
    wifiMac = NqosWifiMacHelper.Default()
    wifiPhy = YansWifiPhyHelper.Default()
    wifiChannel = YansWifiChannelHelper.Default()
    wifiPhy.SetChannel(wifiChannel.Create())
    ssid = Ssid("wifi-default")

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid))
    wifi.Install(wifiPhy, wifiMac, aps.container)

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid))
    wifi.Install(wifiPhy, wifiMac, cars)

    # Install NDN stack on all nodes
    ndnHelper = ndn.StackHelper()
    ndnHelper.SetDefaultRoutes(True)
    ndnHelper.InstallAll()

    # Choosing forwarding strategy
    ndn.StrategyChoiceHelper.InstallAll("/prefix", "/localhost/nfd/strategy/multicast")

    # Installing applications

    # Consumer
    consumerHelper = ndn.AppHelper("ns3::ndn::ConsumerCbr")
    # Consumer will request /prefix/0, /prefix/1, ...
    consumerHelper.SetPrefix("/prefix")
    consumerHelper.SetAttribute("Frequency", StringValue("10")) # 10 interests a second
    consumerHelper.Install(cars)

    # Producer
    producerHelper = ndn.AppHelper("ns3::ndn::Producer")
    # Producer will reply to all requests starting with /prefix
    producerHelper.SetPrefix("/prefix")
    producerHelper.SetAttribute("PayloadSize", StringValue("1024"))
    producerHelper.Install(producer)

    Simulator.Stop(Seconds(20.0))

    Simulator.Run()
    Simulator.Destroy()

if __name__ == '__main__':
    main()

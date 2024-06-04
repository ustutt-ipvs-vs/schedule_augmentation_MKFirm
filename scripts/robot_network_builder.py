import argparse
import math
import random
import itertools
import json

DATA_RATE = 12500000 # 100Mbps
PROPAGATION_DELAY = 1000 # 1us (also covers Ethernet inter frame gap...)
PROCESSING_DELAY = 0 

CT_PERIOD = 4000000 # 4ms
CT_FRAMESIZE = 100 # 100 byte
CT_E2E_LATENCY = 500000 # 500us
CT_JITTER = 0 # zero arrival jitter

WT_PERIOD = 20000000 # 20ms
WT_FRAMESIZE = 100 # 100 byte
WT_E2E_LATENCY = 20000000 # 20ms
WT_JITTER = 5000000 # 5ms

WT_RELIABILITY = 0.5 

OMNETPP_X = 700
OMNETPP_Y = 500

omnetpp_ned_header = """
package d6g.simulations.{package};

import inet.networks.base.TsnNetworkBase;
import inet.node.contract.IEthernetNetworkNode;
import inet.node.ethernet.EthernetLink;
import d6g.devices.DetCom;
import d6g.devices.tsntranslator.TTInterface;
import d6g.devices.tsntranslator.TTChannel;
import d6g.devices.tsntranslator.TsnTranslator;
import d6g.networks.DetComNetworkBase;
import d6g.apps.edgecloud.UdpEdgeCloudBasicApp;
import inet.node.tsn.TsnDevice;
import inet.node.tsn.TsnSwitch;
import ned.DatarateChannel;
import d6g.distribution.histogram.Histogram;
import d6g.distribution.histogram.HistogramContainer;


network {network} extends DetComNetworkBase
{{	
 submodules:
    histogramContainer:HistogramContainer{{
        @display("p=100,800;is=s");
    }}	
"""
tsn_device = """    {name}: TsnDevice {{
            @display("p={x},{y}");  
    }}
"""
tsn_switch = """    {name}: TsnSwitch {{
            @display("p={x},{y}");  
    }}
"""
detcom = """    {name}: DetCom {{
            @display("p={x},{y}");  
    }}
"""
switch_link = "    {source}.ethg++ <--> EthernetLink <--> {target}.ethg++;\n"
dstt_link = "    {source}.ethg++ <--> EthernetLink <--> {target}.dsttg++;\n"
nwtt_link = "    {source}.nwttg++ <--> EthernetLink <--> {target}.ethg++;\n"

omnetpp_ini_header = """
[General]
network = d6g.simulations.{package}.{network}
sim-time-limit = 10s
description = "{network}"

**.displayGateSchedules = true
**.gateFilter = "**.ethg[1].**"
**.gateScheduleVisualizer.height = 16
**.gateScheduleVisualizer.placementHint = "top"

**.datarate = 100Mbps
**.bitrate = 100Mbps

*.histogramContainer.histograms = {{
    Uplink: "../../../deterministic6g_data/PD-Wireless-5G-2a/uplink_0.xml", 
    Downlink: "../../../deterministic6g_data/PD-Wireless-5G-2a/downlink_0.xml"}}
*.detcom.**.delayDownlink = rngProvider("histogramContainer","Downlink")
*.detcom.**.delayUplink = rngProvider("histogramContainer","Uplink")
"""

omnetpp_udp_base = """
*.{device}.hasOutgoingStreams = true
*.{device}.numApps = {apps}
*.{device}.app[*].source.productionInterval = 20ms
"""
omnetpp_udp_app = """
*.{sender}.app[{sender_app}].typename = "UdpSourceApp"
*.{sender}.app[{sender_app}].packetName = "{packet_name}"
*.{sender}.app[{sender_app}].io.destAddress = "{receiver}"
*.{sender}.app[{sender_app}].io.destPort = 2000 + {receiver_app}
*.{sender}.app[{sender_app}].source.packetLength = {frame_size}B - 54B
*.{receiver}.app[{receiver_app}].typename = "UdpSinkApp"
*.{receiver}.app[{receiver_app}].io.localPort = 2000 + {receiver_app}
"""
omnetpp_udp_streams = {}


offset = 0
network = {"nodes": [], "links": []}
streams = []
omnetpp_ned = ["", ""]
omnetpp_ini = ["", ""]

def random_path(size: int, offset: int):
    talker = random.randrange(2**(size-1)-1, 2**size-1) 
    path = [talker]
    while path[-1] != 0:
        path.append(math.floor((path[-1]-1)/2))

    path = [i + offset for i in path]
    return path

def build_core(size : int, ct: int, bypass: bool):
    global offset, omnetpp_ned

    for node in range(0, 2**size-1):
        # device
        network["nodes"].append({"id": offset + node, "name": f"CORE_{str(node).zfill(2)}", "processing_delay": PROCESSING_DELAY})

        if node == 0: 
            omnetpp_ned[0] += detcom.format(name="detcom", x=OMNETPP_X-100, y=OMNETPP_Y)
            omnetpp_ned[0] += tsn_switch.format(name=f"CORE_{str(node).zfill(2)}", x=OMNETPP_X, y=OMNETPP_Y)
        elif node < 2**(size-1)-1:
            level = math.floor(math.log2(node+1))
            omnetpp_ned[0] += tsn_switch.format(name=f"CORE_{str(node).zfill(2)}", x=OMNETPP_X + 100 * level, y=int(OMNETPP_Y + 100 * 2**(size-1-level) * (node - 2**level - 2**(level-1) + 1.5)))
        else:
            level = math.floor(math.log2(node+1))
            omnetpp_ned[0] += tsn_device.format(name=f"CORE_{str(node).zfill(2)}", x=OMNETPP_X + 100 * level, y=int(OMNETPP_Y + 100 * (node - 2**level - 2**(level-1) + 1.5)))
        omnetpp_udp_streams[f"CORE_{str(node).zfill(2)}"] = 0

        # links
        if node > 0:
            network["links"].append({"type": 0, "data_rate": DATA_RATE, "propagation_delay": PROPAGATION_DELAY, "source": offset+node, "target": offset+math.floor((node-1)/2) })
            network["links"].append({"type": 0, "data_rate": DATA_RATE, "propagation_delay": PROPAGATION_DELAY, "target": offset+node, "source": offset+math.floor((node-1)/2) })

            if node <= 2:
                omnetpp_ned[1] += switch_link.format(source="CORE_00", target=f"CORE_{str(node).zfill(2)}")
            else:
                omnetpp_ned[1] += switch_link.format(source=f"CORE_{str(math.floor((node-1)/2)).zfill(2)}", target=f"CORE_{str(node).zfill(2)}")
        else:
            omnetpp_ned[1] += nwtt_link.format(source="detcom", target="CORE_00")

        if node == 2:
            network["links"].append({"type": 0, "data_rate": DATA_RATE, "propagation_delay": PROPAGATION_DELAY, "target": offset+1, "source": offset+2 })
            network["links"].append({"type": 0, "data_rate": DATA_RATE, "propagation_delay": PROPAGATION_DELAY, "target": offset+2, "source": offset+1 })
            omnetpp_ned[1] += switch_link.format(source="CORE_01", target="CORE_02")

    # cross traffic
    for s in range(0, ct): 
        path_a = random_path(size, offset) if not bypass else random_path(size, offset)[:-1]
        path_b = []
        while len(path_b) == 0:
            path_b = random_path(size, offset) if not bypass else random_path(size, offset)[:-1]
            path_b = [x for x in path_b if x not in path_a]
        path_a = path_a[:len(path_b)+1]
        path = path_a + list(reversed(path_b))
        route = list(itertools.pairwise(path))

        for i in range(0, int(WT_PERIOD / CT_PERIOD)):
            streams.append({"name": f"CORE_CT{str(s).zfill(2)}_{str(i).zfill(2)}", "period": CT_PERIOD, "phase": i * CT_PERIOD, "e2e_latency": CT_E2E_LATENCY, "jitter": CT_JITTER, "frame_size": CT_FRAMESIZE, "route": route, "rti_map": None, "weight": 1.0})

            omnetpp_ini[1] += omnetpp_udp_app.format(sender=f"CORE_{str(path[0]).zfill(2)}", receiver=f"CORE_{str(path[-1]).zfill(2)}", sender_app=omnetpp_udp_streams[f"CORE_{str(path[0]).zfill(2)}"], receiver_app=omnetpp_udp_streams[f"CORE_{str(path[-1]).zfill(2)}"], packet_name=f"CORE_CT{str(s).zfill(2)}_{str(i).zfill(2)}", frame_size=CT_FRAMESIZE)
            omnetpp_udp_streams[f"CORE_{str(path[0]).zfill(2)}"] += 1
            omnetpp_udp_streams[f"CORE_{str(path[-1]).zfill(2)}"] += 1


    offset += 2**size - 1

def build_agv(n: int, size : int, csize: int, wt_in: int, wt_out: int, ct: int, bypass: bool):
    global offset, OMNETPP_Y

    for node in range(0, 2**size-1):
        # device
        network["nodes"].append({"id": offset + node, "name": f"AGV{n}_{str(node).zfill(2)}", "processing_delay": PROCESSING_DELAY})

        if node == 0:
            omnetpp_ned[0] += tsn_switch.format(name=f"AGV{n}_00", x=OMNETPP_X-200, y=OMNETPP_Y)
        elif node < 2**(size-1)-1:
            level = math.floor(math.log2(node+1))
            omnetpp_ned[0] += tsn_switch.format(name=f"AGV{n}_{str(node).zfill(2)}", x=OMNETPP_X - 100 * (level+2), y=int(OMNETPP_Y + 100 * 2**(size-1-level) * (node - 2**level - 2**(level-1) + 1.5)))
        else:
            level = math.floor(math.log2(node+1))
            omnetpp_ned[0] += tsn_device.format(name=f"AGV{n}_{str(node).zfill(2)}", x=OMNETPP_X - 100 * (level+2), y=int(OMNETPP_Y + 100 * (node - 2**level - 2**(level-1) + 1.5)))
        omnetpp_udp_streams[f"AGV{n}_{str(node).zfill(2)}"] = 0

        # links
        if node > 0:
            network["links"].append({"type": 0, "data_rate": DATA_RATE, "propagation_delay": PROPAGATION_DELAY, "source": offset+node, "target": offset+math.floor((node-1)/2) })
            network["links"].append({"type": 0, "data_rate": DATA_RATE, "propagation_delay": PROPAGATION_DELAY, "target": offset+node, "source": offset+math.floor((node-1)/2) })
            omnetpp_ned[1] += switch_link.format(source=f"AGV{n}_{str(node).zfill(2)}", target=f"AGV{n}_{str(math.floor((node-1)/2)).zfill(2)}")
        if node == 2:
            network["links"].append({"type": 0, "data_rate": DATA_RATE, "propagation_delay": PROPAGATION_DELAY, "target": offset+1, "source": offset+2 })
            network["links"].append({"type": 0, "data_rate": DATA_RATE, "propagation_delay": PROPAGATION_DELAY, "target": offset+2, "source": offset+1 })
            omnetpp_ned[1] += switch_link.format(source=f"AGV{n}_02", target=f"AGV{n}_01")
        if node == 0:
            network["links"].append({"type": 1, "data_rate": DATA_RATE, "propagation_delay": PROPAGATION_DELAY, "multiple_subcarriers": True, "target": offset, "source": 0 })
            network["links"].append({"type": 1, "data_rate": DATA_RATE, "propagation_delay": PROPAGATION_DELAY, "multiple_subcarriers": True, "target": 0, "source": offset })
            omnetpp_ned[1] += dstt_link.format(source=f"AGV{n}_00", target="detcom")

    # cross traffic
    for s in range(0, ct): 
        path_a = random_path(size, offset) if not bypass else random_path(size, offset)[:-1]
        path_b = []
        while len(path_b) == 0:
            path_b = random_path(size, offset) if not bypass else random_path(size, offset)[:-1]
            path_b = [x for x in path_b if x not in path_a]
        path_a = path_a[:len(path_b)+1]
        path = path_a + list(reversed(path_b))
        route = list(itertools.pairwise(path))

        for i in range(0, int(WT_PERIOD / CT_PERIOD)):
            streams.append({"name": f"AGV{n}_CT{str(s).zfill(2)}_{str(i).zfill(2)}", "period": CT_PERIOD, "phase": i * CT_PERIOD, "e2e_latency": CT_E2E_LATENCY, "jitter": CT_JITTER, "frame_size": WT_FRAMESIZE, "route": route, "rti_map": None, "weight": 1.0})

            omnetpp_ini[1] += omnetpp_udp_app.format(sender=f"AGV{n}_{str(path[0]-offset).zfill(2)}", receiver=f"AGV{n}_{str(path[-1]-offset).zfill(2)}", sender_app=omnetpp_udp_streams[f"AGV{n}_{str(path[0]-offset).zfill(2)}"], receiver_app=omnetpp_udp_streams[f"AGV{n}_{str(path[-1]-offset).zfill(2)}"], packet_name=f"AGV{n}_CT{str(s).zfill(2)}_{str(i).zfill(2)}", frame_size=CT_FRAMESIZE)
            omnetpp_udp_streams[f"AGV{n}_{str(path[0]-offset).zfill(2)}"] += 1
            omnetpp_udp_streams[f"AGV{n}_{str(path[-1]-offset).zfill(2)}"] += 1

    # incoming wireless traffic
    for s in range(0, wt_in): 
        route = list(itertools.pairwise(random_path(csize, 0))) + [(0,offset)] + list(itertools.pairwise(list(reversed(random_path(size, offset)))))
        streams.append({"name": f"CORE_AGV{n}_{str(s).zfill(2)}", "period": WT_PERIOD, "phase": 0, "e2e_latency": WT_E2E_LATENCY, "jitter": WT_JITTER, "frame_size": WT_FRAMESIZE, "route": route, "rti_map": [{"edge": (0,offset), "reliability": WT_RELIABILITY, "histogram": "../data/downlink_histogram.json"}], "weight": 1.0})

        omnetpp_ini[1] += omnetpp_udp_app.format(sender=f"CORE_{str(route[0][0]).zfill(2)}", receiver=f"AGV{n}_{str(route[-1][1]-offset).zfill(2)}", sender_app=omnetpp_udp_streams[f"CORE_{str(route[0][0]).zfill(2)}"], receiver_app=omnetpp_udp_streams[f"AGV{n}_{str(route[-1][1]-offset).zfill(2)}"], packet_name=f"CORE_AGV{n}_{str(s).zfill(2)}", frame_size=WT_FRAMESIZE)
        omnetpp_udp_streams[f"CORE_{str(route[0][0]).zfill(2)}"] += 1
        omnetpp_udp_streams[f"AGV{n}_{str(route[-1][1]-offset).zfill(2)}"] += 1

    # outgoing wireless traffic
    for s in range(0, wt_out): 
        route = list(itertools.pairwise(random_path(size, offset))) + [(offset,0)] + list(itertools.pairwise(list(reversed(random_path(csize, 0)))))
        streams.append({"name": f"AGV{n}_CORE_{str(s).zfill(2)}", "period": WT_PERIOD, "phase": 0, "e2e_latency": WT_E2E_LATENCY, "jitter": WT_JITTER, "frame_size": WT_FRAMESIZE, "route": route, "rti_map": [{"edge": (offset,0), "reliability": WT_RELIABILITY, "histogram": "../data/uplink_histogram.json"}], "weight": 1.0})

        omnetpp_ini[1] += omnetpp_udp_app.format(sender=f"AGV{n}_{str(route[0][0]-offset).zfill(2)}", receiver=f"CORE_{str(route[-1][1]).zfill(2)}", sender_app=omnetpp_udp_streams[f"AGV{n}_{str(route[0][0]-offset).zfill(2)}"], receiver_app=omnetpp_udp_streams[f"CORE_{str(route[-1][1]).zfill(2)}"], packet_name=f"AGV{n}_CORE_{str(s).zfill(2)}", frame_size=CT_FRAMESIZE)
        omnetpp_udp_streams[f"AGV{n}_{str(route[0][0]-offset).zfill(2)}"] += 1
        omnetpp_udp_streams[f"CORE_{str(route[-1][1]).zfill(2)}"] += 1

    offset += 2**size - 1
    OMNETPP_Y += 100 * 2**(size-1)


parser = argparse.ArgumentParser(
                    prog='Robost Network Builder',
                    description='builds a simple network with N AGVs that want to communication with M edge servers')

parser.add_argument('-no_bypass', '--cross_traffic_bypass', default=True, action='store_false') 

parser.add_argument('-agvs', '--agv_size_specification', type=int, nargs='+', default=[3]) 
parser.add_argument('-agv_wt_out', '--agv_outgoing_wireless_traffic_specification',  type=int, nargs='+', default=[10]) 
parser.add_argument('-agv_wt_in', '--agv_ingoing_wireless_traffic_specification',  type=int, nargs='+', default=[10]) 
parser.add_argument('-agv_ct', '--agv_cross_traffic_specification',  type=int, nargs='+', default=[1]) 

parser.add_argument('-core', '--core_size_specification', type=int, default=4) 
parser.add_argument('-core_ct', '--core_cross_traffic_specification', type=int, default=1) 

parser.add_argument('-pkg', '--package_name', type=str, default="agv") 
parser.add_argument('-net', '--network_name', type=str, default="AGVNetwork") 

args = parser.parse_args()

omnetpp_ned[0] = omnetpp_ned_header.format(package=args.package_name, network=args.network_name)
omnetpp_ned[1] = "\nconnections:\n"
omnetpp_ini[0] = omnetpp_ini_header.format(package=args.package_name, network=args.network_name)
build_core(args.core_size_specification, args.core_cross_traffic_specification, args.cross_traffic_bypass)
for i in range(len(args.agv_size_specification)):
    build_agv(i, args.agv_size_specification[i], args.core_size_specification, args.agv_ingoing_wireless_traffic_specification[i], args.agv_outgoing_wireless_traffic_specification[i], args.agv_cross_traffic_specification[i], args.cross_traffic_bypass)

print(f"generated network with {len(streams)} streams")

with open("data/network.json", "w") as f:
    json.dump(network, f, indent=4, sort_keys=True)

with open("data/streams.json", "w") as f:
    json.dump(streams, f, indent=4, sort_keys=True)

omnetpp_ned = omnetpp_ned[0] + omnetpp_ned[1] + "}"
with open("data/network.ned", "w") as f:
    f.write(omnetpp_ned)

for key in omnetpp_udp_streams:
    if omnetpp_udp_streams[key] > 0:
        omnetpp_ini[0] += omnetpp_udp_base.format(device=key, apps=omnetpp_udp_streams[key])
omnetpp_ini = omnetpp_ini[0] + omnetpp_ini[1]
with open("data/omnetpp_1.ini", "w") as f:
    f.write(omnetpp_ini)

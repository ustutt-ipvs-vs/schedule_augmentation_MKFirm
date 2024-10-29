import argparse
import math
import random
import json

OMNETPP_NED_MIN_POS = 200
OMNETPP_NED_MAX_POS = 1000

PORT = 2000

omnetpp_ned_header = """package d6g.simulations.{package};

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
switch_link = "    {source}.ethg++ <--> EthernetLink {{ datarate={datarate}Mbps; delay={delay}ns; }} <--> {target}.ethg++;\n"
dstt_link = "    {source}.ethg++ <--> EthernetLink <--> {target}.dsttg++;\n"
nwtt_link = "    {source}.nwttg++ <--> EthernetLink <--> {target}.ethg++;\n"

def parse_json_file(filename):
    with open(filename) as f:
        return json.load(f) 


def build_network_description_file(topology, ned_output, package, network_name):
    ned = omnetpp_ned_header.format(network=network_name, package=package)
    device_map = {}
    link_map = {}

    for node in topology["nodes"]:
        x = node["position"].split(",")[0] if "position" in node else random.randint(OMNETPP_NED_MIN_POS, OMNETPP_NED_MAX_POS)
        y = node["position"].split(",")[1] if "position" in node else random.randint(OMNETPP_NED_MIN_POS, OMNETPP_NED_MAX_POS)
        if node["is_switch"]:
            ned += tsn_switch.format(name=node["name"], x=x, y=y)
        else:
            ned += tsn_device.format(name=node["name"], x=x, y=y)

        device_map[node["id"]] = node
        device_map[node["id"]]["ifaces"] = []
        device_map[node["id"]]["identifier_entries"] = []
        device_map[node["id"]]["encoder_entries"] = []
        device_map[node["id"]]["app"] = 0
        device_map[node["id"]]["has_outgoing_streams"] = "false"
        device_map[node["id"]]["ip_address"] = None

    ned += "\n connections:\n"
    for link in topology["links"]:
        link_map[f"{link['source']}-{link['target']}"] = link
        if link["source"] > link["target"]:
            continue

        ned += switch_link.format(source=device_map[link["source"]]["name"], target=device_map[link["target"]]["name"], datarate=link["link_speed_mbps"], delay=link["propagation_delay_ns"])
        device_map[link["source"]]["ifaces"].append(link["target"])
        device_map[link["target"]]["ifaces"].append(link["source"])
    ned += "}"

    with open(ned_output, "w") as f:
        f.write(ned)

    return device_map, link_map

omnetpp_ini = """
{header}
#--------------------------------------------
# Ethernet Link Specification
#--------------------------------------------
{links}

#--------------------------------------------
# End-Station Specification
#--------------------------------------------
{talkers}

#--------------------------------------------
# TT Stream Specification
#--------------------------------------------
{tt_streams}

#--------------------------------------------
# ET Stream Specification
#--------------------------------------------
{et_streams}

#--------------------------------------------
# Bridge Specification 
#--------------------------------------------
{bridges}

"""

omnetpp_ini_header = """[{scenario}]
network = d6g.simulations.{package}.{network}
sim-time-limit = {sim_time}s
description = {network}
"""

talker_ini = """
*.{device}.hasOutgoingStreams = {has_outgoing_streams}
*.{device}.numApps = {apps}
"""

channel_ini = '*.{source}.eth[{iface}].bitrate = {datarate}Mbps\n'

pcp_ini_identifier_entry = '{{stream: "{stream}", packetFilter: expr(udp.destPort == {dest_port})}}'
pcp_ini_encoder_entry = '{{stream: "{stream}", pcp: {pcp}}}'
pcp_ini = """
*.{talker}.bridging.streamIdentifier.identifier.mapping = [{identifier_entries}]
*.{talker}.bridging.streamCoder.encoder.mapping = [{encoder_entries}]
"""

tt_stream_ini = """
# Stream {stream}: period {stream_period}ms, deadline {deadline}ms
*.{talker}.app[{talker_app}].typename = "UdpSourceApp"
*.{talker}.app[{talker_app}].packetName = "tt_{stream}_{frame}"
*.{talker}.app[{talker_app}].io.destAddress = "{listener}"
*.{talker}.app[{talker_app}].io.destPort = {port}
*.{talker}.app[{talker_app}].source.packetLength = {frame_size}B - 58B # 58B = 8B (UDP) + 20B (IP) + 14B (ETH MAC) + 4B (Dot1Q) + 4B (ETH FCS) + 8B (ETH PHY)
*.{talker}.app[{talker_app}].source.productionInterval = {period}ms
*.{talker}.app[{talker_app}].source.initialProductionOffset = {offset}ms
*.{listener}.app[{listener_app}].typename = "UdpSinkApp"
*.{listener}.app[{listener_app}].io.localPort = {port}
"""

et_stream_ini = """
*.{talker}.app[{talker_app}].typename = "UdpBasicBurst"
*.{talker}.app[{talker_app}].packetName = "et_{stream}"
*.{talker}.app[{talker_app}].destAddresses = "{listener}"
*.{talker}.app[{talker_app}].chooseDestAddrMode = "once"
*.{talker}.app[{talker_app}].destPort = {port}
*.{talker}.app[{talker_app}].burstDuration = {burst_duration}ms
*.{talker}.app[{talker_app}].startTime = {interevent_time}ms + exponential({exp_param}ms)
*.{talker}.app[{talker_app}].sleepDuration = {interevent_time}ms + exponential({exp_param}ms)
*.{talker}.app[{talker_app}].sendInterval = 9999s # burst consists of a single packet
*.{talker}.app[{talker_app}].messageLength = {frame_size}B - 58B # 58B = 8B (UDP) + 20B (IP) + 14B (ETH MAC) + 4B (Dot1Q) + 4B (ETH FCS) + 8B (ETH PHY)
*.{listener}.app[{listener_app}].typename = "UdpSinkApp"
*.{listener}.app[{listener_app}].io.localPort = {port}
"""

shaping_ini = """
*.{device}.hasEgressTrafficShaping = true
*.{device}.eth[*].macLayer.queue.transmissionGate[0..6].initiallyOpen = false
*.{device}.eth[*].macLayer.queue.numTrafficClasses = {queues}
"""

gcl_ini = """
# {exact_transmissions}
*.{device}.eth[{iface}].macLayer.queue.transmissionGate[{queue}].offset = {offset}ms
*.{device}.eth[{iface}].macLayer.queue.transmissionGate[{queue}].durations = [{gcl}]
"""

def gcl_port_finder(gcl, stream_id):
    pcp_val = -1
    transmission_offsets = []
    for port in gcl:
        for pcp in gcl[port]["gcl_per_pcp"]:
            for entry in gcl[port]["gcl_per_pcp"][pcp]:
                if entry["streams"][0]["stream_id"] == stream_id:
                    pcp_val = int(pcp)
                    transmission_offsets.append(entry["open_time_ns"])
        if pcp_val != -1:
            break
    return pcp_val, transmission_offsets

def build_ini_file(gcl, tt_streams, et_streams, device_map, link_map, topology, ini_output, scenario, package, network_name, sim_time):
    global PORT

    hyper_period = math.lcm(*[stream["cycle_time_ns"] for stream in tt_streams]) / 1e6
    ini = omnetpp_ini_header.format(scenario=scenario, package=package, network=network_name, sim_time=sim_time)

    ini_links = ""
    for link in topology["links"]:
        source = device_map[link["source"]]
        iface = source["ifaces"].index(link["target"])
        ini_links += channel_ini.format(source=source["name"], iface=iface, datarate=link["link_speed_mbps"])

    ini_tt_streams = ""
    for stream in tt_streams:
        pcp, transmission_offsets = gcl_port_finder(gcl[str(stream["source"])]["ports"], stream["id"])

        for i,offset in enumerate(transmission_offsets):
            ini_tt_streams += tt_stream_ini.format(talker=device_map[stream["source"]]["name"], listener=device_map[stream["target"]]["name"], port=PORT, talker_app=device_map[stream["source"]]["app"], listener_app=device_map[stream["target"]]["app"], frame_size=stream["frame_size_byte"], period=hyper_period, offset=offset / 1e6, stream=stream["id"], frame=i, stream_period=stream["cycle_time_ns"]/1e6, deadline=stream["deadline_ns"]/1e6)

            device_map[stream["source"]]["identifier_entries"].append(pcp_ini_identifier_entry.format(stream=f"{stream['id']}_{i}", dest_port=PORT))
            device_map[stream["source"]]["encoder_entries"].append(pcp_ini_encoder_entry.format(stream=f"{stream['id']}_{i}", pcp=pcp))

            device_map[stream["target"]]["app"] += 1
            device_map[stream["source"]]["app"] += 1
            PORT += 1
            device_map[stream["source"]]["has_outgoing_streams"] = "true"

    ini_et_streams = ""
    for stream in et_streams:
        drate = link_map[stream["route"][0]["name"]]["link_speed_mbps"]
        burst_duration = stream["bucket_size_byte"] / (drate - stream["rate_mbps"]) * 8/1e3
        ini_et_streams += et_stream_ini.format(talker=device_map[stream["source"]]["name"], listener=device_map[stream["target"]]["name"], port=PORT, talker_app=device_map[stream["source"]]["app"], listener_app=device_map[stream["target"]]["app"], frame_size=stream["frame_size_byte"], stream=stream["id"], burst_duration=burst_duration, interevent_time=stream["min_inter_event_time_ns"] / 1e6, exp_param=10)

        device_map[stream["source"]]["identifier_entries"].append(pcp_ini_identifier_entry.format(stream=f"et_{stream['id']}", dest_port=PORT))
        device_map[stream["source"]]["encoder_entries"].append(pcp_ini_encoder_entry.format(stream=f"et_{stream['id']}", pcp=7))

        device_map[stream["target"]]["app"] += 1
        device_map[stream["source"]]["app"] += 1
        device_map[stream["source"]]["has_outgoing_streams"] = "true"
        PORT += 1

    ini_talkers = ""
    for node_id in device_map:
        if device_map[node_id]["is_switch"]:
            continue
        node = device_map[node_id]
        ini_talkers += talker_ini.format(device=node["name"], has_outgoing_streams=node["has_outgoing_streams"], apps=node["app"])
        if not node["has_outgoing_streams"]:
            continue
        ini_talkers += pcp_ini.format(talker=node["name"], identifier_entries=", ".join(node["identifier_entries"]), encoder_entries=", ".join(node["encoder_entries"]))

    ini_bridges = ""
    for node_id in device_map:
        if not device_map[node_id]["is_switch"]:
            continue
        node = device_map[node_id]
        ini_bridges += shaping_ini.format(device=node["name"], queues=node["queues_per_port"])

    for bridge in gcl:
        if not device_map[int(bridge)]["is_switch"]:
            continue
        for port in gcl[bridge]["ports"]:
            iface = device_map[int(bridge)]["ifaces"].index(gcl[bridge]["ports"][port]["target"])
            for pcp in gcl[bridge]["ports"][port]["gcl_per_pcp"]:
                exact_transmissions = []
                durations = []
                time = 0
                for entry in gcl[bridge]["ports"][port]["gcl_per_pcp"][pcp]:
                    durations.append(max(entry["open_time_ns"] - time, 1))
                    exact_transmissions.append(f"{entry['streams'][0]['stream_id']}_{entry['streams'][0]['frame_number']}: {entry["open_time_ns"] / 1e6}ms")
                    durations.append(entry["close_time_ns"] - entry["open_time_ns"])
                    time = entry["close_time_ns"]
                durations[0] += hyper_period * 1e6 - entry["close_time_ns"]
                offset = round((hyper_period * 1e6 - entry["close_time_ns"]) / 1e6, 6)

                if int(pcp) >= 2:
                    queue = int(pcp)
                else:
                    queue = 1 - int(pcp)

                ini_bridges += gcl_ini.format(device=device_map[int(bridge)]["name"], iface=iface, queue=queue, offset=offset, gcl=", ".join([f"{round(d / 1e6, 6)}ms" for d in durations]), exact_transmissions=", ".join(exact_transmissions))

    ini = omnetpp_ini.format(header=ini, links=ini_links, talkers=ini_talkers, bridges=ini_bridges, tt_streams=ini_tt_streams, et_streams=ini_et_streams)

    with open(ini_output, "w") as f:
        f.write(ini)

def main():
    parser = argparse.ArgumentParser(
                        prog='Omnetpp Builder',
                        description='Converts the Scheduler\'s Output in Omnetpp NED and INI files')
    parser.add_argument('-t', '--topology_input', default="input/topology.json") 
    parser.add_argument('-s', '--tt_streams_input', default="input/streams.json") 
    parser.add_argument('-e', '--et_streams_input', default="input/emergency_streams.json") 
    parser.add_argument('-g', '--gcl_input', default="release/sample_output.json") 
    parser.add_argument('-ned', '--ned_output', default="data/network.ned") 
    parser.add_argument('-ini', '--ini_output', default="data/omnetpp.ini") 
    parser.add_argument('--package_name', default="emergency_traffic") 
    parser.add_argument('--network_name', default="EmergencyTrafficNetwork") 
    parser.add_argument('--scenario', default="General") 
    parser.add_argument('--simulation_time', type=int, default=10, help="in seconds") 

    args = parser.parse_args()

    topology = parse_json_file(args.topology_input)
    device_map, link_map = build_network_description_file(topology, args.ned_output, args.package_name, args.network_name)
    gcl = parse_json_file(args.gcl_input)
    tt_streams = parse_json_file(args.tt_streams_input)
    et_streams = parse_json_file(args.et_streams_input)
    build_ini_file(gcl, tt_streams, et_streams, device_map, link_map, topology, args.ini_output, args.scenario, args.package_name, args.network_name, args.simulation_time)

if __name__ == '__main__':
  main()


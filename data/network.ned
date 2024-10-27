package d6g.simulations.emergency_traffic;

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


network EmergencyTrafficNetwork extends DetComNetworkBase
{	
 submodules:
    histogramContainer:HistogramContainer{
        @display("p=100,800;is=s");
    }	
    n0: TsnSwitch {
            @display("p=25,93");  
    }
    n1: TsnSwitch {
            @display("p=26,32");  
    }
    n2: TsnDevice {
            @display("p=68,20");  
    }
    n3: TsnDevice {
            @display("p=90,33");  
    }

 connections:
    n0.ethg++ <--> EthernetLink { datarate=1000Mbps; delay=200ns; } <--> n1.ethg++;
    n1.ethg++ <--> EthernetLink { datarate=1000Mbps; delay=200ns; } <--> n0.ethg++;
    n0.ethg++ <--> EthernetLink { datarate=1000Mbps; delay=200ns; } <--> n2.ethg++;
    n2.ethg++ <--> EthernetLink { datarate=1000Mbps; delay=200ns; } <--> n0.ethg++;
    n1.ethg++ <--> EthernetLink { datarate=1000Mbps; delay=200ns; } <--> n3.ethg++;
    n3.ethg++ <--> EthernetLink { datarate=1000Mbps; delay=200ns; } <--> n1.ethg++;
}
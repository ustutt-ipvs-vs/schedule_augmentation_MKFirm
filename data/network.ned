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
            @display("p=723.5407239819006,776.8291571753987");  
    }
    n1: TsnDevice {
            @display("p=1152.7149321266968,839.4305239179955");  
    }
    n2: TsnDevice {
            @display("p=568.6934389140272,395.6719817767654");  
    }
    n3: TsnDevice {
            @display("p=452.71493212669685,1095.6719817767653");  
    }

 connections:
    n0.ethg++ <--> EthernetLink { datarate=100Mbps; delay=200ns; } <--> n1.ethg++;
    n0.ethg++ <--> EthernetLink { datarate=100Mbps; delay=200ns; } <--> n2.ethg++;
    n0.ethg++ <--> EthernetLink { datarate=100Mbps; delay=200ns; } <--> n3.ethg++;
}
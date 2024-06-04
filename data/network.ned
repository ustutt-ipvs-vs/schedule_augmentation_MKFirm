
package d6g.simulations.agv;

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


network AGVNetwork extends DetComNetworkBase
{	
 submodules:
    histogramContainer:HistogramContainer{
        @display("p=100,800;is=s");
    }	
    detcom: DetCom {
            @display("p=600,500");  
    }
    CORE_00: TsnSwitch {
            @display("p=700,500");  
    }
    CORE_01: TsnSwitch {
            @display("p=800,300");  
    }
    CORE_02: TsnSwitch {
            @display("p=800,700");  
    }
    CORE_03: TsnSwitch {
            @display("p=900,200");  
    }
    CORE_04: TsnSwitch {
            @display("p=900,400");  
    }
    CORE_05: TsnSwitch {
            @display("p=900,600");  
    }
    CORE_06: TsnSwitch {
            @display("p=900,800");  
    }
    CORE_07: TsnDevice {
            @display("p=1000,150");  
    }
    CORE_08: TsnDevice {
            @display("p=1000,250");  
    }
    CORE_09: TsnDevice {
            @display("p=1000,350");  
    }
    CORE_10: TsnDevice {
            @display("p=1000,450");  
    }
    CORE_11: TsnDevice {
            @display("p=1000,550");  
    }
    CORE_12: TsnDevice {
            @display("p=1000,650");  
    }
    CORE_13: TsnDevice {
            @display("p=1000,750");  
    }
    CORE_14: TsnDevice {
            @display("p=1000,850");  
    }
    AGV0_00: TsnSwitch {
            @display("p=500,500");  
    }
    AGV0_01: TsnSwitch {
            @display("p=400,400");  
    }
    AGV0_02: TsnSwitch {
            @display("p=400,600");  
    }
    AGV0_03: TsnDevice {
            @display("p=300,350");  
    }
    AGV0_04: TsnDevice {
            @display("p=300,450");  
    }
    AGV0_05: TsnDevice {
            @display("p=300,550");  
    }
    AGV0_06: TsnDevice {
            @display("p=300,650");  
    }

connections:
    detcom.nwttg++ <--> EthernetLink <--> CORE_00.ethg++;
    CORE_00.ethg++ <--> EthernetLink <--> CORE_01.ethg++;
    CORE_00.ethg++ <--> EthernetLink <--> CORE_02.ethg++;
    CORE_01.ethg++ <--> EthernetLink <--> CORE_02.ethg++;
    CORE_01.ethg++ <--> EthernetLink <--> CORE_03.ethg++;
    CORE_01.ethg++ <--> EthernetLink <--> CORE_04.ethg++;
    CORE_02.ethg++ <--> EthernetLink <--> CORE_05.ethg++;
    CORE_02.ethg++ <--> EthernetLink <--> CORE_06.ethg++;
    CORE_03.ethg++ <--> EthernetLink <--> CORE_07.ethg++;
    CORE_03.ethg++ <--> EthernetLink <--> CORE_08.ethg++;
    CORE_04.ethg++ <--> EthernetLink <--> CORE_09.ethg++;
    CORE_04.ethg++ <--> EthernetLink <--> CORE_10.ethg++;
    CORE_05.ethg++ <--> EthernetLink <--> CORE_11.ethg++;
    CORE_05.ethg++ <--> EthernetLink <--> CORE_12.ethg++;
    CORE_06.ethg++ <--> EthernetLink <--> CORE_13.ethg++;
    CORE_06.ethg++ <--> EthernetLink <--> CORE_14.ethg++;
    AGV0_00.ethg++ <--> EthernetLink <--> detcom.dsttg++;
    AGV0_01.ethg++ <--> EthernetLink <--> AGV0_00.ethg++;
    AGV0_02.ethg++ <--> EthernetLink <--> AGV0_00.ethg++;
    AGV0_02.ethg++ <--> EthernetLink <--> AGV0_01.ethg++;
    AGV0_03.ethg++ <--> EthernetLink <--> AGV0_01.ethg++;
    AGV0_04.ethg++ <--> EthernetLink <--> AGV0_01.ethg++;
    AGV0_05.ethg++ <--> EthernetLink <--> AGV0_02.ethg++;
    AGV0_06.ethg++ <--> EthernetLink <--> AGV0_02.ethg++;
}
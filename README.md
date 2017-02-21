#Sloth - DPDK-based High-Performance Latency Generator for WAN simulation etc.

### Behavior: 

Packets are forwarded between interface pairs (like in testpmd). While forwarding, a specific delay is added
to the packets.

### Installation:

Extract DPDK (http://dpdk.org) in any directory, configure it to your NIC needs (especially configure it to compile the required
PMDs). See the DPDK manual for further info. Then hit make install T=<your target>.

Edit the Makefile and adapt RTE\_SDK to the path of the DPDK root directory and RTE\_TARGET to DPDK's platform target you have
compiled.

Adapt run.sh to your needs, especially change the PCI addresses to your interfaces (see dpdk.org for the EAL environment syntax and 
the usage of dpdk-devbind.py Also change the EAL parameters to load the appropriate PMD for your NIC.

Hit make.



### Configuration:

Copy the file latgen-example.xml to latgen.xml. Edit latgen.xml and set your desired latency. 

As Sloth must preallocate the queue length, also adapt the qlen parameter. E.g. the "maxbw" method automatically 
calculates the queue length required for a given maximum bandwidth. If the queue length is too small, packets
are dropped and denoted as such in the stats.

If the "ignoremacs" tag is set, interfaces are enumerated based on the PCI address. If this tag is removed, the
interfaces are instead enumerated based on the MAC addresses stated in the config.

### Misc:

Successfully tested in the following NICs:

- VirtIO
- Intel 82599
- Netronome NFP-4000 Virtual Function

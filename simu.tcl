# ======================================================================
# Define options
# ======================================================================
set opt(chan)  	Channel/WirelessChannel
set opt(prop)		Propagation/TwoRayGround
set opt(netif)		Phy/WirelessPhy
set opt(mac)		Mac/802_11
#set opt(ifq)		Queue/DropTail/PriQueue
set opt(ifq)		CMUPriQueue
set opt(ll)			LL
set opt(ant)        Antenna/OmniAntenna

set opt(x)			600	;# X dimension of the topography
set opt(y)			600		;# Y dimension of the topography
set opt(cp)			""	;# "node_connection"
set opt(sc)			""	;# "node_location"

set opt(ifqlen)		50		;# max packet in ifq
set opt(nn)			[lindex $argv 1]		;# number of nodes
set opt(seed)		0.0
set opt(end)		999999.0		;# simulation time
set opt(simEnd)		[expr $opt(end) + 0.000001]	
set opt(measureStart)		1500.0
set opt(trace_name)	trace.tr		;# trace file
set opt(rp)         DSDV            ;# routing protocol script

set opt(initEnergy)	99999999
set opt(rxPower)	100
set opt(txPower)	100

set opt(clock)		[clock seconds]
set opt(seqNo)		[lindex $argv 0]
# ======================================================================
# needs to be fixed later
set AgentTrace			OFF
set RouterTrace			OFF
set MacTrace			OFF

LL set mindelay_		50us
LL set delay_			25us
LL set bandwidth_		0	;# not used

Queue/DropTail/PriQueue set Prefer_Routing_Protocols    1

# unity gain, omni-directional antennas
# set up the antennas to be centered in the node and 1.5 meters above it
Antenna/OmniAntenna set X_ 0
Antenna/OmniAntenna set Y_ 0
Antenna/OmniAntenna set Z_ 1.5
Antenna/OmniAntenna set Gt_ 1.0
Antenna/OmniAntenna set Gr_ 1.0

# Initialize the SharedMedia interface with parameters to make
# it work like the 914MHz Lucent WaveLAN DSSS radio interface
Phy/WirelessPhy set CPThresh_ 10.0
#Phy/WirelessPhy set CSThresh_ 3.00435e-08			;#Carrier Sensing Range = 80
Phy/WirelessPhy set CSThresh_ 3.92405e-08			;#Carrier Sensing Range = 70
Phy/WirelessPhy set RXThresh_ 7.69113e-08			;#Transmission Range = 50
Phy/WirelessPhy set Rb_ 2*1e6
Phy/WirelessPhy set Pt_ 0.2818
Phy/WirelessPhy set freq_ 914e+6 
Phy/WirelessPhy set L_ 1.0

#Mac/802_11		set		basicRate_		1Mb
#Mac/802_11		set		dataRate_		1Mb

#NS/tcl/mobility/dsdv.tcl
#Agent/DSDV		set perup_				600
Agent/DSDV		set perup_				300

Application/BitTorrent set seqNo_				$opt(seqNo)

Application/BitTorrent set max_peer_num_		[lindex $argv 2]
Application/BitTorrent set max_neighbor_num_	[lindex $argv 3]
Application/BitTorrent set far_max_neighbor_num_ 40

Application/BitTorrent set Near_far_limit_ 2
Application/BitTorrent set far_quantum_  2

Application/BitTorrent set max_upload_num_		4
Application/BitTorrent set max_download_num_	4

# local_rarest_select_flag_ : 1 - PeerTable, 2 - NeighborTable
Application/BitTorrent set local_rarest_select_flag_ 2
# control_tcp_flag_ : 0 - UDP, 1 - TCP
Application/BitTorrent set control_tcp_flag_			0
# use_bidirection_tcp_flag_ : 0 - uni-direction, 1 - bi-direction
Application/BitTorrent set use_bidirection_tcp_flag_	0

# How to select Node to upload - 0 : basic, 1 : Only 1-Hop node, 2 : Closest, 3 : Seed_Random
Application/BitTorrent set selectNodeToUpload_flag_	3

# How to select a random node for optimistic slot or when there is no best slot ( 0: RANDOMLY, 1 : RANDOMLY_HOPS)

Application/BitTorrent set  selectRandomNode_flag_ 0 
                                    
# How to select Neighbors from peerTable - 0 : closest first, 1 : random  2: 2 hops
Application/BitTorrent set selectNeighbor_flag_	1	

# Whether to print the Current PieceList Output(1) or not(0)
Application/BitTorrent set printCPL_flag_			1

Application/BitTorrent set peerTable_flag_				1

Application/BitTorrent set try_upload_interval_			[expr 10* [Application/BitTorrent set max_upload_num_]]
Application/BitTorrent set choking_best_slot_num_			3
Application/BitTorrent set choking_optimistic_slot_num_		1

Application/BitTorrent set into_steady_state_time_		2000
Application/BitTorrent set peer_update_interval1_		1000
Application/BitTorrent set peer_update_interval2_		1000
Application/BitTorrent set neighbor_select_interval1_	[expr 2*[Application/BitTorrent set try_upload_interval_]]
Application/BitTorrent set neighbor_select_interval2_	[expr 2*[Application/BitTorrent set try_upload_interval_]]

Application/BitTorrent set received_bytes_reset_interval_	80

Application/BitTorrent set number_of_nodes_in_simulation_ $opt(nn)

Agent/BitAgent	set flooding_ttl_		[lindex $argv 4]

PiecePool		set piece_num_			100
PiecePool		set block_num_			10
PiecePool		set block_size_			[expr 10*1000]
# ======================================================================

set file_name_	12345
set complete_node_	0

proc usage { argv0 }  {
	puts "Usage: $argv0"
	puts "\tmandatory arguments:"
	puts "\t\t\[-x MAXX\] \[-y MAXY\]"
	puts "\toptional arguments:"
	puts "\t\t\[-cp conn pattern\] \[-sc scenario\] \[-nn nodes\]"
	puts "\t\t\[-seed seed\] \[-stop sec\] \[-tr tracefile\]\n"
}

proc getopt {argc argv} {
	global opt
	lappend optlist cp nn seed sc stop tr x y

	for {set i 0} {$i < $argc} {incr i} {
		set arg [lindex $argv $i]
		if {[string range $arg 0 0] != "-"} continue

		set name [string range $arg 1 end]
		set opt($name) [lindex $argv [expr $i+1]]
	}
}

Application/BitTorrent instproc completeNotice { } {
	global ns_ opt complete_node_

#	puts "completeNotice start"

	set complete_node_ [expr $complete_node_ + 1]
	if { $complete_node_ >= $opt(nn) } {
		$ns_ at [expr [$ns_ now] + 10 - 0.00001] "measure_result"
		$ns_ at [expr [$ns_ now] + 10] "$ns_ halt"
	}

puts "complete - $complete_node_"
}

proc load_script {} {
#
# Source the Connection and Movement scripts
#
	global opt

	if { $opt(cp) == "" } {
		puts "*** NOTE: no connection pattern specified."
			set opt(cp) "none"
	} else {
		puts "Loading connection pattern..."
		source $opt(cp)
	}

	if { $opt(sc) == "" } {
		puts "*** NOTE: no scenario file specified."
			set opt(sc) "none"
	} else {
		puts "Loading scenario file..."
		source $opt(sc)
		puts "Load complete..."
	}
}

proc trace {} {
	global ns_ opt
	
	set tracefd [open $opt(trace_name) w]
	$ns_ trace-all $tracefd

	$ns_ at $opt(end) "$ns_ flush-trace"
	$ns_ at $opt(simEnd) "close $tracefd"
}

proc create-topology {} {
	global ns_ god_ opt node_ bit_
#
# Initialize Global Variables
#
	set chan	[new $opt(chan)]
	set prop	[new $opt(prop)]
	set topo	[new Topography]

	$topo load_flatgrid $opt(x) $opt(y)
	$prop topography $topo

#
# Create God
#
	set god_	[create-god [expr $opt(nn)]]

#
#  Create the specified number of nodes $opt(nn) and "attach" them
#  the channel.
#

#global node setting
	$ns_ node-config -adhocRouting $opt(rp)	\
		 -llType $opt(ll) \
		 -macType $opt(mac) \
		 -ifqType $opt(ifq) \
		 -ifqLen $opt(ifqlen) \
		 -antType $opt(ant) \
		 -propType $opt(prop) \
		 -phyType $opt(netif) \
		 -channel [new $opt(chan)] \
		 -topoInstance $topo \
		 -agentTrace OFF \
		 -routerTrace OFF \
		 -macTrace OFF \
		 -MovementTrace OFF

#		 -energyModel "EnergyModel"	\
#		 -initialEnergy $opt(initEnergy) \
#		 -rxPower $opt(rxPower) \
#		 -txPower $opt(txPower) \


	for {set i 0} {$i < $opt(nn) } {incr i} {
		set node_($i)		[$ns_ node $i]
		set id				[$node_($i) id]
		set bitAgent_($i)	[new Agent/BitAgent]
		set bit_($i)		[new Application/BitTorrent $id $node_($i) $opt(nn)]

		set addr			[$node_($i) node-addr]

		set X_loc			[expr ($i % 10) * 40 + 100]
		set Y_loc			[expr ($i / 10) * 40 + 100]

		$node_($i)	random-motion	0		;# disable random motion
		$node_($i)	set	X_	$X_loc
		$node_($i)	set	Y_	$Y_loc
		$node_($i)	set Z_	0.0

		$ns_ attach-agent $node_($i) $bitAgent_($i)
		$bit_($i) attach-agent $bitAgent_($i)

		puts "node($i) - Loc($X_loc:$Y_loc) - addr($addr) id($id) name($node_($i)) bitAgent($bitAgent_($i))"

		make-piecePool $i

		$ns_ at $opt(measureStart) "$bit_($i) start"

#		$bit_($i) setLogLevel FAIL 1
#		$bit_($i) setLogLevel ERROR 0
#		$bit_($i) setLogLevel DEBUG 1
#		$bit_($i) setLogLevel STATE 0
		$bit_($i) setLogLevel CRITICAL 0
	}
}

proc make-piecePool { i } {
	global ns_ opt bit_ file_name_

	set piecePool	[new PiecePool $file_name_]
	$bit_($i)		setPiecePool	$piecePool
}

proc make-traffic {} {
	global opt bit_ ns_

#	set piecePool [$bit_([expr $opt(nn)/2]) piecePool]
	set piecePool [$bit_(0) piecePool]
	$piecePool setAsComplete

#	for {set i 0} {$i < $opt(nn) } {incr i} {
#		$ns_ at [expr $opt(end)-0.9] "$bit_($i) print"
#	}
}

proc measure_result {} {
	global ns_ bit_ opt bit_ node_

	set max_peer			[Application/BitTorrent set max_peer_num_]
	set max_neighbor		[Application/BitTorrent set max_neighbor_num_]
        
	set ttl					[Agent/BitAgent set flooding_ttl_]
	set tcpFlag				[Application/BitTorrent set control_tcp_flag_]

#set timefd [open N($opt(nn))-CN($max_upload)-BT($best_slot)-OT($optimistic_slot)-PN($piece_num)-BN($block_num)-BS($block_size).time w]
	set timefd [open out-nodenum$opt(nn)-maxpeer$max_peer-maxneighbor$max_neighbor-ttl$ttl-seq$opt(seqNo).time w]

	for {set i 0} {$i < $opt(nn) } {incr i} {
		set start_time [$bit_($i) startTime]
		set finish_time [$bit_($i) finishTime]
		set send_bytes [$bit_($i) sendBytes]
                set x_coordinate [$node_($i) set X_]
                set y_coordinate [$node_($i) set Y_]

		puts $timefd [format "%d\t%.5f\t%.5f\t%.5f" $i $x_coordinate $y_coordinate $finish_time]
	}

	close $timefd
        $bit_(0) printRT
	
	for { set i 0} {$i < $opt(nn) } {incr i} {
		#$bit_($i) printHCB
#		$bit_($i) print
	}

	puts "\nRunning Time : [expr ([clock seconds] - $opt(clock))] secs\n"
}

# ======================================================================
# Main Program
# ======================================================================
#source ../lib/ns-bsnode.tcl
#source ../mobility/com.tcl

# do the get opt again incase the routing protocol file added some more
# options to look for

if {$opt(seed) > 0} {
	puts "Seeding Random number generator with $opt(seed)\n"
	ns-random $opt(seed)
}

#
# Perform Main Configuration
#
set ns_		[new Simulator]
$ns_		use-scheduler Heap

getopt $argc $argv
trace
create-topology
load_script
make-traffic

#
# Do to stop the simulation
#
$ns_ at $opt(end) "measure_result"
$ns_ at $opt(simEnd) "$ns_ halt"

puts "Starting Simulation...seqNo($opt(seqNo) nodeNum($opt(nn))"
$ns_ run

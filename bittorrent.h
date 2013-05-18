/*
  This is the header file of BitTorrent Class.($NS/bittorrent/BitTorrent.h)
  This code is made at Sophia Antipolis INRIA.
 */

//
// BitTorrent
//

#ifndef ns_bittorrent_h
#define ns_bittorrent_h

#include <stdlib.h>
#include <tcl.h>
#include <random.h>

#include "simulator.h"
#include "packet.h"
#include "trace.h"
#include "../webcache/tcpapp.h"
#include "../dsdv/dsdv.h"

#include "../bittorrent/bittcpapp.h"
#include "../bittorrent/bitagent.h"
#include "../bittorrent/bittable.h"

#define MAXRETRY  	100
#define BIG 250
#define NEAR 0
#define FAR 1

enum BitConType { NO_ONE = 0, OWNER, PEER };
enum BitState {
	BEFORE_START = 0,
	HELLO_FLOODING,
	NEIGHBOR_SELECTION,
	PIECE_SELECTION,
	TRY_UPLOAD,
	FINISH_DOWNLOAD,
	STOPPED
};

enum ChokingSlotType { BEST_SLOT = 0, OPTIMISTIC_SLOT, };

enum LogType {
	FAIL = 0,
	ERROR,
	DEBUG,
	STATE,
	CRITICAL_STATE,
	WARNING,
};

enum { UPLOAD = 0, DOWNLOAD };

enum { BASIC = 0, ONE_HOP, CLOSEST, SEED_RANDOM, };

enum { RANDOMLY=0, RANDOMLY_HOPS, };

struct HCBEntry {
	int hopCount_;
	unsigned long upBytes_; 
	unsigned long dwBytes_; 
	int numOfRequest_;
	int numOfAccept_;
	int numOfSend_;
};

struct HCTEntry {
	int hopCount_;
	double time_;
};

struct ratioInfo {
        float ratio0_;
        float ratio25_;
        float ratio50_;
        float ratio75_;
        float ratio100_;
         };

class BitTorrent;
class BitConnection;
class BitTcpApp;
class BitAgent;
class BitTorrentPayload;
class BitTorrentControl;

class BitNeighbor;
class FarBitNeighbor;
class PiecePool;
class PeerTable;
class NeighborTable;
class FarNeighborTable;

class BitPeerUpdateHandler;
class BitNeighborSelecteHandler;
class GlobalRatioInfoList;

class GlobalBitTorrentList {
public:
	GlobalBitTorrentList();
	~GlobalBitTorrentList();

	static GlobalBitTorrentList& instance() {
		return (*instance_);
	}

	int addNode(BitTorrent *node);
	BitTorrent *getNode(int id);

	void printCPL(int min, double now, int seqNo);
        void printCompletenessTimeRatio(int seqNo);
        void Initialise_ratio_Info();
        void Update_ratio_info(int cpt,struct ratioInfo *rI);
        void printsharingInfo(int seqNo);
        void printsharingRatioInfo(int seqNo);
        void printsharing_to_allRatioInfo(int seqNo);
        void printsharing_versus_hops(int seqNo);
        void Compute_numberhops_to_source();

	const Tcl_HashTable *table() { return table_; }
        GlobalRatioInfoList* getratioInfo();

protected:
	Tcl_HashTable *table_;
        GlobalRatioInfoList* GrIL;
	static GlobalBitTorrentList* instance_;
};

class GlobalRatioInfoList {
public:
	GlobalRatioInfoList();
	~GlobalRatioInfoList();
        int length();
        struct ratioInfo get_hop(int cpt);
        float getratio(int cpt,int per);
        void addratio(int cpt,float radded,int per);
        int get_contributing_nodes(int cpt);
        void set_contributing_nodes(int cpt, int contrib);
        void setsharingInfo(int n);
        void setsharingInfovalue(int i,int j, int b);
        void addvaluesharingInfo(int i,int j, int b);
        int getnn();
        int getsharingInfo(int i,int j);

        void computessharingratioInfo();
        float getsharingratioInfo(int i, int j);

        void computessharing_to_allratioInfo();
        float getsharing_to_allratioInfo(int i);

        void computesharing_versus_hops();
        float getsharing_versus_hops(int i);

        void setNumberhopstosource(int i, int h);
        int gethopstosource(int i);

protected:
int leng;
struct ratioInfo Infotable[20];
int sharingInfo[50][50];
float sharingratioInfo[50][50];
float sharing_to_allratioInfo[50][2];
float sharing_versus_hops[20][3];
int nn;
int *contributeinfo;
};

class GlobalBitConnectionList {
public:
	GlobalBitConnectionList();
	~GlobalBitConnectionList();

	static GlobalBitConnectionList& instance() {
		return (*instance_);
	}

	int get_id(int owner_id, int peer_id);

	int addConnection(BitConnection *bitCon);
	BitConnection *getConnection(int owner_id, int peer_id);

	const Tcl_HashTable *table() { return table_; }

protected:
	Tcl_HashTable *table_;
	static GlobalBitConnectionList* instance_;
};

class BitTorrent : public Application {
	friend class BitPeerUpdateHandler;
	friend class BitNeighborSelectHandler;
	friend class BitTryUploadHandler;
	friend class BitReceivedBytesResetHandler;

public:
	BitTorrent(int id, char* tcl_node, int max_node);
	virtual ~BitTorrent();

	virtual int command(int argc, const char*const* argv);
	void trace(int flag, const char* func_name, int line_num, const char *fmt, ...);
	int id() const { return id_; }
	char* tcl_node() { return tcl_node_; }
	int maxNodeNum() const { return max_node_num_; }
	int logLevel() { return log_level_; }
	bool logLevel(int level);
	void setLogLevel(int level, bool onOff);

	void processBitAgentPacket(struct hdr_bitAgent *pkt);
	void processDataPacket(BitTorrentPayload *payload);
	void processControlPacket(BitTorrentControl *control);

        bool neighborsandmehaveeverything();

        void updateratioInfo();
        struct ratioInfo* getratioInfo();
        int HopstoSeed();

	virtual void process_data(int size, AppData* d);
	virtual AppData* get_data(int&, AppData*) {
		// Do not support it
		abort();
		return NULL;
	}

	bool hasFile(int file);

//	DSDV_Agent *routingAgent() { return routingAgent_; }

	PiecePool *piecePool() { return piecePool_; }
	PeerTable *peerTable() { return peerTable_; }
	NeighborTable *neighborTable() { return neighborTable_; }

	int use_bidirection_tcp_flag() { return use_bidirection_tcp_flag_; }

	void getStateStr(char *buff);
	void print();

	int cur_upload_num_;
	int cur_download_num_;
protected:
	// member functions of BitTorrent
	void start();

	int nextPiece();
	int calPieceCount(int piece_id);

	void sendBitPacket(int pktType, int dst, int new1);
	void sendBitPacket2(int pktType, int dst, int flag, int piece_index, int block_index);

	void set_piecePool(PiecePool* pp);

	void peerUpdateCallback(Event *evt);
	void neighborSelectCallback(Event *evt);
	void tryUploadCallback(Event *evt);
	void receivedBytesResetCallback(Event *evt);

	bool isBestSlot();
	bool isOptimisticSlot();

	BitNeighbor* getFirstNode();
	BitNeighbor* getMostDownloadNode();
	BitNeighbor* getClosestNode();
	BitNeighbor* getRandomNode();

        FarBitNeighbor* getFirstFarNode();
	FarBitNeighbor* getMostDownloadFarNode();
	FarBitNeighbor* getRandomFarNode();


	int selectNodeToUploadSerial();
	int selectNodeToUploadParallel(int selectType);		// selectType : BEST_SLOT, OPTIMISTIC_SLOT
	void tryUpload();
	int sendData(int neighbor, int piece_index, int block_index);

	// member variables of BitTorrent
	int id_;						// Node id
	int max_node_num_;				// The maximum number of nodes which will be set at the tcl scripts
	char tcl_node_[128];			// Tcl $node_ name	(ex: _o13)
	Tcl_Channel log_;				// Log file descriptor

	DSDV_Agent *routingAgent_;
	BitAgent *bitAgent_;			// Same variable with agent_

	PiecePool *piecePool_;			// Piece repository

	PeerTable *peerTable_;
	NeighborTable *neighborTable_;
        FarNeighborTable *far_neighborTable_;

	int max_peer_num_;				// binded variable with Tcl script
	int max_neighbor_num_;			// binded variable with Tcl script
        int far_max_neighbor_num_;

	int max_upload_num_;			// binded variable with Tcl script
	int max_download_num_;			// binded variable with Tcl script

	class BitPeerUpdateHandler* peerUpdateHandler_;
	class BitNeighborSelectHandler* neighborSelectHandler_;
	class BitTryUploadHandler* tryUploadHandler_;
	class BitReceivedBytesResetHandler* receivedBytesResetHandler_;

        struct ratioInfo rI;

	unsigned long sendBytes_;		// Send bytes to All Neighbors

	double lastup_;					// time of last update of peerTable
	double startTime_;				// time of command "start" called
	double finishTime_;				// time to finish download
	double steadyStateTime_;		// Tcl bind variables
	double peerUpdateInterval1_;	// Tcl bind variables
	double peerUpdateInterval2_;	// Tcl bind variables
	double neighborSelectInterval1_;	// Tcl bind variables
	double neighborSelectInterval2_;	// Tcl bind variables
	double tryUploadInterval_;		// Tcl bind variables
	double receivedBytesResetInterval_;
      		// Tcl bind variables
        int number_of_nodes_in_simulation_;
        int Near_far_limit_;
        int used_quantum_;
        int far_quantum_;        

	int log_level_;
	BitState state_;

	int *local_rarest_count_;
	int local_rarest_piece_num_;
	int local_rarest_select_flag_;			// flag to select local_rarest from peerTable(1) or from neighborTable(2)
	int control_tcp_flag_;					// flag to use TCP(1) protocol for some control messages (PIECEOFFER, UPDATE_PIECELIST) or use UDP(0)
	int use_bidirection_tcp_flag_;					// flag to use TCP for bi-direction(1) or only uni-direction(0)
	int peerTable_flag_;					// flag to remove the Seed peers when I'm a seed (1) or not (0)

	int selectNodeToUpload_flag_;			// flag for how to select node to upload : 0 - default, 1 - only within 1-hop
        int selectRandomNode_flag_;

	int selectNeighbor_flag_;				// flag for how to select neighbors from peerTable : 0 - closest, 1 - random

	int printCPL_flag_;						// flag about whether to print the Current PieceList output(1) or not(0)

	int choking_slot_index_;
	int served_slot_num_;
	int global_choking_slot_index_;
	int choking_slot_num_;					// choking_slot_num = choking_best_slot_num + choking_optimistic_slot_num
	int choking_best_slot_num_;				// Tcl bind variables
	int choking_optimistic_slot_num_;		// Tcl bind variables

	// Related with Metric
	int seqNo_;								// sequence number of experiments - Only the node with id = 0 will access this variables.

	Tcl_HashTable *hopCountBytesTable_;
        Tcl_HashTable *hopCountTimeTable_;
		// HopCountBytes --> HCB
	void addBytesHCB(int flag, int hopCount, unsigned long bytes);
	void addCountHCB(int pktType, int offer_flag, int hopCount);
        void addTimeHCT(int hopCount);
	void printHCB(int fd,double now);
        void printHCT(int fd,double now);						// make output file for HCB

	void printCPL(double now);

	bool checkEqualPieceList2(int *pieceList1, int node);
	bool checkEqualPieceList(int *pieceList1, int *pieceList2);

	int writeDownloadTime();
};

/***************************************************/
/** Owner - A bittorrent node who want to download **/
/** Peer  - A bittorrent node might upload ***/
/***************************************************/
class BitConnection {
public:
	BitConnection(BitTorrent *owner, BitTorrent *peer);
	~BitConnection();

	BitTorrent* bitOwner() { return bitOwner_; }
	BitTorrent* bitPeer() { return bitPeer_; }
	BitTcpApp* tcpappOwner() { return tcpappOwner_; }
	BitTcpApp* tcpappPeer() { return tcpappPeer_; }

	BitConType whoAmI(BitTorrent *node) {
		if (node->id() == bitOwner_->id()) return OWNER;
		else if (node->id() == bitPeer_->id()) return PEER;
		else return NO_ONE;
	}

	int sendControl(BitTorrentControl *control);
	int sendData(BitTorrentPayload *payload);

	void trace(int flag, const char* func_name, int line_num, const char *fmt, ...);

protected:
	BitTcpApp*		tcpappPeer_;
	BitTcpApp*		tcpappOwner_;
	BitTorrent*		bitPeer_;
	BitTorrent*		bitOwner_;
};

#endif // ns_bittorrent_h

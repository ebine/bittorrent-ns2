/*
  This is the header file of Table Class related with BitTorrent.($NS/bittorrent/bittable.h)
  This code is made at Sophia Antipolis INRIA.
 */

//
// BitTable
//

#ifndef ns_bittorrent_table_h
#define ns_bittorrent_table_h

#include <stdlib.h>
#include <string.h>
#include <tcl.h>

#include "simulator.h"
#include "trace.h"

#include "../bittorrent/bittorrent.h"
#include "../bittorrent/bittcpapp.h"

enum PieceState { INCOMPLETE = 0, DOWNLOADING, COMPLETE };

class BitTorrent;
class BitConnection;
class BitTcpApp;

class BitTorrentControl;
class BitTorrentPayload;
//class GlobalBitTorrentList;

class PiecePool;

//*************************************************************//
/*********** Peer, Neighbor related Data Structure *************/
//*************************************************************//
class BitPeer {
public:
  BitPeer(int id, int hopCount, int piece_num);
	~BitPeer();

	int id() { return id_; }
	int hopCount() { return hopCount_; }

	bool connectFlag() { return connectFlag_; }
	BitConnection *bitCon() { return bitCon_; }

	int pieceNum() { return piece_num_; }

	int*	pieceList() { return pieceList_; }
	void setPieceList(int* pieceList);
	bool hasPiece(int index) {
		if (pieceList_ == NULL) return false;
		if (pieceList_[index] == COMPLETE) return true;
		return false;
	}
	bool isSeed();

	void connect(BitTorrent *owner);
	int sendControl(BitTorrentControl *control, BitTorrent *owner);

	void print();
        void updatehopCount(DSDV_Agent *rA);

protected:
	BitConnection *bitCon_;
	bool connectFlag_;

	int id_;
	int hopCount_;

	int				piece_num_;
	int*			pieceList_;
};

class PeerTable {
public:
	PeerTable(int max_num);
	~PeerTable();

	int addPeer(BitPeer *peer);
	int removePeer(int id);
	BitPeer *getPeer(int id);

	int num() { return peer_num_; }
	int max_num() { return max_num_; }
	Tcl_HashTable *table() { return table_; }

	void removeSeedPeer();

	void print();

protected:
	Tcl_HashTable *table_;
	int max_num_;
	int peer_num_;
};

class BitNeighbor : public BitPeer {
public:
	BitNeighbor(int id, int hopCount, int piece_num);
	~BitNeighbor();

	bool rejectFlag() { return rejectFlag_; }
	void setRejectFlag(bool flag) { rejectFlag_ = flag; }
	bool selectFlag() { return selectFlag_; }
	void setSelectFlag(bool flag) { selectFlag_ = flag; }

	int sendDataSlotIndex() { return sendDataSlotIndex_; }
	void setSendDataSlotIndex(int slot) { sendDataSlotIndex_ = slot; }
	void resetSendDataSlotIndex() { sendDataSlotIndex_ = -1; }

        void update_hops(DSDV_Agent *rA);

	unsigned long getBytes() { return receivedBytes_; }
	void addBytes(int bytes) { receivedBytes_ += (unsigned long)bytes; }
	void resetBytes() { receivedBytes_ = 0; }

	//int sendData(int piece_index, int block_index, int size);
	int sendData(BitTorrentPayload *payload);

	void print();

protected:
	bool rejectFlag_;
	bool selectFlag_;
	int sendDataSlotIndex_;
       
	unsigned long receivedBytes_;
};

class FarBitNeighbor : public BitPeer {
public:
	FarBitNeighbor(int id, int hopCount,int piece_num);
	~FarBitNeighbor();

	bool rejectFlag() { return rejectFlag_; }
	void setRejectFlag(bool flag) { rejectFlag_ = flag; }
	bool selectFlag() { return selectFlag_; }
	void setSelectFlag(bool flag) { selectFlag_ = flag; }

	int sendDataSlotIndex() { return sendDataSlotIndex_; }
	void setSendDataSlotIndex(int slot) { sendDataSlotIndex_ = slot; }
	void resetSendDataSlotIndex() { sendDataSlotIndex_ = -1; }

        void update_hops(DSDV_Agent *rA);

	unsigned long getBytes() { return receivedBytes_; }
	void addBytes(int bytes) { receivedBytes_ += (unsigned long)bytes; }
	void resetBytes() { receivedBytes_ = 0; }

	//int sendData(int piece_index, int block_index, int size);
	int sendData(BitTorrentPayload *payload);

	void print();

protected:
	bool rejectFlag_;
	bool selectFlag_;
	int sendDataSlotIndex_;
        float probability_;
        double begin_;
        double end_;

	unsigned long receivedBytes_;
};

class NeighborTable {
public:
	NeighborTable(int max_num);
	~NeighborTable();

	int addNeighbor(BitNeighbor *peer);
	BitNeighbor *getNeighbor(int id);
	int removeNeighbor(int id);

	void removeAll();
	void resetRejectFlag();
	void resetSelectFlag();
	void resetReceivedBytes();
//	void resetSendDataSlotIndex();

	int num() { return neighbor_num_; }
	int one_hop_num() { return one_hop_num_; }
	int max_num() { return max_num_; }
	Tcl_HashTable *table() { return table_; }
        bool has_piece(int i);

	void print();

protected:
	Tcl_HashTable *table_;
	int max_num_;
	int neighbor_num_;
	int one_hop_num_;
};

class FarNeighborTable {
public:
	FarNeighborTable(int max_num);
	~FarNeighborTable();

	int addFarNeighbor(FarBitNeighbor *peer);
	FarBitNeighbor *getFarNeighbor(int id);
	int removeFarNeighbor(int id);

       void updatehopscount(DSDV_Agent* rA);
       FarBitNeighbor * select_randomNode();
       int Choose_random_hop(int max_h);
       int Count_number_neighbor_hop(int h);
       FarBitNeighbor* Select_Random_neighbor_hops(int h);

	void removeAll();
	void resetRejectFlag();
	void resetSelectFlag();
	void resetReceivedBytes();
//	void resetSendDataSlotIndex();
        int max_hops();

	int num() { return neighbor_num_; }
	int one_hop_num() { return one_hop_num_; }
	int max_num() { return max_num_; }
	Tcl_HashTable *table() { return table_; }

	void print();

protected:
	Tcl_HashTable *table_;
	int max_num_;
	int neighbor_num_;
	int one_hop_num_;
};

//*************************************************************//
/*********** Piece, Block related Data Structure ***************/
//*************************************************************//
class Piece {
public:
	Piece(int id, int block_num, int block_size);
	~Piece ();

	//int& id() { return id_; }
	int id() { return id_; }
	int block_size() { return block_size_; }
	int block_num() { return block_num_; }
	int state() { return state_; }

	void setState(int state);
#ifdef BIT_DOWNLOADING
	void setBlockState(int index, int state);
#else
	void setBlockState(int index, bool state);
#endif
	void checkComplete();

	bool hasBlock(int index);

	char* stateToString();
	char* blockToString();
	void getBlockListStr(char *buf);

	int nextBlock();
	void printBlock(); 

	void print();

protected:
	int block_num_;
	int block_size_;
	int id_;
#ifdef BIT_DOWNLOADING
	int* blockList_;
#else
	bool* blockList_;
#endif
	int state_;
};

class PiecePool : public TclObject {
public:
	PiecePool();
	PiecePool(int file_name);
	~PiecePool();

	virtual int command(int argc, const char*const* argv);

	int			add_piece(Piece *piece);
	Piece*		get_piece(int id);
	bool		has_piece(int id);
	bool		isCompletePiece(int id);

	int			next_block(int piece_id);

	int			setCompletePiece(int piece_id);
	int			setCompleteBlock(int piece_id, int block_id);
#ifdef BIT_DOWNLOADING
	int			setDownloadBlock(int piece_id, int block_id);
#endif

	int piece_size() { return piece_size_; }
	int piece_num() { return piece_num_; }
	int block_num() { return block_num_; }
	int block_size() { return block_size_; }

	int* pieces_state();
	bool doesDownloadComplete();
        float RatioDowloadedPieces();
	bool hasCompletePiece();

	int file_name() { return file_name_; }
	bool isSameFile(int file_name) {
		if (file_name_ == file_name) return true;
		return false;
	}

	void print();

protected:
	void update();

	int file_name_;
	int piece_num_;
	int piece_size_;
	int block_num_;
	int block_size_;
	int* pieces_state_;
	Tcl_HashTable *pieces_;
};

#endif //ns_bittorrent_table_h

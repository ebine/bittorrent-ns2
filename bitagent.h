/*
  This is the header file of BitTorrent Agent Class.($NS/bittorrent/bitagent.h)
  This code is made at Sophia Antipolis INRIA.
 */

//
// BitTorrent Agent
//

#ifndef ns_bittorrent_agent_h
#define ns_bittorrent_agent_h

#include "packet.h"
#include "ip.h"

#include "../bittorrent/bittorrent.h"
#include "../bittorrent/bittable.h"

#define MAX_TTL  			128
#define HDR_BITAGENT(p)		(hdr_bitAgent::access(p))

enum BitPacketType {
	HELLO = 0,
	HELLO_REPLY,
	PIECEOFFER_REQUEST,
	PIECEOFFER_REPLY,
	UPDATE_PIECELIST,
};

enum OfferFlagType {
	REJECT = 0,
	ACCEPT,
	CONTINUE,
        FINAL_REJECT,
};

struct hdr_bitAgent {
	int pktType_;
	int ttl_;
	int src_id_;
	int dst_id_;
        int new_;

	// For HELLO, HELLO_REPLY, UPDATE_PIECE
	int file_name_;
	int piece_num_;
	int* pieceList_;

	// For PIECEOFFER_REPLY
	int offer_flag_;			// 0 - REJECT, 1 - ACCEPT
	int piece_index_;
	int block_index_;

	// Header access methods
	static int offset_;
	inline static int& offset() { return offset_; }
	inline static hdr_bitAgent* access(const Packet* p) {
		return (hdr_bitAgent*) p->access(offset_);
	}

	// Member Functions of hdr_bitAgent
	hdr_bitAgent() {
		file_name_ = 0;
		piece_num_ = 0;
		pieceList_ = NULL;

		piece_index_ = 0;
		block_index_ = 0;
	}
	
	~hdr_bitAgent() {
		if ( (piece_num_ > 0) && pieceList_ != NULL) {
			printf("Free the pieceList of hdr_bigAgent\n");
			delete []pieceList_;
		}
	}

	void copy(struct hdr_bitAgent *bith) {
		pktType_	= bith->pktType_;
		ttl_		= bith->ttl_;
		src_id_		= bith->src_id_;
		dst_id_		= bith->dst_id_;
                new_            = bith->new_;
		file_name_	= bith->file_name_;

		piece_num_	= bith->piece_num_;

		if (piece_num_ > 0) {
			pieceList_ = new int[piece_num_+1];
			for(int i = 0; i < piece_num_; i++) {
				pieceList_[i] = bith->pieceList_[i];
			}
		}

		piece_index_ = bith->piece_index_;
		block_index_ = bith->block_index_;
		return;
	}
};

class BitAgent : public Agent {
public:
	BitAgent();
	~BitAgent();

	bool bitFlag() { return bitFlag_; }
	void setBitFlag(bool flag) { bitFlag_ = flag; }

	int flooding_ttl() { return flooding_ttl_; }

	Packet* allocpacket();

	int sendBitPacket(Packet *pkt, int dst);

	virtual void sendmsg(int nbytes, const char *flags = 0) {
		sendmsg(nbytes, NULL, flags);
	}
	virtual void sendmsg(int nbytes, AppData* data, const char *flags = 0);
	virtual void recv(Packet* pkt, Handler*);
	virtual int command(int argc, const char*const* argv);

protected:
	int flooding_ttl_;
	bool bitFlag_;
};

#endif // ns_bittorrent_agent_h

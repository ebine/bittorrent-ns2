/*
 *
  This is the source code file of BitTorrent Agent Class.($NS/bittorrent/bitagent.cc)

 */

//
// BitTorrent Agent
// 

#include "bitagent.h"

int hdr_bitAgent::offset_;
static class BitAgentHeaderClass : public PacketHeaderClass {
public:
  BitAgentHeaderClass() : PacketHeaderClass("PacketHeader/BitAgent",
							sizeof(hdr_bitAgent)) {
		bind_offset(&hdr_bitAgent::offset_);
	}
} class_bitAgent_hdr;

static class BitAgentClass : public TclClass {
public:
	BitAgentClass() : TclClass("Agent/BitAgent") {}
	TclObject* create(int, const char*const*) {
		return (new BitAgent());
	}
} class_bittorrent_agent;

BitAgent::BitAgent() : Agent(PT_BITTO) {
	bind("flooding_ttl_", &flooding_ttl_);

	bitFlag_ = false;
}

BitAgent::~BitAgent() {
}

Packet* BitAgent::allocpacket() {
	Packet* pkt = allocpkt();
	return pkt;
}

int BitAgent::sendBitPacket(Packet *pkt, int dst) {
	Scheduler &s = Scheduler::instance();
	double now = s.clock();

	struct hdr_bitAgent *bith = hdr_bitAgent::access(pkt);
	struct hdr_ip *iph = HDR_IP(pkt);

	bith->src_id_ = Agent::addr();
	bith->dst_id_ = dst;

	iph->saddr() = Agent::addr();
	iph->daddr() = dst;
	iph->dport() = iph->sport();

	switch(bith->pktType_) {
		// Flooding Packet
		case HELLO:
			bith->ttl_ = flooding_ttl_;
			break;
		// Unicast Packet
		case HELLO_REPLY:
		case PIECEOFFER_REQUEST:
		case PIECEOFFER_REPLY:
		case UPDATE_PIECELIST:
			bith->ttl_ = MAX_TTL;
			break;
		default:
			fprintf(stderr, "[BitAgent:sendBitPacket] now[%.5f] node[%s:%d:%d] invalid pktType[%d] dst[%d]\n", now, name(), here_.addr_, here_.port_, bith->pktType_, dst);
			break;
	}

	send(pkt, (Handler*) 0);
	return 1;
}

void BitAgent::sendmsg(int nbytes, AppData* data, const char* flags) {
	return;
}

void BitAgent::recv(Packet* pkt, Handler*) {
	struct hdr_bitAgent *bith = hdr_bitAgent::access(pkt);
	struct hdr_ip *iph = HDR_IP(pkt);

	Scheduler &s = Scheduler::instance();
	double now = s.clock();

	bool appProcessFlag = false;
//	int pktType = bith->pktType_;

//	printf("[fables:BitAgent:recv] now[%.5f] node[%s:%d:%d] recv packet from [%d:%d] pktType[%d] src[%d] dst[%d]\n", now, name(), here_.addr_, here_.port_, iph->saddr(), iph->sport(), bith->pktType_, bith->src_id_, bith->dst_id_);

	if (bith->src_id_ == here_.addr_) {
		return;
	}

	if (bitFlag_) {
		((BitTorrent *)app_)->processBitAgentPacket(bith);
	}

	if (app_ && appProcessFlag) {
	printf("[fables:BitAgent:recv]  now[%.5f] node[%s:%d:%d] I have app_[%s]\n", now, name(), here_.addr_, here_.port_, app_->name());

		// If an application is attached, pass the data to the app
		hdr_cmn* h = hdr_cmn::access(pkt);
		app_->process_data(h->size(), pkt->userdata());
	}

	if ((u_int32_t)iph->daddr() == IP_BROADCAST && bith->ttl_ > 1) {		// Rebroadcast --> flooding
//	printf("[fables:BitAgent:recv]  now[%.5f] node[%s:%d:%d] I will rebroadcast ttl[%d]\n", now, name(), here_.addr_, here_.port_, bith->ttl_);

		bith->ttl_--;

		Packet *pktre = allocpkt();
		struct hdr_bitAgent *bithre = hdr_bitAgent::access(pktre);
		struct hdr_ip *iphre = HDR_IP(pktre);

		bithre->copy(bith);

		iphre->saddr() = Agent::addr();
		iphre->daddr() = IP_BROADCAST;
		iphre->dport() = iphre->sport();

		send(pktre, (Handler*) 0);
	}

	//if (bith->piece_num_ != 0) delete[] bith->pieceList_;
	Packet::free(pkt);
}


int BitAgent::command(int argc, const char*const* argv) {
	if (argc == 3) {
		if (strcmp(argv[1], "sendBitPacket") == 0) {
			//sendBitPacket(atoi(argv[2]), IP_BROADCAST); prototype changed
			return TCL_OK;
		}
	} else if (argc == 4) {
		if (strcmp(argv[1], "send") == 0) {
			PacketData* data = new PacketData(1 + strlen(argv[3]));
			strcpy((char*)data->data(), argv[3]);
			sendmsg(atoi(argv[2]), data);
			return (TCL_OK);
		}
	} else if (argc == 5) {
		if (strcmp(argv[1], "sendmsg") == 0) {
			PacketData* data = new PacketData(1 + strlen(argv[3]));
			strcpy((char*)data->data(), argv[3]);
			sendmsg(atoi(argv[2]), data, argv[4]);
			return (TCL_OK);
		}
	}
	return (Agent::command(argc, argv));
}

// end of bitagent.cc file

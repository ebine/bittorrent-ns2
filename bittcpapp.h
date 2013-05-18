/*
  This is the header file of BitTorrent Class.($NS/bittorrent/BitTorrent.h)
  This code is made at Sophia Antipolis INRIA.
 */

//
// BitTorrent
//

#ifndef ns_bittcpapp_h
#define ns_bittcpapp_h

#include "../bittorrent/bittorrent.h"
#include "../webcache/tcpapp.h"

class BitTorrent;

////////////////////////////////////////////////////
///// Start of BitTorrentControl class  /////
////////////////////////////////////////////////////
class BitTorrentControl : public AppData {
private:
  // AppData variables
	int size_;
	char* str_;
	
public:
	// BitTorrent variables
	int pktType_;

	int pieceIndex_;
	int blockIndex_;
	int sendSize_;

	int piece_num_;
	int* pieceList_;

	int file_name_;
	int offer_flag_;			// 0 - REJECT, 1 - ACCEPT
	int piece_index_;
	int block_index_;
        int new_;

	int sender_id_;
	int receiver_id_;

	BitTorrentControl(int pktType) : AppData(BITTO_CONTROL), size_(0), str_(NULL) {
		pktType_ = pktType;

		pieceIndex_ = -1;
		blockIndex_ = -1;
		sendSize_ = 4+4+4+4;
                new_=-1;
		piece_num_ = -1;
		pieceList_ = NULL;

		file_name_ = -1;
		offer_flag_ = 0;
		piece_index_ = -1;
		block_index_ = -1;

		sender_id_ = -1;
		receiver_id_ = -1;
	}

	BitTorrentControl(BitTorrentControl& data) : AppData(data) {
		pktType_ = data.pktType_;

		pieceIndex_ = data.pieceIndex_;
		blockIndex_ = data.blockIndex_;
		sendSize_ = data.sendSize_;

		piece_num_ = data.piece_num_;
		if ( (piece_num_ > 0) && (data.pieceList_ != NULL) ) {
			pieceList_ = new int[piece_num_+1];
			for(int i = 0; i < piece_num_; i++) {
				pieceList_[i] = data.pieceList_[i];
			}
		} else {
			pieceList_ = NULL;
		}

		file_name_ = data.file_name_;
		offer_flag_ = data.offer_flag_;
                new_=data.new_;
		piece_index_ = data.piece_index_;
		block_index_ = data.block_index_;

		sender_id_ = data.sender_id_;
		receiver_id_ = data.receiver_id_;

		size_ = data.size_;
		if (size_ > 0) {
			str_ = new char[size_];
			strcpy(str_, data.str_);
		} else str_ = NULL;
	}

	virtual ~BitTorrentControl() {
		if (str_ != NULL) delete []str_;
		if (pieceList_ != NULL) delete []pieceList_;
	}

	char* str() { return str_; }
	virtual int size() const { return AppData::size() + size_; }

	void set_string(const char* s) {
		if ((s == NULL) || (*s == 0)) 
			str_ = NULL, size_ = 0;
		else {
			size_ = strlen(s) + 1;
			str_ = new char[size_];
			assert(str_ != NULL);
			strcpy(str_, s);
		}
	}

	virtual AppData* copy() {
		return new BitTorrentControl(*this);
	}
};
////////////////////////////////////////////////////
///// End of BitTorrentControl Class /////
////////////////////////////////////////////////////

////////////////////////////////////////////////////
///// Start of BitTorrentPayload class  /////
////////////////////////////////////////////////////
class BitTorrentPayload : public AppData {
private:
	// AppData variables
	int size_;
	char* str_;
	
public:
	// BitTorrent variables
	int pieceIndex_;
	int blockIndex_;
	int sendSize_;
	int sender_id_;
	int receiver_id_;

	BitTorrentPayload(int pieceIndex, int blockIndex, int sendSize) : AppData(BITTO_PAYLOAD), size_(0), str_(NULL) {
		pieceIndex_ = pieceIndex;
		blockIndex_ = blockIndex;
		sendSize_ = sendSize;

		sender_id_ = -1;
		receiver_id_ = -1;
	}

	BitTorrentPayload(BitTorrentPayload& data) : AppData(data) {
		pieceIndex_ = data.pieceIndex_;
		blockIndex_ = data.blockIndex_;
		sendSize_ = data.sendSize_;

		sender_id_ = data.sender_id_;
		receiver_id_ = data.receiver_id_;

		size_ = data.size_;
		if (size_ > 0) {
			str_ = new char[size_];
			strcpy(str_, data.str_);
		} else str_ = NULL;
	}

	virtual ~BitTorrentPayload() {
		if (str_ != NULL) delete []str_;
	}

	char* str() { return str_; }
	virtual int size() const { return AppData::size() + size_; }

	void set_string(const char* s) {
		if ((s == NULL) || (*s == 0)) 
			str_ = NULL, size_ = 0;
		else {
			size_ = strlen(s) + 1;
			str_ = new char[size_];
			assert(str_ != NULL);
			strcpy(str_, s);
		}
	}

	virtual AppData* copy() {
		return new BitTorrentPayload(*this);
	}
};
////////////////////////////////////////////////////
///// End of BitTorrentPayload Class /////
////////////////////////////////////////////////////

class BitTcpApp : public TcpApp {
public:
	BitTcpApp(Agent *tcp, BitTorrent *owner, BitTorrent *peer);
	~BitTcpApp();

	BitTorrent* owner() { return owner_; }
	BitTorrent* peer() { return peer_; }

	int sendControl(BitTorrentControl *control);
	int sendData(BitTorrentPayload *payload);

//	virtual void recv(int nbytes);
//	virtual void send(int nbytes, AppData *data);

//	void connect(BitTcpApp *dst) { dst_ = dst; }

	virtual void process_data(int size, AppData* data);
	virtual AppData* get_data(int&, AppData*) {
		// Not supported
		abort();
		return NULL;
	}

	// Do nothing when a connection is terminated
	virtual void resume();

protected:
	virtual int command(int argc, const char*const* argv);

//	BitTcpApp *dst_;
	BitTorrent *owner_;
	BitTorrent *peer_;
};

#endif // ns_bittcpapp_h

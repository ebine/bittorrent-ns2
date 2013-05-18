/*
 *
  This is the source code file of BitTorrent Class.($NS/bittorrent/BitTorrent.cc)
  This code is made at Sophia Antipolis INRIA.
 */

//
// BitTcpApp
// 
#include "bittcpapp.h"

////////////////////////////////////////////////////
///// Start of Static Tcl class  /////
////////////////////////////////////////////////////
static class BitTcpAppClass : public TclClass {
public:
  BitTcpAppClass() : TclClass("Application/TcpApp/BitTcpApp") {}
	TclObject* create(int argc, const char*const* argv) {
		if (argc != 7) return NULL;
		Agent *tcp = (Agent *)TclObject::lookup(argv[4]);
		if (tcp == NULL) return NULL;

		BitTorrent *owner = (BitTorrent *)TclObject::lookup(argv[5]);
		if (owner == NULL) return NULL;
		BitTorrent *peer = (BitTorrent *)TclObject::lookup(argv[6]);
		if (peer == NULL) return NULL;

		return (new BitTcpApp(tcp, owner, peer));
	}
} class_bittcp_app;
////////////////////////////////////////////////////
///// End of Static Tcl Class /////
////////////////////////////////////////////////////


////////////////////////////////////////////////////
///// Start of BitTcpApp class  /////
////////////////////////////////////////////////////
BitTcpApp::BitTcpApp(Agent *tcp, BitTorrent *owner, BitTorrent *peer) : 
	TcpApp(tcp)
{
	owner_ = owner;
	peer_ = peer;
}

BitTcpApp::~BitTcpApp()
{
	agent_->attachApp(NULL);
	// should remove BitTcpApp (BitConnection) from BitTorrent pool??????!!!!!
}

//void BitTcpApp::send(int nbytes, AppData *cbk) {
//	TcpApp::send(nbytes, cbk);
//}

//void BitTcpApp::recv(int size) {
//	TcpApp::recv(size);
//}

void BitTcpApp::resume() { // Do nothing
}

int BitTcpApp::command(int argc, const char*const* argv)
{
	if (strcmp(argv[1], "send") == 0) {
		/*
		 * <app> send <size> <piece_id> <block_id>
		 */
		int size = atoi(argv[2]);
		int piece_index = atoi(argv[3]);
		int block_index = atoi(argv[4]);

		{
			BitTorrentPayload *tmp = new BitTorrentPayload(piece_index, block_index, size);
			send(size, tmp);
		}
		return (TCL_OK);
	}
	return TcpApp::command(argc, argv);
}

int BitTcpApp::sendControl(BitTorrentControl *control) {
//	control->sender_id_ = owner_->id();
//	control->receiver_id_ = peer_->id();

	send(control->sendSize_, control);
	return 1;
}

//int BitTcpApp::sendData(int piece_index, int block_index, int size) {
int BitTcpApp::sendData(BitTorrentPayload *payload) {
	//BitTorrentPayload *payload = new BitTorrentPayload(piece_index, block_index, size);

//	payload->sender_id_ = owner_->id();
//	payload->receiver_id_ = peer_->id();

	// calculate the number of connection for upload and download
	{
		owner_->cur_upload_num_++;
		peer_->cur_download_num_++;
	}

	send(payload->sendSize_, payload);
	return 1;
}

void BitTcpApp::process_data(int size, AppData* data) 
{
	if (data == NULL) return;

	if (data->type() == BITTO_PAYLOAD) {
		// calculate the number of connection for upload and download
		{
			peer_->cur_upload_num_--;
			owner_->cur_download_num_--;
		}

		BitTorrentPayload* bitto = (BitTorrentPayload *)data;

		if (bitto->receiver_id_ == owner_->id()) {
			owner_->processDataPacket(bitto);
		} else if (bitto->receiver_id_ == peer_->id()) {
			peer_->processDataPacket(bitto);
		}
	} else if (data->type() == BITTO_CONTROL) {
		BitTorrentControl* bitto = (BitTorrentControl *)data;
		//owner_->processControlPacket(bitto);
		if (bitto->receiver_id_ == owner_->id()) {
			owner_->processControlPacket(bitto);
		} else if (bitto->receiver_id_ == peer_->id()) {
			peer_->processControlPacket(bitto);
		}
	}
	return;
}
////////////////////////////////////////////////////
///// End of BitTcpApp Class /////
////////////////////////////////////////////////////

// end of BitBitTcpApp.cc file

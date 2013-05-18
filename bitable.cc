/*
 *
  This is the source code file of BitTable Class.($NS/bittorrent/bittable.cc)
  This code is made at Sophia Antipolis INRIA.
 */

//
// BitTable
// 
#include "bittable.h"
#include "math.h"

// Peer related objects
BitPeer::BitPeer(int id, int hopCount, int piece_num) {
  if (id < 0 || hopCount < 0) {
		fprintf(stderr, "[BitPeer:constructor] invalid parameter - id[%d] hopCount[%d]\n",
				id, hopCount);
		abort();
		return;
	}

	id_ = id;
	hopCount_ = hopCount;

	piece_num_ = piece_num;
	pieceList_ = new int[piece_num_+1];
	for(int i = 0; i < piece_num_ + 1; i++) pieceList_[i] = INCOMPLETE;

	connectFlag_ = false;
	bitCon_ = NULL;
	return;
}

BitPeer::~BitPeer() {
	if (pieceList_ && pieceList_ != NULL) delete []pieceList_;
	//????? bitCon_->disconnect()?????!!!!!
}

void BitPeer::setPieceList(int* pieceList) {
	if (pieceList == NULL) {
		fprintf(stderr, "[BitNeighbor:setPieceList] neighbor[%d:%d] NULL pieceList passed\n", id_, hopCount_);
		return;
	}

	for(int i = 0; i < piece_num_; i++) {
		pieceList_[i] = pieceList[i];
	}

	return;
}

bool BitPeer::isSeed() {
	for(int i = 0; i < piece_num_; i++) {
		if (pieceList_[i] != COMPLETE) return false;
	}

	BitTorrent *bitNode = GlobalBitTorrentList::instance().getNode(id_);
	if (bitNode == NULL) return false;
	else {
		if (bitNode->piecePool()->doesDownloadComplete()) return true;
	}
	return false;
}

void BitPeer::updatehopCount(DSDV_Agent *rA)
{
hopCount_= rA->hopCount(id_);
}


// Will be called at BitTorrent before the call sendData and sendControl
void BitPeer::connect(BitTorrent *owner) {
//	printf("\t\tOwner[%d] start to connect node[%d] - flag[%d] bitCon[%x]\n", owner->id(), id_, connectFlag_, bitCon_);
	if (connectFlag_ == true && bitCon_ != NULL) return;

//	printf("\t\tOwner[%d] will make BitConnection to node[%d] - flag[%d] bitCon[%x]\n", owner->id(), id_, connectFlag_, bitCon_);

	BitTorrent* peer = GlobalBitTorrentList::instance().getNode(id());
	if (peer == NULL) {
		fprintf(stderr, "[BitPeer:connect] invalid peer - owner[%d] peer[%d]\n", owner->id(), id());
		return;
	}

	// Check whether the bit connection already exists between owner and peer
	BitConnection* bitCon = GlobalBitConnectionList::instance().getConnection(owner->id(), peer->id());
	if (bitCon != NULL) {
		bitCon_ = bitCon;
		connectFlag_ = true;
//		printf("\t\tOwner[%d] already has BitConnection with node[%d] - flag[%d] bitCon[%x]\n", owner->id(), id(), connectFlag_, bitCon_);
		return;
	} else {
		if ( (owner->use_bidirection_tcp_flag() == 1) && (bitCon = GlobalBitConnectionList::instance().getConnection(peer->id(), owner->id()), bitCon != NULL) ) {
			bitCon_ = bitCon;
			connectFlag_ = true;
		} else {
			//bitCon = new BitConnection(owner, peer);
			bitCon = new BitConnection(owner, peer);
			if (GlobalBitConnectionList::instance().addConnection(bitCon) < 0) {
				fprintf(stderr, "[BitPeer:connect] owner[%d] peer[%d] - addConnection fail\n", owner->id(), id());
				delete bitCon;			//!!!!!!!
				return;
			}
			bitCon_ = bitCon;
			connectFlag_ = true;
	//		printf("\t\tOwner[%d] adds BitConnection with node[%d] - flag[%d] bitCon[%x]\n", owner->id(), id(), connectFlag_, bitCon_);
		}
	}

	return;
}

int BitPeer::sendControl(BitTorrentControl *control, BitTorrent *owner) {
	if ( (connectFlag_ == false) || (bitCon_ == NULL) ) return -1;
	bitCon_->sendControl(control);
	return 1;
}

void BitPeer::print() {
	char buff[piece_num_*2+200];

	sprintf(buff, "Piece_List[");
	for(int i = 0; i < piece_num_; i++) {
		char temp[8];
		sprintf(temp, "%d", pieceList_[i]);
		strcat(buff, temp);
	}
	strcat(buff, "]");

	printf("Peer - ID[%d] bitCon[Owner[%d]:Peer[%d]] HopCount[%d] Piece_Num[%d] %s\n",
			id_,
			bitCon_ == NULL ? -1 : bitCon_->bitOwner()->id(),
			bitCon_ == NULL ? -1 : bitCon_->bitPeer()->id(),
			hopCount_, piece_num_, buff);
}

PeerTable::PeerTable(int max_num) {
	peer_num_ = 0;
	if (max_num > 0) max_num_ = max_num;
	else max_num_ = 0;

	table_ = new Tcl_HashTable;
	Tcl_InitHashTable(table_, TCL_ONE_WORD_KEYS);
}

PeerTable::~PeerTable() {
	if (table_ != NULL) {
		Tcl_HashEntry *he;
		Tcl_HashSearch hs;

		for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
			BitPeer *peer = (BitPeer *)Tcl_GetHashValue(he);
			Tcl_DeleteHashEntry(he);
			delete peer;
		}
		Tcl_DeleteHashTable(table_);
		delete table_;
	}
}

int PeerTable::addPeer(BitPeer *peer) {
	if ( (max_num_ != 0) && (peer_num_ >= max_num_) ) return -1;

	int newEntry = 1;
	Tcl_HashEntry *he = Tcl_CreateHashEntry(table_,
						(const char *) peer->id(),
						&newEntry);
	if (he == NULL) return -1;
	Tcl_SetHashValue(he, (ClientData)peer);
	peer_num_++;
	return 1;
}

int PeerTable::removePeer(int id) {
	Tcl_HashEntry *he = Tcl_FindHashEntry(table_,(const char *)id);
	if (he != NULL) {
		BitPeer *peer = (BitPeer *)Tcl_GetHashValue(he);
		Tcl_DeleteHashEntry(he);
		delete peer;

		peer_num_--;
	}

	return 1;
}

void PeerTable::removeSeedPeer() {
	Tcl_HashEntry *he;
	Tcl_HashSearch hs;

	for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
		BitPeer *peer = (BitPeer *)Tcl_GetHashValue(he);
		if ( (peer != NULL) && (peer->isSeed()) ) {
			removePeer(peer->id());
		}
	}

	return;
}

BitPeer* PeerTable::getPeer(int id) {
	Tcl_HashEntry *he = Tcl_FindHashEntry(table_, (const char *)id);
	if (he == NULL) return NULL;
	return (BitPeer *)Tcl_GetHashValue(he);
}

void PeerTable::print() {
	printf("############## PeerTable Print Start ###############\n");
	printf("Peer_Num[%d] Max_Peer_Num[%d]\n", peer_num_, max_num_);
	{
		Tcl_HashEntry *he;
		Tcl_HashSearch hs;

		for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
			BitPeer *peer = (BitPeer *)Tcl_GetHashValue(he);
			peer->print();
		}
	}
	printf("############## PeerTable Print End #################\n");
}

// Neighbor related objects
BitNeighbor::BitNeighbor(int id, int hopCount, int piece_num) : BitPeer(id, hopCount, piece_num) {
	rejectFlag_ = false;
	selectFlag_ = false;

	resetSendDataSlotIndex();
	resetBytes();
}

BitNeighbor::~BitNeighbor() {
}

int BitNeighbor::sendData(BitTorrentPayload *payload) {
	if ( (connectFlag_ == false) || (bitCon_ == NULL) ) return -1;
	bitCon_->sendData(payload);
	return 1;
}



void BitNeighbor::print() {
	char buff[piece_num_*5+200];

	sprintf(buff, "Piece_List[");
	for(int i = 0; i < piece_num_; i++) {
		char temp[8];
		sprintf(temp, "%d", pieceList_[i]);
		strcat(buff, temp);
	}
	strcat(buff, "]");

	printf("Neighbor - ID[%d] HopCount[%d] Piece_Num[%d] %s rejectFlag[%d] connectFlag[%d] bitCon[Owner[%d]:Peer[%d]] receivedBytes[%ld] sendDataSlotIndex[%d]\n",
			id_, hopCount_, piece_num_, buff, rejectFlag_, connectFlag_,
			bitCon_ == NULL ? -1 : bitCon_->bitOwner()->id(),
			bitCon_ == NULL ? -1 : bitCon_->bitPeer()->id(),
			receivedBytes_, sendDataSlotIndex_
			);
	return;
}

FarBitNeighbor::FarBitNeighbor(int id, int hopCount,int piece_num) : BitPeer(id, hopCount, piece_num) {
	rejectFlag_ = false;
	selectFlag_ = false;

	resetSendDataSlotIndex();
	resetBytes();
}

FarBitNeighbor::~FarBitNeighbor() {
}

int FarBitNeighbor::sendData(BitTorrentPayload *payload) {
	if ( (connectFlag_ == false) || (bitCon_ == NULL) ) return -1;
	bitCon_->sendData(payload);
	return 1;
}



void FarBitNeighbor::print() {
	char buff[piece_num_*5+200];

	sprintf(buff, "Piece_List[");
	for(int i = 0; i < piece_num_; i++) {
		char temp[8];
		sprintf(temp, "%d", pieceList_[i]);
		strcat(buff, temp);
	}
	strcat(buff, "]");

	printf("Neighbor - ID[%d] HopCount[%d] Piece_Num[%d] %s rejectFlag[%d] connectFlag[%d] bitCon[Owner[%d]:Peer[%d]] receivedBytes[%ld] sendDataSlotIndex[%d]\n",
			id_, hopCount_, piece_num_, buff, rejectFlag_, connectFlag_,
			bitCon_ == NULL ? -1 : bitCon_->bitOwner()->id(),
			bitCon_ == NULL ? -1 : bitCon_->bitPeer()->id(),
			receivedBytes_, sendDataSlotIndex_
			);
	return;
}



void FarBitNeighbor::update_hops(DSDV_Agent *rA)
{
hopCount_= rA->hopCount(id_);

}



NeighborTable::NeighborTable(int max_num) {
	one_hop_num_ = 0;
	neighbor_num_ = 0;
	if (max_num > 0) max_num_ = max_num;
	else max_num_ = 0;

	table_ = new Tcl_HashTable;
	Tcl_InitHashTable(table_, TCL_ONE_WORD_KEYS);
}

NeighborTable::~NeighborTable() {
	if (table_ != NULL) {
		Tcl_HashEntry *he;
		Tcl_HashSearch hs;

		for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
			BitNeighbor *neighbor = (BitNeighbor *)Tcl_GetHashValue(he);

			Tcl_DeleteHashEntry(he);
			delete neighbor;
		}
		Tcl_DeleteHashTable(table_);
		delete table_;
	}
}

int NeighborTable::addNeighbor(BitNeighbor *neighbor) {
	if ( (max_num_ != 0) && (neighbor_num_ >= max_num_) ) return -1;

	int newEntry = 1;
	Tcl_HashEntry *he = Tcl_CreateHashEntry(table_,
						(const char *) neighbor->id(),
						&newEntry);
	if (he == NULL) return -1;
	Tcl_SetHashValue(he, (ClientData)neighbor);
	neighbor_num_++;
	if (neighbor->hopCount() == 1) one_hop_num_++;
	return 1;
}

int NeighborTable::removeNeighbor(int id) {
	Tcl_HashEntry *he = Tcl_FindHashEntry(table_,(const char *)id);
	if (he != NULL) {
		BitNeighbor *neighbor = (BitNeighbor *)Tcl_GetHashValue(he);
		Tcl_DeleteHashEntry(he);
		delete neighbor;

		neighbor_num_--;
	}

	return 1;
}

void NeighborTable::removeAll() {
	if (neighbor_num_ <= 0) return;

	Tcl_HashEntry *he;
	Tcl_HashSearch hs;

	for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
		BitNeighbor *neighbor = (BitNeighbor *)Tcl_GetHashValue(he);
		Tcl_DeleteHashEntry(he);
		delete neighbor;
	}

	neighbor_num_ = 0;
	one_hop_num_ = 0;
	return;
}

void NeighborTable::resetRejectFlag() {
	if (neighbor_num_ <= 0) return;

	Tcl_HashEntry *he;
	Tcl_HashSearch hs;

	for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
		BitNeighbor *neighbor = (BitNeighbor *)Tcl_GetHashValue(he);
		neighbor->setRejectFlag(false);
	}

	return;
}

void NeighborTable::resetSelectFlag() {
	if (neighbor_num_ <= 0) return;

	Tcl_HashEntry *he;
	Tcl_HashSearch hs;

	for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
		BitNeighbor *neighbor = (BitNeighbor *)Tcl_GetHashValue(he);
		neighbor->setSelectFlag(false);
	}

	return;
}

void NeighborTable::resetReceivedBytes() {
	if (neighbor_num_ <= 0) return;

	Tcl_HashEntry *he;
	Tcl_HashSearch hs;

	for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
		BitNeighbor *neighbor = (BitNeighbor *)Tcl_GetHashValue(he);
		neighbor->resetBytes();
	}

	return;
}

bool NeighborTable::has_piece(int i)
{
bool answer;
answer=false;
Tcl_HashEntry *he;
Tcl_HashSearch hs;

	for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
		BitNeighbor *neighbor = (BitNeighbor *)Tcl_GetHashValue(he);
		if(neighbor->hasPiece(i)==true)
                {
                answer=true;
                break;
                }
	}

return(answer);
}


/*
void NeighborTable::resetSendDataSlotIndex() {
	if (neighbor_num_ <= 0) return;

	Tcl_HashEntry *he;
	Tcl_HashSearch hs;

	for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
		BitNeighbor *neighbor = (BitNeighbor *)Tcl_GetHashValue(he);
		neighbor->resetSendDataSlotIndex();
	}

	return;
} */



BitNeighbor* NeighborTable::getNeighbor(int id) {
	Tcl_HashEntry *he = Tcl_FindHashEntry(table_, (const char *)id);
	if (he == NULL) return NULL;
	return (BitNeighbor *)Tcl_GetHashValue(he);
}

void NeighborTable::print() {
	printf("############## NeighborTable Print Start ###############\n");
	printf("Neighbor_Num[%d] Max_Neighbor_Num[%d]\n", neighbor_num_, max_num_);
	{
		Tcl_HashEntry *he;
		Tcl_HashSearch hs;

		for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
			BitNeighbor *neighbor = (BitNeighbor *)Tcl_GetHashValue(he);
			neighbor->print();
		}
	}
	printf("############## NeighborTable Print End #################\n");
}


FarNeighborTable::FarNeighborTable(int max_num) {
	one_hop_num_ = 0;
	neighbor_num_ = 0;
	if (max_num > 0) max_num_ = max_num;
	else max_num_ = 0;

	table_ = new Tcl_HashTable;
	Tcl_InitHashTable(table_, TCL_ONE_WORD_KEYS);
}

FarNeighborTable::~FarNeighborTable() {
	if (table_ != NULL) {
		Tcl_HashEntry *he;
		Tcl_HashSearch hs;

		for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
			FarBitNeighbor *neighbor = (FarBitNeighbor *)Tcl_GetHashValue(he);

			Tcl_DeleteHashEntry(he);
			delete neighbor;
		}
		Tcl_DeleteHashTable(table_);
		delete table_;
	}
}

int FarNeighborTable::addFarNeighbor(FarBitNeighbor *neighbor) {
        printf("le nombre maximum est egale a %d \n",max_num_);
        printf("le nombre actuel est egale a %d \n",neighbor_num_);
	if ( (max_num_ != 0) && (neighbor_num_ >= max_num_) ) return -1;

	int newEntry = 1;
	Tcl_HashEntry *he = Tcl_CreateHashEntry(table_,
						(const char *) neighbor->id(),
						&newEntry);
	if (he == NULL)
        {
      return -1;
         printf("he == NULL, add Far neighbor \n");
        }
	Tcl_SetHashValue(he, (ClientData)neighbor);
	neighbor_num_++;
	if (neighbor->hopCount() == 1) one_hop_num_++;
	return 1;
}

int FarNeighborTable::removeFarNeighbor(int id) {
	Tcl_HashEntry *he = Tcl_FindHashEntry(table_,(const char *)id);
	if (he != NULL) {
		FarBitNeighbor *neighbor = (FarBitNeighbor *)Tcl_GetHashValue(he);
		Tcl_DeleteHashEntry(he);
		delete neighbor;
		neighbor_num_--;
	}

	return 1;
}

void FarNeighborTable::removeAll() {
	if (neighbor_num_ <= 0) return;

	Tcl_HashEntry *he;
	Tcl_HashSearch hs;

	for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
		FarBitNeighbor *neighbor = (FarBitNeighbor *)Tcl_GetHashValue(he);
		Tcl_DeleteHashEntry(he);
		delete neighbor;
	}

	neighbor_num_ = 0;
	one_hop_num_ = 0;
	return;
}

void FarNeighborTable::resetRejectFlag() {
	if (neighbor_num_ <= 0) return;

	Tcl_HashEntry *he;
	Tcl_HashSearch hs;

	for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
		FarBitNeighbor *neighbor = (FarBitNeighbor *)Tcl_GetHashValue(he);
		neighbor->setRejectFlag(false);
	}

	return;
}

void FarNeighborTable::resetSelectFlag() {
	if (neighbor_num_ <= 0) return;

	Tcl_HashEntry *he;
	Tcl_HashSearch hs;

	for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
		FarBitNeighbor *neighbor = (FarBitNeighbor *)Tcl_GetHashValue(he);
		neighbor->setSelectFlag(false);
	}

	return;
}

void FarNeighborTable::resetReceivedBytes() {
	if (neighbor_num_ <= 0) return;

	Tcl_HashEntry *he;
	Tcl_HashSearch hs;

	for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
		FarBitNeighbor *neighbor = (FarBitNeighbor *)Tcl_GetHashValue(he);
		neighbor->resetBytes();
	}

	return;
}


void FarNeighborTable::updatehopscount(DSDV_Agent * rA)
{
 Tcl_HashEntry *he;
 Tcl_HashSearch hs;
  for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) 
  {
  FarBitNeighbor *neighbor = (FarBitNeighbor *)Tcl_GetHashValue(he);
  neighbor->update_hops(rA);
  int h =neighbor->hopCount();
  //if (h==BIG)
  //{
  //removeFarNeighbor(neighbor->id());
  //}
}
}


int FarNeighborTable::max_hops()
{
 Tcl_HashEntry *he;
 Tcl_HashSearch hs;
 int max;
 max=0;
for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
			FarBitNeighbor *neighbor = (FarBitNeighbor *)Tcl_GetHashValue(he);
                          printf("neighbor->hopCount()== %d \n",neighbor->hopCount());
		      if(neighbor->hopCount() >= max) /*&& neighbor->hopCount()!=BIG*/
                        {
                         max = neighbor->hopCount();
                        }
}
printf("max= %d \n",max);
return(max);
}



int FarNeighborTable::Choose_random_hop(int max_hop)
{
double *probability = new double[max_hop+1];
double *begin_interval = new double[max_hop+1];
double *end_interval = new double[max_hop+1];
double S;
int h;

S= (double) ( 2*(double)max_hop - 1);

        begin_interval[1]=(double)0; 
       for(h=1;h<max_hop+1;h++)
       {
              if (h> (max_hop - 2))
              {
             probability[h]= (double)((double)h/(double)S);
        printf("probability of %d is %f \n", h,probability[h]);
              }
              else
              {
              probability[h]=0;
        printf("probability of %d is %f \n",h,probability[h]);
              }

              end_interval[h]= begin_interval[h] + probability[h];

              if (h!=max_hop)
              {
             begin_interval[h+1] = end_interval[h];
              }
      }

double rn = Random::uniform();

for(h=1;h<max_hop+1;h++)
{
if (begin_interval[h]<=(double)rn && end_interval[h] > (double)rn)
{
break;
}
}
delete probability;
probability=NULL;
delete begin_interval;
begin_interval=NULL;
delete end_interval;
end_interval=NULL; 
return(h);
}

int FarNeighborTable::Count_number_neighbor_hop(int h)
{
int n;
n=0;
Tcl_HashEntry *he;
Tcl_HashSearch hs;
for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
			FarBitNeighbor *neighbor = (FarBitNeighbor *)Tcl_GetHashValue(he);
if (neighbor->hopCount()==h)
{
n++;
}
}
return(n);
}

FarBitNeighbor * FarNeighborTable::Select_Random_neighbor_hops(int h)
{
printf("Selecting random neighbor hops\n");
FarBitNeighbor *r_neighbor;
int number=Count_number_neighbor_hop(h);
printf("Le nombre de noeuds se trouvant a %d sauts est egal a %d\n",h,number);

int random1 = Random::integer(number);
while (random1==0)
{
random1 = 1;
}
printf("Le nombre aleatoire choisit est : %d \n",random1);
int occurrence = 0;
Tcl_HashEntry *he;
Tcl_HashSearch hs;
for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
			FarBitNeighbor *neighbor = (FarBitNeighbor *)Tcl_GetHashValue(he);
if (neighbor->hopCount()==h)
{
occurrence++;
printf("occurence = %d \n", occurrence);

if (occurrence==random1)
{
r_neighbor=neighbor;
break;
}
}
}

return(r_neighbor);
}

FarBitNeighbor * FarNeighborTable::select_randomNode()
{
FarBitNeighbor *neighbor;
int max_h = max_hops();
printf("le nombre maximum de sauts est egale a : %d \n", max_h);
if (max_h == 0)
{
return(NULL);
}
int h = Choose_random_hop(max_h);
while (Count_number_neighbor_hop(h)==0)
{
h = Choose_random_hop(max_h);
}
printf("le nombre de saut choisit aleatoirement est : %d \n",h);
neighbor=Select_Random_neighbor_hops(h);
printf("le voisin choisit est %d \n", neighbor->id());
return(neighbor);
}


FarBitNeighbor* FarNeighborTable::getFarNeighbor(int id) {
	Tcl_HashEntry *he = Tcl_FindHashEntry(table_, (const char *)id);
	if (he == NULL) return NULL;
	return (FarBitNeighbor *)Tcl_GetHashValue(he);
}

void FarNeighborTable::print() {
	printf("############## NeighborTable Print Start ###############\n");
	printf("Neighbor_Num[%d] Max_Neighbor_Num[%d]\n", neighbor_num_, max_num_);
	{
		Tcl_HashEntry *he;
		Tcl_HashSearch hs;

		for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
			FarBitNeighbor *neighbor = (FarBitNeighbor *)Tcl_GetHashValue(he);
			neighbor->print();
		}
	}
	printf("############## FarNeighborTable Print End #################\n");
}
////////////////////////////////////////////////////
///// Start of implementation of PiecePool Class /////
////////////////////////////////////////////////////
static class PiecePoolClass : public TclClass {
public:
	PiecePoolClass() : TclClass("PiecePool") {}
	TclObject* create(int argc, const char*const* argv) {
		if (argc != 5) return NULL;
		return (new PiecePool(atoi(argv[4])));
	}
} class_piecepool_agent;

Piece::Piece(int id, int block_num, int block_size) {
	id_ = id;
	block_num_ = block_num;
	block_size_ = block_size;
#ifdef BIT_DOWNLOADING
	blockList_ = new int[block_num_+1];
	for(int i = 0; i < block_num + 1; i++) blockList_[i] = INCOMPLETE;
#else
	blockList_ = new bool[block_num_+1];
	for(int i = 0; i < block_num + 1; i++) blockList_[i] = false;
#endif

	state_ = INCOMPLETE;
}

Piece::~Piece () {
	if (blockList_) delete []blockList_;
}

char* Piece::stateToString() {
	switch(state_) {
		case COMPLETE:	return strdup("COMPLETE");
		case DOWNLOADING:	return strdup("DOWNLOADING");
		case INCOMPLETE:	return strdup("IMCOMPLETE");
		default: return NULL;
	}
	return NULL;
}

void Piece::setState(int state) {
	switch(state) {
		case COMPLETE:
			state_ = state;
#ifdef BIT_DOWNLOADING
			for (int i = 0; i < block_num_; i++) blockList_[i] = COMPLETE;
#else
			for (int i = 0; i < block_num_; i++) blockList_[i] = true;
#endif
			break;
		case DOWNLOADING:
			state_ = state;
			break;
		case INCOMPLETE:
			state_ = state;
#ifdef BIT_DOWNLOADING
			for (int i = 0; i < block_num_; i++) blockList_[i] = INCOMPLETE;
#else
			for (int i = 0; i < block_num_; i++) blockList_[i] = false;
#endif
			break;
		default:
			break;
	}
	return;
}

#ifdef BIT_DOWNLOADING
void Piece::setBlockState(int index, int state) {
#else
void Piece::setBlockState(int index, bool state) {
#endif
	if (index < 0 || index >= block_num_) return;

	blockList_[index] = state;
	checkComplete();
	return;
}

void Piece::checkComplete() {
	bool completeFlag = true, imcompleteFlag = true;

	for (int i = 0; i < block_num_; i++) {
#ifdef BIT_DOWNLOADING
		if (blockList_[i] == COMPLETE) {
#else
		if (blockList_[i]) {
#endif
			imcompleteFlag = false;
		} else {
			completeFlag = false;
		}
	}

	if (completeFlag) state_ = COMPLETE;
//	if (imcompleteFlag) state_ = INCOMPLETE;
//	if ( (completeFlag == false) && (imcompleteFlag == false) ) state_ = DOWNLOADING;

	return;
}

bool Piece::hasBlock(int index) {
	if (index < 0 || index >= block_num_) return false;
//	if (state_ == COMPLETE) return true;
#ifdef BIT_DOWNLOADING
	if (blockList_[index] == COMPLETE) return true;
#else
	if (blockList_[index]) return true;
#endif
	return false;
}

//!!!!!!!!!!!!!!!!!!!!
char* Piece::blockToString() {
//		char temp[1024];
//		for (int i = 0; i < block_num_; i++) {
//		}
	return NULL;
}

void Piece::getBlockListStr(char *buf) {
	memset(buf, 0x00, sizeof(buf));
	sprintf(buf, "BlockList - [");
	for (int i = 0; i < block_num_; i++) {
		if ( ((i % 5) == 0) && (i != 0) ) strcat(buf, " ");
		char temp[3];
		memset(temp, 0x00, sizeof(temp));
		sprintf(temp, "%d", blockList_[i]);
		strcat(buf, temp);
	}
	strcat(buf, "]");
}

void Piece::print() {
//	char buf[262144];
	char buf[block_num_*2+100];

	memset(buf, 0x00, sizeof(buf));
	sprintf(buf, "BlockList[");
	for (int i = 0; i < block_num_; i++) {
		if ( ((i % 5) == 0) && (i != 0) ) strcat(buf, " ");
		char temp[3];
		memset(temp, 0x00, sizeof(temp));
		sprintf(temp, "%d", blockList_[i]);
		strcat(buf, temp);
	}
	strcat(buf, "]");
	printf("Piece - ID[%d] State[%d] %s\n", id_, state_, buf);
}

int Piece::nextBlock() {
	for (int i = 0; i < block_num_; i++) {
#ifdef BIT_DOWNLOADING
		if (blockList_[i] == INCOMPLETE) return i;
#else
		if (blockList_[i] != true) return i;
#endif
	}
#ifdef BIT_DOWNLOADING
	if (state_ == COMPLETE) return -1;
	else {
		for (int i = 0; i < block_num_; i++) {
			if (blockList_[i] == DOWNLOADING) return i;
		}
		printf("Something Wrong in nextBlock!!!!!!! - piece[%d] state[%d]\n", id_, state_);
		print();
		abort();
		return -1;
	}
#endif
	state_ = COMPLETE;
	return -1;
}

void Piece::printBlock() {
	//!!!!!!!!!!!
}

PiecePool::PiecePool() {
	bind("piece_num_", &piece_num_);
	bind("block_num_", &block_num_);
	bind("block_size_", &block_size_);

	piece_size_ = 0;

	pieces_ = NULL;
	pieces_state_ = NULL;
}

PiecePool::PiecePool(int file_name) {
	pieces_state_ = NULL;

	file_name_ = file_name;

	bind("piece_num_", &piece_num_);
	bind("block_num_", &block_num_);
	bind("block_size_", &block_size_);

	piece_size_ = block_num_ * block_size_;

	if (piece_num_ <= 0 || block_num_ <= 0 || block_size_ <= 0) {
		fprintf(stderr, "[BitTorrent:PiecePool:constructor] invalid parameters - piece_num[%d] block_num[%d] block_size[%d]\n", piece_num_, block_num_, block_size_);
		abort();
		return;
	}

	pieces_ = new Tcl_HashTable;
	Tcl_InitHashTable(pieces_, TCL_ONE_WORD_KEYS);
	pieces_state_ = new int[piece_num_+1];

	for(int i = 0; i < piece_num_; i++) {
		Piece *piece = new Piece(i, block_num_, block_size_);
		add_piece(piece);
		pieces_state_[i] = piece->state();
	}

	pieces_state_[piece_num_] = COMPLETE;
}

PiecePool::~PiecePool()
{
	if (pieces_state_ != NULL) {
		delete []pieces_state_;
	}

	if (pieces_ != NULL) {
		Tcl_HashEntry *he;
		Tcl_HashSearch hs;

		for (he = Tcl_FirstHashEntry(pieces_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
			Piece *piece = (Piece *)Tcl_GetHashValue(he);
			Tcl_DeleteHashEntry(he);
			delete piece;
		}
		Tcl_DeleteHashTable(pieces_);
		delete pieces_;
	}
}

int PiecePool::setCompletePiece(int piece_id) {
	if (piece_id < 0 || piece_id >= piece_num_) return -1;

	Piece* piece = get_piece(piece_id);
	if (piece != NULL) {
		piece->setState(COMPLETE);
	} else return -1;
	return 1;
}

int PiecePool::setCompleteBlock(int piece_id, int block_id) {
	if (piece_id < 0 || piece_id >= piece_num_) return -1;
	if (block_id < 0 || block_id >= block_num_) return -1;

	Piece* piece = get_piece(piece_id);
	if (piece != NULL) {
#ifdef BIT_DOWNLOADING
		piece->setBlockState(block_id, COMPLETE);
#else
		piece->setBlockState(block_id, true);
#endif
	} else return -1;
	return 1;
}

#ifdef BIT_DOWNLOADING
int PiecePool::setDownloadBlock(int piece_id, int block_id) {
	if (piece_id < 0 || piece_id >= piece_num_) return -1;
	if (block_id < 0 || block_id >= block_num_) return -1;

	Piece* piece = get_piece(piece_id);
	if (piece != NULL) {
		piece->setBlockState(block_id, DOWNLOADING);
	} else return -1;
	return 1;
}
#endif

int PiecePool::command(int argc, const char*const* argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "listPiece") == 0) {
			Tcl_HashEntry *he;
			Tcl_HashSearch hs;

			for (he = Tcl_FirstHashEntry(pieces_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
				char buff[262144], *temp;
				Piece *piece = (Piece *)Tcl_GetHashValue(he);

				temp = piece->stateToString();
				printf("Piece[%d] - state[%s] block_num[%d] block_size[%d]\n", piece->id(), temp, piece->block_num(), piece->block_size());
				piece->getBlockListStr(buff);
				printf("%s\n", buff);
				memset(buff, 0x00, sizeof(buff));
				delete temp;
			}
			return TCL_OK;
		} else if (strcmp(argv[1], "setAsComplete") == 0) {
			for (int i = 0; i < piece_num_; i++) {
				setCompletePiece(i);
			}
			update();
			return TCL_OK;
		} else if (strcmp(argv[1], "print") == 0) {
			print();
			return TCL_OK;
		}
	} else if (argc == 3) {
		if (strcmp(argv[1], "setCompletePiece") == 0) {
			//if (setCompletePiece(atoi(argv[2])) < 0) return TCL_ERROR;
			setCompletePiece(atoi(argv[2]));
			return TCL_OK;
		}
	} else if (argc == 4) {
		if (strcmp(argv[1], "setCompleteBlock") == 0) {
			//if (setCompleteBlock(atoi(argv[2]), atoi(argv[3])) < 0) return TCL_ERROR;
			setCompleteBlock(atoi(argv[2]), atoi(argv[3]));
			return TCL_OK;
		}
	}
	return TclObject::command(argc, argv);
}

int PiecePool::add_piece(Piece *piece) {
	if (piece == NULL) return -1;

	int newEntry = 1;
	//long key = piece->id();
	Tcl_HashEntry *he = Tcl_CreateHashEntry(pieces_,
			//(const char *)key,
			(const char *)piece->id(),
			&newEntry);
	if (he == NULL) return -1;

	Tcl_SetHashValue(he, (ClientData)piece);
	return 0;
}

bool PiecePool::isCompletePiece(int id) {
	return has_piece(id);
}

bool PiecePool::has_piece(int id) {
	Piece *piece = get_piece(id);
	if (piece == NULL) {
		fprintf(stderr, "[BitTorrent:PiecePool:has_piece] id[%d] - get_piece returned NULL\n", id);
		return false;
	}

	if (piece->state() == COMPLETE) return true;
	//!!!!!!!!!! How to handle DOWNLOADING!!!!!!!!!!!!?????????
	//Not to return true for DOWNLOADING
	return false;
}

Piece* PiecePool::get_piece(int id)
{
	Tcl_HashEntry *he = Tcl_FindHashEntry(pieces_, (const char *)id);
	if (he == NULL) {
		fprintf(stderr, "[BitTorrent:PiecePool:get_piece] id[%d] - he is NULL\n", id);
		return NULL;
	}
	return (Piece *)Tcl_GetHashValue(he);
}

int PiecePool::next_block(int piece_id) {
	Piece *piece = get_piece(piece_id);
	if (piece == NULL) {
		fprintf(stderr, "[BitTorrent:PiecePool:next_block] can not find piece_id[%d]\n", piece_id);
		return -1;
	}

	if (piece->state() == COMPLETE) return -1;
	return piece->nextBlock();
}

int* PiecePool::pieces_state() {
	update();
	return pieces_state_;
}

bool PiecePool::hasCompletePiece() {
//	static bool flag = false;
//	printf("\tPiecePool[%s] - flag[%d]\n", name(), flag);

//	if (flag == true) return true;

	update();

	for(int i = 0; i < piece_num_; i++) {
		if (pieces_state_[i] == COMPLETE) {
//			flag = true;
//			return flag;
			return true;
		}
	}
//	return flag;
	return false;
}

bool PiecePool::doesDownloadComplete() {
	update();

	for(int i = 0; i < piece_num_; i++) {
		if (pieces_state_[i] != COMPLETE) {
			return false;
		}
	}

	return true;
}
float PiecePool::RatioDowloadedPieces() {
       float completed_pieces=0;
       float ratio;
       update();
       
       for(int i = 0; i < piece_num_; i++) {
		if (pieces_state_[i] == COMPLETE) {
			completed_pieces=completed_pieces+1;
		}
	}
        ratio= (float) (completed_pieces/(float)piece_num_);

	return(ratio);

}

void PiecePool::update() {
	for(int i = 0; i < piece_num_; i++) {
		Piece *piece = get_piece(i);
		if (piece == NULL) {
			fprintf(stderr, "[BitTorrent:PiecePool:update] id[%d] - piece is NULL\n", i);
			return;
		}
		pieces_state_[i] = piece->state();
	}

	return;
}

void PiecePool::print() {
	printf("############## PiecePool Print Start ###############\n");
	printf("file_name[%d], piece[%dnums:%dbytes] block[%dnums:%dbytes]\n", file_name(), piece_num(), piece_size(), block_num(), block_size());
	for(int i = 0; i < piece_num(); i++) {
		Piece *piece = get_piece(i);
		if (piece == NULL) abort();
		else piece->print();
	}
	printf("############### PiecePool Print End ###############\n");
}

////////////////////////////////////////////////////
///// End of implementation of PiecePool Class /////
////////////////////////////////////////////////////

// end of BitTable.cc file

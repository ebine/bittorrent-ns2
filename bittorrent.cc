  /*
	*
	This is the source code file of BitTorrent Class.($NS/bittorrent/BitTorrent.cc)
	This code is made at Sophia Antipolis INRIA.
	*/
	
	//
	// BitTorrent
	// 
	#include <assert.h>
	#include <string.h>
	#include <stdarg.h>
	
	#include "bittorrent.h"
	
	#define log_fail(buf, ...)		trace(FAIL, __func__, __LINE__, buf, ##__VA_ARGS__)
	#define log_error(buf, ...)		trace(ERROR, __func__, __LINE__, buf, ##__VA_ARGS__)
	#define log_state(buf, ...)		trace(STATE, __func__, __LINE__, buf, ##__VA_ARGS__)
	#define log_debug(buf, ...)		trace(DEBUG, __func__, __LINE__, buf, ##__VA_ARGS__)
	#define log_critical(buf, ...)	trace(CRITICAL_STATE, __func__, __LINE__, buf, ##__VA_ARGS__)
	#define log_warning(buf, ...)	trace(WARNING, __func__, __LINE__, buf, ##__VA_ARGS__)
	
	GlobalBitTorrentList globalList;
	GlobalBitTorrentList* GlobalBitTorrentList::instance_;
	
	GlobalBitConnectionList globalConList;
	GlobalBitConnectionList* GlobalBitConnectionList::instance_;
	
	////////////////////////////////////////////////////
	///// Start of Static Tcl class  /////
	////////////////////////////////////////////////////
	static class BitTorrentClass : public TclClass {
	public:
		BitTorrentClass() : TclClass("Application/BitTorrent") {}
		TclObject* create(int argc, const char*const* argv) {
			if (argc != 7) return NULL;
	
			int id = atoi(argv[4]);
			int max_node = atoi(argv[6]);
			if (id < 0) return NULL;
			if (max_node <= 0) return NULL;
			return (new BitTorrent(id, (char *)argv[5], max_node));
		}
	} class_BitTorrent_app;
	
	////////////////////////////////////////////////////
	///// End of Static Tcl class  /////
	////////////////////////////////////////////////////
	
	class BitPeerUpdateHandler : public TimerHandler {
	public:
		BitPeerUpdateHandler(BitTorrent *bit) { bit_ = bit; }
		virtual void expire(Event *e) { bit_->peerUpdateCallback(e); }
	
	private:
		BitTorrent *bit_;
	};
	
	class BitNeighborSelectHandler : public TimerHandler {
	public:
		BitNeighborSelectHandler(BitTorrent *bit) { bit_ = bit; }
		virtual void expire(Event *e) { bit_->neighborSelectCallback(e); }
	
	private:
		BitTorrent *bit_;
	};
	
	class BitTryUploadHandler : public TimerHandler {
	public:
		BitTryUploadHandler(BitTorrent *bit) { bit_ = bit; }
		virtual void expire(Event *e) { bit_->tryUploadCallback(e); }
	
	private:
		BitTorrent *bit_;
	};
	
	class BitReceivedBytesResetHandler : public TimerHandler {
	public:
		BitReceivedBytesResetHandler(BitTorrent *bit) { bit_ = bit; }
		virtual void expire(Event *e) { bit_->receivedBytesResetCallback(e); }
	
	private:
		BitTorrent *bit_;
	};
	
	////////////////////////////////////////////////////
	///// Start of Global class  /////
	////////////////////////////////////////////////////
	GlobalBitTorrentList::GlobalBitTorrentList() {
		table_ = new Tcl_HashTable;
		GrIL = new GlobalRatioInfoList();
		Tcl_InitHashTable(table_, TCL_ONE_WORD_KEYS);
	
		instance_ = this;
	}
	
	GlobalBitTorrentList::~GlobalBitTorrentList() {
		if (table_ != NULL) {
			Tcl_DeleteHashTable(table_);
			delete table_;
		}
	
		instance_ = NULL;
	}
	
	int GlobalBitTorrentList::addNode(BitTorrent *node) {
		if (node == NULL)  return -1;
	
		int newEntry = 1;
		Tcl_HashEntry *he = Tcl_CreateHashEntry(table_,
							(const char *) node->id(),
							&newEntry);
		if (he == NULL) return -1;
		Tcl_SetHashValue(he, (ClientData)node);
		return 1;
	}
	
	BitTorrent* GlobalBitTorrentList::getNode(int id) {
		Tcl_HashEntry *he = Tcl_FindHashEntry(table_, (const char *)id);
		if (he == NULL) return NULL;
		return (BitTorrent *)Tcl_GetHashValue(he);
	}
	
	void GlobalBitTorrentList::printCPL(int min, double now, int seqNo) {
		FILE *fp = NULL;
		char fileName[128];
	
		//sprintf(fileName, "cpl_min%d_now%f", min, now);
		sprintf(fileName, "cpl_min%d_seq%d", min, seqNo);
		if (fp = fopen(fileName, "w"), fp == NULL) {
			printf("\t!!GlobalBitTorrentList fopen fail for CPL - file_name[%s]\n", fileName);
			return;
		}
	
		fprintf(fp, "### seqNo[%d] now[%.5f] - NodeId\tComplete Piece ID\n", seqNo, now);
	
		Tcl_HashEntry *he;
		Tcl_HashSearch hs;
	
		for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
			BitTorrent *node = (BitTorrent *)Tcl_GetHashValue(he);
			if (node == NULL) continue;
	
			PiecePool* piecePool = node->piecePool();
			if (piecePool == NULL) continue;
	
			int pieceNum = piecePool->piece_num();
			int *pieceList = piecePool->pieces_state();
	
			for(int i = 0; i < pieceNum; i++) {
				if (pieceList[i] == COMPLETE) {
					fprintf(fp, "%d\t%d\n", node->id(), i);
				}
			}
		}
	
		fclose(fp);
		return;
	}
	void GlobalBitTorrentList::printCompletenessTimeRatio(int seqNo) {
		FILE *fp = NULL;
		char fileName[128];
		
	
		//sprintf(fileName, "cpl_min%d_now%f", min, now);
		sprintf(fileName, "CompletenesstimeRatio_simulation%d",seqNo);
		if (fp = fopen(fileName, "w"), fp == NULL) {
			printf("\t!!GlobalBitTorrentList fopen fail for CTR - file_name[%s]\n", fileName);
			return;
		}
	
		Tcl_HashEntry *he;
		Tcl_HashSearch hs;
	
		Initialise_ratio_Info();
	
		for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
			BitTorrent *node = (BitTorrent *)Tcl_GetHashValue(he);
			if (node == NULL) continue;
			struct ratioInfo *rI= (struct ratioInfo *) node->getratioInfo();
			int number_of_hops_to_source = node->HopstoSeed();
			Update_ratio_info(number_of_hops_to_source,rI);
		}
		for (int cpt=0; cpt< GrIL->length();cpt++)
		{
		fprintf(fp, "%d\t%f\t%f\t%f\t%f\t%f\n",cpt,GrIL->getratio(cpt,0),GrIL->getratio(cpt,25),GrIL->getratio(cpt,50),GrIL->getratio(cpt,75),GrIL->getratio(cpt,100));
		}
		fclose(fp);
		return;
	
	
	}
	
	void GlobalBitTorrentList::printsharingInfo(int seqNo) {
		FILE *fp = NULL;
		char fileName[128];
		
	
		//sprintf(fileName, "cpl_min%d_now%f", min, now);
		sprintf(fileName, "SharingInfo%d",seqNo);
		if (fp = fopen(fileName, "w"), fp == NULL) {
			printf("\t!!GlobalBitTorrentList fopen fail for CTR - file_name[%s]\n", fileName);
			return;
		}
	
		for (int i=0; i<GrIL->getnn(); i++)
		{
		for(int  j=0; j<GrIL->getnn();j++)
		{
		fprintf(fp, "%d ",GrIL->getsharingInfo(i,j));
		}
		fprintf(fp, "\n");
		}
		fclose(fp);
		return;
	
	
	}
	
	void GlobalBitTorrentList::printsharingRatioInfo(int seqNo) {
		FILE *fp = NULL;
		char fileName[128];
		
	
		//sprintf(fileName, "cpl_min%d_now%f", min, now);
		sprintf(fileName, "SharingRatioInfo%d",seqNo);
		if (fp = fopen(fileName, "w"), fp == NULL) {
			printf("\t!!GlobalBitTorrentList fopen fail for CTR - file_name[%s]\n", fileName);
			return;
		}
	
		GrIL->computessharingratioInfo();
	
		for (int i=0; i<GrIL->getnn(); i++)
		{
		for(int  j=0; j<GrIL->getnn();j++)
		{
		fprintf(fp, "%.5f ",GrIL->getsharingratioInfo(i,j));
		}
		fprintf(fp, "\n");
		}
		fclose(fp);
		return;
	
	
	}
	
	void GlobalBitTorrentList:: Compute_numberhops_to_source()
	{
		Tcl_HashEntry *he;
		Tcl_HashSearch hs;
	
		for(he = Tcl_FirstHashEntry(table_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
			BitTorrent *node = (BitTorrent *)Tcl_GetHashValue(he);
			//if (node == NULL) continue;
			//struct ratioInfo *rI= (struct ratioInfo *) node->getratioInfo();
			int number_of_hops_to_source = node->HopstoSeed();
			//Update_ratio_info(number_of_hops_to_source,rI);
			GrIL->setNumberhopstosource(node->id(), node->HopstoSeed());
		}
	}
	void GlobalBitTorrentList:: printsharing_to_allRatioInfo(int seqNo)
	{
		FILE *fp = NULL;
		char fileName[128];
		
	
		//sprintf(fileName, "cpl_min%d_now%f", min, now);
		sprintf(fileName, "Sharing_to_allRatioInfo%d",seqNo);
		if (fp = fopen(fileName, "w"), fp == NULL) {
			printf("\t!!GlobalBitTorrentList fopen fail for CTR - file_name[%s]\n", fileName);
			return;
		}
		Compute_numberhops_to_source();
		GrIL->computessharing_to_allratioInfo();
	
		for (int i=0; i<GrIL->getnn(); i++)
		{
		fprintf(fp, "%.5f %d",GrIL->getsharing_to_allratioInfo(i), GrIL->gethopstosource(i));
		fprintf(fp, "\n");
		}
		fclose(fp);
		return;
	
	}
	
	void GlobalBitTorrentList::printsharing_versus_hops(int seqNo)
	{
		FILE *fp = NULL;
		char fileName[128];
		
	
		//sprintf(fileName, "cpl_min%d_now%f", min, now);
		sprintf(fileName, "Sharing_versus_hops%d",seqNo);
		if (fp = fopen(fileName, "w"), fp == NULL) {
			printf("\t!!GlobalBitTorrentList fopen fail for CTR - file_name[%s]\n", fileName);
			return;
		}
		
		GrIL->computesharing_versus_hops();
	
		for (int i=0; i<20; i++)
		{
		fprintf(fp, "%d %.5f",i, GrIL->getsharing_versus_hops(i));
		fprintf(fp, "\n");
		}
		fclose(fp);
		return;
	
	}
	void GlobalBitTorrentList::Initialise_ratio_Info()
	{
	
	printf("Initialise\n");
	}
	
	void GlobalBitTorrentList::Update_ratio_info(int cpt,struct ratioInfo *rI)
	{
	GrIL-> addratio(cpt,rI->ratio0_,0);
	GrIL-> addratio(cpt,rI->ratio25_,25);
	GrIL->addratio(cpt,rI->ratio50_,50);
	GrIL->addratio(cpt,rI->ratio75_,75);
	GrIL->addratio(cpt,rI->ratio100_,100);
	GrIL->set_contributing_nodes(cpt,GrIL->get_contributing_nodes(cpt)+1);
	
	}
	GlobalRatioInfoList* GlobalBitTorrentList::getratioInfo()
	{
	return(GrIL);
	}
	
	GlobalRatioInfoList::GlobalRatioInfoList()
	{
	leng=20;
	contributeinfo= new int[leng];
	int cpt;
	for(cpt=0;cpt<leng;cpt++)
	{
	contributeinfo[cpt]=0;
	Infotable[cpt].ratio0_=0;
	Infotable[cpt].ratio25_=0;
	Infotable[cpt].ratio50_=0;
	Infotable[cpt].ratio75_=0;
	Infotable[cpt].ratio100_=0;
	}
	}
	
	GlobalRatioInfoList::~GlobalRatioInfoList()
	{
	
	delete contributeinfo;
	
	contributeinfo=NULL;
	}
	
	float GlobalRatioInfoList::getsharing_versus_hops(int i)
	{
	return(sharing_versus_hops[i][2]);
	}
	
	void GlobalRatioInfoList::computesharing_versus_hops()
	{
	for(int cpt=0; cpt<20;cpt++)
	{
	sharing_versus_hops[cpt][0]=0;
	sharing_versus_hops[cpt][1]=0;
	sharing_versus_hops[cpt][2]=0;
	}
	for (int j=0;j<20;j++)
	{
	for (int i=0; i<nn;i++)
	{
	if (sharing_to_allratioInfo[i][1]==j)
	{
	sharing_versus_hops[j][0]=sharing_versus_hops[j][0]+sharing_to_allratioInfo[i][0];
	sharing_versus_hops[j][1]=sharing_versus_hops[j][1]+1;
	sharing_versus_hops[j][2]=sharing_versus_hops[j][0]/sharing_versus_hops[j][1];
	}
	}
	}
	}
	
	float GlobalRatioInfoList::getsharing_to_allratioInfo(int i)
	{
	return(sharing_to_allratioInfo[i][0]);
	}
	void GlobalRatioInfoList::computessharing_to_allratioInfo()
	{
	float sum;
	int num_sum;
	float r;
	
	for(int i=0;i<nn;i++)
	{
	sum =0;
	num_sum=0;
	for(int j=0;j<nn;j++)
	{
	if (sharingratioInfo[i][j]!=-1)
	{
	sum=sum + sharingratioInfo[i][j];
	num_sum++;
	}
	}
	sharing_to_allratioInfo[i][0] = (float)sum/(float)num_sum;
	}
	}
	
	int GlobalRatioInfoList::gethopstosource(int i)
	{
	return((int)sharing_to_allratioInfo[i][1]);
	}
	void GlobalRatioInfoList::setNumberhopstosource(int i, int h)
	{
	sharing_to_allratioInfo[i][1]=(float)h;
	}
	
	void GlobalRatioInfoList::computessharingratioInfo()
	{
	int min;
	int max;
	int uij;
	int uji;
	for (int i=0; i<nn;i++)
	{
	for (int j=0;j<nn;j++)
	{
	uij=sharingInfo[i][j];
	uji=sharingInfo[j][i];
	
	if (uij > uji)
	{
	min=uji;
	max=uij;
	}
	else
	{
	min=uij;
	max=uji;
	}
	
	if (max==0)
	{
	sharingratioInfo[i][j]=-1;
	}
	else
	{
	sharingratioInfo[i][j]=(float)min/(float)max;
	}
	
	}
	}
	
	}
	
	float GlobalRatioInfoList::getsharingratioInfo(int i, int j)
	{
	return(sharingratioInfo[i][j]);
	}
	
	int GlobalRatioInfoList::getsharingInfo(int i,int j)
	{
	return(sharingInfo[i][j]);
	}
	
	int GlobalRatioInfoList::getnn()
	{
	return(nn);
	}
	
	
	void GlobalRatioInfoList::setsharingInfo(int n)
	{
	nn=n;
	for (int i=0;i<nn;i++)
	{
	for(int j=0;j<nn;j++)
	{
	setsharingInfovalue(i,j,0);
	}
	}
	}
	
	void GlobalRatioInfoList::setsharingInfovalue(int i,int j,int b)
	{
	sharingInfo[i][j]=b;
	}
	
	void GlobalRatioInfoList::addvaluesharingInfo(int i,int j, int b)
	{
	sharingInfo[i][j] = sharingInfo[i][j] + b; 
	}
	
	int  GlobalRatioInfoList::length()
	{
	return(leng);
	}
	
	float GlobalRatioInfoList::getratio(int cpt,int per)
	{
	float rratio;
	if(per==0)
	{
	rratio=Infotable[cpt].ratio0_;
	}
	if(per==25)
	{
	rratio=Infotable[cpt].ratio25_;
	}
	if(per==50)
	{
	rratio=Infotable[cpt].ratio50_;
	}
	if(per==75)
	{
	rratio=Infotable[cpt].ratio75_;
	}
	if(per==100)
	{
	rratio=Infotable[cpt].ratio100_;
	}
	return(rratio);
	}
	void  GlobalRatioInfoList::addratio(int cpt,float radded,int per)
	{
	int contrib;
	int incrementedcontrib;
	float oldvalue;
	float newvalue;
	float oldsum;
	float newsum;
	contrib=contributeinfo[cpt];
	incrementedcontrib=contrib+1;
	
	if (per==0)
	{
	oldvalue=Infotable[cpt].ratio0_;
	oldsum= (float) (oldvalue * (float)contrib);
	newsum= oldsum + radded;
	newvalue= (float) newsum / (float) incrementedcontrib;
	Infotable[cpt].ratio0_=newvalue;
	
	}
	if(per==25)
	{
	oldvalue=Infotable[cpt].ratio25_;
	oldsum= (float) (oldvalue * (float)contrib);
	newsum= oldsum + radded;
	newvalue= (float) newsum / (float) incrementedcontrib;
	Infotable[cpt].ratio25_=newvalue;
	}
	if(per==50)
	{
	oldvalue=Infotable[cpt].ratio50_;
	oldsum= (float) (oldvalue * (float)contrib);
	newsum= oldsum + radded;
	newvalue= (float) newsum / (float) incrementedcontrib;
	Infotable[cpt].ratio50_=newvalue;
	}
	if(per==75)
	{
	oldvalue=Infotable[cpt].ratio75_;
	oldsum= (float) (oldvalue * (float)contrib);
	newsum= oldsum + radded;
	newvalue= (float) newsum / (float) incrementedcontrib;
	Infotable[cpt].ratio75_=newvalue;
	}
	if(per==100)
	{
	oldvalue=Infotable[cpt].ratio100_;
	oldsum= (float) (oldvalue * (float)contrib);
	newsum= oldsum + radded;
	newvalue= (float) newsum / (float) incrementedcontrib;
	Infotable[cpt].ratio100_=newvalue;
	}
	}
	struct ratioInfo GlobalRatioInfoList::get_hop(int cpt)
	{
	struct ratioInfo rI;
	rI=Infotable[cpt];
	return(rI);
	}
	int GlobalRatioInfoList::get_contributing_nodes(int cpt)
	{
	int cn;
	cn=contributeinfo[cpt];
	return(cn);
	}
	void GlobalRatioInfoList::set_contributing_nodes(int cpt, int contrib)
	{
	contributeinfo[cpt]=contrib;
	}
	
	GlobalBitConnectionList::GlobalBitConnectionList() {
		table_ = new Tcl_HashTable;
		Tcl_InitHashTable(table_, TCL_ONE_WORD_KEYS);
	
		instance_ = this;
	}
	
	GlobalBitConnectionList::~GlobalBitConnectionList() {
		if (table_ != NULL) {
			Tcl_DeleteHashTable(table_);
			delete table_;
		}
	
		instance_ = NULL;
	}
	
	int GlobalBitConnectionList::get_id(int owner_id, int peer_id) {
		int id = owner_id * 1000 + peer_id;
		return id;
	}
	
	int GlobalBitConnectionList::addConnection(BitConnection *bitCon) {
		if (bitCon == NULL)  return -1;
	
		int id = get_id(bitCon->bitOwner()->id(), bitCon->bitPeer()->id());
		int newEntry = 1;
		Tcl_HashEntry *he = Tcl_CreateHashEntry(table_,
							(const char *) id,
							&newEntry);
		if (he == NULL) return -1;
		Tcl_SetHashValue(he, (ClientData)bitCon);
		return 1;
	}
	
	BitConnection* GlobalBitConnectionList::getConnection(int owner_id, int peer_id) {
		int id = get_id(owner_id, peer_id);
		Tcl_HashEntry *he = Tcl_FindHashEntry(table_, (const char *)id);
		if (he == NULL) return NULL;
		return (BitConnection *)Tcl_GetHashValue(he);
	}
	////////////////////////////////////////////////////
	///// End of Global Class /////
	////////////////////////////////////////////////////
	
	////////////////////////////////////////////////////
	///// Start of BitConnection Tcl class  /////
	////////////////////////////////////////////////////
	BitConnection::BitConnection(BitTorrent *owner, BitTorrent *peer) {
		bitOwner_	= owner;
		bitPeer_	= peer;
	
		{
			int advanceNum = 10;
			char buf[1024];
			char tcp1[128], tcp2[128], app1[128], app2[128];
			Tcl& tcl = Tcl::instance();
			Simulator& ns = Simulator::instance();
	
                        printf("CREATING TCP CONNECTION\n");
			// create FullTcpOwner, FullTcpPeer
			sprintf(buf, "new Agent/TCP/FullTcp");
			tcl.eval(buf);
			sprintf(tcp1, "%s", tcl.result());
	
			sprintf(buf, "new Agent/TCP/FullTcp");
			tcl.eval(buf);
			sprintf(tcp2, "%s", tcl.result());
	
			//sprintf(buf, "%s attach-agent %s %s", ns.name(), owner->name(), tcp1);
			sprintf(buf, "%s attach-agent %s %s", ns.name(), owner->tcl_node(), tcp1);
			tcl.eval(buf);
			sprintf(buf, "%s attach-agent %s %s", ns.name(), peer->tcl_node(), tcp2);
			tcl.eval(buf);
	
	//		sprintf(buf, "%s connect %s %s", ns.name(), tcp1, tcp2);
	//		tcl.eval(buf);
	//
	//		sprintf(buf, "%s listen", tcp2);
	//		tcl.eval(buf);
	
			sprintf(buf, "%s connect %s %s", ns.name(), tcp1, tcp2);
			tcl.eval(buf);
	
			sprintf(buf, "%s listen", tcp2);
			tcl.eval(buf);
	
			sprintf(buf, "%s advance %d", tcp1, advanceNum);
	//		tcl.eval(buf);
	
	
			// create BitTcpAppOwner, BitTcpAppPeer
			sprintf(buf, "new Application/TcpApp/BitTcpApp %s %s %s", tcp1, owner->name(), peer->name());
			tcl.eval(buf);
			sprintf(app1, "%s", tcl.result());
			tcpappOwner_ = (BitTcpApp *)TclObject::lookup(app1);
			if (tcpappOwner_ == NULL) {
				log_fail("app1[%s] lookup fail", app1);
                                 printf("app1[%s] lookup fail",app1);
				abort();
			}
	
			sprintf(buf, "new Application/TcpApp/BitTcpApp %s %s %s", tcp2, peer->name(), owner->name());
			tcl.eval(buf);
			sprintf(app2, "%s", tcl.result());
			tcpappPeer_ = (BitTcpApp *)TclObject::lookup(app2);
			if (tcpappPeer_ == NULL) {
				log_fail("app2[%s] lookup fail", app2);
                                printf("app2[%s] lookup fail",app2);
				abort();
			}
	
			sprintf(buf, "%s connect %s", app1, app2);
			tcl.eval(buf);
		}
	
		/*!!!!!!!!!!!!!!!!?????????????????
		tcpappOwner_->target(tcpappPeer_);
		tcpappPeer_->target(tcpappOwner_);
		*/
		return;
	}
	
	BitConnection::~BitConnection() {
	}
	
	int BitConnection::sendControl(BitTorrentControl *control) {
		if (bitOwner_->id() == control->sender_id_) {
			// The case that the owner send a packet to its peer
			tcpappOwner_->sendControl(control);
		} else if (bitPeer_->id() == control->sender_id_) {
			// The case that the peer send a packet to its owner
			tcpappPeer_->sendControl(control);
		} else {
			log_fail("invalid sender_id[%d] receiver_id[%d] - bitOwner[%d] bitPeer[%d]", control->sender_id_, control->receiver_id_, bitOwner_->id(), bitPeer_->id());
		}
	
		if (0) {
		if (control->pktType_ == PIECEOFFER_REPLY) {
			// For PIECEOFFER_REPLY, we'll reuse the TCP connection which used for PIECEOFFER_REQUEST
			tcpappPeer_->sendControl(control);
		} else {
			tcpappOwner_->sendControl(control);
		}
		}
		return 1;
	}
	
	int BitConnection::sendData(BitTorrentPayload* payload) {
		tcpappOwner_->sendData(payload);
		return 1;
	}
	
	void BitConnection::trace(int flag, const char* func_name, int line_num, const char* fmt, ...) {
		Scheduler &s = Scheduler::instance();
		double now = s.clock();
	
		va_list ap;
		int len;
		char buff[10240];
	
		va_start(ap, fmt);
		len = vsprintf(buff, fmt, ap);
		va_end(ap);
	
		switch(flag) {
			case FAIL:
				printf("F %.5f ID[%d:%d] %s %d %s - [%s]\n", now, bitOwner_->id(), bitPeer_->id(), __FILE__, line_num, func_name, buff);
				break;
			case ERROR:
				printf("E %.5f ID[%d:%d] %s %d %s - [%s]\n", now, bitOwner_->id(), bitPeer_->id(), __FILE__, line_num, func_name, buff);
				break;
			case DEBUG:
	//			printf("D %.5f ID[%d:%d] %s - [%s]\n", now, bitOwner_->id(), bitPeer_->id(), func_name, buff);
				break;
			case STATE:
	//			printf("S %.5f ID[%d:%d] %s\n", now, bitOwner_->id(), bitPeer_->id(), buff);
				break;
			case CRITICAL_STATE:
				printf("\nC %.5f ID[%d:%d] %s %d %s - [%s]\n", now, bitOwner_->id(), bitPeer_->id(), __FILE__, line_num, func_name, buff);
				break;
			case WARNING:
				printf("W %.5f ID[%d:%d] %s %d %s - [%s]\n", now, bitOwner_->id(), bitPeer_->id(), __FILE__, line_num, func_name, buff);
				break;
		}
		return;
	}
	
	////////////////////////////////////////////////////
	///// End of BitConnection Class /////
	////////////////////////////////////////////////////
	
	BitTorrent::BitTorrent(int id, char* tcl_node, int max_node) : log_(0) {
		char temp[512], buf[512];
		Tcl& tcl = Tcl::instance();
	
		state_ = BEFORE_START;
		log_level_ = 0;
		startTime_=-1;
		finishTime_ = 0;
		sendBytes_ = 0;
	
		setLogLevel(CRITICAL_STATE, true);
		setLogLevel(FAIL, true);
		setLogLevel(WARNING, true);
	
		agent_ = NULL;
		bitAgent_ = NULL;
	
		id_ = id;
	
		memset(tcl_node_, 0x00, sizeof(tcl_node_));
		sprintf(tcl_node_, "%s", tcl_node);
	
		max_node_num_ = max_node;
	
		memset(temp, 0x00, sizeof(temp));
		sprintf(temp, "%s set ragent_", tcl_node_);
		tcl.eval(temp);
	
		memset(buf, 0x00, sizeof(buf));
		sprintf(buf, "%s", tcl.result());
		routingAgent_ = (DSDV_Agent *)TclObject::lookup(buf);
		if (routingAgent_ == NULL) {
			log_fail("routingAgent[%s] lookup fail", buf);
                     printf("routing agent is null \n");
			abort();
		}
	
		GlobalBitTorrentList::instance().addNode(this);
	
		bind("number_of_nodes_in_simulation_", &number_of_nodes_in_simulation_);
	
		if (id_==0)
		{
		GlobalBitTorrentList::instance().getratioInfo()->setsharingInfo(number_of_nodes_in_simulation_);
		}
	
                used_quantum_=0;
		bind("max_peer_num_", &max_peer_num_);
		bind("max_neighbor_num_", &max_neighbor_num_);
		bind("peer_update_interval1_", &peerUpdateInterval1_);
		bind("neighbor_select_interval1_", &neighborSelectInterval1_);
		bind("into_steady_state_time_", &steadyStateTime_);
		bind("peer_update_interval2_", &peerUpdateInterval2_);
		bind("neighbor_select_interval2_", &neighborSelectInterval2_);
	
		bind("try_upload_interval_", &tryUploadInterval_);
		bind("received_bytes_reset_interval_", &receivedBytesResetInterval_);
	
		if ( (peerUpdateInterval1_ < 0) || 
				(peerUpdateInterval2_ < 0) ||
				(steadyStateTime_ < 0) ||
				(tryUploadInterval_ < 0) ||
				(receivedBytesResetInterval_ < 0) ||
				(neighborSelectInterval1_ < 0) ||
				(neighborSelectInterval2_ < 0) ) {
			log_fail("steadyStateTime[%f] peer_update_interval[%f:%f] neighbor_select_interval[%f:%f] try_upload_interval[%f] receivedBytesResetInterval[%f] invalid",
					steadyStateTime_, peerUpdateInterval1_, peerUpdateInterval2_, neighborSelectInterval1_, neighborSelectInterval2_, tryUploadInterval_, receivedBytesResetInterval_);
			abort();
		}
	
		if (max_peer_num_ <= 0) max_peer_num_ = -1;
		if (max_neighbor_num_ <= 0) max_neighbor_num_ = -1;

                    bind("far_max_neighbor_num_",&far_max_neighbor_num_);
                bind("Near_far_limit_",&Near_far_limit_);
                bind("far_quantum_",&far_quantum_);
	
		peerTable_ = new PeerTable(max_peer_num_);
		neighborTable_ = new NeighborTable(max_neighbor_num_);
                printf("table des lointains cree avec far_max_neighbor_num_ = %d \n", far_max_neighbor_num_);
		far_neighborTable_ = new FarNeighborTable(far_max_neighbor_num_);
	
		peerUpdateHandler_ = new BitPeerUpdateHandler(this);
		neighborSelectHandler_ = new BitNeighborSelectHandler(this);
		tryUploadHandler_ = new BitTryUploadHandler(this);
		receivedBytesResetHandler_ = new BitReceivedBytesResetHandler(this);
	
		bind("max_upload_num_", &max_upload_num_);
		bind("max_download_num_", &max_download_num_);

              

		if ( (max_upload_num_ <= 0) || (max_download_num_ <= 0) ) {
			log_fail("max_upload_num[%d] max_download_num[%d]", max_upload_num_, max_download_num_);
			abort();
		} else {
			cur_upload_num_ = 0;
			cur_download_num_ = 0;
		}
	
		local_rarest_piece_num_ = -1;
		bind("local_rarest_select_flag_", &local_rarest_select_flag_);
		if ( local_rarest_select_flag_ != 1 && local_rarest_select_flag_ != 2 ) {
			log_fail("local_rarest_select_flag[%d] should be set 1(peerTable) or 2(neighborTable)", local_rarest_select_flag_);
			abort();
		}
	
		choking_slot_index_ = 0;
		global_choking_slot_index_ = 0;
		served_slot_num_ = 0;
	
		bind("choking_best_slot_num_", &choking_best_slot_num_);
		bind("choking_optimistic_slot_num_", &choking_optimistic_slot_num_);
		if ( (choking_best_slot_num_ < 0) && (choking_optimistic_slot_num_ < 0) ) {
			log_fail("choking_best_slot_num[%d] choking_optimistic_slot_num[%d]",
					choking_best_slot_num_, choking_optimistic_slot_num_);
			abort();
		} else {
			choking_slot_num_ = choking_best_slot_num_ + choking_optimistic_slot_num_;
			if (choking_slot_num_ <= 0) {
				log_fail("choking_slot_num should not be 0 - check choking_best_slot_num and choking_optimistic_slot_num");
				abort();
			} else {
				if ( (max_upload_num_ > 0) && (choking_slot_num_ != max_upload_num_) ) {
					log_warning("For parallel, choking_slot_num[%d] should be same with max_upload_num[%d]", choking_slot_num_, max_upload_num_);
				}
			}
		}
	
		bind("control_tcp_flag_", &control_tcp_flag_);
		bind("use_bidirection_tcp_flag_", &use_bidirection_tcp_flag_);
	
		{
			//hopCountBytesTable_ = new Tcl_HashTable;
			//Tcl_InitHashTable(hopCountBytesTable_, TCL_ONE_WORD_KEYS);
			//hopCountTimeTable_ = new Tcl_HashTable;
			//Tcl_InitHashTable(hopCountTimeTable_, TCL_ONE_WORD_KEYS);
	
	
		}
	
		bind("peerTable_flag_", &peerTable_flag_);
	
		bind("selectNodeToUpload_flag_", &selectNodeToUpload_flag_);
		bind("selectRandomNode_flag_",&selectRandomNode_flag_);
		if ( (selectNodeToUpload_flag_ != BASIC) &&
				(selectNodeToUpload_flag_ != CLOSEST) &&
				(selectNodeToUpload_flag_ != SEED_RANDOM) &&
				(selectNodeToUpload_flag_ != ONE_HOP) ) {
			log_fail("invalid selectNodeToUpload_flag[%d]", selectNodeToUpload_flag_);
			abort();
		}
	
		bind("selectNeighbor_flag_", &selectNeighbor_flag_);
		if ( (selectNeighbor_flag_ != 0) &&
				(selectNeighbor_flag_ != 1) && (selectNeighbor_flag_ != 2 ) ) {
			log_fail("invalid selectNeighbor_flag[%d]", selectNeighbor_flag_);
			abort();
		}
	
		printCPL_flag_ = 0;
		bind("printCPL_flag_", &printCPL_flag_);
	
		//seqNo_ = 0;
		bind("seqNo_", &seqNo_);
		rI.ratio0_=-1;
		rI.ratio25_=-1;
		rI.ratio50_=-1;
		rI.ratio75_=-1;
		rI.ratio100_=-1;
		return;
	}
	
	BitTorrent::~BitTorrent() {
		if (local_rarest_count_ != NULL) {
			delete []local_rarest_count_;
		}
	
		//{
			//Tcl_HashEntry *he;
			//Tcl_HashSearch hs;
	
			//for(he = Tcl_FirstHashEntry(hopCountBytesTable_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
			//	struct HCBEntry *entry = (struct HCBEntry *)Tcl_GetHashValue(he);
			//	Tcl_DeleteHashEntry(he);
			//	delete entry;
			//}
			//Tcl_DeleteHashTable(hopCountBytesTable_);
			//delete hopCountBytesTable_;
	
		// for(he = Tcl_FirstHashEntry(hopCountTimeTable_, &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
			//	struct HCTEntry *entry = (struct HCTEntry *)Tcl_GetHashValue(he);
			//	Tcl_DeleteHashEntry(he);
			//	delete entry;
			//}
		//	Tcl_DeleteHashTable(hopCountTimeTable_);
		//	delete hopCountTimeTable_;
		//
	}
	
	void BitTorrent::set_piecePool(PiecePool* pp) {
		piecePool_ = pp;
	
		if (piecePool_ != NULL && piecePool_->piece_num() > 0) {
			local_rarest_count_ = new int[piecePool_->piece_num()+1];
			for (int i = 0; i < piecePool_->piece_num(); i++) {
				if (piecePool_->has_piece(i)) local_rarest_count_[i] = -1;
				else local_rarest_count_[i] = 0;
			}
		} else {
			local_rarest_count_ = NULL;
		}
	}
	
	void BitTorrent::getStateStr(char* buff) {
		switch(state_) {
			case BEFORE_START:
				sprintf(buff, "BEFORE START");
				break;
			case HELLO_FLOODING:
				sprintf(buff, "HELLO_FLOODING");
				break;
			case NEIGHBOR_SELECTION:
				sprintf(buff, "NEIGHBOR_SELECTION");
				break;
			case PIECE_SELECTION:
				sprintf(buff, "PIECE_SELECTION");
				break;
			case TRY_UPLOAD:
				sprintf(buff, "TRY_UPLOAD");
				break;
			case FINISH_DOWNLOAD:
				sprintf(buff, "FINISH_DOWNLOAD");
				break;
			case STOPPED:
				sprintf(buff, "STOPPED");
				break;
			default:
				sprintf(buff, "UNKNOWN STATE!!!!!");
				break;
		}
	
		return;
	}
	
	void BitTorrent::updateratioInfo()
	{
	float current_ratio;
	if (startTime_>0)
	{
	rI.ratio0_=startTime_;
	}
	if (finishTime_>0)
	{
	rI.ratio100_=finishTime_;
	}
	if (finishTime_>0 && startTime_>0 && finishTime_==startTime_)
	{
	rI.ratio0_=startTime_;
	rI.ratio25_=startTime_;
	rI.ratio50_=startTime_;
	rI.ratio75_=startTime_;
	rI.ratio100_=startTime_;
	}
	
	
	current_ratio= piecePool_->RatioDowloadedPieces();
	
	if (startTime_> 0 && finishTime_<=0)
	{
	if (current_ratio >=0.25 && rI.ratio25_==-1)
	{
	rI.ratio25_= (float)Scheduler::instance().clock();
	}
	if (current_ratio >=0.50 && rI.ratio50_==-1)
	{
	rI.ratio50_= (float)Scheduler::instance().clock();
	}
	if (current_ratio >=0.75 && rI.ratio75_==-1)
	{
	rI.ratio75_= (float)Scheduler::instance().clock();
	}
	
	}
	}
	struct ratioInfo* BitTorrent::getratioInfo()
	{
	return(&rI);
	}
	
	int BitTorrent::HopstoSeed()
	{
	int hps;
	int source_id;
	source_id=0;
	hps=routingAgent_->hopCount(source_id);
	printf("Number of hops equal:%d\n",hps);
	return(hps);
	}
	// Basic functionalities: 
	int BitTorrent::command(int argc, const char*const* argv)
	{
		Tcl& tcl = Tcl::instance();
	
		if (argc == 2) {
			if (strcmp(argv[1], "id") == 0) {
				tcl.resultf("%d", id_);
				return TCL_OK;
			} else if (strcmp(argv[1], "log") == 0) {
				//!!!! how to use this? - find HTTP script / code
				// Return the name of the log channel
				if (log_ != NULL)
					tcl.resultf("%s", Tcl_GetChannelName(log_));
				else
					tcl.result("");
				return TCL_OK;
			} else if (strcmp(argv[1], "piecePool") == 0) {
				tcl.result(piecePool_->name());
				return TCL_OK;
			} else if (strcmp(argv[1], "fileName") == 0) {
				if (piecePool_ != NULL) {
					tcl.resultf("%d", piecePool_->file_name());
				} else {
					tcl.result("0");
				}
				return TCL_OK;
			} else if (strcmp(argv[1], "state") == 0) {
				/*
				switch(state_) {
					case BEFORE_START:
						tcl.result("BEFORE START");
						break;
					case HELLO_FLOODING:
						tcl.result("HELLO_FLOODING");
						break;
					case NEIGHBOR_SELECTION:
						tcl.result("NEIGHBOR_SELECTION");
						break;
					case PIECE_SELECTION:
						tcl.result("PIECE_SELECTION");
						break;
					case TRY_DOWNLOAD:
						tcl.result("TRY_DOWNLOAD");
						break;
					case FINISH_DOWNLOAD:
						tcl.result("FINISH_DOWNLOAD");
						break;
					case STOPPED:
						tcl.result("STOPPED");
						break;
					default:
						tcl.result("UNKNOWN STATE!!!!!");
						break;
				}
				*/
				char buff[1024];
				tcl.result(buff);
				return TCL_OK;
			} else if (strcmp(argv[1], "printPeerTable") == 0) {
				peerTable_->print();
				return TCL_OK;
			} else if (strcmp(argv[1], "printNeighborTable") == 0) {
				neighborTable_->print();
				return TCL_OK;
			} else if (strcmp(argv[1], "printPiecePool") == 0) {
				printf("########### BitTorrent Node[%d] printPiecePool ##############\n", id_);
				piecePool_->print();
				printf("###########################################\n");
				return TCL_OK;
			} else if (strcmp(argv[1], "print") == 0) {
				print();
				return TCL_OK;
			} else if (strcmp(argv[1], "start") == 0) {
				start();
				return TCL_OK;
		/**** Metric Command Start ***/
			} else if (strcmp(argv[1], "startTime") == 0) {
				tcl.resultf("%.5f", startTime_);
				return TCL_OK;
			} else if (strcmp(argv[1], "finishTime") == 0) {
				tcl.resultf("%.5f", finishTime_);
				return TCL_OK;
			} else if (strcmp(argv[1], "sendBytes") == 0) {
				tcl.resultf("%ld", sendBytes_);
				return TCL_OK;
			} else if (strcmp(argv[1], "printHCB") == 0) {
				//printHCB(-1,999999);
				return TCL_OK;
			} else if (strcmp(argv[1], "printRT") == 0) {
			if (id_ ==0)  GlobalBitTorrentList::instance().printCompletenessTimeRatio(seqNo_);
			if (id_ == 0) GlobalBitTorrentList::instance().printsharingRatioInfo(seqNo_);
			if (id_ == 0) GlobalBitTorrentList::instance().printsharing_to_allRatioInfo(seqNo_);
			if (id_ == 0) GlobalBitTorrentList::instance().printsharing_versus_hops(seqNo_);
				
				return TCL_OK;
			}
		/**** Metric Command End ***/
		} else if (argc == 3) {
			if (strcmp(argv[1], "setPiecePool") == 0) {
				PiecePool *pool = (PiecePool*)TclObject::lookup(argv[2]);
				if (pool == NULL) return TCL_ERROR;
				set_piecePool(pool);
				return TCL_OK;
			} else if (strcmp(argv[1], "log") == 0) {
				//!!!! how to use this? - find HTTP script / code
				int mode;
				log_ = Tcl_GetChannel(tcl.interp(), 
						(char*)argv[2], &mode);
				if (log_ == 0) {
					tcl.resultf("%d: invalid log file handle %s\n",
						id_, argv[2]);
					return TCL_ERROR;
				}
				return TCL_OK;
			} else if (strcmp(argv[1], "hasFile") == 0) {
				if (hasFile(atoi(argv[2]))) {
					tcl.result("1");
				} else {
					tcl.result("0");
				}
				return TCL_OK;
			} else if (strcmp(argv[1], "disconnect") == 0) {
				//!!!! how to implement disconnect for Dynamic TCP stacking
				/*
				* <http> disconnect <client> 
				* Delete the association of source and sink TCP.
				*/
				/*
				BitTorrent *client = 
					(BitTorrent *)TclObject::lookup(argv[2]);
				delete_cnc(client);
				*/
				return TCL_OK;
			} else if (strcmp(argv[1], "attach-agent") == 0) {
				agent_ = (Agent *)TclObject::lookup(argv[2]);
				if (agent_ == NULL) return TCL_ERROR;
	
				bitAgent_ = (BitAgent *)agent_;
				bitAgent_->setBitFlag(true);
				agent_->attachApp(this);
				return TCL_OK;
			}
		} else {
			if (strcmp(argv[1], "evTrace") == 0) { 
				// log related?????!!!!!
				char buf[1024], *p;
				if (log_ != 0) {
					sprintf(buf, TIME_FORMAT" i %d ", 
					BaseTrace::round(Scheduler::instance().clock()), 
						id_);
					p = &(buf[strlen(buf)]);
					for (int i = 2; i < argc; i++) {
						strcpy(p, argv[i]);
						p += strlen(argv[i]);
						*(p++) = ' ';
					}
					// Stick in a newline.
					*(p++) = '\n', *p = 0;
					Tcl_Write(log_, buf, p-buf);
				}
				return TCL_OK;
			} else if (strcmp(argv[1], "setLogLevel") == 0) {
				if (argc != 4) return TCL_ERROR;
	
				bool onOff = atoi(argv[3]);
	
				if (strcmp(argv[2], "FAIL") == 0) {
					setLogLevel(FAIL, onOff);
				} else if (strcmp(argv[2], "ERROR") == 0) {
					setLogLevel(ERROR, onOff);
				} else if (strcmp(argv[2], "DEBUG") == 0) {
					setLogLevel(DEBUG, onOff);
				} else if (strcmp(argv[2], "STATE") == 0) {
					setLogLevel(STATE, onOff);
				} else if (strcmp(argv[2], "CRITICAL") == 0) {
					setLogLevel(CRITICAL_STATE, onOff);
				} else if (strcmp(argv[2], "WARNING") == 0) {
					setLogLevel(WARNING, onOff);
				}
	
				return TCL_OK;
			}
		}
	
		return Application::command(argc, argv);
	}
	
	bool BitTorrent::logLevel(int level) {
		int result = 0;
		result = log_level_ & (1 << level);
		result = result >> level;
		if (result > 0) return true;
		else return false;
	}
	
	void BitTorrent::setLogLevel(int level, bool onOff) {
		if (logLevel(level)) {
			// flag : 1 --> 0
			if (!onOff) {
				log_level_ &= 0 << level;
			}
		} else {
			// flag : 0 --> 1
			if (onOff) {
				log_level_ |= 1 << level;
			}
		}
		return;
	}
	
	void BitTorrent::start() {
		if (!agent_ || !bitAgent_) {
			log_fail("has no agent_");
			abort();
			return;
		} else {
                         printf("START: %d\n",id_);
			Scheduler &s = Scheduler::instance();
			double now = s.clock();
	
			state_ = HELLO_FLOODING;
	
	//		peerUpdateHandler_->sched(0.75+Random::uniform(0.25));
			peerUpdateHandler_->sched(1);
			neighborSelectHandler_->sched(neighborSelectInterval1_);
			tryUploadHandler_->sched(tryUploadInterval_);
			receivedBytesResetHandler_->sched(receivedBytesResetInterval_);
	
			lastup_ = now; 
			startTime_ = now;
	
			local_rarest_piece_num_ = -1;
	
			if (piecePool_->doesDownloadComplete()) {
				finishTime_ = now;
				log_critical("start with completed file - download_time[%.5f]", finishTime_ - startTime_);
	
				char buff[1024];
				Tcl& tcl = Tcl::instance();
	
				sprintf(buff, "%s completeNotice", name());
				tcl.eval(buff);
	
				writeDownloadTime();
			}
		}
	updateratioInfo();
	}
	
	void BitTorrent::trace(int flag, const char* func_name, int line_num, const char* fmt, ...) {
		Scheduler &s = Scheduler::instance();
		double now = s.clock();
	
		va_list ap;
		int len;
		char buff[10240];
	
		va_start(ap, fmt);
		len = vsprintf(buff, fmt, ap);
		va_end(ap);
	
		switch(flag) {
			case FAIL:
				if (logLevel(FAIL)) printf("F %.5f ID[%d] %s %d %s - [%s]\n", now, id_, __FILE__, line_num, func_name, buff);
				break;
			case ERROR:
				if (logLevel(ERROR)) printf("E %.5f ID[%d] %s %d %s - [%s]\n", now, id_, __FILE__, line_num, func_name, buff);
				break;
			case DEBUG:
				if (logLevel(DEBUG)) printf("D %.5f ID[%d] %s - [%s]\n", now, id_, func_name, buff);
				break;
			case STATE:
				if (logLevel(STATE)) printf("S %.5f ID[%d] %s\n", now, id_, buff);
				break;
			case CRITICAL_STATE:
				if (logLevel(CRITICAL_STATE)) printf("\nC %.5f ID[%d] %s\n", now, id_, buff);
				break;
			case WARNING:
				if (logLevel(WARNING)) printf("W %.5f ID[%d] %s %d %s - [%s]\n", now, id_, __FILE__, line_num, func_name, buff);
				break;
		}
		return;
	}
	
	void BitTorrent::peerUpdateCallback(Event *evt) {
		Scheduler &s = Scheduler::instance();
		double now = s.clock();
	
		log_state("called for PeerUpdate");
	
		//if (id_ == 0)
		//{ 
		//printCPL(now);
		//}
	
	
	// printHCB(-1,now);
		//printHCT(-1,now);
	
               printf("PEER UPDATE CALLED (id = %d)\n",id_);
		state_ = HELLO_FLOODING;
	
		if (peerTable_flag_ == 1 && piecePool_->doesDownloadComplete()) {
			// When I'm a seed
			peerTable_->removeSeedPeer();
		}
	        peerTable_->print();
		
               if (logLevel(STATE)) {
			log_state("\tBefore print PeerTable");
			peerTable_->print();
			log_state("\tAfter print PeerTable");
		}
	
		sendBitPacket(HELLO, IP_BROADCAST,1);
	
		double peerUpdateInterval;
	
		if (now < startTime_ + steadyStateTime_) {
			peerUpdateInterval = peerUpdateInterval1_ * (0.75 + Random::uniform(0.25));
		} else {
			peerUpdateInterval = peerUpdateInterval2_ * (0.75 + Random::uniform(0.25));
		}
	
		peerUpdateHandler_->resched(peerUpdateInterval);
		lastup_ = now;
		return;
	}
	
	void BitTorrent::neighborSelectCallback(Event *evt) {
		Scheduler &s = Scheduler::instance();
		double now = s.clock();
	
printf("Calling neighbor selection (id = %d)\n",id_);
		log_state("called for Neighbor Selection");
	
		//if (id_ == 0) printCPL(now);
	
		state_ = NEIGHBOR_SELECTION;
	
		{
			Tcl_HashEntry *he;
			Tcl_HashSearch hs;
	
			// Remove All Neighbors before send PIECEOFFER_REQUEST
			neighborTable_->removeAll();
		        far_neighborTable_->removeAll();
	
			
				// All peers will be neighbors
				for(he = Tcl_FirstHashEntry(peerTable_->table(), &hs); he != NULL; he = Tcl_NextHashEntry(&hs)) {
					BitPeer *peer = (BitPeer *)Tcl_GetHashValue(he);
					if (peer == NULL) continue;
					
					peer->updatehopCount(routingAgent_);
					
					//if (peer->hopCount() != BIG)
				//	{
	
					if (peer->hopCount()<= Near_far_limit_)
					{
					BitNeighbor *neighbor = new BitNeighbor(peer->id(), peer->hopCount(), piecePool_->piece_num());
					neighbor->setPieceList(peer->pieceList());
	
					int ret = -1;
					if (ret = neighborTable_->addNeighbor(neighbor), ret < 0) {
                                               printf("ERREUR AJOUT PROCHE (id = %d)\n",id_);
						log_error("addNeighbor Fail!!!!!!!!!!!!!!!!!");
						delete neighbor;
					} else log_state("add neighbor[%d:%d] to its NeighborTable[%d]", peer->id(), peer->hopCount(), neighborTable_->num());
					} 
					else
					{
					FarBitNeighbor *Farneighbor = new FarBitNeighbor(peer->id(), peer->hopCount() , piecePool_->piece_num());
				        if(Farneighbor==NULL)
                                        {
                                        printf("Mauvaise creation de noeud lointain\n");
                                        }
					int ret = -1;
					if (ret = far_neighborTable_->addFarNeighbor(Farneighbor), ret < 0) {
                                               printf("ERREUR AJOUT FAR (id = %d)\n",id_);
						log_error("addFarNeighbor Fail!!!!!!!!!!!!!!!!!");
						delete Farneighbor;
					} else log_state("add Farneighbor[%d:%d] to its NeighborTable[%d]", peer->id(), peer->hopCount(), far_neighborTable_->num());
					} 
	
				//	}
				
	}
			
		}
	
		// calculate the local_rarest_count_
		nextPiece();
	
		state_ = TRY_UPLOAD;
		if (now < startTime_ + steadyStateTime_) {
			neighborSelectHandler_->resched(neighborSelectInterval1_);
		} else {
			neighborSelectHandler_->resched(neighborSelectInterval2_);
		}
	
		if (logLevel(STATE)) {
			log_state("\tBefore print NeighborTable");
			neighborTable_->print();
			log_state("\tAfter print NeighborTable");
		}
             // neighborTable_->print();
              //far_neighborTable_->print();
             
		return;
	}
	
	void BitTorrent::tryUploadCallback(Event *evt) {
		Scheduler &s = Scheduler::instance();
		double now = s.clock();
	
		//if (id_ == 0) printCPL(now);
		if (id_ == 0) GlobalBitTorrentList::instance().printsharingInfo(seqNo_);
		global_choking_slot_index_++;
		choking_slot_index_ = global_choking_slot_index_ % choking_slot_num_;
		if ( (max_upload_num_ == 1) && (choking_slot_index_ == 0) ) served_slot_num_ = 0;
		else if (max_upload_num_ > 1) served_slot_num_ = 0;
	
		log_state("called for TryUpload global_slot[%d] slot[%d]",
				global_choking_slot_index_, choking_slot_index_);
	
		if (max_upload_num_ > 1) {
			// When parallel transmission, node should reset selectFlag at every timeslots
			neighborTable_->resetSelectFlag();
			far_neighborTable_->resetSelectFlag();
	
		} else if (max_upload_num_ == 1) {
			// When serial transmission, node should reset selectFlag at every best_slot_num + optimistic_slot_num timeslots
			if (choking_slot_index_ == 0) {
				neighborTable_->resetSelectFlag();
				far_neighborTable_->resetSelectFlag();
			}
		}
	
		neighborTable_->resetRejectFlag();
		far_neighborTable_->resetRejectFlag();
	
		tryUpload();
		tryUploadHandler_->resched(tryUploadInterval_);
		return;
	}
	
	void BitTorrent::receivedBytesResetCallback(Event *evt) {
		Scheduler &s = Scheduler::instance();
		double now = s.clock();
	
		log_state("called for ReceivedBytesReset");
	
		//if (id_ == 0) printCPL(now);
	
		neighborTable_->resetReceivedBytes();
		far_neighborTable_->resetReceivedBytes();
	
		receivedBytesResetHandler_->resched(receivedBytesResetInterval_);
		return;
	}
	
	void BitTorrent::processDataPacket(BitTorrentPayload *payload) {
		int nextBlock = -1;
	        int hop;
		log_state("receiveDataPacket - piece[%d] block[%d] size[%d]", payload->pieceIndex_, payload->blockIndex_, payload->sendSize_);
	
		piecePool_->setCompleteBlock(payload->pieceIndex_, payload->blockIndex_);
	
		{
			// addBytes to receivedBytes
			GlobalBitTorrentList::instance().getratioInfo()->addvaluesharingInfo(payload->sender_id_,id_, (int)payload->sendSize_);
	
			
			BitPeer *peer = peerTable_->getPeer(payload->sender_id_);
                        if (peer==NULL)
                         {
                         printf("received data packet from NULL peer\n");
                         hop=routingAgent_->hopCount(payload->sender_id_);
                         }
                         else
                         {
                         hop=peer->hopCount();
                         }
			
			//if (hop!=BIG)
			//{
			if (hop<= Near_far_limit_)
			{
                        printf("received data from near (id = %d)\n",id_);
			BitNeighbor *neighbor = neighborTable_->getNeighbor(payload->sender_id_);
			if ( (neighbor != NULL) && (payload->sendSize_ > 0) ) {
				neighbor->addBytes(payload->sendSize_);
				//addBytesHCB(DOWNLOAD, neighbor->hopCount(), ((unsigned long)payload->sendSize_));
				if ( (selectNodeToUpload_flag_ == ONE_HOP) && (neighbor->hopCount() != 1) ) {
					log_fail("try to send data to node[%d] hopCount[%d]", neighbor->id(), neighbor->hopCount());
					print();
					abort();
				}
			}
			}
			else
			{
                        printf("received data from far (id = %d)\n",id_);
			FarBitNeighbor *neighbor = far_neighborTable_->getFarNeighbor(payload->sender_id_);
			if ( (neighbor != NULL) && (payload->sendSize_ > 0) ) {
				neighbor->addBytes(payload->sendSize_);
				//addBytesHCB(DOWNLOAD, neighbor->hopCount(), ((unsigned long)payload->sendSize_));
				if ( (selectNodeToUpload_flag_ == ONE_HOP) && (neighbor->hopCount() != 1) ) {
					log_fail("try to send data to node[%d] hopCount[%d]", neighbor->id(), neighbor->hopCount());
					print();
					abort();
				}
			}
			}
	
			
		//}
	}
	
	
		nextPiece();
	
	
		updateratioInfo();
	
		if (cur_download_num_ >= max_download_num_) return;
	
		if (nextBlock = piecePool_->next_block(payload->pieceIndex_), nextBlock != -1) {
	#ifdef BIT_DOWNLOADING
			piecePool_->setDownloadBlock(payload->pieceIndex_, nextBlock);
	#endif
			//!!!!!!!!!!! Should add some contraints to check the tryUploadTimer is expired or not
			sendBitPacket2(PIECEOFFER_REPLY, payload->sender_id_, CONTINUE, payload->pieceIndex_, nextBlock);
		} else {
			if (piecePool_->isCompletePiece(payload->pieceIndex_)) {
				// When the piece is completed to download
				// --> notice it to its all peer
	
				Tcl_HashEntry *he;
				Tcl_HashSearch hs;
	
				for(he = Tcl_FirstHashEntry(peerTable_->table(), &hs);
						he != NULL;
						he = Tcl_NextHashEntry(&hs)) {
					BitPeer *node = (BitPeer *)Tcl_GetHashValue(he);
					if (node->hopCount() <= Near_far_limit_)
					{
					sendBitPacket(UPDATE_PIECELIST, node->id(),1);
					}
				}
	
				// When the node finishs to download the file
				// --> goes to Seed State
				if ( (finishTime_ <= 0) && (piecePool_->doesDownloadComplete()) ) {
					Scheduler &s = Scheduler::instance();
					double now = s.clock();
	
					state_ = FINISH_DOWNLOAD;
					finishTime_ = now;
					updateratioInfo();
					log_critical("complete download the file - download_time[%.5f]", finishTime_ - startTime_);
	
					char buff[1024];
					Tcl& tcl = Tcl::instance();
	
					sprintf(buff, "%s completeNotice", name());
					tcl.eval(buff);
	
					writeDownloadTime();
				}
				else
				{
                                printf("before sending continue \n");
				sendBitPacket2(PIECEOFFER_REPLY, payload->sender_id_, CONTINUE,-1, -1);
                                printf("after sending continue \n");
				}
			}
		}
		
	
		return;
	}
	
	void BitTorrent::processControlPacket(BitTorrentControl *control) {
		if (control_tcp_flag_ == 0) {
			log_fail("invalid packet received at processControlPacket[%d] - control_tcp_flag[%d]", control->pktType_, control_tcp_flag_);
			abort();
			return;
		}
	
		switch(control->pktType_) {
			case HELLO:
                                {
				log_fail("invalid pktType [%d] at processControlPacket", control->pktType_);
				abort();
				break;
                                }
			case HELLO_REPLY:
				{
	#ifdef BITTO
					int hopCount = routingAgent_->hopCount(control->sender_id_);
	#else
					int hopCount = -1;
	#endif
	
					bool seedFlag = true;
					if ( (peerTable_flag_ == 1) && (piecePool_->doesDownloadComplete()) ) {
						for(int i = 0; i < piecePool_->piece_num(); i++) {
							if (control->pieceList_[i] != COMPLETE) {
								seedFlag = false;
								break;
							}
						}
					} else {
						seedFlag = false;
					}
	
					BitPeer *peer = peerTable_->getPeer(control->sender_id_);
					if (max_peer_num_ != -1 && peerTable_->num() >= max_peer_num_) {
						log_error("will not added peer [PeerTable full - %d] - src[%d], pktType[%d], hopCount[%d]", peerTable_->num(), control->sender_id_, control->pktType_, hopCount);
						if (peer != NULL) peer->setPieceList(control->pieceList_);
						break;
					} else if (piecePool_->file_name() != control->file_name_) {
						log_error("will not added peer [Different File - mine[%d] src[%d]] - src[%d], pktType[%d], hopCount[%d]", piecePool_->file_name(), control->file_name_, control->sender_id_, control->pktType_, hopCount);
						if (peer != NULL) peer->setPieceList(control->pieceList_);
						break;
					} else if ( (peerTable_flag_ == 1) && (piecePool_->doesDownloadComplete()) && (seedFlag == true) ) {
							log_error("will not added peer [Seed PeerTable Update - %d] - src[%d], pktType[%d], hopCount[%d]", peerTable_->num(), control->sender_id_, control->pktType_, hopCount);
							if (peer != NULL) peer->setPieceList(control->pieceList_);
							break;
					}
	
					if (peer == NULL) {
						peer = new BitPeer(control->sender_id_, hopCount, piecePool_->piece_num());
						peerTable_->addPeer(peer);
					}
					peer->setPieceList(control->pieceList_);
	
					BitNeighbor *neighbor = neighborTable_->getNeighbor(control->sender_id_);
					if (neighbor != NULL) neighbor->setPieceList(control->pieceList_);
	
					if (control->pktType_ == HELLO) {
						sendBitPacket(HELLO_REPLY, control->sender_id_,1);
					}
	
					nextPiece();
				
				
				break;
                                }
			case UPDATE_PIECELIST:
				{
					BitPeer *peer = peerTable_->getPeer(control->sender_id_);
					if (peer == NULL) {
						log_error("receive UPDATE_PIECELIST from node[%d] who is not its peer", control->sender_id_);
						break;
					} else {
						peer->setPieceList(control->pieceList_);
					}
	
					BitNeighbor *neighbor = neighborTable_->getNeighbor(control->sender_id_);
					if (neighbor != NULL) {
						neighbor->setPieceList(control->pieceList_);
					}
	
					nextPiece();
				
				
				break;
                                }
			case PIECEOFFER_REQUEST:
				{
	//				log_debug("receive PIECEOFFER_REQUEST from node[%d]", control->sender_id_);
	
					bool findFlag = false;
					int piece_index = -1, block_index = -1;
					printf("THIS IS CONTROL: %d \n",control->new_);
	
					if (cur_download_num_ >= max_download_num_ && control->new_==1) {
						findFlag = false;
						piece_index = -1;
						block_index = -1;
					} else {
						BitPeer *peer = peerTable_->getPeer(control->sender_id_);
						if (peer != NULL) {
							peer->setPieceList(control->pieceList_);
						}
						if (peer->hopCount()<= Near_far_limit_)
						{
                                                  nextPiece();
						for (int i = 0; i < piecePool_->piece_num(); i++) {
							if (piecePool_->has_piece(i)) continue;
							if ( (local_rarest_count_[i] != 0) && (local_rarest_count_[i] != local_rarest_piece_num_) ) continue;
	
							if ( (control->piece_num_ > 0) && (control->pieceList_[i] == COMPLETE) ) {
								findFlag = true;
								piece_index = i;
								block_index = piecePool_->next_block(i);
	#ifdef BIT_DOWNLOADING
								if (block_index != -1) {
									piecePool_->setDownloadBlock(piece_index, block_index);
								}
	#endif
								break;
							}
						}
					
					
					
	
					if (findFlag) {
						log_state("I'll accept piece offer from node[%d]", control->sender_id_);
						sendBitPacket2(PIECEOFFER_REPLY, control->sender_id_, ACCEPT, piece_index, block_index);
					} else {
						log_state("I'll reject piece offer from node[%d]", control->sender_id_);
						/*
						log_debug("Here is my piecePool");
						if (logLevel(DEBUG)) piecePool_->print();
						log_debug("Here is my information about node");
						if (logLevel(DEBUG)) {
							BitPeer* peer = peerTable_->getPeer(control->sender_id_);
							if (peer != NULL) peer->print();
						}
	*/
						sendBitPacket2(PIECEOFFER_REPLY, control->sender_id_, REJECT, piece_index, block_index);
					}
				}
				else
				{
				if (neighborsandmehaveeverything()==false)
				{
				for (int i = 0; i < piecePool_->piece_num(); i++) {
				if (piecePool_->has_piece(i)) continue;
				if (neighborTable_->has_piece(i)) continue;
				
				if ( (control->piece_num_ > 0) && (control->pieceList_[i] == COMPLETE) ) {
								findFlag = true;
								piece_index = i;
								block_index = piecePool_->next_block(i);
	#ifdef BIT_DOWNLOADING
								if (block_index != -1) {
									piecePool_->setDownloadBlock(piece_index, block_index);
								}
	#endif
								break;
							}
				
				}
	
				}
	
				if (findFlag) {
						log_state("I'll accept piece offer from node[%d]", control->sender_id_);
						sendBitPacket2(PIECEOFFER_REPLY, control->sender_id_, ACCEPT, piece_index, block_index);
					} else {
						log_state("I'll reject piece offer from node[%d]", control->sender_id_);
						/*
						log_debug("Here is my piecePool");
						if (logLevel(DEBUG)) piecePool_->print();
						log_debug("Here is my information about node");
						if (logLevel(DEBUG)) {
							BitPeer* peer = peerTable_->getPeer(control->sender_id_);
							if (peer != NULL) peer->print();
						}
	*/                                      if (neighborsandmehaveeverything()==false)
						{
						sendBitPacket2(PIECEOFFER_REPLY, control->sender_id_, REJECT, piece_index, block_index);
						}
						else
						{
						sendBitPacket2(PIECEOFFER_REPLY, control->sender_id_, FINAL_REJECT, piece_index, block_index);
						}
					}
				
				}
				
				}
				
				break;
                                 }
			case PIECEOFFER_REPLY:
				{
					if (control->offer_flag_ == ACCEPT) {
						if ( (control->piece_index_ != -1) && (control->block_index_ != -1) ) 
                                                 {
							
								BitNeighbor *node = neighborTable_->getNeighbor(control->sender_id_);
								//if (node != NULL) addCountHCB(PIECEOFFER_REPLY, ACCEPT, node->hopCount());
							
							//Should I set timer here or at tryUploadCallback?
							//-->Yes, but only when the max_upload_num_ == 1
							if (max_upload_num_ == 1)
								tryUploadHandler_->resched(tryUploadInterval_);
							log_state("start to offer piece to node[%d] piece[%d] block[%d]", control->sender_id_, control->piece_index_, control->block_index_);
							sendData(control->sender_id_, control->piece_index_, control->block_index_);
						}
					} else if (control->offer_flag_ == CONTINUE) {
						BitPeer * peer = peerTable_->getPeer(control->sender_id_);
						peer->updatehopCount(routingAgent_);
	
						if (peer->hopCount()<=Near_far_limit_)
						{
						if ( (control->piece_index_ != -1) && (control->block_index_ != -1) ) {
							BitNeighbor *node = neighborTable_->getNeighbor(control->sender_id_);
							if ( (node != NULL) && (node->sendDataSlotIndex() == global_choking_slot_index_) ) {
								//addCountHCB(PIECEOFFER_REPLY, CONTINUE, node->hopCount());
								// When the node->sendDataSlotIndex == global_choking_slot_index
								// --> Within the timeslot which it selected by choking algorithm
								// --> continue to upload data to that node
								sendData(control->sender_id_, control->piece_index_, control->block_index_);
								// When the node->sendDataSlotIndex != global_choking_slot_index
								// --> Not within the timeslot which it selected by choking algorithm
								// --> stop to upload data to that node
								// --> Another node will be selected by tryUpload()
							}
						}
						if ( (control->piece_index_ == -1) && (control->block_index_ == -1) ) {
							BitNeighbor *node = neighborTable_->getNeighbor(control->sender_id_);
							if ( (node != NULL) && (node->sendDataSlotIndex() == global_choking_slot_index_) ) {
								//addCountHCB(PIECEOFFER_REPLY, CONTINUE, node->hopCount());
								// When the node->sendDataSlotIndex == global_choking_slot_index
								// --> Within the timeslot which it selected by choking algorithm
								// --> continue to upload data to that node
							sendBitPacket(PIECEOFFER_REQUEST, node->id(),0);
							
								// When the node->sendDataSlotIndex != global_choking_slot_index
								// --> Not within the timeslot which it selected by choking algorithm
								// --> stop to upload data to that node
								// --> Another node will be selected by tryUpload()
							}
						}
					}
					else
				{
	
				if ( (control->piece_index_ != -1) && (control->block_index_ != -1) ) {
							FarBitNeighbor *node = far_neighborTable_->getFarNeighbor(control->sender_id_);
							if ( (node != NULL) && (node->sendDataSlotIndex() == global_choking_slot_index_) ) {
								//addCountHCB(PIECEOFFER_REPLY, CONTINUE, node->hopCount());
								// When the node->sendDataSlotIndex == global_choking_slot_index
								// --> Within the timeslot which it selected by choking algorithm
								// --> continue to upload data to that node
								sendData(control->sender_id_, control->piece_index_, control->block_index_);
								// When the node->sendDataSlotIndex != global_choking_slot_index
								// --> Not within the timeslot which it selected by choking algorithm
								// --> stop to upload data to that node
								// --> Another node will be selected by tryUpload()
							}
						}
						if ( (control->piece_index_ == -1) && (control->block_index_ == -1) ) {
							FarBitNeighbor *node = far_neighborTable_->getFarNeighbor(control->sender_id_);
							if ( (node != NULL) && (node->sendDataSlotIndex() == global_choking_slot_index_) ) {
								//addCountHCB(PIECEOFFER_REPLY, CONTINUE, node->hopCount());
								// When the node->sendDataSlotIndex == global_choking_slot_index
								// --> Within the timeslot which it selected by choking algorithm
								// --> continue to upload data to that node
							sendBitPacket(PIECEOFFER_REQUEST, node->id(),0);
							
								// When the node->sendDataSlotIndex != global_choking_slot_index
								// --> Not within the timeslot which it selected by choking algorithm
								// --> stop to upload data to that node
								// --> Another node will be selected by tryUpload()
							}
						}
					
					}
	
					} else if (control->offer_flag_ == REJECT) {
						// Set that neighbor's rejectFlag as true
						BitPeer * peer = peerTable_->getPeer(control->sender_id_);
						peer->updatehopCount(routingAgent_);
	
						if (peer->hopCount()<=Near_far_limit_)
						{
						BitNeighbor *neighbor = neighborTable_->getNeighbor(control->sender_id_);
						if (neighbor != NULL) {
							neighbor->setRejectFlag(true);
							// For Parallel transmission, we'll loop to selectNodeToUpload from served_slot_num_ to max_upload_num_.
							// Therefore, when the node rejects my offer, the node should find another node to offer based on the number of max_upload_num_ - served_slot_num_.
							if (max_upload_num_ > 1) served_slot_num_--;
	
							// Find another neighbor to upload
								// When the reject message is arrived after expire the timer, ignore it.
							if (neighbor->sendDataSlotIndex() == global_choking_slot_index_) tryUpload();
						}
						}
						else
						{
						FarBitNeighbor *neighbor = far_neighborTable_->getFarNeighbor(control->sender_id_);
						if (neighbor != NULL) {
							neighbor->setRejectFlag(true);
							// For Parallel transmission, we'll loop to selectNodeToUpload from served_slot_num_ to max_upload_num_.
							// Therefore, when the node rejects my offer, the node should find another node to offer based on the number of max_upload_num_ - served_slot_num_.
							if (max_upload_num_ > 1) served_slot_num_--;
	
							// Find another neighbor to upload
							// When the reject message is arrived after expire the timer, ignore it.
							if (neighbor->sendDataSlotIndex() == global_choking_slot_index_) tryUpload();
							}
						}
						
									
	
						
					} 
                                        else if (control->offer_flag_ == FINAL_REJECT) {
	
				FarBitNeighbor *neighbor = far_neighborTable_->getFarNeighbor(control->sender_id_);
                                     if (neighbor!=NULL)
                                      {
				 far_neighborTable_->removeFarNeighbor(control->sender_id_);
                                 if (max_upload_num_ > 1) served_slot_num_--;
                                 if (neighbor->sendDataSlotIndex() == global_choking_slot_index_) tryUpload();
                                      }
					}
				
                                
				break;
				}
			
			default:
				break;
		
		}
		return;
		
	}
	
	
	void BitTorrent::processBitAgentPacket(hdr_bitAgent *pkt) {
		switch(pkt->pktType_) {
			case HELLO:
			case HELLO_REPLY:
				{
	#ifdef BITTO
					int hopCount = routingAgent_->hopCount(pkt->src_id_);
	#else
					int hopCount = -1;
	#endif
	
					bool seedFlag = true;
					if ( (peerTable_flag_ == 1) && (piecePool_->doesDownloadComplete()) ) {
						for(int i = 0; i < piecePool_->piece_num(); i++) {
							if (pkt->pieceList_[i] != COMPLETE) {
								seedFlag = false;
								break;
							}
						}
					} else {
						seedFlag = false;
					}
	
					BitPeer *peer = peerTable_->getPeer(pkt->src_id_);
					if (max_peer_num_ != -1 && peerTable_->num() >= max_peer_num_) {
						log_error("will not added peer [PeerTable full - %d] - src[%d], pktType[%d], hopCount[%d]", peerTable_->num(), pkt->src_id_, pkt->pktType_, hopCount);
						if (peer != NULL) peer->setPieceList(pkt->pieceList_);
						break;
					} else if (piecePool_->file_name() != pkt->file_name_) {
						log_error("will not added peer [Different File - mine[%d] src[%d]] - src[%d], pktType[%d], hopCount[%d]", piecePool_->file_name(), pkt->file_name_, pkt->src_id_, pkt->pktType_, hopCount);
						if (peer != NULL) peer->setPieceList(pkt->pieceList_);
						break;
					} else if ( (peerTable_flag_ == 1) && (piecePool_->doesDownloadComplete()) && (seedFlag == true) ) {
							log_error("will not added peer [Seed PeerTable Update - %d] - src[%d], pktType[%d], hopCount[%d]", peerTable_->num(), pkt->src_id_, pkt->pktType_, hopCount);
							if (peer != NULL) peer->setPieceList(pkt->pieceList_);
							break;
					}
	
					if (peer == NULL) {
						peer = new BitPeer(pkt->src_id_, hopCount, piecePool_->piece_num());
						peerTable_->addPeer(peer);
					}
					peer->setPieceList(pkt->pieceList_);
	
					BitNeighbor *neighbor = neighborTable_->getNeighbor(pkt->src_id_);
					if (neighbor != NULL) neighbor->setPieceList(pkt->pieceList_);
	
					if (pkt->pktType_ == HELLO) {
						sendBitPacket(HELLO_REPLY, pkt->src_id_,1);
					}
	
					nextPiece();
				
				
				break;
                                }
			case UPDATE_PIECELIST:
				{
					BitPeer *peer = peerTable_->getPeer(pkt->src_id_);
					if (peer == NULL) {
						log_error("receive UPDATE_PIECELIST from node[%d] who is not its peer", pkt->src_id_);
						break;
					} else {
						peer->setPieceList(pkt->pieceList_);
					}
	
					BitNeighbor *neighbor = neighborTable_->getNeighbor(pkt->src_id_);
					if (neighbor != NULL) {
						neighbor->setPieceList(pkt->pieceList_);
					}
	
					nextPiece();
				
				
				break;
                                }
			case PIECEOFFER_REQUEST:
				{
					bool findFlag = false;
					int piece_index = -1, block_index = -1;
					printf("THIS IS OFFER NEW: %d\n",pkt->new_);
	
					if (cur_download_num_ >= max_download_num_ && pkt->new_==1) {
                                                printf("Nous sommes dans le cas : cur_download_num_ >= max_download_num_\n");
						findFlag = false;
						piece_index = -1;
						block_index = -1;
					} else {
						BitPeer *peer = peerTable_->getPeer(pkt->src_id_);
						if (peer != NULL) {
							peer->setPieceList(pkt->pieceList_);
						}
                                                if (peer==NULL)
                                                 {
                                                 printf("nous sommes dans le cas peer = NULL \n");
                                                 //sendBitPacket2(PIECEOFFER_REPLY, pkt->src_id_, REJECT, piece_index, block_index);
                                                 }
                                                 else
                                                 {
						if (peer->hopCount()<= Near_far_limit_)
						{
                                                  printf("RECEIVE PIECE OFFER REQUEST FROM NEAR (id = %d)\n",id_);
                                                   nextPiece();
						for (int i = 0; i < piecePool_->piece_num(); i++) {
							if (piecePool_->has_piece(i)) continue;
							if ( (local_rarest_count_[i] != 0) && (local_rarest_count_[i] != local_rarest_piece_num_) ) continue;
							//if (local_rarest_count_[i] != local_rarest_piece_num_) continue;
							//if (peer->hasPiece(i)) {
							if ( (pkt->piece_num_ > 0) && (pkt->pieceList_[i] == COMPLETE) ) {
								findFlag = true;
								piece_index = i;
								block_index = piecePool_->next_block(i);
	#ifdef BIT_DOWNLOADING
								if (block_index != -1) {
									piecePool_->setDownloadBlock(piece_index, block_index);
								}
	#endif
								break;
							}
						}
					
	
					if (findFlag) {
						log_state("I'll accept piece offer from node[%d]", pkt->src_id_);
						sendBitPacket2(PIECEOFFER_REPLY, pkt->src_id_, ACCEPT, piece_index, block_index);
                                                 printf("ACCEPT FROM NEAR\n");
					} else {
						log_state("I'll reject piece offer from node[%d]", pkt->src_id_);
	/*
						log_debug("Here is my piecePool");
						if (logLevel(DEBUG)) piecePool_->print();
						log_debug("Here is my information about node");
						if (logLevel(DEBUG)) {
							BitPeer* peer = peerTable_->getPeer(pkt->src_id_);
							if (peer != NULL) peer->print();
						}
	*/                                      printf("REJECT FROM NEAR\n");
						sendBitPacket2(PIECEOFFER_REPLY, pkt->src_id_, REJECT, piece_index, block_index);
					
				}}
				
				else
				{
                                 printf("RECEIVE PIECE OFFER REQUEST FROM FAR (id = %d)\n", id_);
			if (neighborsandmehaveeverything()==false)
				{
				for (int i = 0; i < piecePool_->piece_num(); i++) {
				if (piecePool_->has_piece(i)) continue;
				if (neighborTable_->has_piece(i)) continue;
				
				if ( (pkt->piece_num_ > 0) && (pkt->pieceList_[i] == COMPLETE) ) {
								findFlag = true;
								piece_index = i;
								block_index = piecePool_->next_block(i);
	#ifdef BIT_DOWNLOADING
								if (block_index != -1) {
									piecePool_->setDownloadBlock(piece_index, block_index);
								}
	#endif
								break;
							}
				
				}
	
				}
	
				if (findFlag) {
                                                printf("ACCEPT FROM FAR (id = %d) \n",id_);
						log_state("I'll accept piece offer from node[%d]", pkt->src_id_);
						sendBitPacket2(PIECEOFFER_REPLY, pkt->src_id_, ACCEPT, piece_index, block_index);
					} else {
                                                printf("REJECT FROM FAR (id = %d)\n",id_);
						log_state("I'll reject piece offer from node[%d]", pkt->src_id_);
						/*
						log_debug("Here is my piecePool");
						if (logLevel(DEBUG)) piecePool_->print();
						log_debug("Here is my information about node");
						if (logLevel(DEBUG)) {
							BitPeer* peer = peerTable_->getPeer(control->sender_id_);
							if (peer != NULL) peer->print();
						}
	*/                                      if (neighborsandmehaveeverything()==false)
						{
                                                printf("rejet partiel\n");
						sendBitPacket2(PIECEOFFER_REPLY, pkt->src_id_, REJECT, piece_index, block_index);
						}
						else
						{
                                                printf("rejet total\n");
						sendBitPacket2(PIECEOFFER_REPLY, pkt->src_id_, FINAL_REJECT, piece_index, block_index);
						}
					}
				
				
				
					}
				}
				}
				
				break;
                                }
			case PIECEOFFER_REPLY:
				{
					if (pkt->offer_flag_ == ACCEPT) {
						if ( (pkt->piece_index_ != -1) && (pkt->block_index_ != -1) ) 
                                                 {
							printf(" RECEIVED PIECE OFFER REPLY ACCEPT (id = %d)\n",id_);
								BitNeighbor *node = neighborTable_->getNeighbor(pkt->src_id_);
								//if (node != NULL) addCountHCB(PIECEOFFER_REPLY, ACCEPT, node->hopCount());
							// if (node != NULL) addTimeHCT(node->hopCount());
							
							//Should I set timer here or at tryUploadCallback?
							//-->Yes, but only when the max_upload_num_ == 1
							if (max_upload_num_ == 1)
								tryUploadHandler_->resched(tryUploadInterval_);
							log_state("start to offer piece to node[%d] piece[%d] block[%d]", pkt->src_id_, pkt->piece_index_, pkt->block_index_);
							sendData(pkt->src_id_, pkt->piece_index_, pkt->block_index_);
						}
					} else if (pkt->offer_flag_ == CONTINUE) {
                                            printf("received continue\n");
						BitPeer * peer = peerTable_->getPeer(pkt->src_id_);
                                                int hop;
                                              if (peer == NULL)
                                              {
                                              hop=routingAgent_->hopCount(pkt->src_id_);
                                              printf("received continue from NULL peer\n");
                                              }
                                              else
                                              {
						peer->updatehopCount(routingAgent_);
                                                hop=peer->hopCount();
                                               }
	
						if (hop <=Near_far_limit_)
						{
						if ( (pkt->piece_index_ != -1) && (pkt->block_index_ != -1) ) {
							BitNeighbor *node = neighborTable_->getNeighbor(pkt->src_id_);
							if ( (node != NULL) && (node->sendDataSlotIndex() == global_choking_slot_index_) ) {
								//addCountHCB(PIECEOFFER_REPLY, CONTINUE, node->hopCount());
								// When the node->sendDataSlotIndex == global_choking_slot_index
								// --> Within the timeslot which it selected by choking algorithm
								// --> continue to upload data to that node
								sendData(pkt->src_id_, pkt->piece_index_, pkt->block_index_);
								// When the node->sendDataSlotIndex != global_choking_slot_index
								// --> Not within the timeslot which it selected by choking algorithm
								// --> stop to upload data to that node
								// --> Another node will be selected by tryUpload()
							}
						}
						if ( (pkt->piece_index_ == -1) && (pkt->block_index_ == -1) ) {
							BitNeighbor *node = neighborTable_->getNeighbor(pkt->src_id_);
							if ( (node != NULL) && (node->sendDataSlotIndex() == global_choking_slot_index_) ) {
								//addCountHCB(PIECEOFFER_REPLY, CONTINUE, node->hopCount());
								// When the node->sendDataSlotIndex == global_choking_slot_index
								// --> Within the timeslot which it selected by choking algorithm
								// --> continue to upload data to that node
							sendBitPacket(PIECEOFFER_REQUEST, node->id(),0);
							
								// When the node->sendDataSlotIndex != global_choking_slot_index
								// --> Not within the timeslot which it selected by choking algorithm
								// --> stop to upload data to that node
								// --> Another node will be selected by tryUpload()
							}
						}
						}
						else
						{
						if ( (pkt->piece_index_ != -1) && (pkt->block_index_ != -1) ) {
							FarBitNeighbor *node = far_neighborTable_->getFarNeighbor(pkt->src_id_);
							if ( (node != NULL) && (node->sendDataSlotIndex() == global_choking_slot_index_) ) {
								//addCountHCB(PIECEOFFER_REPLY, CONTINUE, node->hopCount());
								// When the node->sendDataSlotIndex == global_choking_slot_index
								// --> Within the timeslot which it selected by choking algorithm
								// --> continue to upload data to that node
								sendData(pkt->src_id_, pkt->piece_index_, pkt->block_index_);
								// When the node->sendDataSlotIndex != global_choking_slot_index
								// --> Not within the timeslot which it selected by choking algorithm
								// --> stop to upload data to that node
								// --> Another node will be selected by tryUpload()
							}
						}
						if ( (pkt->piece_index_ == -1) && (pkt->block_index_ == -1) ) {
							FarBitNeighbor *node = far_neighborTable_->getFarNeighbor(pkt->src_id_);
							if ( (node != NULL) && (node->sendDataSlotIndex() == global_choking_slot_index_) ) {
								//addCountHCB(PIECEOFFER_REPLY, CONTINUE, node->hopCount());
								// When the node->sendDataSlotIndex == global_choking_slot_index
								// --> Within the timeslot which it selected by choking algorithm
								// --> continue to upload data to that node
							sendBitPacket(PIECEOFFER_REQUEST, node->id(),0);
							
								// When the node->sendDataSlotIndex != global_choking_slot_index
								// --> Not within the timeslot which it selected by choking algorithm
								// --> stop to upload data to that node
								// --> Another node will be selected by tryUpload()
							}
						}
						}
						
					} else if (pkt->offer_flag_ == REJECT) {
	
						BitPeer * peer = peerTable_->getPeer(pkt->src_id_);
						peer->updatehopCount(routingAgent_);
                                                   if (peer ==NULL)
                                                   {
                                                    printf("received reject from NULL peer\n");
                                                   }
	                                         if (peer != NULL)
                                                  {
						if (peer->hopCount()<=Near_far_limit_)
						{
						BitNeighbor *neighbor = neighborTable_->getNeighbor(pkt->src_id_);
                                                if (neighbor != NULL) {
							neighbor->setRejectFlag(true);
							// For Parallel transmission, we'll loop to selectNodeToUpload from served_slot_num_ to max_upload_num_.
							// Therefore, when the node rejects my offer, the node should find another node to offer based on the number of max_upload_num_ - served_slot_num_.
							if (max_upload_num_ > 1) served_slot_num_--;
	
							// Find another neighbor to upload
							// When the reject message is arrived after expire the timer, ignore it.
							if (neighbor->sendDataSlotIndex() == global_choking_slot_index_) tryUpload();
						}
	
						if (0) {
							log_debug("node[%d] reject my pieceoffer", pkt->src_id_);
							log_debug("Here is my pieceList");
							if (logLevel(DEBUG)) piecePool_->print();
	
							log_debug("Here is my information about peer");
							if (logLevel(DEBUG)) neighbor->print();
						}
						}
						else
						{
						FarBitNeighbor *neighbor = far_neighborTable_->getFarNeighbor(pkt->src_id_);
                                                 if (neighbor != NULL) {
							neighbor->setRejectFlag(true);
							// For Parallel transmission, we'll loop to selectNodeToUpload from served_slot_num_ to max_upload_num_.
							// Therefore, when the node rejects my offer, the node should find another node to offer based on the number of max_upload_num_ - served_slot_num_.
							if (max_upload_num_ > 1) served_slot_num_--;
	
							// Find another neighbor to upload
							// When the reject message is arrived after expire the timer, ignore it.
							if (neighbor->sendDataSlotIndex() == global_choking_slot_index_) tryUpload();
						}
	
						if (0) {
							log_debug("node[%d] reject my pieceoffer", pkt->src_id_);
							log_debug("Here is my pieceList");
							if (logLevel(DEBUG)) piecePool_->print();
	
							log_debug("Here is my information about peer");
							if (logLevel(DEBUG)) neighbor->print();
						}
						}
	                                  }
						
					} else if (pkt->offer_flag_ == FINAL_REJECT) {
	
				FarBitNeighbor *neighbor = far_neighborTable_->getFarNeighbor(pkt->src_id_);
                                if (neighbor!= NULL)
                                 {
				far_neighborTable_->removeFarNeighbor(pkt->src_id_);
				  }
                                  }
				
				break;
                         }
			default:
				break;
		}
		return;
	}
 bool BitTorrent::neighborsandmehaveeverything()
{
bool answer;
answer=true;
for(int i=0;i<piecePool_->piece_num();i++)
{
if (neighborTable_->has_piece(i)== false && piecePool_->has_piece(i)==false)
{
answer=false;
break;
}
}
return(answer);
}



	
	void BitTorrent::process_data(int size, AppData* payload) {
	}
	
	void BitTorrent::sendBitPacket(int pktType, int dst,int new1) {
		if ( (control_tcp_flag_ == 1) &&
				(pktType != HELLO) ) {
			// use TCP to send control packet except HELLO
			BitTorrentControl *control = new BitTorrentControl(pktType);
	
			control->sender_id_ = id_;
			control->receiver_id_ = dst;
			control->new_=new1;
	
			switch(pktType) {
				case PIECEOFFER_REQUEST:
					{
                                            
	//					log_debug("send PIECEOFFER_REQUEST to node[%d]", dst);
						BitPeer *node = peerTable_->getPeer(dst);
						if (node == NULL) {
							log_fail("invalid dst[%d] for sendBitPacket(TCP) - he's not neighbor - pktType[%d]", dst, pktType);
							return;
						} else {
							//addCountHCB(PIECEOFFER_REQUEST, 0, node->hopCount());
						}
	
						control->file_name_ = piecePool_->file_name();
						control->piece_num_ = piecePool_->piece_num();
						if (control->piece_num_ > 0) {
							control->pieceList_ = new int[piecePool_->piece_num()+1];
							int *my_pieces = piecePool_->pieces_state();
	
							for(int i = 0; i < piecePool_->piece_num(); i++) {
								control->pieceList_[i] = my_pieces[i];
							}
						} else control->pieceList_ = NULL;
	
						node->connect(this);
						if (node->sendControl(control, this) < 0) {
							log_fail("sendControl error - dst[%d]", dst);
						}
					}
					break;
				case HELLO_REPLY:
					{
						BitPeer *node = peerTable_->getPeer(dst);
						if (node == NULL) {
							log_fail("invalid dst[%d] for sendBitPacket(TCP) - he's not peer - pktType[%d]", dst, pktType);
							return;
						}
	
						control->file_name_ = piecePool_->file_name();
						control->piece_num_ = piecePool_->piece_num();
						if (control->piece_num_ > 0) {
							control->pieceList_ = new int[piecePool_->piece_num()+1];
							int *my_pieces = piecePool_->pieces_state();
	
							for(int i = 0; i < piecePool_->piece_num(); i++) {
								control->pieceList_[i] = my_pieces[i];
							}
						} else control->pieceList_ = NULL;
	
						node->connect(this);
						if (node->sendControl(control, this) < 0) {
							log_error("sendControl error - dst[%d]", dst);
						}
					}
					break;
				case UPDATE_PIECELIST:
					{
						BitPeer *node = peerTable_->getPeer(dst);
						if (node == NULL) {
							log_fail("invalid dst[%d] for sendBitPacket(TCP) - he's not peer - pktType[%d]", dst, pktType);
							return;
						}
	
						control->file_name_ = piecePool_->file_name();
						control->piece_num_ = piecePool_->piece_num();
						if (control->piece_num_ > 0) {
							control->pieceList_ = new int[piecePool_->piece_num()+1];
							int *my_pieces = piecePool_->pieces_state();
	
							for(int i = 0; i < piecePool_->piece_num(); i++) {
								control->pieceList_[i] = my_pieces[i];
							}
						} else control->pieceList_ = NULL;
	
						node->connect(this);
						if (node->sendControl(control, this) < 0) {
							log_error("sendControl error - dst[%d]", dst);
						}
					}
					break;
				default:
					break;
			}
		} else {
			// use UDP
			Packet *pkt = bitAgent_->allocpacket();
			struct hdr_bitAgent *bith = hdr_bitAgent::access(pkt);
			bith->new_=new1;
			bith->pktType_ = pktType;
	
			switch(pktType) {
				case HELLO:
				case HELLO_REPLY:
				case UPDATE_PIECELIST:
					bith->file_name_ = piecePool_->file_name();
					bith->piece_num_ = piecePool_->piece_num();
	
					if (bith->piece_num_ > 0) {
						bith->pieceList_ = new int[piecePool_->piece_num()+1];
						int *my_pieces = piecePool_->pieces_state();
	
						for(int i = 0; i < piecePool_->piece_num(); i++) {
							bith->pieceList_[i] = my_pieces[i];
						}
					} else bith->pieceList_ = NULL;
	
					if (0) {
						char buff[1024];
	
						if (bith->piece_num_ > 0) {
							memset(buff, 0x00, sizeof(buff));
							sprintf(buff, "PieceList - ");
							for(int i = 0; i < bith->piece_num_; i++) {
								char temp[10];
	
								sprintf(temp, "%d", bith->pieceList_[i]);
								strcat(buff, temp);
							}
						} else {
							sprintf(buff, "NO_PIECE");
						}
						log_state("send HELLO or HELLO_REPLY packet - node[%d] with piece_num[%d] piece_list[%s]", dst, bith->piece_num_, buff);
					}
					break;
				case PIECEOFFER_REQUEST:
					{
						BitPeer *node = peerTable_->getPeer(dst);
						if (node == NULL) {
                                                       printf("COULD NOT FIND DESTINATION NODE FOR PIECE OFFER REQUEST (id = %d)\n",id_);
							log_fail("invalid dst[%d] for sendBitPacket(TCP) - he's not neighbor - pktType[%d]", dst, pktType);
							return;
						} else {
							//addCountHCB(PIECEOFFER_REQUEST, 0, node->hopCount());
						}
	
						bith->file_name_ = piecePool_->file_name();
						bith->piece_num_ = piecePool_->piece_num();
						if (bith->piece_num_ > 0) {
							bith->pieceList_ = new int[piecePool_->piece_num()+1];
							int *my_pieces = piecePool_->pieces_state();
	
							for(int i = 0; i < piecePool_->piece_num(); i++) {
								bith->pieceList_[i] = my_pieces[i];
							}
						} else bith->pieceList_ = NULL;
					}
					break;
				case PIECEOFFER_REPLY:
					log_fail("!!!!!!!!!! Don't use sendBitPacket for PIECEOFFER_REPLY - use sendBitPacket2 - dst[%d]", dst);
					abort();
					break;
				default:
					log_fail("invalid pktType[%d] dst[%d]", pktType, dst);
					break;
			}
	
			bitAgent_->sendBitPacket(pkt, dst);
		}
		return;
	}
	
	void BitTorrent::sendBitPacket2(int pktType, int dst, int flag, int piece_index, int block_index) {
		if (pktType != PIECEOFFER_REPLY) {
			log_fail("!!!!!!!!!! Don't use sendBitPacket2 for this type[%d] - use sendBitPacket - dst[%d]", pktType, dst);
			return;
		}
	
		if (control_tcp_flag_ == 1) {
			// use TCP to send PIECEOFFER_REPLY
			// For PIECEOFFER_REPLY, we'll reuse the TCP connection which used for PIECEOFFER_REQUEST
			// Because, the sender of PIECEOFFER_REQUEST can be not my peer and neighbor
			BitTorrent *peer = GlobalBitTorrentList::instance().getNode(dst);
			if (peer == NULL) {
				log_fail("invalid dst[%d] for sendBitPacket2(TCP) - he's not neighbor - pktType[%d]", dst, pktType);
				return;
			}
	
			BitTorrentControl *control = new BitTorrentControl(pktType);
	
			control->offer_flag_ = flag; 
			control->piece_index_ = piece_index;
			control->block_index_ = block_index;
	
			control->sender_id_ = id_;
			control->receiver_id_ = dst;
	
			BitConnection* bitCon = GlobalBitConnectionList::instance().getConnection(peer->id(), id());
			if (bitCon == NULL) {
				if (use_bidirection_tcp_flag_ == 1) {
					bitCon = GlobalBitConnectionList::instance().getConnection(id(), peer->id());
	
					if (bitCon == NULL) {
						log_fail("cannot find BitConnection for PIECEOFFER_REPLY - owner[%d] peer[%d]\n", peer->id(), id());
                                                 printf("cannot find BitConnection for PIECEOFFER_REPLY - owner[%d] peer[%d]\n",peer->id(), id());
						abort();
						return;
					}
				}
			}
			bitCon->sendControl(control);
		} else {
			// use UDP to send PIECEOFFER_REPLY
			Packet *pkt = bitAgent_->allocpacket();
			struct hdr_bitAgent *bith = hdr_bitAgent::access(pkt);
	
			bith->pktType_ = pktType;
	
			bith->offer_flag_ = flag; 
			bith->piece_index_ = piece_index;
			bith->block_index_ = block_index;
	
			bitAgent_->sendBitPacket(pkt, dst);
		}
		return;
	}
	
	bool BitTorrent::hasFile(int file) {
		return piecePool_->isSameFile(file);
	}
	
	int BitTorrent::calPieceCount(int piece_id) {
		int count = 0;
	
		if (local_rarest_select_flag_ == 1) {
			Tcl_HashEntry *he;
			Tcl_HashSearch hs;
	
			for(he = Tcl_FirstHashEntry(peerTable_->table(), &hs);
					he != NULL;
					he = Tcl_NextHashEntry(&hs)) {
				BitPeer *node = (BitPeer *)Tcl_GetHashValue(he);
				if (node->hasPiece(piece_id)) count++;
			}
		} else {
			Tcl_HashEntry *he;
			Tcl_HashSearch hs;
	
			for(he = Tcl_FirstHashEntry(neighborTable_->table(), &hs);
					he != NULL;
					he = Tcl_NextHashEntry(&hs)) {
				BitNeighbor *node = (BitNeighbor *)Tcl_GetHashValue(he);
				if (node->hasPiece(piece_id)) count++;
			}
		}
		return count;
	}
	
	int BitTorrent::nextPiece() {
	//	log_debug("nextPiece start - completeFlag[%d] peerTable[%d] neighborTable[%d]", piecePool_->doesDownloadComplete(), peerTable_->num(), neighborTable_->num());
	
		if (piecePool_->doesDownloadComplete()) return -1;
		else if (local_rarest_select_flag_ == 1 && peerTable_->num() <= 0) {
			log_error("call nextPiece without peer[%d]", peerTable_->num());
			return -1;
		} else if (local_rarest_select_flag_ == 2 && neighborTable_->num() <= 0) {
			log_error("call nextPiece without neighbor[%d]", neighborTable_->num());
			return -1;
		}
	
		int rarest = -1;
		int piece_num = piecePool_->piece_num();
		int* pieces_state = piecePool_->pieces_state();
	
	
	
		// Set local_rarest_count_ as my having piecesState
		for(int i = 0; i < piece_num; i++) {
			if (pieces_state[i] == INCOMPLETE) {
				local_rarest_count_[i] = calPieceCount(i);
			} else {
				//!!!!!!????? Think what's better to set -1 or 0
				//Because if rare_count is 0, BitTorrent node doesn't try to download to piece (It cannot)
				local_rarest_count_[i] = -1;
			}
	
	//		log_debug("pieceState[%d] = %d, local_rarest_count[%d] = %d", 
	//				i, pieces_state[i], i, local_rarest_count_[i]);
		}
	
		{
			// Select StartPoint among not COMPLETE piece_index
			for(int i = 0; i < piece_num; i++) {
				if (local_rarest_count_[i] != -1 && local_rarest_count_[i] != 0) {
					rarest = i;
					break;
				}
			}
	
			if (rarest == -1) {
				int num = 0;
				if (local_rarest_select_flag_ == 1) {
					num = peerTable_->num();
				} else {
					num = neighborTable_->num();
				}
				if (num != 0) log_error("can not found local_rarest_count more than -1 - No peer or neighbor has no piece that I don't have");
				return -1;
			}
	
			// Search the rarest piece_index
			for(int i = 0; i < piece_num; i++) {
				//!!!!!!????? Think what's better to set -1 or 0
				//Because if rare_count is 0, BitTorrent node doesn't try to download to piece (It cannot)
				if (local_rarest_count_[i] == -1) continue;
				//if (local_rarest_count_[i] == 0) continue;
				if (local_rarest_count_[i] < local_rarest_count_[rarest]) {
					rarest = i;
				} else
				{
				if (local_rarest_count_[i]==local_rarest_count_[rarest])
				{
				int random1 = Random::integer(6);
				if (random1>3)
				{
				rarest=i;
				}
				}
				}
			}
	
			if (local_rarest_count_[rarest] == -1) {
				log_fail("something wrong - local_rarest_count_[rarest] == -1!!!!!");
                                printf("something wrong - local_rarest_count_[rarest] == -1!!!!!");
				abort();
				return -1;
			}
		}
	
		local_rarest_piece_num_ = local_rarest_count_[rarest];
		
		return rarest;
	}
	
	bool BitTorrent::isOptimisticSlot() {
		return isBestSlot() ? false : true;
	}
	
	bool BitTorrent::isBestSlot() {
		if (choking_slot_index_ < choking_best_slot_num_) return true;
		else return false;
	}
	
	BitNeighbor* BitTorrent::getFirstNode() {
		Tcl_HashEntry *he;
		Tcl_HashSearch hs;
	
		for(he = Tcl_FirstHashEntry(neighborTable_->table(), &hs);
				he != NULL;
				he = Tcl_NextHashEntry(&hs)) {
			BitNeighbor *node = (BitNeighbor *)Tcl_GetHashValue(he);
			if (node == NULL) {
				log_fail("node is NULL - neighborNum[%d]\n", neighborTable_->num());
				continue;
			}
			log_debug("node[%d][%d] select[%d] isSeed[%d]", node->id(), node->rejectFlag(), node->selectFlag(), node->isSeed());
	//		log_debug("selectNodeToUpload_flag[%d] ONE_HOP[%d] hopCount[%d]", selectNodeToUpload_flag_, ONE_HOP, node->hopCount());
			if ( (node->rejectFlag() == false) &&
					(node->selectFlag() == false) &&
					(node->isSeed() == false) ) {
					if ( (selectNodeToUpload_flag_ == ONE_HOP) && (node->hopCount() != 1) ) continue;
					else return node;
			}
		}
	
		return NULL;
	}
	
	FarBitNeighbor* BitTorrent::getFirstFarNode() {
		
	        Tcl_HashEntry *he;
		Tcl_HashSearch hs;
	
		for(he = Tcl_FirstHashEntry(far_neighborTable_->table(), &hs);
				he != NULL;
				he = Tcl_NextHashEntry(&hs)) {
			FarBitNeighbor *node = (FarBitNeighbor *)Tcl_GetHashValue(he);
			if (node == NULL) {
				log_fail("node is NULL - neighborNum[%d]\n", far_neighborTable_->num());
				continue;
			}
	//		log_debug("node[%d] reject[%d] select[%d] isSeed[%d]", node->id(), node->rejectFlag(), node->selectFlag(), node->isSeed());
	//		log_debug("selectNodeToUpload_flag[%d] ONE_HOP[%d] hopCount[%d]", selectNodeToUpload_flag_, ONE_HOP, node->hopCount());
			if ( (node->rejectFlag() == false) &&
					(node->selectFlag() == false)) {
					if ( (selectNodeToUpload_flag_ == ONE_HOP) && (node->hopCount() != 1) ) continue;
					else return node;
			}
		}
	
		return NULL;
	}
	
	
	BitNeighbor* BitTorrent::getMostDownloadNode() {
		BitNeighbor *bestNeighbor = NULL;
		unsigned int bestBytes = 0;
	
		Tcl_HashEntry *he;
		Tcl_HashSearch hs;
	
		for(he = Tcl_FirstHashEntry(neighborTable_->table(), &hs);
				he != NULL;
				he = Tcl_NextHashEntry(&hs)) {
			BitNeighbor *node = (BitNeighbor *)Tcl_GetHashValue(he);
			if (node == NULL) {
				log_fail("node is NULL - neighborNum[%d]\n", neighborTable_->num());
				continue;
			}
	//		log_debug("node[%d] reject[%d] select[%d] bytes[%ld] isSeed[%d]", node->id(), node->rejectFlag(), node->selectFlag(), node->getBytes(), node->isSeed());
			if ( (node->rejectFlag() == false) &&
					(node->selectFlag() == false) &&
					(node->isSeed() == false) &&
					(node->getBytes() > bestBytes)
					) {
				if ( (selectNodeToUpload_flag_ == ONE_HOP) && (node->hopCount() != 1) ) continue;
				else {
					bestNeighbor = node;
					bestBytes = node->getBytes();
				}
			}
		}
		if (bestBytes==0)
		{
		bestNeighbor=NULL;
		}
	//	if (bestNeighbor != NULL) log_debug("bestBytes[%ld] bestNeighbor[%d]", bestBytes, bestNeighbor->id());
	
		return bestNeighbor;
	}
	
	FarBitNeighbor* BitTorrent::getMostDownloadFarNode() {
		FarBitNeighbor *bestNeighbor = NULL;
		unsigned int bestBytes = 0;
	
		Tcl_HashEntry *he;
		Tcl_HashSearch hs;
	
		for(he = Tcl_FirstHashEntry(far_neighborTable_->table(), &hs);
				he != NULL;
				he = Tcl_NextHashEntry(&hs)) {
			FarBitNeighbor *node = (FarBitNeighbor *)Tcl_GetHashValue(he);
			if (node == NULL) {
				log_fail("node is NULL - neighborNum[%d]\n", far_neighborTable_->num());
				continue;
			}
	//		log_debug("node[%d] reject[%d] select[%d] bytes[%ld] isSeed[%d]", node->id(), node->rejectFlag(), node->selectFlag(), node->getBytes(), node->isSeed());
			if ( (node->rejectFlag() == false) &&
					(node->selectFlag() == false) &&
					(node->getBytes() > bestBytes)
					) {
				if ( (selectNodeToUpload_flag_ == ONE_HOP) && (node->hopCount() != 1) ) continue;
				else {
					bestNeighbor = node;
					bestBytes = node->getBytes();
				}
			}
		}
		if (bestBytes==0)
		{
		bestNeighbor=NULL;
		}
	//	if (bestNeighbor != NULL) log_debug("bestBytes[%ld] bestNeighbor[%d]", bestBytes, bestNeighbor->id());
	
		return bestNeighbor;
	}
	
	BitNeighbor* BitTorrent::getClosestNode() {
		BitNeighbor *closestNode = NULL;
		int hopCount = bitAgent_ == NULL ? 1000 : bitAgent_->flooding_ttl() + 1;
	
		Tcl_HashEntry *he;
		Tcl_HashSearch hs;
	
		for(he = Tcl_FirstHashEntry(neighborTable_->table(), &hs);
				he != NULL;
				he = Tcl_NextHashEntry(&hs)) {
			BitNeighbor *node = (BitNeighbor *)Tcl_GetHashValue(he);
			if (node == NULL) {
				log_fail("node is NULL - neighborNum[%d]\n", neighborTable_->num());
				continue;
			}
			if ( (node->rejectFlag() == false) &&
					(node->selectFlag() == false) &&
					(node->isSeed() == false) &&
					(node->hopCount() < hopCount)
					) {
				closestNode = node;
				hopCount = node->hopCount();
			}
		}
		
		return closestNode;
	}
	
	BitNeighbor* BitTorrent::getRandomNode() {

		int maxNum = MAXRETRY < neighborTable_->max_num() ? MAXRETRY : neighborTable_->max_num();
	
	// printf(" FLAG = %d \n", selectRandomNode_flag_);
	
	

		for (int i = 0; i < maxNum; i++) 
                {
			//int random = Random::integer(neighborTable_->num());
			int random = Random::integer(max_node_num_);
			//printf("RN : %d \n", random);
			BitNeighbor *node = neighborTable_->getNeighbor(random);
			if ( (node != NULL) && 
					(node->selectFlag() == false) &&
					(node->rejectFlag() == false) &&
					(node->isSeed() == false) ) {
				if ( (selectNodeToUpload_flag_ == ONE_HOP) && (node->hopCount() != 1) ) continue;
				else return node;
			}
		}
	
	
	
		return NULL;
	}
	
	FarBitNeighbor* BitTorrent::getRandomFarNode() {
 	printf("get random far node executed \n");

        if (routingAgent_ == NULL)
        {
        printf("Routing agent is NULL \n");
         }
        int maxNum = MAXRETRY < far_neighborTable_->max_num() ? MAXRETRY : far_neighborTable_->max_num();
	far_neighborTable_->updatehopscount(routingAgent_);
        for (int i = 0; i < maxNum; i++) 
        {
	FarBitNeighbor *node = far_neighborTable_->select_randomNode();

	if ( (node != NULL) && 
					(node->selectFlag() == false) &&
					(node->rejectFlag() == false) ){
				if ( (selectNodeToUpload_flag_ == ONE_HOP) && (node->hopCount() != 1) ) continue;
				else return node;
			}
	}
	return NULL;
	}
	
	
	
	int BitTorrent::selectNodeToUploadSerial() {
		if ( (selectNodeToUpload_flag_ == ONE_HOP) || (selectNodeToUpload_flag_ == CLOSEST) )  {
			log_fail("I didn't implement serial upload except basic node select");
			abort();
			return -1;
		}
	
		if (neighborTable_->num() <= 0 && far_neighborTable_->num()<=0) return -1;
		if (max_upload_num_ != 1) return -1;
	
	//	log_debug("Serial Transmission");
	//	log_debug("global_slot[%d] choking_slot_index[%d] best[%d:%d] optimistic[%d:%d]", global_choking_slot_index_, choking_slot_index_, choking_best_slot_num_, isBestSlot(), choking_optimistic_slot_num_, isOptimisticSlot());
		BitNeighbor *node = NULL;
		if (piecePool_->doesDownloadComplete()) {
			// Basic scheme for choking algorithm of seed state node
			// For best slots, select the closest node based on hopCount
			// For optimistic slots, select a node randomly
	//			log_debug("This is Seed State");
			if (isBestSlot()) {
	//			log_debug("select ClosestNode");
				if (node = getClosestNode(), node == NULL) {
	//			log_debug("select FirstNode");
					if (node = getFirstNode(), node == NULL) {
						return -1;
					}
				}
			} else {
	//			log_debug("select RandomNode");
				if (node = getRandomNode(), node == NULL) {
					if (node = getFirstNode(), node == NULL) {
	//			log_debug("select FirstNode");
						return -1;
					}
				}
			}
		} else {
	//			log_debug("This is Leecher State - neighbor_num[%d] slot_num[%d]", neighborTable_->num(), choking_slot_num_);
			if (neighborTable_->num() <= choking_slot_num_) {
				// When all the neighbors can be serviced at time slots
				// --> Round-Robin
	//			log_debug("select FirstNode - round-robin");
				if (node = getFirstNode(), node == NULL) {
					return -1;
				}
			} else {
				// Basic scheme for choking algorithm
				// For best slots, select the best node based on neighbor's download bytes
				// For optimistic slots, select a node randomly
				if (isBestSlot()) {
	//				log_debug("select MostDownloadNode");
					if (node = getMostDownloadNode(), node == NULL) {
	//				log_debug("select FirstNode");
						if (node = getRandomNode(), node == NULL) {
	//				log_debug("select FirstNode");
						if (node = getFirstNode(), node == NULL) {
							return -1;
						}
					}
					}
				} else if (isOptimisticSlot()) {
	//				log_debug("select RandomNode");
					if (node = getRandomNode(), node == NULL) {
	//				log_debug("select FirstNode");
						if (node = getFirstNode(), node == NULL) {
							return -1;
						}
					}
				}
			}
		}
	
		served_slot_num_++;
		node->setSelectFlag(true);
		node->setSendDataSlotIndex(global_choking_slot_index_);
		return node->id();
	}
	
	int BitTorrent::selectNodeToUploadParallel(int selectType) {
		if (neighborTable_->num() <= 0 && far_neighborTable_->num() <=0)
                 {
                 printf("RETURN - 1 : neighborTable_->num() <= 0 && far_neighborTable_->num() <=0\n");
                 return -1;
                 }
		if (max_upload_num_ <= 1) return -1;
	
				log_debug("Parallel Transmission");
		BitNeighbor *node = NULL;
		FarBitNeighbor *node_far= NULL;
	
		switch(selectNodeToUpload_flag_) {
			case BASIC:
				if (piecePool_->doesDownloadComplete()) {
						log_debug("This is Seed State");
					if (selectType == BEST_SLOT) {
						log_debug("This is Best Slot - select ClosestNode");
						if (node = getClosestNode(), node == NULL) {
						log_debug("This is Best Slot - select FirstNode");
							if (node = getFirstNode(), node == NULL) {
								return -1;
							}
						}
					} else if (selectType == OPTIMISTIC_SLOT) {
						log_debug("This is Optimistic Slot - select RandomNode");
						if (node = getRandomNode(), node == NULL) {
							if (node = getFirstNode(), node == NULL) {
							log_debug("select FirstNode");
								return -1;
							}
						}
					}
				} else {
						log_debug("This is Leecher State - neighbor_num[%d] slot_num[%d]", neighborTable_->num(), choking_slot_num_);
					if (neighborTable_->num() <= choking_slot_num_) {
						// When all the neighbors can be serviced at time slots
						// --> Round-Robin
						log_debug("select FirstNode - round-robin");
						if (node = getFirstNode(), node == NULL) {
							return -1;
						}
					} else {
						if (selectType == BEST_SLOT) {
								log_debug("select MostDownloadNode");
							if (node = getMostDownloadNode(), node == NULL) {
								log_debug("select FirstNode");
								if (node = getFirstNode(), node == NULL) {
									return -1;
								}
							}
						} else if (selectType == OPTIMISTIC_SLOT) {
								log_debug("select RandomNode");
							if (node = getRandomNode(), node == NULL) {
								log_debug("select FirstNode");
								if (node = getFirstNode(), node == NULL) {
									return -1;
								}
							}
						}
					}
				}
				break;
			case SEED_RANDOM:
				if (piecePool_->doesDownloadComplete()) {
					log_debug("This is Seed State - select always Random");
					if (used_quantum_ < far_quantum_)
					{
                                                  printf("SELECT FROM NEAR NEIGHBORS used quantum : %d / %d  (id = %d)\n",used_quantum_,far_quantum_,id_);
					         if (node = getRandomNode(), node == NULL) {
						  if (node = getFirstNode(), node == NULL) {
						    log_debug("select FirstNode");
						   if (node_far = getRandomFarNode(), node_far==NULL)
						    {
                                                      if (node_far = getFirstFarNode(), node_far==NULL)
                                                       {
							return -1;
                                                    printf("COULD NOT SELECT FROM NEAR (id = %d) \n",id_);
						      }
                                                      }
						}
					}
					used_quantum_++;
					}
					else
					{
                                        printf("SELECT FROM FAR NEIGHBORS (id = %d) \n", id_);
					if (node_far = getRandomFarNode(), node_far == NULL) {
						if (node_far = getFirstFarNode(), node_far == NULL)
                                                {
						log_debug("select FirstNode");
						if(node = getRandomNode(), node == NULL)
						{
                                                    if(node = getFirstNode(), node == NULL)
                                                    {
							return -1;
                                                 printf("Could not select from far neighbors (id = %d) \n",id_);
						  }
                                                }
						}
					}
					used_quantum_=0;
					}
					
				} else {
						log_debug("This is Leecher State - neighbor_num[%d] slot_num[%d]", neighborTable_->num(), choking_slot_num_);
					if (neighborTable_->num() + far_neighborTable_->num() <= choking_slot_num_) {
						// When all the neighbors can be serviced at time slots
						// --> Round-Robin
                                             if(used_quantum_ < far_quantum_)
                                             {
                                             if (node = getFirstNode(), node == NULL) {
                                               if (node_far = getFirstFarNode(),node_far == NULL)
                                               {
						return -1;
                                                }
					       }
                                              used_quantum_++;
                                              }
                                              else
                                              {
                                               if (node_far = getFirstFarNode(), node == NULL) {
                                                   if (node=getFirstNode(), node==NULL)
                                                   {
						return -1;
                                                    }
					       }
                                              used_quantum_=0;
                                              }
						log_debug("select FirstNode - round-robin");
						
					} else {
						if (selectType == BEST_SLOT) {
						node = getMostDownloadNode();
						node_far=getMostDownloadFarNode();
							if (node == NULL && node_far== NULL) 
							{
							node = getRandomNode();
								if (node == NULL)
								{         
								node_far =getRandomFarNode();
								if (node_far == NULL)
								{
								node = getFirstNode();
									if (node == NULL)
									{
									node_far= getFirstFarNode();
										if (node_far==NULL)
										{
										return -1;
										}
									}
								} 
								}
							}else if (node != NULL && node_far != NULL)
							{
							if (node->getBytes() > node_far->getBytes())
							{
							node_far=NULL;
							}
							else
							{
							node=NULL;
							}
							}
					
								
	
						} else if (selectType == OPTIMISTIC_SLOT) {
								log_debug("select RandomNode");
					if (used_quantum_ < far_quantum_)
					{
					if (node = getRandomNode(), node == NULL) {
						if (node = getFirstNode(), node == NULL) {
						log_debug("select FirstNode");
						if (node_far = getRandomFarNode(), node_far==NULL)
						{
                                                       if (node_far= getFirstFarNode(), node_far==NULL)
                                                       {
							return -1;
                                                       }
						}
						}
					}
					used_quantum_++;
					}
					else
					{
					if (node_far = getRandomFarNode(), node_far == NULL) {
					       if(node_far = getFirstFarNode(), node_far==NULL)
                                                {  	
						if(node = getRandomNode(), node == NULL)
						{
                                                   if(node = getFirstNode(), node == NULL)
                                                   {
							return -1;
                                                   }
						}
                                             }
						
					}
					used_quantum_=0;
					}
						}
					}
				}
				break;
			case ONE_HOP:
						log_debug("This is ONE_HOP - 1hop[%d] slot_num[%d]", neighborTable_->one_hop_num(), choking_slot_num_);
				if (neighborTable_->one_hop_num() > choking_slot_num_) {
					log_debug("Have to choose neighbors will be served");
					if (selectType == BEST_SLOT) {
							log_debug("This is BEST Slot - select MostDownloadNode");
						if (node = getMostDownloadNode(), node == NULL) {
		//					log_debug("select FirstNode");
	//						if (node = getFirstNode(), node == NULL) {
								return -1;
	//						}
						}
					} else if (selectType == OPTIMISTIC_SLOT) {
							log_debug("This is Optimistic Slot - select RandomNode");
						if (node = getRandomNode(), node == NULL) {
							log_debug("select FirstNode");
							if (node = getFirstNode(), node == NULL) {
								return -1;
							}
						}
					}
				} else {
					log_debug("all the neighbors can be served");
					// When all the neighbors can be served at time slots
					// --> Round-Robin
					log_debug("select FirstNode - round-robin");
					if (node = getFirstNode(), node == NULL) {
						return -1;
					}
				}
				break;
			case CLOSEST:
				if (selectType == BEST_SLOT) {
					log_debug("This is Best Slot - select ClosestNode");
					if (node = getClosestNode(), node == NULL) {
					log_debug("This is Best Slot - select FirstNode");
						if (node = getFirstNode(), node == NULL) {
							return -1;
						}
					}
				} else if (selectType == OPTIMISTIC_SLOT) {
					log_debug("This is Optimistic Slot - select RandomNode");
					if (node = getRandomNode(), node == NULL) {
						if (node = getFirstNode(), node == NULL) {
						log_debug("select FirstNode");
							return -1;
						}
					}
				}
				break;
		}
	
		served_slot_num_++;
	if (node != NULL && node_far == NULL)
	{
	node->setSelectFlag(true);
	node->setSendDataSlotIndex(global_choking_slot_index_);
		Random::seed_heuristically();
		return node->id();
	}
	
	if (node_far != NULL && node == NULL)
	{
	node_far->setSelectFlag(true);
	node_far->setSendDataSlotIndex(global_choking_slot_index_);
	Random::seed_heuristically();
	return node_far->id();
	}
        if (node_far != NULL && node != NULL)
        {
        printf("UNE ERREUR DE CHOIX DE VOISIN (id = %d)\n",id_);
        }
       return -1;
	}	
	
	void BitTorrent::tryUpload() {
	//	if (cur_upload_num_ >= max_upload_num_) return;
	
	//	nextPiece();
		if (piecePool_->hasCompletePiece() == false) {
			log_state("I don't have any completePiece - don't send PIECEOFFER_REQUEST");
			if (logLevel(STATE)) piecePool_->print();
			return;
		}
	
		if (logLevel(STATE)) {
			log_state("upload_num_[%d/%d] neighborTable[%d]", cur_upload_num_, max_upload_num_, neighborTable_->num());
			log_state("\tBefore print PiecePool");
			piecePool_->print();
			log_state("\tAfter print PiecePool");
		}
	
		if (max_upload_num_ == 1) {
			// The case for serial transmission
			int neighbor = selectNodeToUploadSerial();
			log_debug("selectNodeToUpload return [%d]", neighbor);
			if (neighbor >= 0) {
				if ( (neighborTable_->num() > 0) && (served_slot_num_ >= neighborTable_->num()) ) {
					// To prevent the case that the node cannot offer piece even though the time slots are left to serve, because of the selectFlag.
					// --> resetSelectFlag()
					neighborTable_->resetSelectFlag();
					served_slot_num_ = 0;
				}
	
				sendBitPacket(PIECEOFFER_REQUEST, neighbor,1);
			}
		} else if (max_upload_num_ > 1) {
			int loop_start = served_slot_num_ >= 0 ? served_slot_num_ : 0;
	
			if (logLevel(DEBUG)) {
				log_debug("\tStart to select node for Parallel transmission");
	//			neighborTable_->print();
				log_debug("\tneighbor print end");
			}
			// The case for parallel transmission
			for (int i = loop_start; i < max_upload_num_; i++) {
				int neighbor = -1;
	
				if ( (i >= 0) && (i < choking_best_slot_num_) ) {
					log_debug("select as BEST_SLOT");
                                         printf("Choix best slot (id = %d )\n",id_);
					neighbor = selectNodeToUploadParallel(BEST_SLOT);
				} else if ( (i >= choking_best_slot_num_) && (i < choking_best_slot_num_ + choking_optimistic_slot_num_) ) {
					log_debug("select as OPTIMISTIC_SLOT (id = %d) \n",id_);
                                         printf("Choix optimistic slot (id = %d)\n",id_);
					neighbor = selectNodeToUploadParallel(OPTIMISTIC_SLOT);
				} else {
					log_debug("select as BEST_SLOT else case");
					neighbor = selectNodeToUploadParallel(BEST_SLOT);
				}
	
				log_state("selectNodeToUpload return [%d]", neighbor);
				if (neighbor >= 0) {
					if (logLevel(DEBUG)) {
						BitNeighbor* debugNeighbor = neighborTable_->getNeighbor(neighbor);
						log_debug("\tneighbor print start");
						if (debugNeighbor != NULL) debugNeighbor->print();
						log_debug("\tneighbor print end");
					}
					sendBitPacket(PIECEOFFER_REQUEST, neighbor,1);
				}
			}
		}
	
		return;
	}
	
	int BitTorrent::sendData(int neighbor, int piece_index, int block_index) {
	
		
	
	//	if (cur_upload_num_ >= max_upload_num_) return -1;
	
		log_critical("start to send data to neighbor[%d] piece[%d] block[%d]", neighbor, piece_index, block_index);
		log_state("start to send data to neighbor[%d] piece[%d] block[%d]", neighbor, piece_index, block_index);
	
		BitNeighbor *node = neighborTable_->getNeighbor(neighbor);
		if (node !=NULL)
                {
	
                printf("SENDIND DATA TO NEAR (id = %d) \n",id_);
		BitTorrentPayload *payload = new BitTorrentPayload(piece_index, block_index, piecePool_->block_size());
	
		payload->sender_id_ = id_;
		payload->receiver_id_ = neighbor;
	        
                 printf("Before connect\n");
		node->connect(this);
                 printf("After connect\n");
		// Unit of size is bytes
		//node->sendData(piece_index, block_index, piecePool_->block_size());
	        printf("before sending\n");
		node->sendData(payload);
                printf("after sending\n");
		
		sendBytes_ += ((unsigned long)piecePool_->block_size());
		//addBytesHCB(UPLOAD, node->hopCount(), ((unsigned long)piecePool_->block_size()));
	
		if ( (selectNodeToUpload_flag_ == ONE_HOP) && (node->hopCount() != 1) ) {
			log_fail("try to send data to node[%d] hopCount[%d]", node->id(), node->hopCount());
			print();
			abort();
		}}
                else
                {
                printf("SENDING DATA TO FAR (id = %d)\n",id_);
                FarBitNeighbor *node1 = far_neighborTable_->getFarNeighbor(neighbor);
                BitTorrentPayload *payload = new BitTorrentPayload(piece_index, block_index, piecePool_->block_size());
	
		payload->sender_id_ = id_;
		payload->receiver_id_ = neighbor;
	
		node1->connect(this);
		// Unit of size is bytes
		//node->sendData(piece_index, block_index, piecePool_->block_size());
	
		node1->sendData(payload);
		
		sendBytes_ += ((unsigned long)piecePool_->block_size());
		//addBytesHCB(UPLOAD, node->hopCount(), ((unsigned long)piecePool_->block_size()));
	
		if ( (selectNodeToUpload_flag_ == ONE_HOP) && (node->hopCount() != 1) ) {
			log_fail("try to send data to node[%d] hopCount[%d]", node1->id(), node1->hopCount());
			print();
			abort();
		}
                }
		return 1;
	}
	
	void BitTorrent::print() {
		printf("\n############## BitTorrent Print Start ###############\n");
		printf("BitTorrentNode[%d] routingAgent[%s] bitAgent[%s] max_peer_num[%d] max_neighbor_num[%d]\n", id_, routingAgent_->name(), bitAgent_->name(), max_peer_num_, max_neighbor_num_);
		printf("Download_Connections[%d/%d] Upload_Connection[%d/%d] sendBytes[%ld]\n", cur_download_num_, max_download_num_, cur_upload_num_, max_upload_num_, sendBytes_);
		printf("lastUp[%.5f] startTime[%.5f] finishTime[%.5f] steadyStateTime[%f] peerUpdateInterval[%f:%f] neighborSelectInterval[%f:%f] tryUploadInterval[%f] receivedBytesResetInterval[%f]\n",
				lastup_, startTime_, finishTime_, steadyStateTime_, peerUpdateInterval1_, peerUpdateInterval2_, neighborSelectInterval1_, neighborSelectInterval2_, tryUploadInterval_, receivedBytesResetInterval_);
		printf("choking_slot_index[%d] global_choking_slot_index[%d] slot_num[%d] best_slot_num[%d] optimistic_slot_num[%d] isBestSlotFlag[%d]\n",
				choking_slot_index_, global_choking_slot_index_, choking_slot_num_, choking_best_slot_num_, choking_optimistic_slot_num_, isBestSlot());
		printf("control_tcp_flag[%d] use_bidirection_tcp_flag[%d]\n",
				control_tcp_flag_, use_bidirection_tcp_flag_);
		{
			char temp[piecePool_->piece_num()+20];
	
			memset(temp, 0x00, sizeof(temp));
			sprintf(temp, "local_rarest_count[");
			for(int i = 0; i < piecePool_->piece_num(); i++) {
				char temp2[10];
				memset(temp2, 0x00, sizeof(temp2));
				if (local_rarest_count_[i] != -1)
					sprintf(temp2, "%d", local_rarest_count_[i]);
				else
					sprintf(temp2, "X");
				strcat(temp, temp2);
			}
			strcat(temp, "]");
			printf("local_rarest_select_flag[%d] local_rarest_piece_num[%d] %s\n", local_rarest_select_flag_, local_rarest_piece_num_, temp);
		}
		piecePool_->print();
		peerTable_->print();
		neighborTable_->print();
		//printHCB(1,9999999);
		printf("############## BitTorrent Print End #################\n\n");
	
		return;
	}
	
	void BitTorrent::addBytesHCB(int flag, int hopCount, unsigned long bytes) {
		struct HCBEntry *entry = NULL;
		
		Tcl_HashEntry *he = Tcl_FindHashEntry(hopCountBytesTable_, (const char *)hopCount);
		if (he == NULL) {
			struct HCBEntry *entry = new HCBEntry;
			int newEntry = 1;
	
			entry->hopCount_ = hopCount;
			entry->upBytes_ = 0;
			entry->dwBytes_ = 0;
			entry->numOfRequest_ = 0;
			entry->numOfAccept_ = 0;
			entry->numOfSend_ = 0;
	
			if (flag == UPLOAD) entry->upBytes_ = bytes;
			else entry->dwBytes_ = bytes;
	
			he = Tcl_CreateHashEntry(hopCountBytesTable_,
								(const char *) hopCount,
								&newEntry);
			if (he == NULL) {
				log_fail("Tcl_CreateHashEntry fail for addBytesHCB - hopCount[%d]", hopCount);
				return;
			}
			Tcl_SetHashValue(he, (ClientData)entry);
		} else {
			entry = (struct HCBEntry *)Tcl_GetHashValue(he);
			if (flag == UPLOAD) entry->upBytes_ += bytes;
			else entry->dwBytes_ += bytes;
		}
	
		return;
	}
	
	void BitTorrent::addCountHCB(int pktType, int offer_flag, int hopCount) {
		struct HCBEntry *entry = NULL;
		
		Tcl_HashEntry *he = Tcl_FindHashEntry(hopCountBytesTable_, (const char *)hopCount);
		if (he == NULL) {
			struct HCBEntry *entry = new HCBEntry;
			int newEntry = 1;
	
			entry->hopCount_ = hopCount;
			entry->upBytes_ = 0;
			entry->dwBytes_ = 0;
			entry->numOfRequest_ = 0;
			entry->numOfAccept_ = 0;
			entry->numOfSend_ = 0;
	
			if (pktType == PIECEOFFER_REQUEST) entry->numOfRequest_ = 1;
			else {
				if (offer_flag == ACCEPT) {
					entry->numOfAccept_ = 1;
					entry->numOfSend_ = 1;
				} else if (offer_flag == CONTINUE) {
					entry->numOfSend_ = 1;
					log_fail("Something wrong in addCountHCB - HCBEntry is not exist even offer_flag == CONTINUE");
				}
			}
	
			he = Tcl_CreateHashEntry(hopCountBytesTable_,
								(const char *) hopCount,
								&newEntry);
			if (he == NULL) {
				log_fail("Tcl_CreateHashEntry fail for addBytesHCB - hopCount[%d]", hopCount);
				return;
			}
			Tcl_SetHashValue(he, (ClientData)entry);
		} else {
			entry = (struct HCBEntry *)Tcl_GetHashValue(he);
			if (pktType == PIECEOFFER_REQUEST) entry->numOfRequest_++;
			else {
				if (offer_flag == ACCEPT) {
					entry->numOfAccept_++;
					entry->numOfSend_++;
				} else if (offer_flag == CONTINUE) {
					entry->numOfSend_++;
				}
			}
		}
	
		return;
	}
	void BitTorrent::addTimeHCT(int hopCount) {
		struct HCTEntry *entry = NULL;
		
		Tcl_HashEntry *he = Tcl_FindHashEntry(hopCountTimeTable_, (const char *)hopCount);
		if (he == NULL) {
			struct HCTEntry *entry = new HCTEntry;
			int newEntry = 1;
	
			entry->hopCount_ = hopCount;
			entry->time_= tryUploadInterval_;
			he = Tcl_CreateHashEntry(hopCountTimeTable_,
								(const char *) hopCount,
								&newEntry);
			
			Tcl_SetHashValue(he, (ClientData)entry);
		} else {
			entry = (struct HCTEntry *)Tcl_GetHashValue(he);
			entry->time_=entry->time_ + tryUploadInterval_;
		}
	
		return;
	}
	
	void BitTorrent::printHCB(int fd, double now) {
		FILE *fp = NULL;
		char buff[1024];
		int minute;
		int i;
	for (i=0;i<200;i=i+2)
	{
	if (now > (double) (startTime_ + (i * 60)) )
	{
		minute=i;
	}
		}
	
		if (fd == -1 ) {
			sprintf(buff, "HopCount_Bytes_I%d_%d_%d", id_,minute,seqNo_);
			if (fp = fopen(buff, "w"), fp == NULL) {
				log_fail("fopen fail - file_name[%s]", buff);
				return;
			} else {
				fprintf(fp, "### ID[%d] max_upload_num[%d] tcp_flag[%d] bidirection_flag[%d] Minute [%d] Simulation[%d]\n", id_, max_upload_num_, control_tcp_flag_, use_bidirection_tcp_flag_,minute,seqNo_);
				fprintf(fp, "### HopCount\tUpload Bytes\n");
			}
		} else {
			printf("############## HopCount_Bytes Print Start ###############\n");
		}
	
		Tcl_HashEntry *he;
		Tcl_HashSearch hs;
	
		for(he = Tcl_FirstHashEntry(hopCountBytesTable_, &hs);
				he != NULL;
				he = Tcl_NextHashEntry(&hs)) {
			struct HCBEntry *entry = (struct HCBEntry *)Tcl_GetHashValue(he);
			if (fd == -1) {				// print to File
				fprintf(fp, "%d\t%ld\t%d\n",
						entry->hopCount_, entry->upBytes_,minute);
			} else {					// print to Screen
				printf("[H%d:U%ld:D%ld:R%d:A:%d:S:%d] ",
						entry->hopCount_, entry->upBytes_, entry->dwBytes_,
						entry->numOfRequest_, entry->numOfAccept_, entry->numOfSend_);
			}
		}
	
		if (fd == -1 ) {
			fclose(fp);
		} else {
			printf("\n############## HopCount_Bytes Print End ###############\n");
		}
	
		return;
	}
	
	void BitTorrent::printHCT(int fd, double now) {
		FILE *fp = NULL;
		char buff[1024];
		int minute;
		int i;
	for (i=0;i<200;i=i+2)
	{
	if (now > (double) (startTime_ + (i * 60)) )
	{
		minute=i;
	}
		}
	
		if (fd == -1 ) {
			sprintf(buff, "HopCount_Time_I%d_%d_%d", id_,minute,seqNo_);
			if (fp = fopen(buff, "w"), fp == NULL) {
				log_fail("fopen fail - file_name[%s]", buff);
				return;
			} else {
				fprintf(fp, "### ID[%d] max_upload_num[%d] tcp_flag[%d] bidirection_flag[%d] Minute [%d] Simulation[%d]\n", id_, max_upload_num_, control_tcp_flag_, use_bidirection_tcp_flag_,minute,seqNo_);
				fprintf(fp, "### HopCount\tTime\n");
			}
		} else {
			printf("############## HopCount_Time Print Start ###############\n");
		}
	
		Tcl_HashEntry *he;
		Tcl_HashSearch hs;
	
		for(he = Tcl_FirstHashEntry(hopCountTimeTable_, &hs);
				he != NULL;
				he = Tcl_NextHashEntry(&hs)) {
			struct HCTEntry *entry = (struct HCTEntry *)Tcl_GetHashValue(he);
			if (fd == -1) {				// print to File
				fprintf(fp, "%d\t%f\t%d\n",
						entry->hopCount_, entry->time_,minute);
			} else {					// print to Screen
				printf("[H%d] ",
						entry->hopCount_);
			}
		}
	
		if (fd == -1 ) {
			fclose(fp);
		} else {
			printf("\n############## HopCount_Time print End ###############\n");
		}
	
		return;
	}
	void BitTorrent::printCPL(double now) {
		if (printCPL_flag_ == 0) return;
	
		if (now > startTime_ + 50 * 2 * 60) {
			GlobalBitTorrentList::instance().printCPL(14, now, seqNo_);
		} else if (now > startTime_ + 40 * 2 * 60) {
			GlobalBitTorrentList::instance().printCPL(13, now, seqNo_);
		} else if (now > startTime_ + 30 * 2 * 60) {
			GlobalBitTorrentList::instance().printCPL(12, now, seqNo_);
		} else if (now > startTime_ + 20 * 2 * 60) {
			GlobalBitTorrentList::instance().printCPL(11, now, seqNo_);
		} else if (now > startTime_ + 10  * 2 * 60) {
			GlobalBitTorrentList::instance().printCPL(10, now, seqNo_);
		} else if (now > startTime_ + 8  * 2 * 60) {
			GlobalBitTorrentList::instance().printCPL(8, now, seqNo_);
		} else if (now > startTime_ + 6  * 2 * 60) {
			GlobalBitTorrentList::instance().printCPL(6, now, seqNo_);
		} else if (now > startTime_ + 4  * 2 * 60) {
			GlobalBitTorrentList::instance().printCPL(4, now, seqNo_);
		} else if (now > startTime_ + 2  * 2 * 60) {
			GlobalBitTorrentList::instance().printCPL(2, now, seqNo_);
		} else if (now > startTime_ + 1  * 2 * 60) {
			GlobalBitTorrentList::instance().printCPL(1, now, seqNo_);
		}
	}
	
	bool BitTorrent::checkEqualPieceList2(int *pieceList1, int node) {
		BitTorrent *peer = GlobalBitTorrentList::instance().getNode(node);
	
		if (!checkEqualPieceList(pieceList1, peer->piecePool()->pieces_state())) {
			log_debug("######### BitTorrent Node[%d] piecePool print start", node);
			peer->piecePool()->print();
			log_debug("######### BitTorrent Node[%d] piecePool print end", node);
			return false;
		}
		return true;
	}
	
	bool BitTorrent::checkEqualPieceList(int *pieceList1, int *pieceList2) {
		for (int i = 0; i < piecePool_->piece_num(); i++) {
			int int1 = pieceList1[i];
			int int2 = pieceList2[i];
			if (int1 != int2) {
				log_debug("pieceList1[%d:%d] different with pieceList2[%d:%d] with i[%d]", int1, pieceList1[i], int2, pieceList2[2], i);
				return false;
			}
		}
	
		return true;
	}
	
	int BitTorrent::writeDownloadTime() {
		static int firstFlag = 0;
		FILE *fp = NULL;
		char fileName[1024];
	
		sprintf(fileName, "progress-tcp%d-ttl%d-max%d.time", control_tcp_flag_, bitAgent_->flooding_ttl(), max_peer_num_);
	/*	if (fp = fopen(fileName, "r"), fp == NULL) {
			firstFlag = true;
		} else {
			firstFlag = false;
			fclose(fp);
		}
	*/
		if (firstFlag == 0) {
			if (fp = fopen(fileName, "w"), fp == NULL) {
				printf("fopen error - fileName[%s]\n", fileName);
				return -1;
			}
	
			fprintf(fp, "### TcpFlag[%d] BiDirection[%d] MaxPeer[%d] MaxNeighbor[%d] TTL[%d] MaxUploadNum[%d-%d/%d]\n",
				control_tcp_flag_, use_bidirection_tcp_flag_,
				max_peer_num_, max_neighbor_num_, bitAgent_->flooding_ttl(),
				max_upload_num_, choking_best_slot_num_, choking_optimistic_slot_num_);
			fprintf(fp, "### Id\tStartTime\tFinishTime\tSendBytes\n");
		} else {
			if (fp = fopen(fileName, "a"), fp == NULL) {
				printf("fopen error - fileName[%s]\n", fileName);
				return -1;
			}
	
			char temp[10240];
			while(fgets(temp, sizeof(temp), fp) != NULL) {
			}
		}
	
		fprintf(fp, "%d\t%.5f\t%.5f\t%ld\n",
			id_, startTime_, finishTime_, sendBytes_);
		fclose(fp);
		firstFlag++;
		return 1;
	}
	// end of BitTorrent.cc file

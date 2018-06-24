/*
 *   FILE: toplevel.c
 * AUTHOR: Hong Zhang (romanzhg@stanford.edu)
 *   DATE: Apr 23, 23:59:59 PST 2013
 *  DESCR:
 */

/* #define DEBUG */

#include "main.h"
#include <string>
#include "mazewar.h"
#include <stdlib.h>
#include <time.h>


static bool		updateView;	/* true if update needed */
MazewarInstance::Ptr M;

/* Use this socket address to send packets to the multi-cast group. */
static Sockaddr         groupAddr;
#define MAX_OTHER_RATS  (MAX_RATS - 1)

/* data structures*/
#define TAG_LENGTH 10
static int SEQNUM = 1;
static int TAGKEY = 1;
Missile missiles[2];
TagInfo tagInfos[TAG_LENGTH];
TagInfo tagHistory[TAG_LENGTH];
int tagHistoryIterator=0;
RatInfo otherRats[MAX_OTHER_RATS];
short GUID;
time_t stateCooldown;
time_t shootCooldown;
time_t taggedCooldown;
char *ratName;

int main(int argc, char *argv[])
{
    Loc x(1);
    Loc y(5);
    Direction dir(0);

    signal(SIGHUP, quit);
    signal(SIGINT, quit);
    signal(SIGTERM, quit);

	if(argc<5){
		getName("Welcome to CS244B MazeWar!\n\nYour Name", &ratName);
		ratName[strlen(ratName)-1] = 0;
	}
	if(argc == 5){
		ratName = strdup(argv[1]);
		x = Loc(atoi(argv[2]));
		y = Loc(atoi(argv[3]));
		if (argv[4][0] == 'n')
			dir = Direction(NORTH);
		if (argv[4][0] == 's')
			dir = Direction(SOUTH);
		if (argv[4][0] == 'e')
			dir = Direction(EAST);
		if (argv[4][0] == 'w')
			dir = Direction(WEST);
	}

    M = MazewarInstance::mazewarInstanceNew(string(ratName));
    MazewarInstance* a = M.ptr();
    strncpy(M->myName_, ratName, NAMESIZE);
    //free(ratName);

    MazeInit(argc, argv);

	if(argc<5){
	    NewPosition(M);
	}
	else {
		if (M->maze_[x.value()][y.value()])
			NewPosition(M);
		else{
			M->xlocIs(x);
			M->ylocIs(y);
			M->dirIs(dir);
		}

	}

    /* So you can see what a Rat is supposed to look like, we create
    one rat in the single player mode Mazewar.
    It doesn't move, you can't shoot it, you can just walk around it */

    for(int i=0; i<2; i++){
	missiles[i].x = new Loc(0);
	missiles[i].y = new Loc(0);
	missiles[i].dir = new Direction(0);
	missiles[i].valid = false;
    }
    memset(otherRats, 0, sizeof(otherRats));
    memset(tagInfos, 0, sizeof(tagInfos));
    memset(tagHistory, 0, sizeof(tagHistory));
    srand (time(NULL));
    GUID = (short int)rand();
    time(&stateCooldown);
    time(&shootCooldown);
    time(&taggedCooldown);

    play();

    return 0;
}


/* ----------------------------------------------------------------------- */

void
play(void)
{
	MWEvent		event;
	MW244BPacket	incoming;

	event.eventDetail = &incoming;

	while (TRUE) {
		NextEvent(&event, M->theSocket());
		if (!M->peeking())
			switch(event.eventType) {
			case EVENT_A:
				aboutFace();
				break;

			case EVENT_S:
				leftTurn();
				break;

			case EVENT_D:
				forward();
				break;

			case EVENT_F:
				rightTurn();
				break;

			case EVENT_BAR:
				backward();
				break;

			case EVENT_LEFT_D:
				peekLeft();
				break;

			case EVENT_MIDDLE_D:
				shoot();
				break;

			case EVENT_RIGHT_D:
				peekRight();
				break;

			case EVENT_NETWORK:
				processPacket(&event);
				break;

			case EVENT_INT:
				quit(0);
				break;

			}
		else
			switch (event.eventType) {
			case EVENT_RIGHT_U:
			case EVENT_LEFT_U:
				peekStop();
				break;

			case EVENT_NETWORK:
				processPacket(&event);
				break;
			}

		ratStates();		/* clean house */

		manageMissiles();

		DoViewUpdate();

		/* Any info to send over network? */

	}
}

/* ----------------------------------------------------------------------- */

static	Direction	_aboutFace[NDIRECTION] ={SOUTH, NORTH, WEST, EAST};
static	Direction	_leftTurn[NDIRECTION] =	{WEST, EAST, NORTH, SOUTH};
static	Direction	_rightTurn[NDIRECTION] ={EAST, WEST, SOUTH, NORTH};

void
aboutFace(void)
{
	M->dirIs(_aboutFace[MY_DIR]);
	updateView = TRUE;
}

/* ----------------------------------------------------------------------- */

void
leftTurn(void)
{
	M->dirIs(_leftTurn[MY_DIR]);
	updateView = TRUE;
}

/* ----------------------------------------------------------------------- */

void
rightTurn(void)
{
	M->dirIs(_rightTurn[MY_DIR]);
	updateView = TRUE;
}

/* ----------------------------------------------------------------------- */

/* remember ... "North" is to the right ... positive X motion */

void
forward(void)
{
	register int	tx = MY_X_LOC;
	register int	ty = MY_Y_LOC;

	switch(MY_DIR) {
	/*
	case NORTH:	if (!M->maze_[tx+1][ty])	tx++; break;
	case SOUTH:	if (!M->maze_[tx-1][ty])	tx--; break;
	case EAST:	if (!M->maze_[tx][ty+1])	ty++; break;
	case WEST:	if (!M->maze_[tx][ty-1])	ty--; break;
	default:
		MWError("bad direction in Forward");
	*/
	case NORTH:	if (validMove(tx+1,ty))	tx++; break;
	case SOUTH:	if (validMove(tx-1,ty))	tx--; break;
	case EAST:	if (validMove(tx,ty+1))	ty++; break;
	case WEST:	if (validMove(tx,ty-1))	ty--; break;
	default:
		MWError("bad direction in Forward");
	}
	if ((MY_X_LOC != tx) || (MY_Y_LOC != ty)) {
		M->xlocIs(Loc(tx));
		M->ylocIs(Loc(ty));
		updateView = TRUE;
	}
}

/* ----------------------------------------------------------------------- */

void backward()
{
	register int	tx = MY_X_LOC;
	register int	ty = MY_Y_LOC;

	switch(MY_DIR) {
	/*
	case NORTH:	if (!M->maze_[tx-1][ty])	tx--; break;
	case SOUTH:	if (!M->maze_[tx+1][ty])	tx++; break;
	case EAST:	if (!M->maze_[tx][ty-1])	ty--; break;
	case WEST:	if (!M->maze_[tx][ty+1])	ty++; break;
	*/
	case NORTH:	if (validMove(tx-1,ty))	tx--; break;
	case SOUTH:	if (validMove(tx+1,ty))	tx++; break;
	case EAST:	if (validMove(tx,ty-1))	ty--; break;
	case WEST:	if (validMove(tx,ty+1))	ty++; break;
	default:
		MWError("bad direction in Backward");
	}
	if ((MY_X_LOC != tx) || (MY_Y_LOC != ty)) {
		M->xlocIs(Loc(tx));
		M->ylocIs(Loc(ty));
		updateView = TRUE;
	}
}

/* ----------------------------------------------------------------------- */

void peekLeft()
{
	M->xPeekIs(MY_X_LOC);
	M->yPeekIs(MY_Y_LOC);
	M->dirPeekIs(MY_DIR);

	switch(MY_DIR) {
	case NORTH:	if (!M->maze_[MY_X_LOC+1][MY_Y_LOC]) {
				M->xPeekIs(MY_X_LOC + 1);
				M->dirPeekIs(WEST);
			}
			break;

	case SOUTH:	if (!M->maze_[MY_X_LOC-1][MY_Y_LOC]) {
				M->xPeekIs(MY_X_LOC - 1);
				M->dirPeekIs(EAST);
			}
			break;

	case EAST:	if (!M->maze_[MY_X_LOC][MY_Y_LOC+1]) {
				M->yPeekIs(MY_Y_LOC + 1);
				M->dirPeekIs(NORTH);
			}
			break;

	case WEST:	if (!M->maze_[MY_X_LOC][MY_Y_LOC-1]) {
				M->yPeekIs(MY_Y_LOC - 1);
				M->dirPeekIs(SOUTH);
			}
			break;

	default:
			MWError("bad direction in PeekLeft");
	}

	/* if any change, display the new view without moving! */

	if ((M->xPeek() != MY_X_LOC) || (M->yPeek() != MY_Y_LOC)) {
		M->peekingIs(TRUE);
		updateView = TRUE;
	}
}

/* ----------------------------------------------------------------------- */

void peekRight()
{
	M->xPeekIs(MY_X_LOC);
	M->yPeekIs(MY_Y_LOC);
	M->dirPeekIs(MY_DIR);

	switch(MY_DIR) {
	case NORTH:	if (!M->maze_[MY_X_LOC+1][MY_Y_LOC]) {
				M->xPeekIs(MY_X_LOC + 1);
				M->dirPeekIs(EAST);
			}
			break;

	case SOUTH:	if (!M->maze_[MY_X_LOC-1][MY_Y_LOC]) {
				M->xPeekIs(MY_X_LOC - 1);
				M->dirPeekIs(WEST);
			}
			break;

	case EAST:	if (!M->maze_[MY_X_LOC][MY_Y_LOC+1]) {
				M->yPeekIs(MY_Y_LOC + 1);
				M->dirPeekIs(SOUTH);
			}
			break;

	case WEST:	if (!M->maze_[MY_X_LOC][MY_Y_LOC-1]) {
				M->yPeekIs(MY_Y_LOC - 1);
				M->dirPeekIs(NORTH);
			}
			break;

	default:
			MWError("bad direction in PeekRight");
	}

	/* if any change, display the new view without moving! */

	if ((M->xPeek() != MY_X_LOC) || (M->yPeek() != MY_Y_LOC)) {
		M->peekingIs(TRUE);
		updateView = TRUE;
	}
}

/* ----------------------------------------------------------------------- */

void peekStop()
{
	M->peekingIs(FALSE);
	updateView = TRUE;
}

/* ----------------------------------------------------------------------- */

void shoot()
{
	if(difftime(time(NULL), shootCooldown) < 2)
		return;
	time(&shootCooldown);
	
	M->scoreIs( M->score().value()-1 );
	UpdateScoreCard(M->myRatId().value());
	for(int i=0; i< 2; i++){
		if(missiles[i].valid == true)
			continue;
		*missiles[i].x = Loc(MY_X_LOC);
                *missiles[i].y = Loc(MY_Y_LOC);
                *missiles[i].dir = Direction(MY_DIR);
		missiles[i].valid = true;
		return;
	}
}

/* ----------------------------------------------------------------------- */

/*
 * Exit from game, clean up window
 */

void quit(int sig)
{
	sendLeave();
	StopWindow();
	exit(0);
}


/* ----------------------------------------------------------------------- */

void NewPosition(MazewarInstance::Ptr m)
{
	Loc newX(0);
	Loc newY(0);
	Direction dir(0); /* start on occupied square */

	while (M->maze_[newX.value()][newY.value()]) {
	  /* MAZE[XY]MAX is a power of 2 */
	  newX = Loc(random() & (MAZEXMAX - 1));
	  newY = Loc(random() & (MAZEYMAX - 1));

	  /* In real game, also check that square is
	     unoccupied by another rat */
	}

	/* prevent a blank wall at first glimpse */

	if (!m->maze_[(newX.value())+1][(newY.value())]) dir = Direction(NORTH);
	if (!m->maze_[(newX.value())-1][(newY.value())]) dir = Direction(SOUTH);
	if (!m->maze_[(newX.value())][(newY.value())+1]) dir = Direction(EAST);
	if (!m->maze_[(newX.value())][(newY.value())-1]) dir = Direction(WEST);

	m->xlocIs(newX);
	m->ylocIs(newY);
	m->dirIs(dir);
}

void DoViewUpdate()
{
	if (updateView) {	/* paint the screen */
		ShowPosition(MY_X_LOC, MY_Y_LOC, MY_DIR);
		if (M->peeking())
			ShowView(M->xPeek(), M->yPeek(), M->dirPeek());
		else
			ShowView(MY_X_LOC, MY_Y_LOC, MY_DIR);
		updateView = FALSE;
	}
}

/* ----------------------------------------------------------------------- */

void MWError(char *s)

{
	StopWindow();
	fprintf(stderr, "CS244BMazeWar: %s\n", s);
	perror("CS244BMazeWar");
	exit(-1);
}

/* ----------------------------------------------------------------------- */

/* This is just for the sample version, rewrite your own */
Score GetRatScore(RatIndexType ratId)
{
	if (ratId.value() == M->myRatId().value())
		{ return(M->score()); }
	else
		return otherRats[ratId.value()-1].score;
}

/* ----------------------------------------------------------------------- */

/* This is just for the sample version, rewrite your own */
char *GetRatName(RatIndexType ratId)
{
	if (ratId.value() == M->myRatId().value())
		return(M->myName_);
	else
		return otherRats[ratId.value()-1].username;
}

/* ----------------------------------------------------------------------- */

/* This is just for the sample version, rewrite your own if necessary */
void ConvertIncoming(MW244BPacket *p)
{
	p->header.guid = ntohs(p->header.guid);
	p->header.pkid = ntohs(p->header.pkid); 
	p->header.seq = ntohl(p->header.seq);

	PktState * pktState;
	PktUsernameReq * pktUsernameReq;
	PktTagged * pktTagged;
	PktTaggedAck * pktTaggedAck;
        switch(p->header.type) {
                case PACKET_STATE:
                        pktState = (PktState *)p->data;
			pktState->score = ntohl(pktState->score);
			pktState->tagKey = ntohl(pktState->tagKey);
                        break;
                case PACKET_USERNAME_REQ:
			pktUsernameReq = (PktUsernameReq *)p->data;
			pktUsernameReq->guid = ntohs(pktUsernameReq->guid);
                        break;
                case PACKET_USERNAME_ACK:
                        break;
                case PACKET_TAGGED:
			pktTagged = (PktTagged *) p->data;
			pktTagged->guid = ntohs(pktTagged->guid);
			pktTagged->tagKey = ntohl(pktTagged->tagKey);
                        break;
                case PACKET_TAGGED_ACK:
			pktTaggedAck = (PktTaggedAck *) p->data;
			pktTaggedAck->guid = ntohs(pktTaggedAck->guid);
			pktTaggedAck->tagKey = ntohl(pktTaggedAck->tagKey);
                        break;
                case PACKET_LEAVE:
                        break;
                default:
                        break;
        }

}

/* ----------------------------------------------------------------------- */

/* This is just for the sample version, rewrite your own if necessary */
void ConvertOutgoing(MW244BPacket *p)
{
	// fill in the header, and then convert for network byte order
	p->header.pkid= htons(0x4d57); 
	p->header.version = 1;
	p->header.guid = htons(GUID);
	p->header.seq = htonl(SEQNUM);
	SEQNUM++;

	PktState * pktState;
	PktUsernameReq * pktUsernameReq;
	PktTagged * pktTagged;
	PktTaggedAck * pktTaggedAck;
	PktUsernameAck * pktUsernameAck;
        switch(p->header.type) {
                case PACKET_STATE:
                        pktState = (PktState *)p->data;
			pktState->score = htonl(pktState->score);
			pktState->tagKey = htonl(pktState->tagKey);
                        break;
                case PACKET_USERNAME_REQ:
			pktUsernameReq = (PktUsernameReq *)p->data;
			pktUsernameReq->guid = htons(pktUsernameReq->guid);
                        break;
                case PACKET_USERNAME_ACK:
			//for(int i=0;i<6;i++){
			//	pktUsernameAck->username+i*4 = htonl(pktUsernameAck->username+i*4);
			//}
                        break;
                case PACKET_TAGGED:
			pktTagged = (PktTagged *) p->data;
			pktTagged->guid = htons(pktTagged->guid);
			pktTagged->tagKey = htonl(pktTagged->tagKey);
                        break;
                case PACKET_TAGGED_ACK:
			pktTaggedAck = (PktTaggedAck *) p->data;
			pktTaggedAck->guid = htons(pktTaggedAck->guid);
			pktTaggedAck->tagKey = htonl(pktTaggedAck->tagKey);
                        break;
                case PACKET_LEAVE:
                        break;
                default:
                        break;
        }
}

/* ----------------------------------------------------------------------- */

/* This is just for the sample version, rewrite your own */
void ratStates()
{
	UpdateScoreCard(RatIndexType(0));
	// send out my state
	sendState();

	// update state from the others
	RatInfo * info;
        for(int i=0; i<MAX_OTHER_RATS ; i++){
		info = &otherRats[i];
		if(!info->valid)
			continue;
		SetRatPosition(RatIndexType(i+1),Loc(info->x), Loc(info->y),Direction(info->dir));
		// if location collide, move to a new adjacent location
		if((Loc(info->x) == MY_X_LOC) && (Loc(info->y) == MY_Y_LOC))
			moveToAdjacent();
		// get username
		if(strlen(info->username) == 0){
			sendUserNameReq(info->guid, false);
		}
		// check the last packet time, is larger than 10 seconds then consider user as left
		if(difftime(time(NULL), info->pktTime) > 10){
			info->valid = false;
			memset(info->username, 0, 24);
			ClearRatPosition(RatIndexType(i+1));
			UpdateScoreCard(RatIndexType(i+1));
			disableTagInfo(info->guid);
			continue;
		}
			
	}

	// send tag information to others
	TagInfo * tagInfo;
        for(int j=0; j<TAG_LENGTH; j++){
		tagInfo = &tagInfos[j];
		if(!tagInfo->valid)
			continue;
		sendTagged(tagInfo->guid, tagInfo->tagKey);
	}
	updateView = TRUE;
}

void manageMissiles()
{
	RatInfo * info;
	TagInfo * tagInfo;
	int tx, ty;
	for (int i=0; i<2; i++){
		if(!missiles[i].valid)
			continue;

		tx = missiles[i].x->value();
		ty = missiles[i].y->value();
		
		// advance the missile
		switch(missiles[i].dir->value()){
			case NORTH:     *missiles[i].x = Loc(tx+1);	break;
			case SOUTH:     *missiles[i].x = Loc(tx-1);	break;
			case EAST:      *missiles[i].y = Loc(ty+1);	break;
			case WEST:      *missiles[i].y = Loc(ty-1);	break;
		}
		// decide is there any collision (with wall for now)
		for(int j=0; j<MAX_OTHER_RATS ; j++){
			info = &otherRats[j];
			if((info->valid) && ((*missiles[i].x) == Loc(info->x)) && ((*missiles[i].y) == Loc(info->y))){
				missiles[i].valid = false;
				clearSquare(Loc(tx), Loc(ty));
				for(int k=0; k<TAG_LENGTH; k++){
					tagInfo = &tagInfos[k];
					if(tagInfo->valid)
						continue;
					tagInfo->valid = true;
					tagInfo->guid = info->guid;
					tagInfo->tagKey = info->tagKey;
					goto outLoop;
				}
				break;
			}
		}
			
		if (M->maze_[missiles[i].x->value()][missiles[i].y->value()]){
			missiles[i].valid = false;
			clearSquare(Loc(tx), Loc(ty));
		}
		outLoop:
		if(missiles[i].valid == false)
			continue;
		// showMissile
		if ((tx == MY_X_LOC) && (ty == MY_Y_LOC))
			showMissile(*missiles[i].x, *missiles[i].y, missiles[i].dir->value(), Loc(tx), Loc(ty), false);
		else
			showMissile(*missiles[i].x, *missiles[i].y, missiles[i].dir->value(), Loc(tx), Loc(ty), true);
		
	}
}

void sendState(){
	if(difftime(time(NULL), stateCooldown)<0.5)
		return;
	time(&stateCooldown);

	MW244BPacket pack;
	PktState * pkt;

	pack.header.type = PACKET_STATE;

	pkt = (PktState *)pack.data;
	pkt->x = MY_X_LOC;
	pkt->y = MY_Y_LOC;
	pkt->dir = MY_DIR;
	pkt->score = M->score().value();
	pkt->tagKey = TAGKEY;

	ConvertOutgoing(&pack);

	if (sendto((int)M->theSocket(), &pack, sizeof(pack), 0, (struct sockaddr *) &groupAddr, sizeof(Sockaddr)) < 0)
          { MWError("Send error"); }
}

void sendUserNameAck(){
	MW244BPacket pack;
	PktUsernameAck * pkt;

	pack.header.type = PACKET_USERNAME_ACK;
	pkt = (PktUsernameAck *)pack.data;
	memset(pkt->username, 0, 24);
	strncpy(pkt->username, ratName, strlen(ratName));
	
	ConvertOutgoing(&pack);
	if (sendto((int)M->theSocket(), &pack, sizeof(pack), 0, (struct sockaddr *) &groupAddr, sizeof(Sockaddr)) < 0)
          { MWError("Send error"); }
}

void sendUserNameReq(short reqGuid, bool reject){
	MW244BPacket pack;
	PktUsernameReq * pkt = (PktUsernameReq *)pack.data;

	pack.header.type = PACKET_USERNAME_REQ;

	if(reject)
		pkt->reject = 0;
	else
		pkt->reject = 0;

	pkt->guid = reqGuid;

        ConvertOutgoing(&pack);
        if (sendto((int)M->theSocket(), &pack, sizeof(pack), 0, (struct sockaddr *) &groupAddr, sizeof(Sockaddr)) < 0)
          { MWError("Send error"); }
}

void sendTagged(short tagGuid, int tagKey){
        if(difftime(time(NULL), taggedCooldown) < 1)
                return;
        time(&taggedCooldown);

        MW244BPacket pack;
	PktTagged * pkt = (PktTagged *) pack.data;

        pack.header.type = PACKET_TAGGED;

	pkt->guid = tagGuid;
	pkt->tagKey = tagKey;
	

        ConvertOutgoing(&pack);
        if (sendto((int)M->theSocket(), &pack, sizeof(pack), 0, (struct sockaddr *) &groupAddr, sizeof(Sockaddr)) < 0)
          { MWError("Send error"); }
}

void sendTaggedAck(short tagGuid, bool ack, int tagKey){
        MW244BPacket pack;
        PktTaggedAck * pkt = (PktTaggedAck *) pack.data;

        pack.header.type = PACKET_TAGGED_ACK;
        pkt->guid = tagGuid;

	if(ack)
		pkt->ack = (short)0xC0;
	else
		pkt->ack = (short)0x00;
        pkt->tagKey = tagKey;

        ConvertOutgoing(&pack);
        if (sendto((int)M->theSocket(), &pack, sizeof(pack), 0, (struct sockaddr *) &groupAddr, sizeof(Sockaddr)) < 0)
          { MWError("Send error"); }
}

void sendLeave(){
	MW244BPacket pack;
	pack.header.type = PACKET_LEAVE;

        ConvertOutgoing(&pack);
        if (sendto((int)M->theSocket(), &pack, sizeof(pack), 0, (struct sockaddr *) &groupAddr, sizeof(Sockaddr)) < 0)
          { MWError("Send error"); }
}

void processState(MW244BPacket * pack){
	PktState * pkt = (PktState * ) pack->data;
	RatInfo * info;

	for(int i=0; i<MAX_OTHER_RATS ; i++){
		info = &otherRats[i];
		if ((info->valid) && ((short)info->guid == (short)pack->header.guid)){
			time(&(info->pktTime));
			//info->pktSeq = pack->header.seq;
			info->score = pkt->score;
			info->tagKey = pkt->tagKey;
			info->x = pkt->x;
			info->y = pkt->y;
			info->dir = pkt->dir;
			UpdateScoreCard(RatIndexType(i+1));
			return;
		}
	}
	for(int i=0; i<MAX_OTHER_RATS ; i++){
		info = &otherRats[i];
		if(info->valid == false){
			// add new user, send username request and return
			info->valid = true;
			info->guid = pack->header.guid;

			time(&(info->pktTime));
			info->pktSeq = pack->header.seq;
			info->score = pkt->score;
			info->tagKey = pkt->tagKey;
			info->x = pkt->x;
			info->y = pkt->y;
			info->dir = pkt->dir;

			SetRatPosition(RatIndexType(i+1), Loc(info->x), Loc(info->y), Direction(info->dir));
			NewScoreCard();
			return;
		}
	}
	// queue full, going to reject
}

void processUserNameReq(MW244BPacket * pack){
	PktUsernameReq * pkt = (PktUsernameReq *) pack->data;
	
	if((short)pkt->guid != (short)GUID)
		return;
	if(pkt->reject)
		return;

	sendUserNameAck();
}


void processUserNameAck(MW244BPacket * pack){
	PktUsernameAck * pkt= (PktUsernameAck *) pack->data;
	RatInfo * info;
	short tmpGuid = pack->header.guid;
	//printf("received username: %s\n", pkt->username);

	for(int i=0; i<MAX_OTHER_RATS ; i++){
                info = &otherRats[i];
                if ((info->valid) && ((short)info->guid == (short)pack->header.guid)){
			strncpy(info->username, pkt->username, strlen(pkt->username));
			UpdateScoreCard(RatIndexType(i+1));
		}
	}
}

void processTagged(MW244BPacket * pack){
	PktTagged* pkt = (PktTagged *) pack->data;
	// first decide if guid match
	if((short)pkt->guid != (short)GUID)
		return;

	if(((int)pkt->tagKey == (int)TAGKEY) || (inTagHistory(pack->header.guid, pkt->tagKey))){
		sendTaggedAck(pack->header.guid, true, pkt->tagKey);

		// record this event in history in case of packet loss
		tagHistory[(tagHistoryIterator)%TAG_LENGTH].guid = pack->header.guid;
		tagHistory[(tagHistoryIterator)%TAG_LENGTH].tagKey = pkt->tagKey;
		tagHistoryIterator++;

		// generate new tagkey and restart game
		TAGKEY++;
		M->scoreIs( M->score().value()-5 );
		NewPosition(M);
		return;
	}
	else{
		sendTaggedAck(pack->header.guid, false, 0);
	}
}

void processTaggedAck(MW244BPacket * pack){
	PktTaggedAck * pkt = (PktTaggedAck *)pack->data;

        if((short)pkt->guid != (short)GUID)
                return;
	
        TagInfo * tagInfo;
        for(int j=0; j<TAG_LENGTH; j++){
                tagInfo = &tagInfos[j];
                if(!tagInfo->valid)
                        continue;
		if((short)pkt->guid == (short)GUID)
			tagInfo->valid = false;
                if(((short)pkt->guid == (short)GUID) &&((int)pkt->tagKey == (int)tagInfo->tagKey)){
			if(pkt->ack){
				M->scoreIs( M->score().value()+11 );
				UpdateScoreCard(RatIndexType(0));
			}
		}
        }

}

void processLeave(MW244BPacket * pack){
	RatInfo * info;
	for(int i=0; i < MAX_OTHER_RATS ; i++){
                info = &otherRats[i];
                if ((info->valid) && ((short)info->guid == (short)pack->header.guid)){
			info->valid = false;
			memset(info->username, 0, 24);
			ClearRatPosition(RatIndexType(i+1));
			UpdateScoreCard(RatIndexType(i+1));
			// disable tag information related to the quit palyer
			disableTagInfo(info->guid);
			return;
		}
	}
}

void disableTagInfo(short tagGuid){
        TagInfo * tagInfo;
        for(int j=0; j<TAG_LENGTH; j++){
                tagInfo = &tagInfos[j];
                if(!tagInfo->valid)
                        continue;
		if(tagInfo->guid == tagGuid)
			tagInfo->valid = false;
        }
}

bool inTagHistory(short tagGuid, int tagKey){
        TagInfo * tagInfo;
        for(int j=0; j<TAG_LENGTH; j++){
                tagInfo = &tagHistory[j];
		if(((short)tagInfo->guid ==(short) tagGuid) &&(tagInfo->tagKey == tagKey))
			return true;
	}
	return false;
}

bool validMove(int x, int y){
	if(M->maze_[x][y])
		return false;

        RatInfo * info;
        for(int i=0; i<MAX_OTHER_RATS ; i++){
                info = &otherRats[i];
		if((Loc(x) == Loc(info->x)) &&(Loc(y) == Loc(info->y)))
			return false;
	}
	return true;
}

void moveToAdjacent(){
        register int    tx = MY_X_LOC;
        register int    ty = MY_Y_LOC;

	int i;
	while(true){
		i = rand()%4;
		switch(i) {
			case 0:     if (!M->maze_[tx+1][ty])        {tx++; goto outRand;} else break;
			case 1:     if (!M->maze_[tx-1][ty])        {tx--; goto outRand;} else break;
			case 2:     if (!M->maze_[tx][ty+1])        {ty++; goto outRand;} else break;
			case 3:     if (!M->maze_[tx][ty-1])        {ty--; goto outRand;} else break;
		}
	}
	outRand:
        M->xlocIs(Loc(tx));
        M->ylocIs(Loc(ty));
        updateView = TRUE;
}

/* ----------------------------------------------------------------------- */

void processPacket (MWEvent *eventPacket)
{
	MW244BPacket		*pack = eventPacket->eventDetail;

	if ((short)pack->header.guid == (short)GUID){
		if((int)pack->header.seq >= (int)SEQNUM){
			// re-pick guid
			GUID = (short int)rand();
		}
		else{
			// ignore the pakcets from myself
			return;
		}
	}

	// drop packet if sequence inverted
        RatInfo * info;
        for(int i=0; i<MAX_OTHER_RATS ; i++){
                info = &otherRats[i];
                if ((info->valid) && ((short)info->guid == (short)pack->header.guid)){
			if(info->pktSeq< pack->header.seq){
				info->pktSeq = pack->header.seq;
				break;
			}
			else
				return;
		}
	}

	switch(pack->header.type) {
                case PACKET_STATE:
			processState(pack);
                        break;
                case PACKET_USERNAME_REQ:
			processUserNameReq(pack);
                        break;
                case PACKET_USERNAME_ACK:
			processUserNameAck(pack);
                        break;
                case PACKET_TAGGED:
			processTagged(pack);
                        break;
                case PACKET_TAGGED_ACK:
			processTaggedAck(pack);
                        break;
                case PACKET_LEAVE:
			processLeave(pack);
                        break;
		default:
			break;

	}
}

/* ----------------------------------------------------------------------- */

/* This will presumably be modified by you.
   It is here to provide an example of how to open a UDP port.
   You might choose to use a different strategy
 */
void
netInit()
{
	Sockaddr		nullAddr;
	Sockaddr		*thisHost;
	char			buf[128];
	int				reuse;
	u_char          ttl;
	struct ip_mreq  mreq;

	/* MAZEPORT will be assigned by the TA to each team */
	M->mazePortIs(htons(MAZEPORT));

	gethostname(buf, sizeof(buf));
	if ((thisHost = resolveHost(buf)) == (Sockaddr *) NULL)
	  MWError("who am I?");
	bcopy((caddr_t) thisHost, (caddr_t) (M->myAddr()), sizeof(Sockaddr));

	M->theSocketIs(socket(AF_INET, SOCK_DGRAM, 0));
	if (M->theSocket() < 0)
	  MWError("can't get socket");

	/* SO_REUSEADDR allows more than one binding to the same
	   socket - you cannot have more than one player on one
	   machine without this */
	reuse = 1;
	if (setsockopt(M->theSocket(), SOL_SOCKET, SO_REUSEADDR, &reuse,
		   sizeof(reuse)) < 0) {
		MWError("setsockopt failed (SO_REUSEADDR)");
	}

	nullAddr.sin_family = AF_INET;
	nullAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	nullAddr.sin_port = M->mazePort();
	if (bind(M->theSocket(), (struct sockaddr *)&nullAddr,
		 sizeof(nullAddr)) < 0)
	  MWError("netInit binding");

	/* Multicast TTL:
	   0 restricted to the same host
	   1 restricted to the same subnet
	   32 restricted to the same site
	   64 restricted to the same region
	   128 restricted to the same continent
	   255 unrestricted

	   DO NOT use a value > 32. If possible, use a value of 1 when
	   testing.
	*/

	ttl = 1;
	if (setsockopt(M->theSocket(), IPPROTO_IP, IP_MULTICAST_TTL, &ttl,
		   sizeof(ttl)) < 0) {
		MWError("setsockopt failed (IP_MULTICAST_TTL)");
	}

	/* join the multicast group */
	mreq.imr_multiaddr.s_addr = htonl(MAZEGROUP);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(M->theSocket(), IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)
		   &mreq, sizeof(mreq)) < 0) {
		MWError("setsockopt failed (IP_ADD_MEMBERSHIP)");
	}

	/*
	 * Now we can try to find a game to join; if none, start one.
	 */
	 
	printf("\n");

	/* set up some stuff strictly for this local sample */
	
	M->myRatIdIs(0);
	M->scoreIs(0);
	SetMyRatIndexType(0);

	/* Get the multi-cast address ready to use in SendData()
           calls. */
	memcpy(&groupAddr, &nullAddr, sizeof(Sockaddr));
	groupAddr.sin_addr.s_addr = htonl(MAZEGROUP);

}


/* ----------------------------------------------------------------------- */

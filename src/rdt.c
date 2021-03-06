#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: SLIGHTLY MODIFIED
 FROM VERSION 1.1 of J.F.Kurose

   This code should be used for PA2, unidirectional or bidirectional
   data transfer protocols (from A to B. Bidirectional transfer of data
   is for extra credit and is not required).  Network properties:
   - one way network delay averages five time units (longer if there
       are other packets in the channel for GBN), but can be larger
   - frames can be corrupted (either the header or the data portion)
       or lost, according to user-defined probabilities
   - frames will be delivered in the order in which they were sent
       (although some can be lost).
**********************************************************************/

#define BIDIRECTIONAL 1 /* change to 1 if you're doing extra credit */
/* and write a routine called B_output */

/* a "pkt" is the data unit passed from layer 3 (teachers code) to layer  */
/* 2 (students' code).  It contains the data (characters) to be delivered */
/* to layer 3 via the students transport level protocol entities.         */
struct pkt
{
    char data[4];
};

enum frmtype {
    DATA,
    ACK,
    PACK
};

/* a frame is the data unit passed from layer 2 (students code) to layer */
/* 1 (teachers code).  Note the pre-defined frame structure, which all   */
/* students must follow. */
struct frm
{
    enum frmtype type;
    int seqnum;
    int acknum;
    int checksum;
    char payload[4];
};

/********* FUNCTION PROTOTYPES. DEFINED IN THE LATER PART******************/
void starttimer(int AorB, float increment);
void stoptimer(int AorB);
void tolayer1(int AorB, struct frm frame);
void tolayer3(int AorB, char datasent[20]);

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/

enum State {
    WAITING_FOR_LAYER3,
    WAITING_FOR_ACK
};

struct Entity {
    enum State state;
    bool outstandingACK;
    int incomingSeq;
    int outgoingSeq;
    float timerInterrupt;
    struct frm lastFrame;
    int lastACK;
}A, B;

int inc_seq(int seq) {
    /* Since the sequence is alternating
     * it can have only two values: 0 and 1. */
    return 1 - seq;
}

int showcrcsteps;  /* show the CRC steps (1) or not (0) */
int piggybacking;  /* do piggybacking (1) or not (0) */
uint8_t generator; /* the CRC generator polynomial */

void send_ack(int AorB, bool isAck, int ack);
void entity_init(struct Entity* entity);
void entity_output(int AorB, struct pkt packet);
void entity_input(int AorB, struct frm frame);
void entity_timerinterrupt(int AorB);
void printbinchar(char c);
void printgenerator();


/*
 * Returns the summation of the int values
 * of all members
 */
int get_checksum(struct frm *frame) {
    int checksum = 0;
    checksum += frame->seqnum;
    checksum += frame->acknum;

    int i;
    for (i = 0; i < 4; ++i)
        checksum += frame->payload[i];

    return checksum;
}

uint8_t encode(struct frm *frame)
{
    /*
     * NOTE: a byte is the same as uint8_t
     */

    /**
     * Length of input message (dividend) in bytes.
     */
    int len = 4 + 3;

    /**
     * The input message in bytes.
     */
    uint8_t *input;

    input = (uint8_t*) malloc(len * sizeof(uint8_t));

    /* Take each element of frm and add its int value to input. */
    for (int i = 0; i < 4; i++) input[i] = frame->payload[i];
    input[4] = frame->seqnum;
    input[5] = frame->acknum;
    if (frame->type == DATA) input[6] = 0;
    if (frame->type == ACK) input[6] = 1;
    if (frame->type == PACK) input[6] = 2;
    // input[7] = '\0';

    /**
     * The byte which will hold the value of CRC remainder.
     */
    uint8_t crc = 0;

    uint8_t c;

    for (int i = 0; i < len; i++) {
        /* Take one byte from input. */
        c = input[i];

        /* XOR it with CRC register and put it in. */
        crc ^= c;

        for (int j = 0; j < 8; j++) {

            /* If MSB of CRC is 1, LShift and XOR with generator.
             * Otherwise, just do LShift.
             *
             * 0x80 == 0b10000000 */
            if (crc & 0x80)
                crc = (crc << 1 ) ^ generator;
            else
                crc <<= 1;
        }
    }

    if (showcrcsteps) {
        printf("ENCODING...\n");
        printf("Input bit string: ");
        for (int i = 0; i < len; i++) printbinchar(input[i]); putchar('\n');
        printf("Generator polynomial: "); printgenerator();
        printf("CRC remainder: %d\n", crc);
        printf("CRC remainder: "); printbinchar(crc); putchar('\n');
        printf("ENCODED.\n");
    }
    // frame->checksum = crc;
    free(input);
    return crc;
}

/**
 * Returns the CRC remainder for a frame.
 */
uint8_t decode(struct frm *frame)
{
    /*
     * Code is the same as #encode()
     * except this time the checksum element will be appended to input.
     */
    int len = 4 + 4;
    uint8_t *input = (uint8_t*) malloc(len * sizeof(uint8_t));
    for (int i = 0; i < 4; i++) input[i] = frame->payload[i];
    input[4] = frame->seqnum;
    input[5] = frame->acknum;
    if (frame->type == DATA) input[6] = 0;
    if (frame->type == ACK) input[6] = 1;
    if (frame->type == PACK) input[6] = 2;
    input[7] = frame->checksum;
    // input[7] = '\0';

    uint8_t c;
    uint8_t crc = 0;

    for (int i = 0; i < len; i++) {
        c = input[i];
        crc ^= c;

        for (int j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1 ) ^ generator;
            else
                crc <<= 1;
        }
    }

    if (showcrcsteps) {
        printf("DECODING...\n");
        printf("Input bit string: ");
        for (int i = 0; i < len; i++) printbinchar(input[i]); putchar('\n');
        printf("Generator polynomial: "); printgenerator();
        printf("CRC remainder: %d\n", crc);
        printf("CRC remainder: "); printbinchar(crc); putchar('\n');
        printf("DECODED\n");
    }
    free(input);
    return crc;
}

void entity_output(int AorB, struct pkt packet)
{
    struct Entity *entity;
    if (AorB == 0) entity = &A;
    else entity = &B;

    if (entity->state != WAITING_FOR_LAYER3) {
        if (AorB == 0)
            printf("  A_output: Packet dropped. ACK not yet received.\n");
        else
            printf("  B_output: Packet dropped. ACK not yet received.\n");
        return;
    }

    /* create a frame to send B */
    struct frm frame;

    if (piggybacking && entity->outstandingACK) {
        frame.type = PACK;
        frame.seqnum = entity->outgoingSeq;
        frame.acknum = entity->lastACK;
        // frame.acknum = inc_seq(entity->incomingSeq);
    } else {
        frame.type = DATA;
        frame.seqnum = entity->outgoingSeq;
        frame.acknum = entity->lastACK;
    }

    memmove(frame.payload, packet.data, 4);
    // frame.checksum = get_checksum(&frame);
    frame.checksum = encode(&frame);

    /* send the frame to B */
    entity->lastFrame = frame;
    entity->state = WAITING_FOR_ACK;
    entity->outstandingACK = false;
    tolayer1(AorB, frame);
    starttimer(AorB, entity->timerInterrupt);

    if (AorB == 0)
        printf("  A_output: Frame sent: %s:%d.\n", frame.payload, frame.type);
    else
        printf("  B_output: Frame sent: %s:%d.\n", frame.payload, frame.type);
}

/* called from layer 3, passed the data to be sent to other side */
void A_output(struct pkt packet)
{
    entity_output(0, packet);
}

/* need be completed only for extra credit */
void B_output(struct pkt packet)
{
    entity_output(1, packet);
}

void entity_input(int AorB, struct frm frame)
{
    struct Entity *entity;
    if (AorB == 0) entity = &A;
    else entity = &B;

    if (frame.type == ACK) {
        if (entity->state != WAITING_FOR_ACK) {
            if (AorB == 0)
                printf("  A_input: Frame dropped. ACK already received.\n");
            else
                printf("  B_input: Frame dropped. ACK already received.\n");
            return;
        }

        // if (frame.checksum != get_checksum(&frame)) {
        if (decode(&frame) != 0) {
            if (AorB == 0)
                printf("  A_input: ACK dropped. Corruption.\n");
            else
                printf("  B_input: ACK dropped. Corruption.\n");
            return;
        }

        if (frame.acknum != entity->outgoingSeq) {
            if (AorB == 0)
                printf("  A_input: ACK dropped. Not the expected ACK.\n");
            else
                printf("  B_input: ACK dropped. Not the expected ACK.\n");
            return;
        }

        if (AorB == 0)
            printf("  A_input: ACK received.\n");
        else
            printf("  B_input: ACK received.\n");

        /* get ready for sendig next packet */
        stoptimer(AorB);
        entity->outgoingSeq = inc_seq(entity->outgoingSeq);
        entity->state = WAITING_FOR_LAYER3;
    }
    else if (frame.type == DATA) {
        // if (frame.checksum != get_checksum(&frame)) {
        if (decode(&frame) != 0) {
            if (AorB == 0)
                printf("  A_input: Frame corrupted.\n");
            else
                printf("  B_input: Frame corrupted.\n");
            send_ack(AorB, false, inc_seq(entity->incomingSeq));
            return;
        }

        if (frame.seqnum != entity->incomingSeq) {
            if (AorB == 0)
                printf("  A_input: Not the expected SEQ.\n");
            else
                printf("  B_input: Not the expected SEQ.\n");
            send_ack(AorB, false, inc_seq(entity->incomingSeq));
            return;
        }

        if (AorB == 0) {
            printf("  A_input: Frame received: %s:%d\n", frame.payload, frame.type);
        } else {
            printf("  B_input: Frame received: %s:%d\n", frame.payload, frame.type);
        }

        send_ack(AorB, true, entity->incomingSeq);

        tolayer3(AorB, frame.payload);
        entity->incomingSeq = inc_seq(entity->incomingSeq);
    } else if (frame.type == PACK) {
        // if (frame.checksum != get_checksum(&frame)) {
        if (decode(&frame) != 0) {
            if (AorB == 0)
                printf("  A_input: PACK corrupted.\n");
            else
                printf("  B_input: PACK corrupted.\n");
            send_ack(AorB, false, inc_seq(entity->incomingSeq));
            return;
        }

        if (frame.seqnum != entity->incomingSeq) {
            if (AorB == 0)
                printf("  A_input: Not the expected SEQ.\n");
            else
                printf("  B_input: Not the expected SEQ.\n");
            send_ack(AorB, false, inc_seq(entity->incomingSeq));
            return;
        }

        if (frame.acknum != entity->outgoingSeq) {
            if (AorB == 0)
                printf("  A_input: PACK dropped. Not the expected ACK.\n");
            else
                printf("  B_input: PACK dropped. Not the expected ACK.\n");
            return;
        }

        if (AorB == 0) {
            printf("  A_input: PACK received: %s:%d\n", frame.payload, frame.type);
        } else {
            printf("  B_input: PACK received: %s:%d\n", frame.payload, frame.type);
        }

        stoptimer(AorB);
        entity->outgoingSeq = inc_seq(entity->outgoingSeq);
        entity->state = WAITING_FOR_LAYER3;

        send_ack(AorB, true, entity->incomingSeq);

        tolayer3(AorB, frame.payload);
        entity->incomingSeq = inc_seq(entity->incomingSeq);
    }
}

/* called from layer 1, when a frame arrives for layer 2 */
void A_input(struct frm frame)
{
    entity_input(0, frame);
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */
/* called from layer 1, when a frame arrives for layer 2 at B*/
void B_input(struct frm frame)
{
    entity_input(1, frame);
}

void entity_timerinterrupt(int AorB)
{
    struct Entity *entity;
    if (AorB == 0) entity = &A;
    else entity = &B;

    if (entity->state != WAITING_FOR_ACK) {
        if (AorB == 0)
            printf("  A_timerinterrupt: Ignored. Not waiting for ACK.\n");
        else
            printf("  B_timerinterrupt: Ignored. Not waiting for ACK.\n");
        return;
    }

    if (AorB == 0)
        printf("  A_timerinterrupt: Resend last frame: %s:%d.\n", A.lastFrame.payload, A.lastFrame.type);
    else
        printf("  B_timerinterrupt: Resend last frame: %s:%d.\n", B.lastFrame.payload, B.lastFrame.type);

    tolayer1(AorB, entity->lastFrame);
    starttimer(AorB, entity->timerInterrupt);
}
/* called when A's timer goes off */
void A_timerinterrupt(void)
{
    entity_timerinterrupt(0);
}

/* called when B's timer goes off */
void B_timerinterrupt(void)
{
    entity_timerinterrupt(1);
}

void send_ack(int AorB, bool isAck, int ack)
{
    struct Entity *entity;
    if (AorB == 0) entity = &A;
    else entity = &B;

    if (piggybacking && !entity->outstandingACK) {
        if (AorB == 0)
            printf("  A_input: Waiting for piggybacking.\n");
        else
            printf("  B_input: Waiting for piggybacking.\n");
        entity->lastACK = ack;
        entity->outstandingACK = true;
    } else {
        if (AorB == 0) {
            if (isAck)
                printf("  A_input: Send ACK.\n");
            else
                printf("  A_input: Send NACK.\n");
        } else {
            if (isAck)
                printf("  B_input: Send ACK.\n");
            else
                printf("  B_input: Send NACK.\n");
        }
        struct frm frame;
        frame.type = ACK;
        frame.seqnum = entity->incomingSeq;
        if (piggybacking) frame.acknum = entity->lastACK;
        else frame.acknum = ack;
        // frame.checksum = get_checksum(&frame);
        frame.checksum = encode(&frame);
        tolayer1(AorB, frame);
        entity->outstandingACK = false;
    }
}

void entity_init(struct Entity* entity)
{
    entity->state = WAITING_FOR_LAYER3;
    entity->incomingSeq = 0;
    entity->outgoingSeq = 0;
    entity->timerInterrupt = 100;
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init(void)
{
    entity_init(&A);
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init(void)
{
    entity_init(&B);
}


/*****************************************************************
***************** NETWORK EMULATION CODE STARTS BELOW ***********
The code below emulates the layer 1 and below network environment:
    - emulates the tranmission and delivery (possibly with bit-level corruption
        and frame loss) of frames across the layer 1/2 interface
    - handles the starting/stopping of a timer, and generates timer
        interrupts (resulting in calling students timer handler).
    - generates packet to be sent (passed from later 3 to 2)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
the emulator, you're welcome to look at the code - but again, you should have
to, and you defeinitely should not have to modify
******************************************************************/

struct event
{
    float evtime;       /* event time */
    int evtype;         /* event type code */
    int eventity;       /* entity where event occurs */
    struct frm *frmptr; /* ptr to frame (if any) assoc w/ this event */
    struct event *prev;
    struct event *next;
};
struct event *evlist = NULL; /* the event list */

/* possible events: */
#define TIMER_INTERRUPT 0
#define FROM_LAYER3 1
#define FROM_LAYER1 2

#define OFF 0
#define ON 1
#define A 0
#define B 1

int TRACE = 1;     /* for my debugging */
int nsim = 0;      /* number of packets from 3 to 2 so far */
int nsimmax = 0;   /* number of pkts to generate, then stop */
float time = 0.000;
float lossprob;    /* probability that a frame is dropped  */
float corruptprob; /* probability that one bit is frame is flipped */
float lambda;      /* arrival rate of packets from layer 3 */
int ntolayer1;     /* number sent into layer 1 */
int nlost;         /* number lost in media */
int ncorrupt;      /* number corrupted by media*/


void init();
void generate_next_arrival(void);
void insertevent(struct event *p);

#define WRITE_DOC 1

FILE *fp;

void printgenerator()
{
    putchar('1');
    printbinchar(generator);
    putchar('\n');
}
void printbinchar(char c)
{
    for (int i = 7; i >= 0; --i)
    {
        putchar( (c & (1 << i)) ? '1' : '0' );
    }
}

int main()
{
    struct event *eventptr;
    struct pkt pkt2give;
    struct frm frm2give;

    int i, j;
    char c;

    init();
    A_init();
    B_init();

    while (1)
    {
        eventptr = evlist; /* get next event to simulate */
        if (eventptr == NULL)
            goto terminate;
        evlist = evlist->next; /* remove this event from event list */
        if (evlist != NULL)
            evlist->prev = NULL;
        if (TRACE >= 2)
        {
            printf("\nEVENT time: %f,", eventptr->evtime);
            printf("  type: %d", eventptr->evtype);
            if (eventptr->evtype == 0)
                printf(", timerinterrupt  ");
            else if (eventptr->evtype == 1)
                printf(", fromlayer3 ");
            else
                printf(", fromlayer1 ");
            printf(" entity: %d\n", eventptr->eventity);
        }
        time = eventptr->evtime; /* update time to next event time */
        if (eventptr->evtype == FROM_LAYER3)
        {
            if (nsim < nsimmax)
            {
                if (nsim + 1 < nsimmax)
                    generate_next_arrival(); /* set up future arrival */
                /* fill in pkt to give with string of same letter */
                j = nsim % 26;
                for (i = 0; i < 4; i++)
                    pkt2give.data[i] = 97 + j;
                pkt2give.data[4 - 1] = 0;
                if (TRACE > 2)
                {
                    printf("          MAINLOOP: data given to student: ");
                    for (i = 0; i < 4; i++)
                        printf("%c", pkt2give.data[i]);
                    printf("\n");
                }
                nsim++;
                if (eventptr->eventity == A)
                    A_output(pkt2give);
                else
                    B_output(pkt2give);
            }
        }
        else if (eventptr->evtype == FROM_LAYER1)
        {
            frm2give.type = eventptr->frmptr->type;
            frm2give.seqnum = eventptr->frmptr->seqnum;
            frm2give.acknum = eventptr->frmptr->acknum;
            frm2give.checksum = eventptr->frmptr->checksum;
            for (i = 0; i < 4; i++)
                frm2give.payload[i] = eventptr->frmptr->payload[i];
            if (eventptr->eventity == A) /* deliver frame by calling */
                A_input(frm2give); /* appropriate entity */
            else
                B_input(frm2give);
            free(eventptr->frmptr); /* free the memory for frame */
        }
        else if (eventptr->evtype == TIMER_INTERRUPT)
        {
            if (eventptr->eventity == A)
                A_timerinterrupt();
            else
                B_timerinterrupt();
        }
        else
        {
            printf("INTERNAL PANIC: unknown event type \n");
        }
        free(eventptr);
    }

terminate:
    printf(
        " Simulator terminated at time %f\n after sending %d pkts from layer3\n",
        time, nsim);

    if (WRITE_DOC == 1) fclose(fp);
}

void init() /* initialize the simulator */
{
    int i;
    float sum, avg;
    float jimsrand();
    char *gen = (char*) malloc (8 * sizeof(char));

    printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");

    // printf("Enter the number of packets to simulate: ");
    // scanf("%d",&nsimmax);
    // printf("Enter  frame loss probability [enter 0.0 for no loss]:");
    // scanf("%f",&lossprob);
    // printf("Enter frame corruption probability [0.0 for no corruption]:");
    // scanf("%f",&corruptprob);
    // printf("Enter average time between packets from sender's layer3 [ > 0.0]:");
    // scanf("%f",&lambda);
    // printf("Enter TRACE:");
    // scanf("%d",&TRACE);
    // printf("Enter CRC steps (0 or 1):");
    // scanf("%d",&showcrcsteps);
    // printf("Enter piggybacking (0 or 1):");
    // scanf("%d",&piggybacking);
    // printf("Enter generator polynomial (in binary and less than 9 bits):");
    // scanf("%s",gen);

    int casechoice;
    printf("Enter case (1, 2, or 3):");
    scanf("%d", &casechoice);

    if (casechoice == 1) {
            nsimmax        = 5;
            lossprob       = 0.2;
            corruptprob    = 0.2;
            lambda         = 500;
            TRACE          = 1;
            showcrcsteps   = 0;
            piggybacking   = 0;
            free(gen); gen = "11101";
    } else if (casechoice == 2) {
            nsimmax        = 5;
            lossprob       = 0.2;
            corruptprob    = 0.2;
            lambda         = 100;
            TRACE          = 1;
            showcrcsteps   = 0;
            piggybacking   = 1;
            free(gen); gen = "11101";
    } else if (casechoice == 3) {
            nsimmax        = 2;
            lossprob       = 0.0;
            corruptprob    = 0.5;
            lambda         = 500;
            TRACE          = 1;
            showcrcsteps   = 1;
            piggybacking   = 0;
            free(gen); gen = "11101";
    } else {
            nsimmax        = 3;
            lossprob       = 0.2;
            corruptprob    = 0.2;
            lambda         = 10000;
            TRACE          = 1;
            showcrcsteps   = 0;
            piggybacking   = 0;
            free(gen); gen = "11101";
    }

    int len = strlen(gen);
    if (len > 8) { printf("ERROR: Generator entered more than 8 bits.\n"); return; }
    generator = 0;
    for (int i = 0; i < len; i++) {
        int x = len - 1 - i;
        generator += (gen[x] - '0' )* (int) pow(2, i);
    }

    if (WRITE_DOC == 1) {
        if (casechoice == 1) fp = freopen("report1.docx", "w+", stdout);
        else if (casechoice == 2) fp = freopen("report2.docx", "w+", stdout);
        else if (casechoice == 3) fp = freopen("report3.docx", "w+", stdout);
        else fp = freopen("report.docx", "w+", stdout);
    }

    printf("The number of packets to simulate: %d\n", nsimmax);
    printf("Frame loss probability: %f\n", lossprob);
    printf("Frame corruption probability: %f\n", corruptprob);
    printf("Average time between packets from sender's layer3: %f\n", lambda);
    printf("TRACE: %d\n", TRACE);
    printf("CRC steps: %d\n", showcrcsteps);
    printf("Piggybacking: %d\n", piggybacking);
    printf("Generator polynomial: ");
    printgenerator();
    printf("\n\n");

    // return;

    srand(9999); /* init random number generator */
    sum = 0.0;   /* test random number generator for students */
    for (i = 0; i < 1000; i++)
        sum = sum + jimsrand(); /* jimsrand() should be uniform in [0,1] */
    avg = sum / 1000.0;
    if (avg < 0.25 || avg > 0.75)
    {
        printf("It is likely that random number generation on your machine\n");
        printf("is different from what this emulator expects.  Please take\n");
        printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
        exit(1);
    }

    ntolayer1 = 0;
    nlost = 0;
    ncorrupt = 0;

    time = 0.0;              /* initialize time to 0.0 */
    generate_next_arrival(); /* initialize event list */
}

/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
float jimsrand(void)
{
    double mmm = RAND_MAX;
    float x;                 /* individual students may need to change mmm */
    x = rand() / mmm;        /* x should be uniform in [0,1] */
    return (x);
}

/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/

void generate_next_arrival(void)
{
    double x, log(), ceil();
    struct event *evptr;
    float ttime;
    int tempint;

    if (TRACE > 2)
        printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");

    x = lambda * jimsrand() * 2; /* x is uniform on [0,2*lambda] */
    /* having mean of lambda        */
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtime = time + x;
    evptr->evtype = FROM_LAYER3;
    if (BIDIRECTIONAL && (jimsrand() > 0.5))
        evptr->eventity = B;
    else
        evptr->eventity = A;
    insertevent(evptr);
}

void insertevent(struct event *p)
{
    struct event *q, *qold;

    if (TRACE > 2)
    {
        printf("            INSERTEVENT: time is %lf\n", time);
        printf("            INSERTEVENT: future time will be %lf\n", p->evtime);
    }
    q = evlist;      /* q points to header of list in which p struct inserted */
    if (q == NULL)   /* list is empty */
    {
        evlist = p;
        p->next = NULL;
        p->prev = NULL;
    }
    else
    {
        for (qold = q; q != NULL && p->evtime > q->evtime; q = q->next)
            qold = q;
        if (q == NULL)   /* end of list */
        {
            qold->next = p;
            p->prev = qold;
            p->next = NULL;
        }
        else if (q == evlist)     /* front of list */
        {
            p->next = evlist;
            p->prev = NULL;
            p->next->prev = p;
            evlist = p;
        }
        else     /* middle of list */
        {
            p->next = q;
            p->prev = q->prev;
            q->prev->next = p;
            q->prev = p;
        }
    }
}

void printevlist(void)
{
    struct event *q;
    int i;
    printf("--------------\nEvent List Follows:\n");
    for (q = evlist; q != NULL; q = q->next)
    {
        printf("Event time: %f, type: %d entity: %d\n", q->evtime, q->evtype,
               q->eventity);
    }
    printf("--------------\n");
}

/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
void stoptimer(int AorB /* A or B is trying to stop timer */)
{
    struct event *q, *qold;

    if (TRACE > 2)
        printf("          STOP TIMER: stopping timer at %f\n", time);
    /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
    for (q = evlist; q != NULL; q = q->next)
        if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB))
        {
            /* remove this event */
            if (q->next == NULL && q->prev == NULL)
                evlist = NULL;          /* remove first and only event on list */
            else if (q->next == NULL) /* end of list - there is one in front */
                q->prev->next = NULL;
            else if (q == evlist)   /* front of list - there must be event after */
            {
                q->next->prev = NULL;
                evlist = q->next;
            }
            else     /* middle of list */
            {
                q->next->prev = q->prev;
                q->prev->next = q->next;
            }
            free(q);
            return;
        }
    printf("Warning: unable to cancel your timer. It wasn't running.\n");
}

void starttimer(int AorB /* A or B is trying to start timer */, float increment)
{
    struct event *q;
    struct event *evptr;

    if (TRACE > 2)
        printf("          START TIMER: starting timer at %f\n", time);
    /* be nice: check to see if timer is already started, if so, then  warn */
    /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
    for (q = evlist; q != NULL; q = q->next)
        if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB))
        {
            printf("Warning: attempt to start a timer that is already started\n");
            return;
        }

    /* create future event for when timer goes off */
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtime = time + increment;
    evptr->evtype = TIMER_INTERRUPT;
    evptr->eventity = AorB;
    insertevent(evptr);
}

/************************** TOLAYER1 ***************/
void tolayer1(int AorB, struct frm frame)
{
    struct frm *myfrmptr;
    struct event *evptr, *q;
    float lastime, x;
    int i;

    ntolayer1++;

    /* simulate losses: */
    if (jimsrand() < lossprob)
    {
        nlost++;
        if (TRACE > 0)
            printf("          TOLAYER1: frame being lost\n");
        return;
    }

    /* make a copy of the frame student just gave me since he/she may decide */
    /* to do something with the frame after we return back to him/her */
    myfrmptr = (struct frm *)malloc(sizeof(struct frm));
    myfrmptr->type = frame.type;
    myfrmptr->seqnum = frame.seqnum;
    myfrmptr->acknum = frame.acknum;
    myfrmptr->checksum = frame.checksum;
    for (i = 0; i < 4; i++)
        myfrmptr->payload[i] = frame.payload[i];
    if (TRACE > 2)
    {
        printf("          TOLAYER1: type : %d seq: %d, ack %d, check: %d ",
               myfrmptr->type, myfrmptr->seqnum, myfrmptr->acknum, myfrmptr->checksum);
        for (i = 0; i < 4; i++)
            printf("%c", myfrmptr->payload[i]);
        printf("\n");
    }

    /* create future event for arrival of frame at the other side */
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtype = FROM_LAYER1;      /* frame will pop out from layer1 */
    evptr->eventity = (AorB + 1) % 2; /* event occurs at other entity */
    evptr->frmptr = myfrmptr;         /* save ptr to my copy of frame */
    /* finally, compute the arrival time of frame at the other end.
       medium can not reorder, so make sure frame arrives between 1 and 10
       time units after the latest arrival time of frames
       currently in the medium on their way to the destination */
    lastime = time;
    /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next) */
    for (q = evlist; q != NULL; q = q->next)
        if ((q->evtype == FROM_LAYER1 && q->eventity == evptr->eventity))
            lastime = q->evtime;
    evptr->evtime = lastime + 1 + 9 * jimsrand();

    /* simulate corruption: */
    if (jimsrand() < corruptprob)
    {
        ncorrupt++;
        if ((x = jimsrand()) < .75)
            myfrmptr->payload[0] = 'Z'; /* corrupt payload */
        else if (x < .875)
            myfrmptr->seqnum = 999999;
        else
            myfrmptr->acknum = 999999;
        if (TRACE > 0)
            printf("          TOLAYER1: frame being corrupted\n");
    }

    if (TRACE > 2)
        printf("          TOLAYER1: scheduling arrival on other side\n");
    insertevent(evptr);
}

void tolayer3(int AorB, char datasent[20])
{
    int i;
    if (TRACE > 2)
    {
        printf("          TOLAYER3: data received: ");
        for (i = 0; i < 20; i++)
            printf("%c", datasent[i]);
        printf("\n");
    }
}

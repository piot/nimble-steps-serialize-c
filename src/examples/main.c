/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <tiny-libc/tiny_libc.h>

#include <discoid/circular_buffer.h>
#include <nimble-steps-serialize/in_serialize.h>
#include <nimble-steps-serialize/out_serialize.h>
#include <nimble-steps/steps.h>

#include <nimble-steps-serialize/pending_in_serialize.h>
#include <nimble-steps-serialize/pending_out_serialize.h>
#include <nimble-steps/pending_steps.h>

#include <clog/clog.h>
#include <flood/in_stream.h>
#include <flood/out_stream.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

clog_config g_clog;

static void console_log(enum clog_type type, const char* string)
{

    struct timeval tmnow;
    struct tm* tm;

    gettimeofday(&tmnow, NULL);
    tm = localtime(&tmnow.tv_sec);

    int millisec = lrint(tmnow.tv_usec / 1000.0); // Round to nearest millisec
    if (millisec >= 1000) {                       // Allow for rounding up to nearest second
        millisec -= 1000;
        tmnow.tv_sec++;
    }

    char buffer[26];
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm);
    printf("%s.%d: ", buffer, millisec);

    (void) type;
    puts(string);
}

int testCircular()
{
    NbsSteps stepBuffer;

    nbsStepsInit(&stepBuffer, 128, 7);

    const uint8_t* test1 = (const uint8_t*) "hello";

    const uint8_t* test2 = (const uint8_t*) "World!";

    nbsStepsWrite(&stepBuffer, 7, test1, 5 + 1);
    uint8_t buf[16];
    StepId foundStepId;
    nbsStepsRead(&stepBuffer, &foundStepId, buf, 5 + 1);
    printf("first read '%s'\n", buf);

    nbsStepsWrite(&stepBuffer, 8, test2, 6 + 1);

    nbsStepsRead(&stepBuffer, &foundStepId, buf, 6 + 1);
    printf("second read '%s'\n", buf);

    nbsStepsDestroy(&stepBuffer);

    return 0;
}

int testSteps()
{
    NbsSteps steps;

    StepId stepId = 3;

    nbsStepsInit(&steps, 1024, stepId);

    int errorCode = nbsStepsWrite(&steps, stepId, (const uint8_t*) "hello", 6);
    if (errorCode < 0) {
        return errorCode;
    }
    stepId++;
    errorCode = nbsStepsWrite(&steps, stepId, (const uint8_t*) "World!", 7);
    if (errorCode < 0) {
        return errorCode;
    }

    StepId upcomingId;
    nbsStepsPeek(&steps, &upcomingId);
    CLOG_INFO("count before read: %zu (upcoming %d)", nbsStepsCount(&steps), upcomingId);

    uint8_t buf[16];
    StepId readId;
    errorCode = nbsStepsRead(&steps, &readId, buf, 16);
    CLOG_INFO("first step read %d ID:%d '%s'", errorCode, readId, buf);
    if (errorCode < 0) {
        return errorCode;
    }
    nbsStepsPeek(&steps, &upcomingId);
    CLOG_INFO("count now: %zu (upcoming %d)", nbsStepsCount(&steps), upcomingId);

    errorCode = nbsStepsRead(&steps, &readId, buf, 16);
    CLOG_INFO("second step read %d ID:%d '%s'", errorCode, readId, buf);
    nbsStepsPeek(&steps, &upcomingId);
    CLOG_INFO("count now: %zu (upcoming %d)", nbsStepsCount(&steps), upcomingId);
    return errorCode;
}

int testStepsSerialize()
{
    CLOG_INFO("======= steps serialize =====");
    NbsSteps outSteps;

    const StepId firstStepId = 3;
    StepId stepId = firstStepId;

    nbsStepsInit(&outSteps, 1024, stepId);

    int errorCode = nbsStepsWrite(&outSteps, stepId, (const uint8_t*) "hello", 6);
    if (errorCode < 0) {
        return errorCode;
    }
    stepId++;
    errorCode = nbsStepsWrite(&outSteps, stepId, (const uint8_t*) "World!", 7);
    if (errorCode < 0) {
        return errorCode;
    }

    uint8_t serializeBuf[1024];
    FldOutStream outStream;
    fldOutStreamInit(&outStream, serializeBuf, 1024);
    errorCode = nbsStepsOutSerialize(&outStream, firstStepId, &outSteps);
    if (errorCode < 0) {
        return errorCode;
    }

    CLOG_INFO("wrote steps at %d octet count and %d step count", outStream.pos, errorCode);

    NbsSteps inSteps;
    nbsStepsInit(&inSteps, 1024, firstStepId);

    FldInStream inStream;
    fldInStreamInit(&inStream, serializeBuf, outStream.pos);

    StepId firstStepIdRead;
    size_t stepsThatFollow;
    nbsStepsInSerializeHeader(&inStream, &firstStepIdRead, &stepsThatFollow);

    errorCode = nbsStepsInSerialize(&inStream, &inSteps, firstStepIdRead, stepsThatFollow);
    if (errorCode < 0) {
        CLOG_SOFT_ERROR("Couldn't deserialize");
        return errorCode;
    }

    CLOG_INFO("read last stepId %d added count %d", inSteps.expectedWriteId - 1, errorCode);

    return 0;
}

int testPending(struct ImprintAllocatorWithFree* allocatorWithFree)
{
    NbsPendingSteps steps;

    nbsPendingStepsInit(&steps, 0, allocatorWithFree);
    nbsPendingStepsDebugOutput(&steps, "", 1);

    StepId stepId = 4;
    nbsPendingStepsTrySet(&steps, stepId, (const uint8_t*) "hello", 5);
    nbsPendingStepsDebugOutput(&steps, "", 0);

    int errorCode = nbsPendingStepsTrySet(&steps, stepId, (const uint8_t*) "hello2", 6);
    if (errorCode >= 0) {
        CLOG_SOFT_ERROR("should have failed");
        return -2;
    }
    nbsPendingStepsDebugOutput(&steps, "", 0);

    stepId = 6;
    errorCode = nbsPendingStepsTrySet(&steps, stepId, (const uint8_t*) "hello2", 6);
    if (errorCode < 0) {
        CLOG_SOFT_ERROR("failed");
        return -3;
    }
    nbsPendingStepsDebugOutput(&steps, "", 0);

    int canAdvance = nbsPendingStepsCanBeAdvanced(&steps);
    if (canAdvance != 0) {
        CLOG_SOFT_ERROR("should not be advanced");
        return -4;
    }

    stepId = 5;
    errorCode = nbsPendingStepsTrySet(&steps, stepId, (const uint8_t*) "hello2", 6);
    if (errorCode < 0) {
        CLOG_SOFT_ERROR("failed %d", errorCode);
        return -3;
    }
    nbsPendingStepsDebugOutput(&steps, "", 0);

    stepId = 0;
    errorCode = nbsPendingStepsTrySet(&steps, stepId, (const uint8_t*) "hello2", 6);
    if (errorCode < 0) {
        CLOG_SOFT_ERROR("failed %d", errorCode);
        return -3;
    }
    nbsPendingStepsDebugOutput(&steps, "", 0);

    canAdvance = nbsPendingStepsCanBeAdvanced(&steps);
    if (canAdvance != 1) {
        CLOG_SOFT_ERROR("should be advanced");
        return -4;
    }

    const uint8_t* outData;
    size_t outLength;
    StepId outId;

    errorCode = nbsPendingStepsTryRead(&steps, &outData, &outLength, &outId);
    if (errorCode < 0) {
        return errorCode;
    }

    nbsPendingStepsDebugOutput(&steps, "", 0);

    canAdvance = nbsPendingStepsCanBeAdvanced(&steps);
    if (canAdvance != 0) {
        CLOG_SOFT_ERROR("should not be advanced here");
        return -4;
    }

    return 0;
}

int testPending2(struct ImptintAllocatorWithFree* allocatorWithFree)
{
    NbsPendingSteps steps;

    nbsPendingStepsInit(&steps, 0);
    steps.writeIndex = 0;
    steps.expectingWriteId = 10;
    nbsPendingStepsDebugOutput(&steps, "", 0);

    int canAdvance = nbsPendingStepsCanBeAdvanced(&steps);
    if (canAdvance != 0) {
        CLOG_SOFT_ERROR("can not advance empty pending steps");
        return -4;
    }

    StepId stepId = 63;

    int errorCode = nbsPendingStepsTrySet(&steps, stepId, (const uint8_t*) "hello2", 6);
    if (errorCode < 0) {
        CLOG_SOFT_ERROR("should have failed");
        return errorCode;
    }
    nbsPendingStepsDebugOutput(&steps, "", 0);

    canAdvance = nbsPendingStepsCanBeAdvanced(&steps);
    if (canAdvance != 0) {
        CLOG_SOFT_ERROR("can not advance empty pending steps");
        return -4;
    }

    stepId = 69;
    errorCode = nbsPendingStepsTrySet(&steps, stepId, (const uint8_t*) "hello3", 6);
    if (errorCode < 0) {
        CLOG_SOFT_ERROR("failed");
        return errorCode;
    }
    nbsPendingStepsDebugOutput(&steps, "", 0);

    canAdvance = nbsPendingStepsCanBeAdvanced(&steps);
    if (canAdvance != 0) {
        CLOG_SOFT_ERROR("can not advance empty pending steps");
        return -4;
    }

    stepId = 12;
    errorCode = nbsPendingStepsTrySet(&steps, stepId, (const uint8_t*) "hello3", 6);
    if (errorCode < 0) {
        CLOG_SOFT_ERROR("failed");
        return errorCode;
    }
    nbsPendingStepsDebugOutput(&steps, "", 1);

    NbsPendingRange ranges[8];
    const int maxRangeCount = 4;
    const int maxStepsCount = 8;

    int rangeCount2 = nbsPendingStepsRanges(steps.readId, steps.expectingWriteId, steps.receiveMask, ranges,
                                            maxRangeCount, maxStepsCount);
    if (rangeCount2 < 0) {
        CLOG_SOFT_ERROR("problem with range");
        return rangeCount2;
    }
    nbsPendingStepsRangesDebugOutput(ranges, "test pending 2", rangeCount2);

    steps.writeIndex = 63 - 10;
    steps.expectingWriteId = 63;
    nbsPendingStepsDebugOutput(&steps, "", 0);

    canAdvance = nbsPendingStepsCanBeAdvanced(&steps);
    if (canAdvance != 1) {
        CLOG_SOFT_ERROR("can advance step 63");
        return -4;
    }

    const uint8_t* outPayload;
    size_t outLength;
    StepId outId;

    int couldRead = nbsPendingStepsTryRead(&steps, &outPayload, &outLength, &outId);
    if (couldRead < 0) {
        return couldRead;
    }

    nbsPendingStepsReadDestroy(&steps, outId);
    nbsPendingStepsDebugOutput(&steps, "", 0);
    canAdvance = nbsPendingStepsCanBeAdvanced(&steps);
    if (canAdvance != 0) {
        CLOG_SOFT_ERROR("can advance after step 63");
        return -4;
    }

    int rangeCount = nbsPendingStepsRanges(steps.readId, steps.expectingWriteId, steps.receiveMask, ranges,
                                           maxRangeCount, maxStepsCount);
    if (rangeCount < 0) {
        CLOG_SOFT_ERROR("problem with range");
        return rangeCount;
    }
    nbsPendingStepsRangesDebugOutput(ranges, "test pending 2", rangeCount);

    return 0;
}

int addExampleSteps(NbsSteps* target, StepId firstStepId)
{
    uint8_t stepBuffer[1024];

    NimbleStepsOutSerializeLocalParticipants participants;
    participants.participants[0].payload = (const uint8_t*) "Hello";
    participants.participants[0].connectState = NimbleSerializeStepTypeNormal;
    participants.participants[0].payloadCount = 5;
    participants.participants[0].participantIndex = 2;
    participants.participantCount = 1;

    int octetsWritten = nbsStepsOutSerializeCombinedStep(&participants, stepBuffer, 1024);
    if (octetsWritten < 0) {
        return octetsWritten;
    }

    int errorCode = nbsStepsWrite(target, firstStepId, stepBuffer, octetsWritten);
    if (errorCode < 0) {
        return errorCode;
    }

    firstStepId++;
    participants.participants[0].payload = (const uint8_t*) ", World!";
    participants.participants[0].payloadCount = 8;
    participants.participants[0].connectState = NimbleSerializeStepTypeNormal;
    octetsWritten = nbsStepsOutSerializeCombinedStep(&participants, stepBuffer, 1024);
    if (octetsWritten < 0) {
        return octetsWritten;
    }

    errorCode = nbsStepsWrite(target, firstStepId, stepBuffer, octetsWritten);
    if (errorCode <= 0) {
        CLOG_ERROR("can not write same twice");
        return errorCode;
    }

    nbsStepsDebugOutput(target, "created sample steps", 1);

    return 0;
}

int testReceiveServer(const uint8_t* data, size_t octetCount)
{
    NbsSteps fromClientSteps;
    nbsStepsInit(&fromClientSteps, 1024, 0);
    nbsStepsDebugOutput(&fromClientSteps, "received on server from client init", 0);

    FldInStream inStream;
    fldInStreamInit(&inStream, data, octetCount);

    StepId clientReceivedUpToStepId;
    uint64_t receiveMask;

    nbsPendingStepsInSerializeHeader(&inStream, &clientReceivedUpToStepId, &receiveMask);
    CLOG_INFO("server received %d and %016lX", clientReceivedUpToStepId, receiveMask);

    uint8_t stepBuffer[1024];

    NimbleStepsOutSerializeLocalParticipants participants;
    participants.participants[0].payload = (const uint8_t*) "I have no idea";
    participants.participants[0].payloadCount = 14;
    participants.participants[0].participantIndex = 2;
    participants.pariticpants[0].connectState = NimbleSerializeStepTypeNormal;
    participants.participantCount = 1;

    int octetsWritten = nbsStepsOutSerializeCombinedStep(&participants, stepBuffer, 1024);
    if (octetsWritten < 0) {
        return octetsWritten;
    }
    StepId firstStepIdRead;
    size_t stepsThatFollow;
    nbsStepsInSerializeHeader(&inStream, &firstStepIdRead, &stepsThatFollow);

    int something = nbsStepsInSerialize(&inStream, &fromClientSteps, firstStepIdRead, stepsThatFollow);
    if (something < 0) {
        return something;
    }

    nbsStepsDebugOutput(&fromClientSteps, "received on server from client", 1);

    return 0;
}

int testSendReceiveClient()
{
    uint8_t clientToServer[128];
    FldOutStream clientOut;
    NbsSteps clientOutSteps;

    StepId clientOutId = 0;

    nbsStepsInit(&clientOutSteps, 1024, 0);
    addExampleSteps(&clientOutSteps, clientOutId);

    fldOutStreamInit(&clientOut, clientToServer, 128);

    StepId authoritativeStepIdReceivedFromServer = NIMBLE_STEP_MAX;
    uint64_t clientReceiveMask = 0;
    nbsPendingStepsSerializeOutHeader(&clientOut, authoritativeStepIdReceivedFromServer, clientReceiveMask);

    clientOutId = 1;
    int stepCountSentToServer = nbsStepsOutSerialize(&clientOut, clientOutId, &clientOutSteps);
    if (stepCountSentToServer < 0) {
        return stepCountSentToServer;
    }

    return testReceiveServer(clientToServer, clientOut.pos);
}

typedef struct UdpBuffer {
    struct NbsSteps octetBuffer;
} UdpBuffer;

void udpBufferInit(UdpBuffer* self)
{
    nbsStepsInit(&self->octetBuffer, 1200 * 10, 0);
}

void udpBufferSend(UdpBuffer* self, const uint8_t* data, size_t octetCount)
{
    if (octetCount == 0) {
        return;
    }
    uint8_t hi = octetCount / 256;
    uint8_t low = octetCount % 256;

    // CLOG_DEBUG(" udp: send octetCount: %zu", octetCount);
    nbsStepsWrite(&self->octetBuffer, 0, &hi, 1);
    nbsStepsWrite(&self->octetBuffer, 1, &low, 1);
    nbsStepsWrite(&self->octetBuffer, 2, data, octetCount);
}

size_t udpBufferReceive(UdpBuffer* self, uint8_t* data, size_t maxCount)
{
    if (self->octetBuffer.stepsCount < 2) {
        return 0;
    }

    uint8_t hi;
    uint8_t low;
    StepId foundStepId;
    nbsStepsRead(&self->octetBuffer, &foundStepId, &hi, 1);
    nbsStepsRead(&self->octetBuffer, &foundStepId, &low, 1);
    size_t octetCount = hi * 256 + low;
    // CLOG_DEBUG(" udp: receive octetCount: %zu", octetCount);

    nbsStepsRead(&self->octetBuffer, &foundStepId, data, octetCount);

    return octetCount;
}

typedef struct UdpCommunication {
    UdpBuffer in;
    UdpBuffer out;
} UdpCommunication;

void udpCommunicationInit(UdpCommunication* self)
{
    udpBufferInit(&self->out);
    udpBufferInit(&self->in);
}

typedef struct Client {
    NbsPendingSteps pendingAuthoritativeSteps;
    NbsSteps predictedSteps;
    UdpCommunication communication;
    StepId startStepIdToSend;
} Client;

typedef struct ServerReceiver {
    StepId waitingForStepId;
    uint64_t receiveMask;
} ServerReceiver;

typedef struct Server {
    NbsSteps authoritativeSteps;
    NbsSteps firstClientPredictedSteps;
    UdpCommunication communication;

    ServerReceiver firstClient;
} Server;

void sendCommunication(UdpCommunication* target, UdpCommunication* source, const char* debug)
{
    static int counter;
    uint8_t packetBuf[1200];
    while (source->out.octetBuffer.stepsCount > 2) {
        // CLOG_INFO("send %s", debug);
        size_t octetRead = udpBufferReceive(&source->out, packetBuf, 1200);
        int intentionallyDropPacket = ((counter++) % 11) == 0;
        if (intentionallyDropPacket) {
            CLOG_WARN("**** intentionally DROPPING PACKET %s", debug);
        } else
            udpBufferSend(&target->in, packetBuf, octetRead);
    }
    // CLOG_INFO("send %s %zu", debug, octetRead);
}

void sendToServer(Server* server, Client* client)
{
    sendCommunication(&server->communication, &client->communication, "client -> server");
}

void sendToClient(Client* client, Server* server)
{
    sendCommunication(&client->communication, &server->communication, "server->client");
}

void serverInit(Server* self)
{
    nbsStepsInit(&self->firstClientPredictedSteps, 1024, 0);
    nbsStepsInit(&self->authoritativeSteps, 1024, 0);
    udpCommunicationInit(&self->communication);
    self->firstClient.receiveMask = 0;
    self->firstClient.waitingForStepId = 0;
}

int serverSerializeIn(Server* self)
{
    CLOG_DEBUG("--- server serialize in ---");

    uint8_t temp[1200];
    size_t octetRead = udpBufferReceive(&self->communication.in, temp, 1200);
    if (octetRead == 0) {
        CLOG_DEBUG("--- server serialize in encountered nothing =====");
        return 0;
    }
    FldInStream inStream;
    fldInStreamInit(&inStream, temp, octetRead);

    ServerReceiver* firstReceiver = &self->firstClient;

    nbsPendingStepsInSerializeHeader(&inStream, &firstReceiver->waitingForStepId, &firstReceiver->receiveMask);
    CLOG_INFO("client is waiting for %d and receive status %016lX", firstReceiver->waitingForStepId,
              firstReceiver->receiveMask);

    uint8_t defaultStepBuffer[1024];

    NimbleStepsOutSerializeLocalParticipants participants;
    participants.participants[0].payload = (const uint8_t*) "I have no idea";
    participants.participants[0].payloadCount = 14;
    participants.participants[0].participantIndex = 2;
    participants.participants[0].connectState = NimbleSerializeStepTypeNormal;
    participants.participantCount = 1;

    int defaultStepBufferOctetCount = nbsStepsOutSerializeCombinedStep(&participants, defaultStepBuffer, 1024);
    if (defaultStepBufferOctetCount < 0) {
        return defaultStepBufferOctetCount;
    }

    StepId firstStepIdRead;
    size_t stepsThatFollow;
    nbsStepsInSerializeHeader(&inStream, &firstStepIdRead, &stepsThatFollow);
    size_t droppedSteps = nbsStepsDropped(&self->firstClientPredictedSteps, firstStepIdRead);
    if (droppedSteps > 0) {
        CLOG_INFO("steps dropped %zu", droppedSteps);
        StepId stepId = self->firstClientPredictedSteps.expectedWriteId;
        for (size_t i = 0; i < droppedSteps; ++i) {
            int errorCode = nbsStepsWrite(&self->firstClientPredictedSteps, stepId, defaultStepBuffer,
                                          defaultStepBufferOctetCount);
            if (errorCode < 0) {
                return errorCode;
            }
            stepId++;
        }
    }
    int errorCode = nbsStepsInSerialize(&inStream, &self->firstClientPredictedSteps, firstStepIdRead, stepsThatFollow);
    if (errorCode < 0) {
        CLOG_WARN("could not in serialize on server %d", errorCode);
        return errorCode;
    }
    nbsStepsDebugOutput(&self->firstClientPredictedSteps, "received on server from client", 1);

    while (self->firstClientPredictedSteps.expectedWriteId > self->authoritativeSteps.expectedWriteId) {
        uint8_t temp[64];
        StepId foundId;
        int readOctetSize = nbsStepsRead(&self->firstClientPredictedSteps, &foundId, temp, 64);
        if (readOctetSize < 0) {
            CLOG_WARN("could not nbsStepsRead on server %d", readOctetSize);
            return readOctetSize;
        }
        int worked = nbsStepsWrite(&self->authoritativeSteps, foundId, temp, readOctetSize);
        if (worked < 0) {
            CLOG_WARN("could not nbsStepsWrite back on server %d", worked);
            return worked;
        }
    }

    nbsStepsDebugOutput(&self->authoritativeSteps, "authoritative steps on server", 1);

    return 0;
}

int serverSerializeOut(Server* self)
{
    FldOutStream outStream;
    uint8_t temp[1200];
    CLOG_DEBUG("--- server serialize OUT ---");
    fldOutStreamInit(&outStream, temp, 1200);

    ServerReceiver* client = &self->firstClient;
#define MAX_RANGES (4)

    NbsPendingRange ranges[MAX_RANGES];

#define MAX_STEPS (8)

    // CLOG_INFO("creating range from %d %016lX", client->waitingForStepId, client->receiveMask);
    int rangeCount = nbsPendingStepsRanges(client->waitingForStepId, 0, client->receiveMask, ranges, MAX_RANGES,
                                           MAX_STEPS);
    if (rangeCount < 0) {
        CLOG_SOFT_ERROR("problem with ranges");
        return rangeCount;
    }

    if (self->authoritativeSteps.expectedWriteId > client->waitingForStepId) {
        ranges[rangeCount].startId = client->waitingForStepId;
        ranges[rangeCount].count = self->authoritativeSteps.expectedWriteId - client->waitingForStepId;
        CLOG_INFO("force add a range %d %zu", ranges[rangeCount].startId, ranges[rangeCount].count);
        rangeCount++;
    } else {
        CLOG_INFO("no need to force any range. we have %016X and client wants %016X",
                  self->authoritativeSteps.expectedWriteId - 1, client->waitingForStepId);
    }

    nbsPendingStepsRangesDebugOutput(ranges, "server serialize out", rangeCount);

    nbsPendingStepsSerializeOutRanges(&outStream, &self->authoritativeSteps, ranges, rangeCount);

    udpBufferSend(&self->communication.out, temp, outStream.pos);

    return 0;
}

void clientInit(Client* self, struct ImprintAllocatorWithFree* allocatorWithFree)
{
    udpCommunicationInit(&self->communication);
    nbsPendingStepsInit(&self->pendingAuthoritativeSteps, 0, allocatorWithFree);
    nbsStepsInit(&self->predictedSteps, 1024, 0);
    self->startStepIdToSend = 0;

    addExampleSteps(&self->predictedSteps, 0);
}

int clientSerializeInOnePacket(Client* self)
{

    uint8_t temp[1200];
    size_t octetRead = udpBufferReceive(&self->communication.in, temp, 1200);
    if (octetRead == 0) {
        // CLOG_DEBUG("no more udp packets");
        return 0;
    }

    FldInStream inStream;
    fldInStreamInit(&inStream, temp, octetRead);

    nbsPendingStepsInSerialize(&inStream, &self->pendingAuthoritativeSteps);

    StepId lastReceivedIdFromServer = self->pendingAuthoritativeSteps.readId;
    CLOG_INFO(" client: now I'm waiting for %016X", lastReceivedIdFromServer);

    nbsPendingStepsDebugOutput(&self->pendingAuthoritativeSteps, "authoritative steps on client", 0);

    return 1;
}

int clientSerializeIn(Client* self)
{
    CLOG_INFO("=== client serialize in ===");
    while (1) {
        int processCount = clientSerializeInOnePacket(self);
        if (processCount < 1) {
            return processCount;
        }
    }
}

int clientSerializeOut(Client* self)
{
    CLOG_INFO("=== client serialize out ===");
    uint8_t temp[1200];
    FldOutStream outStream;
    fldOutStreamInit(&outStream, temp, 1200);

    StepId lastReceivedIdFromServer = self->pendingAuthoritativeSteps.readId;
    StepId headId;
    uint64_t clientReceiveMask = nbsPendingStepsReceiveMask(&self->pendingAuthoritativeSteps, &headId);
    if (headId != lastReceivedIdFromServer) {
        return -2;
    }

    nbsPendingStepsSerializeOutHeader(&outStream, lastReceivedIdFromServer, clientReceiveMask);

    nbsStepsDebugOutput(&self->predictedSteps, "send predicted", 0);
    CLOG_INFO("client is sending steps from %016X", self->startStepIdToSend);

    nbsStepsOutSerialize(&outStream, self->startStepIdToSend, &self->predictedSteps);
    nbsStepsOutSerializeAdvanceIfNeeded(&self->startStepIdToSend, &self->predictedSteps);

    if (self->predictedSteps.expectedWriteId > self->startStepIdToSend + 3) {
        self->startStepIdToSend++;
    }

    udpBufferSend(&self->communication.out, temp, outStream.pos);

    return 0;
}

#include <imprint/default_setup.h>

int testSendReceiveServer()
{
    Server server;
    Client client;

    ImprintDefaultSetup imprintSetup;
    imprintDefaultSetupInit(&imprintSetup, 1024 * 1024);

    serverInit(&server);
    clientInit(&client, &imprintSetup.slabAllocator.info);

    for (size_t i = 0; i < 30; ++i) {
        if ((i % 2) == 0) {
            addExampleSteps(&client.predictedSteps, i + 2);
        }
        clientSerializeOut(&client);

        sendToServer(&server, &client);

        serverSerializeIn(&server);

        serverSerializeOut(&server);

        sendToClient(&client, &server);

        clientSerializeIn(&client);
    }

    return 0;
}

int main(int argc, char* argv[])
{
    g_clog.log = console_log;

    int errorCode = testSendReceiveServer();
    if (errorCode < 0) {
        CLOG_ERROR("errorcode:%d", errorCode);
    }

    CLOG_INFO("steps example done");
    return 0;
}
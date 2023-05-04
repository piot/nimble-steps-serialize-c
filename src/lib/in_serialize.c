/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <flood/in_stream.h>
#include <nimble-steps-serialize/in_serialize.h>

int nbsStepsInSerializeHeader(FldInStream* stream, StepId* firstStep, size_t* stepsThatFollow)
{
    uint32_t firstStepIdValue;
    fldInStreamReadUInt32(stream, &firstStepIdValue);

    uint8_t stepsThatFollowValue;
    fldInStreamReadUInt8(stream, &stepsThatFollowValue);

    *firstStep = firstStepIdValue;
    *stepsThatFollow = stepsThatFollowValue;

    return 0;
}

size_t nbsStepsDropped(const NbsSteps* self, StepId firstReadStepId)
{
    if (firstReadStepId > self->expectedWriteId) {
        return firstReadStepId - self->expectedWriteId;
    }

    return 0;
}

int nbsStepsInSerialize(FldInStream* stream, NbsSteps* target, StepId firstStepId, size_t stepsThatFollow)
{
    uint8_t buf[1024];
    uint8_t stepOctetCount;
    size_t addedSteps = 0;

    CLOG_C_VERBOSE(&target->log, "stepsInSerialize firstStep %08X count %zu", firstStepId, stepsThatFollow);

    StepId lastIncludedStepIdInStream = firstStepId + stepsThatFollow - 1;
    if (lastIncludedStepIdInStream < target->expectedWriteId) {
        CLOG_C_NOTICE(&target->log, "stepsInSerialize: old steps. last from client request is %08X and the incoming buffer is waiting for %08X", lastIncludedStepIdInStream,
                    target->expectedWriteId)
    }

    for (size_t i = 0; i < stepsThatFollow; ++i) {
        fldInStreamReadUInt8(stream, &stepOctetCount);
        fldInStreamReadOctets(stream, buf, stepOctetCount);
        StepId deserializedStepId = firstStepId + i;
        if (deserializedStepId > target->expectedWriteId) {
            CLOG_EXECUTE(StepId expectedNext = target->expectedWriteId;)
            CLOG_EXECUTE(size_t missingStepCount = deserializedStepId - expectedNext;)
            CLOG_VERBOSE("dropped %zu counts, filling them with default", missingStepCount);
            return -44;
        } else if (deserializedStepId < target->expectedWriteId) {
            // CLOG_VERBOSE("waiting for %08X but received %d hopefully coming later in stream",
            // target->expectedWriteId, stepId);
            continue;
        } else {
            // CLOG_VERBOSE("got exactly what I was waiting for: %d", stepId);
                CLOG_C_VERBOSE(&target->log, "received client step %08X action %d", deserializedStepId, buf[3]);
        }

        if (target->stepsCount < NBS_WINDOW_SIZE / 2) {
            if (discoidBufferWriteAvailable(&target->stepsData) <= stepOctetCount) {
                CLOG_WARN("step buffer count is not full, but the step-data is. %zu left, but needed to write %d",
                          discoidBufferWriteAvailable(&target->stepsData), stepOctetCount)
                return -55;
            }
            int errorCode = nbsStepsWrite(target, deserializedStepId, buf, stepOctetCount);
            if (errorCode < 0) {
                return errorCode;
            }

            addedSteps++;
        } else {
            CLOG_WARN("step buffer is full %zu step count out of %d", target->stepsCount, NBS_WINDOW_SIZE / 2);
        }
    }

    return (int) addedSteps;
}

static int participantsFindDuplicate(NimbleStepsOutSerializeLocalParticipant* participants, size_t count,
                                     uint8_t participantId)
{
    for (size_t i = 0; i < count; ++i) {
        NimbleStepsOutSerializeLocalParticipant* participant = &participants[i];
        if (participant->participantId == participantId) {
            return i;
        }
    }

    return -1;
}

int nbsStepsInSerializeAuthoritativeStep(NimbleStepsOutSerializeLocalParticipants* participants, FldInStream* stream)
{
    uint8_t participantCountValue;
    int errorCode = fldInStreamReadUInt8(stream, &participantCountValue);
    if (errorCode < 0) {
        return errorCode;
    }

    participants->participantCount = participantCountValue;
    if (participants->participantCount > 8) {
        CLOG_SOFT_ERROR("participant count is over max: %zu", participants->participantCount);
        return -4;
    }

    for (size_t i = 0; i < participants->participantCount; ++i) {
        NimbleStepsOutSerializeLocalParticipant* participant = &participants->participants[i];

        fldInStreamReadUInt8(stream, &participant->participantId);
        uint8_t payloadCountValue;
        fldInStreamReadUInt8(stream, &payloadCountValue);
#if 1
        int index = participantsFindDuplicate(participants->participants, i, participant->participantId);
        if (index >= 0) {
            CLOG_ERROR("Problem with duplicate %d", index);
        }
#endif

        participant->payloadCount = payloadCountValue;
        participant->payload = stream->p;
        stream->p += participant->payloadCount;
        // fldInStreamReadOctets(stream, (uint8_t*) participant->payload, participant->payloadCount);
    }

    return stream->pos;
}

int nbsStepsInSerializeAuthoritativeStepHelper(NimbleStepsOutSerializeLocalParticipants* participants,
                                               const uint8_t* stepBuf, size_t octetCount)
{
    FldInStream stepStream;
    fldInStreamInit(&stepStream, stepBuf, octetCount);

    return nbsStepsInSerializeAuthoritativeStep(participants, &stepStream);
}

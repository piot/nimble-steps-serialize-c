/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <flood/in_stream.h>
#include <nimble-steps-serialize/in_serialize.h>


/// Reads steps from the steam and inserts into the target steps buffer
/// @param stream in stream
/// @param target steps target to fill from stream
/// @param firstStepId firstStepId in stream
/// @param stepsThatFollow how many steps that follow
/// @return negative on error
int nbsStepsInSerialize(FldInStream* stream, NbsSteps* target, StepId firstStepId, size_t stepsThatFollow)
{
    uint8_t buf[1024];
    uint8_t stepOctetCount;
    size_t addedSteps = 0;

    CLOG_C_VERBOSE(&target->log, "stepsInSerialize firstStep %08X count %zu", firstStepId, stepsThatFollow)

    StepId lastIncludedStepIdInStream = (StepId) (firstStepId + stepsThatFollow - 1);
    if (lastIncludedStepIdInStream < target->expectedWriteId) {
        CLOG_C_VERBOSE(
            &target->log,
            "stepsInSerialize: old steps. last from client request is %08X and the incoming buffer is waiting for %08X",
            lastIncludedStepIdInStream, target->expectedWriteId)
    }

    for (size_t i = 0; i < stepsThatFollow; ++i) {
        fldInStreamReadUInt8(stream, &stepOctetCount);
        fldInStreamReadOctets(stream, buf, stepOctetCount);
        StepId deserializedStepId = (StepId) (firstStepId + i);
        if (deserializedStepId > target->expectedWriteId) {
            CLOG_EXECUTE(StepId expectedNext = target->expectedWriteId;)
            CLOG_EXECUTE(size_t missingStepCount = deserializedStepId - expectedNext;)
            CLOG_C_NOTICE(&target->log,
                          "resetting incoming prediction buffer. expected %08X, but received %08X. skipping %zu",
                          expectedNext, deserializedStepId, missingStepCount)
            nbsStepsReInit(target, deserializedStepId);
        } else if (deserializedStepId < target->expectedWriteId) {
            // CLOG_VERBOSE("waiting for %08X but received %d hopefully coming later in stream",
            // target->expectedWriteId, stepId);
            continue;
        } else {
            // CLOG_VERBOSE("got exactly what I was waiting for: %d", stepId);
            CLOG_C_VERBOSE(&target->log, "received client step %08X game specific action %d", deserializedStepId,
                           buf[3])
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
            CLOG_WARN("step buffer is full %zu step count out of %d", target->stepsCount, NBS_WINDOW_SIZE / 2)
        }
    }

    return (int) addedSteps;
}

/// Reads a single predicted step.
///
/// Predicted step is just a stepOctetCount and octets.
/// @param stream to read from
/// @param deserializedStepId stepId that the stream should be stored in
/// @param target target step buffer
/// @return number of steps added, negative on error
int nbsStepsInSerializeSinglePredictedStep(FldInStream* stream, StepId deserializedStepId, NbsSteps* target)
{
    uint8_t buf[1024];
    uint8_t stepOctetCount;

    fldInStreamReadUInt8(stream, &stepOctetCount);
    fldInStreamReadOctets(stream, buf, stepOctetCount);
    if (deserializedStepId > target->expectedWriteId) {
        CLOG_EXECUTE(StepId expectedNext = target->expectedWriteId;)
        CLOG_EXECUTE(size_t missingStepCount = deserializedStepId - expectedNext;)
        CLOG_C_NOTICE(&target->log,
                      "resetting incoming prediction buffer. expected %08X, but received %08X. skipping %zu",
                      expectedNext, deserializedStepId, missingStepCount)
        nbsStepsReInit(target, deserializedStepId);
    } else if (deserializedStepId < target->expectedWriteId) {

        return 0;
    } else {
        // CLOG_VERBOSE("got exactly what I was waiting for: %d", stepId);
        CLOG_C_VERBOSE(&target->log, "received client step %08X game specific action %d", deserializedStepId, buf[0])
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

    } else {
        CLOG_WARN("step buffer is full %zu step count out of %d", target->stepsCount, NBS_WINDOW_SIZE / 2)
    }

    return 1;
}

static int participantsFindDuplicate(NimbleStepsOutSerializeLocalParticipant* participants, size_t count,
                                     uint8_t participantId)
{
    for (size_t i = 0U; i < count; ++i) {
        NimbleStepsOutSerializeLocalParticipant* participant = &participants[i];
        if (participant->participantId == participantId) {
            return (int) i;
        }
    }

    return -1;
}

/// Reads combined steps from the stream and returns the parts for each participant
/// @param participants the target for the information about each participant step
/// @param stream in stream
/// @return negative on error
int nbsStepsInSerializeStepsForParticipants(NimbleStepsOutSerializeLocalParticipants* participants, FldInStream* stream)
{
    uint8_t participantCountValue;
    int errorCode = fldInStreamReadUInt8(stream, &participantCountValue);
    if (errorCode < 0) {
        return errorCode;
    }

    participants->participantCount = participantCountValue;
    if (participants->participantCount > 8) {
        CLOG_SOFT_ERROR("participant count is over max: %zu", participants->participantCount)
        return -4;
    }

    for (size_t i = 0; i < participants->participantCount; ++i) {
        NimbleStepsOutSerializeLocalParticipant* participant = &participants->participants[i];

        fldInStreamReadUInt8(stream, &participant->participantId);
        bool hasMask = participant->participantId & 0x80;
        if (hasMask) {
            uint8_t stepType;
            fldInStreamCheckMarker(stream, 0xbd);
            fldInStreamReadUInt8(stream, &stepType);
            participant->stepType = (NimbleSerializeStepType) stepType;
            participant->participantId = participant->participantId & 0x7F;
            participant->payloadCount = 0;
            participant->payload = 0;
            switch (participant->stepType) {
                case NimbleSerializeStepTypeNormal:
                    break;
                case NimbleSerializeStepTypeStepNotProvidedInTime:
                    break;
                case NimbleSerializeStepTypeWaitingForReJoin:
                    break;
                case NimbleSerializeStepTypeJoined:
                    fldInStreamReadUInt8(stream, &participant->localPartyId);
                    break;
                case NimbleSerializeStepTypeLeft:
                    break;
            }
        } else {
            uint8_t payloadCountValue;
            fldInStreamReadUInt8(stream, &payloadCountValue);
#if 1
            int index = participantsFindDuplicate(participants->participants, i, participant->participantId);
            if (index >= 0) {
                CLOG_ERROR("Problem with duplicate %d", index)
            }
#endif

            participant->stepType = NimbleSerializeStepTypeNormal;
            participant->payloadCount = payloadCountValue;
            participant->payload = stream->p;
            stream->p += participant->payloadCount;
        }
    }

    return (int) stream->pos;
}

/// Reads combined steps from the octets and returns the parts for each participant
/// @param participants local participants
/// @param stepBuf buffer to read from
/// @param octetCount number of octets in stepBuf
/// @return negative on failure
int nbsStepsInSerializeStepsForParticipantsFromOctets(NimbleStepsOutSerializeLocalParticipants* participants,
                                                      const uint8_t* stepBuf, size_t octetCount)
{
    FldInStream stepStream;
    fldInStreamInit(&stepStream, stepBuf, octetCount);

    return nbsStepsInSerializeStepsForParticipants(participants, &stepStream);
}

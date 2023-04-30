/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <flood/in_stream.h>
#include <flood/out_stream.h>
#include <nimble-steps-serialize/out_serialize.h>

static int nimbleOutSerializeStep(FldOutStream* stream, const uint8_t* payload, size_t octetCount)
{
    int errorCode = fldOutStreamWriteUInt8(stream, octetCount);
    if (errorCode < 0) {
        return errorCode;
    }

    errorCode = fldOutStreamWriteOctets(stream, payload, octetCount);
    if (errorCode < 0) {
        return errorCode;
    }

#ifdef CONFIGURATION_DEBUG
    int verify = nbsStepsVerifyStep(payload, octetCount);
    if (verify < 0) {
        CLOG_ERROR("step is wrong");
        return verify;
    }
#endif
    return 0;
}

int nbsStepsOutSerializeFixedCountNoHeader(struct FldOutStream* stream, StepId startStepId, size_t redundancyCount,
                                           const NbsSteps* steps)
{
    StepId stepIdToWrite = startStepId;
    uint8_t tempBuf[1024];

    if (redundancyCount == 0) {
        return 0;
    }

    // CLOG_INFO("stepId: %08X redundancyCount:%d storedCount:%d", startStepId, redundancyCount, steps->stepsCount);
    for (size_t i = 0; i < redundancyCount; ++i) {
        int index = nbsStepsGetIndexForStep(steps, stepIdToWrite);
        if (index < 0) {
            return index;
        }
        int octetsCountInStep = nbsStepsReadAtIndex(steps, index, tempBuf, 1024);
        if (octetsCountInStep < 0) {
            CLOG_WARN("could not read steps");
            return octetsCountInStep;
        }

#if 0
        if (tempBuf[3] != 0) {
            CLOG_VERBOSE("serialize out step %08X, action %d to packet", stepIdToWrite, tempBuf[3]);
        }
        // CLOG_VERBOSE("serialize out %08X", stepIdToWrite);
#endif
        nimbleOutSerializeStep(stream, tempBuf, octetsCountInStep);
        stepIdToWrite++;
    }

    return redundancyCount;
}

int nbsStepsOutSerializeFixedCount(struct FldOutStream* stream, StepId startStepId, size_t redundancyCount,
                                   const NbsSteps* steps)
{
    StepId stepIdToWrite = startStepId;

    fldOutStreamWriteUInt32(stream, stepIdToWrite);
    fldOutStreamWriteUInt8(stream, redundancyCount);

    return nbsStepsOutSerializeFixedCountNoHeader(stream, startStepId, redundancyCount, steps);
}

int nbsStepsOutSerialize(struct FldOutStream* stream, StepId startStepId, const NbsSteps* steps)
{
    size_t redundancyCount;
    StepId lastAvailableId = steps->expectedReadId + steps->stepsCount - 1;

    if (steps->stepsCount == 0 || startStepId > lastAvailableId) {
        redundancyCount = 0;
    } else {
        redundancyCount = lastAvailableId - startStepId + 1;
    }
    if (redundancyCount > NimbleSerializeMaxRedundancyCount) {
        redundancyCount = NimbleSerializeMaxRedundancyCount;
    }

    return nbsStepsOutSerializeFixedCount(stream, startStepId, redundancyCount, steps);
}

int nbsStepsOutSerializeCalculateCombinedSize(size_t participantCount, size_t singleParticipantStepOctetCount)
{
    const int fixedHeaderSize = 1;            // participantCount
    const int overheadForEachParticipant = 2; // index and payloadcount

    return fixedHeaderSize + participantCount * overheadForEachParticipant +
           participantCount * singleParticipantStepOctetCount;
}

int nbsStepsOutSerializeAdvanceIfNeeded(StepId* startStepId, const NbsSteps* steps)
{
    if (steps->stepsCount == 0) {
        CLOG_WARN("no steps in buffer, so I wont advance")
        return 0;
    }
    StepId lastAvailableId = steps->expectedReadId + steps->stepsCount - 1;
    if (*startStepId > lastAvailableId) {
        CLOG_SOFT_ERROR(
            "nbsStepsOutSerializeAdvanceIfNeeded: startStepId is after the last thing I know here. %08X last: %08X",
            *startStepId, lastAvailableId);
        return -2;
    }
    size_t leftInBufferCount = lastAvailableId - *startStepId + 1;
    if (leftInBufferCount > 3) {
        *startStepId = *startStepId + 1;
    }
    return 0;
}

/// Serializes a Step for one or more participants with a header
/// Format is [ParticipantCount] [ [LocalIndex] [PayloadCount] [Payload] ].
/// @param participants
/// @param stepBuf
/// @param maxCount
/// @return
int nbsStepsOutSerializeStep(const NimbleStepsOutSerializeLocalParticipants* participants, uint8_t* stepBuf,
                             size_t maxCount)
{
    if (participants->participantCount == 0) {
        CLOG_ERROR("can not serialize steps with no participants");
    }
    FldOutStream stepStream;
    fldOutStreamInit(&stepStream, stepBuf, maxCount);
    fldOutStreamWriteUInt8(&stepStream, participants->participantCount);
    for (size_t i = 0; i < participants->participantCount; ++i) {
        const NimbleStepsOutSerializeLocalParticipant* participant = &participants->participants[i];
        if (participant->participantIndex == 0) {
            CLOG_ERROR("participantId zero is reserved. OutSerializeStep")
        }
        fldOutStreamWriteUInt8(&stepStream, participant->participantIndex);
        fldOutStreamWriteUInt8(&stepStream, participant->payloadCount);
        fldOutStreamWriteOctets(&stepStream, participant->payload, participant->payloadCount);
    }

    return stepStream.pos;
}

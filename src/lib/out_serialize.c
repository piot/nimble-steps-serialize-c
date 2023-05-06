/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <flood/in_stream.h>
#include <flood/out_stream.h>
#include <nimble-steps-serialize/out_serialize.h>

static int nbsStepsOutSerializeCombinedStep(FldOutStream* stream, const uint8_t* payload, size_t octetCount)
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

/// Writes combined steps to the stream starting with specified start TickId
/// @param stream the target octet stream
/// @param startStepId which stepId to start with
/// @param stepCount the number of steps to write
/// @param steps the target buffer to write the steps to
/// @return negative on error
int nbsStepsOutSerializeFixedCountNoHeader(struct FldOutStream* stream, StepId startStepId, size_t stepCount,
                                           const NbsSteps* steps)
{
    StepId stepIdToWrite = startStepId;
    uint8_t tempBuf[1024];

    if (stepCount == 0) {
        return 0;
    }

    // CLOG_INFO("stepId: %08X stepCount:%d storedCount:%d", startStepId, stepCount, steps->stepsCount);
    for (size_t i = 0; i < stepCount; ++i) {
        int index = nbsStepsGetIndexForStep(steps, stepIdToWrite);
        if (index < 0) {
            CLOG_SOFT_ERROR("could not get index for stepId %08X", stepIdToWrite)
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
        nbsStepsOutSerializeCombinedStep(stream, tempBuf, octetsCountInStep);
        stepIdToWrite++;
    }

    return stepCount;
}

static int nbsStepsOutSerializeFixedCount(struct FldOutStream* stream, StepId startStepId, size_t redundancyCount,
                                          const NbsSteps* steps)
{
    StepId stepIdToWrite = startStepId;

    fldOutStreamWriteUInt32(stream, stepIdToWrite);
    fldOutStreamWriteUInt8(stream, redundancyCount);

    return nbsStepsOutSerializeFixedCountNoHeader(stream, startStepId, redundancyCount, steps);
}

/// Writes steps to the octet stream up to a redundancy count
/// If the number of steps in the buffer is smaller or equal to redundancy count, it starts with the first on available
/// for reading. Otherwise it sends the last redundancy count in the buffer.
/// @param stream
/// @param steps
/// @return
int nbsStepsOutSerialize(struct FldOutStream* stream, const NbsSteps* steps)
{
    size_t redundancyCount = steps->stepsCount;
    StepId firstStepId = steps->expectedReadId;

    if (redundancyCount > NimbleSerializeMaxRedundancyCount) {
        redundancyCount = NimbleSerializeMaxRedundancyCount;
        StepId lastAvailableId = steps->expectedReadId + steps->stepsCount - 1;
        firstStepId = lastAvailableId - redundancyCount + 1;
    }

    return nbsStepsOutSerializeFixedCount(stream, firstStepId, redundancyCount, steps);
}

/// Calculates the serialization overhead for the number of participants.
/// @param participantCount
/// @param singleParticipantStepOctetCount
/// @return
int nbsStepsOutSerializeCalculateCombinedSize(size_t participantCount, size_t singleParticipantStepOctetCount)
{
    const int fixedHeaderSize = 1;            // participantCount
    const int overheadForEachParticipant = 2; // index and payloadcount

    return fixedHeaderSize + participantCount * overheadForEachParticipant +
           participantCount * singleParticipantStepOctetCount;
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
        if (participant->participantId == 0) {
            CLOG_ERROR("participantId zero is reserved. OutSerializeStep")
        }
        if (participant->participantId > 32) {
            CLOG_ERROR("too high participant id")
        }
        fldOutStreamWriteUInt8(&stepStream, participant->participantId);
        fldOutStreamWriteUInt8(&stepStream, participant->payloadCount);
        fldOutStreamWriteOctets(&stepStream, participant->payload, participant->payloadCount);
    }

    return stepStream.pos;
}

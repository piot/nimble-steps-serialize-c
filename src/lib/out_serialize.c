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
    int errorCode = fldOutStreamWriteUInt8(stream, (uint8_t) octetCount);
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
        CLOG_ERROR("step is wrong")
        // return verify;
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
ssize_t nbsStepsOutSerializeFixedCountNoHeader(struct FldOutStream* stream, StepId startStepId, size_t stepCount,
                                           const NbsSteps* steps)
{
    StepId stepIdToWrite = startStepId;
    uint8_t tempBuf[1024];

    if (stepCount == 0) {
        return 0;
    }

    size_t stepCountWritten = 0;

    // CLOG_INFO("stepId: %08X stepCount:%d storedCount:%d", startStepId, stepCount, steps->stepsCount);
    for (size_t i = 0; i < stepCount; ++i) {
        int index = nbsStepsGetIndexForStep(steps, stepIdToWrite);
        if (index < 0) {
            CLOG_SOFT_ERROR("could not get index for stepId %08X", stepIdToWrite)
            return index;
        }
        int octetsCountInStep = nbsStepsReadAtIndex(steps, index, tempBuf, 1024);
        if (octetsCountInStep < 0) {
            CLOG_WARN("could not read steps")
            return octetsCountInStep;
        }

        if (stream->pos + (size_t) octetsCountInStep >= stream->size) {
            return (ssize_t) stepCountWritten;
        }

        int serializeResult = nbsStepsOutSerializeCombinedStep(stream, tempBuf, (size_t) octetsCountInStep);
        if (serializeResult < 0) {
            return serializeResult;
        }

        stepIdToWrite++;
        stepCountWritten++;
    }

    return (ssize_t) stepCountWritten;
}

static ssize_t nbsStepsOutSerializeFixedCount(struct FldOutStream* stream, StepId startStepId, size_t redundancyCount,
                                          const NbsSteps* steps)
{
    StepId stepIdToWrite = startStepId;

    fldOutStreamWriteUInt32(stream, stepIdToWrite);

    FldOutStreamStoredPosition numberOfStepsPosition = fldOutStreamTell(stream);

    fldOutStreamWriteUInt8(stream, 0); // Will be filled in later

    CLOG_ASSERT(redundancyCount < 255, "redundancy count is too high %zu", redundancyCount)

    ssize_t numberOfStepsWritten = nbsStepsOutSerializeFixedCountNoHeader(stream, startStepId, redundancyCount, steps);
    if (numberOfStepsWritten < 0) {
        return numberOfStepsWritten;
    }

    FldOutStreamStoredPosition positionAfterWrittenSteps = fldOutStreamTell(stream);

    fldOutStreamSeek(stream, numberOfStepsPosition);

    fldOutStreamWriteUInt8(stream, (uint8_t) numberOfStepsWritten);

    fldOutStreamSeek(stream, positionAfterWrittenSteps);

    return numberOfStepsWritten;
}

/// Writes steps to the octet stream up to a redundancy count
/// If the number of steps in the buffer is smaller or equal to redundancy count, it starts with the first on available
/// for reading. Otherwise it sends the last redundancy count in the buffer.
/// @param stream out stream
/// @param steps the steps collection to serialize
/// @return negative on error, otherwise the number of steps serialized
ssize_t nbsStepsOutSerialize(struct FldOutStream* stream, const NbsSteps* steps)
{
    size_t maxNumberOfStepsInPacket = steps->stepsCount;
    StepId firstStepId = steps->expectedReadId;

/*
    if (maxNumberOfStepsInPacket > NimbleSerializeMaxmaxNumberOfStepsInPacket) {
        maxNumberOfStepsInPacket = NimbleSerializeMaxmaxNumberOfStepsInPacket;
        StepId lastAvailableId = (StepId) (steps->expectedReadId + steps->stepsCount - 1);
        firstStepId = (StepId) (lastAvailableId - maxNumberOfStepsInPacket + 1);
    }
    */

    return nbsStepsOutSerializeFixedCount(stream, firstStepId, maxNumberOfStepsInPacket, steps);
}

/// Calculates the serialization overhead for the number of participants.
/// @param participantCount number of participant input included
/// @param singleParticipantStepOctetCount octetCount for each participant
/// @return size
size_t nbsStepsOutSerializeCalculateCombinedSize(size_t participantCount, size_t singleParticipantStepOctetCount)
{
    const size_t fixedHeaderSize = 1;            // participantCount
    const size_t overheadForEachParticipant = 2; // index and payloadcount

    return fixedHeaderSize + participantCount * overheadForEachParticipant +
           participantCount * singleParticipantStepOctetCount;
}

/// Serializes a Step for one or more participants with a header
/// Format is [ParticipantCount] [ [LocalIndex] [PayloadCount] [Payload] ].
/// @param participants participants
/// @param stepBuf target step buffer
/// @param maxCount maximum count to fill
/// @return number of octets written or error
ssize_t nbsStepsOutSerializeStep(const NimbleStepsOutSerializeLocalParticipants* participants, uint8_t* stepBuf,
                                 size_t maxCount)
{
    if (participants->participantCount == 0) {
        CLOG_ERROR("can not serialize steps with no participants")
        // return -84;
    }
    FldOutStream stepStream;
    fldOutStreamInit(&stepStream, stepBuf, maxCount);
    fldOutStreamWriteUInt8(&stepStream, (uint8_t) participants->participantCount);
    for (size_t i = 0; i < participants->participantCount; ++i) {
        const NimbleStepsOutSerializeLocalParticipant* participant = &participants->participants[i];
        if (participant->participantId == 0) {
            CLOG_ERROR("participantId zero is reserved. OutSerializeStep")
        }
        if (participant->participantId > 32) {
            CLOG_ERROR("too high participant id")
        }

        uint8_t mask = participant->connectState == NimbleSerializeParticipantConnectStateNormal ? 0x00 : 0x80;

        fldOutStreamWriteUInt8(&stepStream, mask | participant->participantId);
        if (mask) {
            //CLOG_VERBOSE("serialize with mask: connectState: %d", participant->connectState)
            fldOutStreamWriteMarker(&stepStream, 0xbd);
            fldOutStreamWriteUInt8(&stepStream, (uint8_t) participant->connectState);
        } else {
            //CLOG_VERBOSE("serialize without mask")
            if (participant->payload == 0 || participant->payloadCount == 0) {
                CLOG_ERROR("illegal participant payload to out-serialize")
            }
            fldOutStreamWriteUInt8(&stepStream, (uint8_t) participant->payloadCount);
            fldOutStreamWriteOctets(&stepStream, participant->payload, participant->payloadCount);
        }
    }

    return (ssize_t) stepStream.pos;
}

/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <flood/out_stream.h>
#include <nimble-steps-serialize/out_serialize.h>
#include <nimble-steps-serialize/pending_out_serialize.h>
#include <nimble-steps/pending_steps.h>

/// Writes receive status into the octet stream
/// Typically used on the client to notify the server of the received pending steps
/// @param stream out stream
/// @param latestStepId latest received stepId
/// @param receiveMask the receive mask to send to host
/// @return negative on error
int nbsPendingStepsSerializeOutHeader(FldOutStream* stream, StepId latestStepId, uint64_t receiveMask)
{
    fldOutStreamWriteUInt32(stream, latestStepId);
    fldOutStreamWriteUInt64(stream, receiveMask);

    return 0;
}

/// Writes ranges of steps into the octet stream
/// Used on the server to send ranges with steps that the client is missing.
/// @param stream out stream
/// @param steps steps collection
/// @param ranges ranges from steps collection to write to stream
/// @param rangeCount number of ranges in ranges
/// @return negative on error
ssize_t nbsPendingStepsSerializeOutRanges(FldOutStream* stream, const NbsSteps* steps, NbsPendingRange* ranges,
                                          size_t rangeCount)
{
    if (fldOutStreamRemainingOctets(stream) < 16) {
        return -1;
    }

    if (rangeCount == 0) {
        fldOutStreamWriteUInt32(stream, 0);
        fldOutStreamWriteUInt8(stream, (uint8_t) 0);
        return 0;
    }

    StepId referenceStepId = ranges[0].startId;

    StepId currentId = referenceStepId;

    fldOutStreamWriteUInt32(stream, referenceStepId);

    FldOutStreamStoredPosition numberOfRangesPosition = fldOutStreamTell(stream);

    fldOutStreamWriteUInt8(stream, (uint8_t) 0);

    size_t numberOfRangesWritten = 0;

    for (size_t i = 0; i < rangeCount; ++i) {
        const NbsPendingRange* range = &ranges[i];
        if (range->startId < currentId) {
            CLOG_SOFT_ERROR("startId can not be lower than currentId %u vs %u", range->startId, currentId)
            return -2;
        }

        if (fldOutStreamRemainingOctets(stream) < 8) {
            break;
        }
        StepId delta = range->startId - referenceStepId;
        fldOutStreamWriteUInt8(stream, (uint8_t) delta);
        // CLOG_INFO("out serialize range header: %08X count:%zu", range->startId, range->count);

        FldOutStreamStoredPosition rangeCountPosition = fldOutStreamTell(stream);

        fldOutStreamWriteUInt8(stream, 0);

        ssize_t writtenStepsInRange = nbsStepsOutSerializeFixedCountNoHeader(stream, range->startId, range->count,
                                                                             steps);
        if (writtenStepsInRange < 0) {
            CLOG_SOFT_ERROR("could not serialize with fixed count no header")
            return writtenStepsInRange;
        }

        FldOutStreamStoredPosition afterFixedCountPosition = fldOutStreamTell(stream);

        fldOutStreamSeek(stream, rangeCountPosition);
        fldOutStreamWriteUInt8(stream, (uint8_t) writtenStepsInRange);
        fldOutStreamSeek(stream, afterFixedCountPosition);

        numberOfRangesWritten++;

        if ((size_t) writtenStepsInRange < range->count) {
            break;
        }
        currentId = (StepId) (range->startId + range->count);
    }

    FldOutStreamStoredPosition afterAllWritesPosition = fldOutStreamTell(stream);

    fldOutStreamSeek(stream, numberOfRangesPosition);
    fldOutStreamWriteUInt8(stream, (uint8_t) numberOfRangesWritten);

    fldOutStreamSeek(stream, afterAllWritesPosition);

    return (ssize_t) numberOfRangesWritten;
}

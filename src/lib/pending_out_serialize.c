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
int nbsPendingStepsSerializeOutRanges(FldOutStream* stream, const NbsSteps* steps, NbsPendingRange* ranges,
                                      size_t rangeCount)
{
    StepId referenceStepId = 0;

    if (rangeCount > 0) {
        referenceStepId = ranges[0].startId;
    }

    StepId currentId = referenceStepId;

    fldOutStreamWriteUInt32(stream, referenceStepId);
    fldOutStreamWriteUInt8(stream, (uint8_t) rangeCount);

    for (size_t i = 0; i < rangeCount; ++i) {
        const NbsPendingRange* range = &ranges[i];
        if (range->startId < currentId) {
            CLOG_SOFT_ERROR("startId can not be lower than currentId %u vs %u", range->startId, currentId)
            return -2;
        }
        StepId delta = range->startId - referenceStepId;
        fldOutStreamWriteUInt8(stream, (uint8_t) delta);
        // CLOG_INFO("out serialize range header: %08X count:%zu", range->startId, range->count);
        fldOutStreamWriteUInt8(stream, (uint8_t) range->count);

        int errorCode = nbsStepsOutSerializeFixedCountNoHeader(stream, range->startId, range->count, steps);
        if (errorCode < 0) {
            CLOG_SOFT_ERROR("could not serialize with fixed count no header")
            return errorCode;
        }
        currentId = (StepId) (range->startId + range->count);
    }

    return 0;
}

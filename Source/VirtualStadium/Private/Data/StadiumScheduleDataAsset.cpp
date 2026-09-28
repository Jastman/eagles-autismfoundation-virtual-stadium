#include "Data/StadiumScheduleDataAsset.h"

int32 UStadiumScheduleDataAsset::GetStopCount() const
{
    return Stops.Num();
}

FStadiumScheduleStop UStadiumScheduleDataAsset::GetStopAt(const int32 Index) const
{
    return Stops.IsValidIndex(Index) ? Stops[Index] : FStadiumScheduleStop();
}


/*
 * pt100.c - PT100 resistance to temperature conversion
 *
 * Copyright (C) 2017 KUNBUS GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License (version 2) as
 * published by the Free Software Foundation.
 */

#include <linux/bug.h>
#include <linux/kernel.h>

static const u16 ai16uPtTable_l[] =
    {
#include <pt100_table.inc>
    };

int GetPt100Temperature(unsigned int i16uResistance_p,
			signed int *pi16sTemperature_p)
{
    int i16uLow_l = 0;
    int i16uHigh_l = ARRAY_SIZE(ai16uPtTable_l) - 1;
    int i16uSearchIndex_l;
    int i16uFullDiff_l;
    int i16uDiff_l;

    // resistance too low??
    if (i16uResistance_p < ai16uPtTable_l[i16uLow_l])
    {
        *pi16sTemperature_p = -2000;
        return -1;
    }

    // resistance too high??
    if (i16uResistance_p > ai16uPtTable_l[i16uHigh_l])
    {
        *pi16sTemperature_p = 8500;
        return 1;
    }

    // binary search in PT100 table
    i16uSearchIndex_l = (i16uLow_l + i16uHigh_l) / 2;
    while (i16uLow_l != i16uSearchIndex_l)
    {
        if (i16uResistance_p < ai16uPtTable_l[i16uSearchIndex_l])
        {
            i16uHigh_l = i16uSearchIndex_l;
        }
        else
        {
            i16uLow_l = i16uSearchIndex_l;
        }
        i16uSearchIndex_l = (i16uLow_l + i16uHigh_l) / 2;
    }

    // Interpolation
    i16uFullDiff_l = ai16uPtTable_l[i16uSearchIndex_l + 1] - ai16uPtTable_l[i16uSearchIndex_l];
    i16uDiff_l = i16uResistance_p - ai16uPtTable_l[i16uSearchIndex_l];
    *pi16sTemperature_p = (i16uSearchIndex_l - 200) * 10 + i16uDiff_l * 10 / i16uFullDiff_l;
    return 0;
}

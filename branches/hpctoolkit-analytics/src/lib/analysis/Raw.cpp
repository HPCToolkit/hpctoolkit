// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2013, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

//************************* System Include Files ****************************

#include <iostream>
#include <string>
using std::string;

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

//*************************** User Include Files ****************************

#include "Raw.hpp"
#include "Util.hpp"

#include <lib/prof/CallPath-Profile.hpp>
#include <lib/prof/Flat-ProfileData.hpp>

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>

#include <lib/support/diagnostics.h>
#include <vector>
#include <map>

//*************************** Forward Declarations ***************************

//****************************************************************************
using namespace std;

void
Analysis::Raw::writeAsText(/*destination,*/ const char* filenm)
{
    using namespace Analysis::Util;
    
    ProfType_t ty = getProfileType(filenm);
    if (ty == ProfType_Callpath) {
        writeAsText_callpath(filenm);
    }
    else if (ty == ProfType_CallpathMetricDB) {
        writeAsText_callpathMetricDB(filenm);
    }
    else if (ty == ProfType_CallpathTrace) {
        writeAsText_callpathTrace(filenm);
    }
    else if (ty == ProfType_Flat) {
        writeAsText_flat(filenm);
    }
    else {
        DIAG_Die(DIAG_Unimplemented);
    }
}

//*****************************************************************************
// interface operations
//*****************************************************************************



class intervals {
private:
    std::map<uint64_t, uint64_t> mymap;
public:
    void insert(uint64_t start, uint64_t end);
    uint64_t get_total();
};

uint64_t intervals::get_total(){
    uint64_t total = 0;
    for(map<uint64_t, uint64_t>::iterator myIter=mymap.begin(); myIter != mymap.end(); myIter++ ){
        total += (*myIter).second - (*myIter).first;
    }
    return total;
}

//-----------------------------------------------------------------------------
// Method insert:
//
//   insert [start,end) into the interval set. merge with overlapping in the
//   set to maintain the invariant that no point is contained in more
//   than one interval.
//-----------------------------------------------------------------------------
void
intervals::insert(uint64_t start, uint64_t end)
{
    if (start >= end) return;
    
    map<uint64_t, uint64_t>::iterator lb;
    map<uint64_t, uint64_t>::iterator ub;
    
    lb = mymap.upper_bound(start);
    ub = mymap.upper_bound(end);
    
    // inserted interval spans or overlaps one or more intervals if
    // iterators are not equal
    bool overlaps = (lb != ub);
    
    if (lb != mymap.begin()) {
        lb--; // move back one interval to see if it contains start
        if (start <= (*lb).second) {
            // start is within existing interval; adjust lower bound of union interval
            start = (*lb).first;
            overlaps = true;
        } else lb++; // lb doesn't contain start; thus, erasure shouldn't include lb
    }
    
    if (ub != mymap.begin()) {
        ub--; // move back one interval to see if it contains end
        if (end <= (*ub).second) {
            // end is within existing interval; adjust upper bound of union interval
            end = (*ub).second;
            overlaps = true;
        }
        ub++; // increment ub because erase will only remove up to but not ub
    }
    
    if (overlaps) {
        // remove any intervals that overlap the range being inserted. erase will
        // remove intervals starting at lb up to but not including ub.
        mymap.erase(lb,ub);
    }
    
    // insert the refined interval
    mymap[start] = end;
    
    return;
}

#define BLOCKED_THRESHOLD (10)

struct SampleInfo_t{
    uint64_t span;
    bool isBlocked;
    bool isAffectingSystem;
};

static vector<vector<uint64_t> > traces;
static vector<vector<SampleInfo_t> > traceSampleInfo;

struct TraceDataStats_t{
    string traceFileName;
    uint64_t minSampleSize;
    uint64_t maxSampleSize;
    uint64_t medianSampleSize;
    uint64_t numTotalSamples;
    uint64_t numBlockedSamples;
    uint64_t blockedTime;
    uint64_t numSystemAffectingBlockedSamples;
    uint64_t systemAffectingBlockedTime;
    uint64_t minTime;
    uint64_t maxTime;
};

struct OverallSampleStats_t{
    uint64_t startTime;
    uint64_t endTime;
    uint64_t simpleBlockedTime;
    uint64_t systemAffectingBlockedTime;
};

static  OverallSampleStats_t overallSampleStats;

static  vector<TraceDataStats_t> traceDataStats;

static  bool IsBlockedSample(const TraceDataStats_t & record, uint64_t span){
    return span > BLOCKED_THRESHOLD * record.medianSampleSize ? true: false;
}

static bool TraceHasExpectedSamples(int traceIndex, uint64_t startTime, uint64_t endTime, double frac) {
	uint64_t timeRange = endTime - startTime;
	uint64_t expectedSamples = timeRange / traceDataStats[traceIndex].medianSampleSize;
	// binary search of the startTime since traces are sorted.
	int64_t left = 0;
	int64_t right = traces[traceIndex].size();
    int64_t mid = (right + left) / 2;
	while (left <= right){
		mid = (right+left)/2;
		if ( traces[traceIndex][mid] == startTime )
			break;
		if ( traces[traceIndex][mid] < startTime ) {
			left = mid + 1;
		} else {
			right = mid - 1;
		}
		
	}
    
	// Search how many sampeles you have
	uint64_t numSamples = 0;
	for ( int64_t i = mid; i < traces[traceIndex].size(); i++) {
		numSamples ++;
		if ( traces[traceIndex][i] > endTime)
			break;
	}
    
	double samplePercentFromExpected = 100.0 * numSamples / expectedSamples;
    
	if ( samplePercentFromExpected < frac)
		return false;
	return true;
    
}

// If we see that > 50% of other traces are receiving samples normally, then we consider ourselves as
// blocking others. The rational is that if all threads are reciving few samples, perhaps all are
// blocked for some work to finish
static  bool IsBlockingSampleAffectingSystem(int traceIndex, uint64_t startTime, uint64_t endTime, int numTraces){
    uint32_t unblockedTraces = 0;
    for( int i = 0 ; i < numTraces; i++){
        if ( i == traceIndex)
            continue; // skip self
        // if trace i has more than 80% of expected samples, then it was not blocked.
        if( TraceHasExpectedSamples(i, startTime, endTime, 80) ){
            unblockedTraces++;
        }
    }
    double unblockedTracePercent = 100.0 * unblockedTraces / numTraces ;
    if ( unblockedTracePercent > 50 )
        return true;
    
    return false;
}


static  void analyzeSampleBlockingImpact(int numTraces){
    intervals intrvl;
    for( int i = 0 ; i < numTraces; i++){
        if (traces[i].size() < 2)
            continue;
        for (int j = 0 ; j < (int)traces[i].size() - 1; j++) {
            uint64_t span = traces[i][j+1] - traces[i][j];
            if(IsBlockedSample(traceDataStats[i], span)){
                intrvl.insert(traces[i][j], traces[i][j+1]);
            }
        }
    }
    
    
    overallSampleStats.startTime = 0xffffffffffffffff;
    overallSampleStats.endTime = 0;
    overallSampleStats.simpleBlockedTime = 0;
    
    for( int i = 0 ; i < numTraces; i++){
        if(traces[i][0] < overallSampleStats.startTime)
            overallSampleStats.startTime = traces[i][0];
        if(traces[i][traces[i].size()-1] > overallSampleStats.endTime)
            overallSampleStats.endTime = traces[i][traces[i].size()-1];
    }
    
    overallSampleStats.simpleBlockedTime = intrvl.get_total();
    
    
    intervals programBlockingIntrvl;
    for( int i = 0 ; i < numTraces; i++){
        if (traces[i].size() < 2)
            continue;
        for (int j = 0 ; j < (int)traces[i].size() - 1; j++) {
            if(traceSampleInfo[i][j].isAffectingSystem){
                programBlockingIntrvl.insert(traces[i][j], traces[i][j+1]);
            }
        }
    }
    
    overallSampleStats.systemAffectingBlockedTime = programBlockingIntrvl.get_total();
    
    double simpleBlockedTimePercent = 100.0 * overallSampleStats.simpleBlockedTime / (overallSampleStats.endTime - overallSampleStats.startTime);
    double systemAffectingBlockedTimePercent = 100.0 * overallSampleStats.systemAffectingBlockedTime / (overallSampleStats.endTime - overallSampleStats.startTime);
    
    fprintf(stdout, "\n---------------------------------------" );
    fprintf(stdout, "\nOverall simpleBlockedTime = %lu / (%lu) = %lf %%",  overallSampleStats.simpleBlockedTime, (overallSampleStats.endTime - overallSampleStats.startTime), simpleBlockedTimePercent);
    fprintf(stdout, "\nOverall systemAffectingBlockedTime = %lu / (%lu) = %lf %%",  overallSampleStats.systemAffectingBlockedTime, (overallSampleStats.endTime - overallSampleStats.startTime), systemAffectingBlockedTimePercent);
    fprintf(stdout, "\n---------------------------------------" );
    
}


static void FindSampleStats(int numTraces) {
    traceSampleInfo.resize(numTraces);
   
#pragma omp parallel for 
    for( int i = 0 ; i < numTraces; i++){
        uint64_t min_span = 0xffffffffffffffff;
        uint64_t max_span = 0;
        uint64_t median = -1;
        vector<uint64_t> tmp;
        if (traces[i].size() < 2){
            min_span = max_span = -1;
            median = - 1;
            traceDataStats[i].minSampleSize = min_span;
            traceDataStats[i].maxSampleSize = max_span;
            traceDataStats[i].medianSampleSize = median;
            continue;
        }
        
        for (int j = 0 ; j < (int)traces[i].size() - 1; j++) {
            uint64_t span = traces[i][j+1] - traces[i][j];
            SampleInfo_t t;
            t.span = span;
            t.isBlocked = false;
            t.isAffectingSystem = false;
            traceSampleInfo[i].push_back(t);
            if(span > max_span)
                max_span = span;
            if(span < min_span)
                min_span = span;
            tmp.push_back(span);
        }
        // sort and find the median;
        std::sort (tmp.begin(), tmp.end());
        median = tmp[tmp.size()/2];
        traceDataStats[i].minSampleSize = min_span;
        traceDataStats[i].maxSampleSize = max_span;
        traceDataStats[i].medianSampleSize = median;
        
        uint64_t blockedSamples = 0;
        uint64_t blockedTime = 0;
        for (uint j = 0; j < traces[i].size() - 1; j++) {
            if (IsBlockedSample(traceDataStats[i], traces[i][j+1] - traces[i][j])){
                blockedSamples++;
                blockedTime += traces[i][j+1] - traces[i][j];
                traceSampleInfo[i][j].isBlocked = true;
            }
        }
        
        traceDataStats[i].numTotalSamples = traces[i].size();
        traceDataStats[i].numBlockedSamples = blockedSamples;
        traceDataStats[i].blockedTime = blockedTime;
        traceDataStats[i].minTime = traces[i][0];
        traceDataStats[i].maxTime = traces[i][traces[i].size()-1];
        tmp.resize(0);
    }
    
#pragma omp parallel for 
    for( int i = 0 ; i < numTraces; i++){
        uint64_t numSystemAffectingBlockedSamples = 0;
        uint64_t systemAffectingBlockedTime = 0;
        for (uint j = 0; j < traces[i].size() - 1; j++) {
            if (traceSampleInfo[i][j].isBlocked &&  IsBlockingSampleAffectingSystem(i, traces[i][j], traces[i][j+1], numTraces) ){
                numSystemAffectingBlockedSamples++;
                systemAffectingBlockedTime += traces[i][j+1] - traces[i][j];
                traceSampleInfo[i][j].isAffectingSystem = true;
            }
        }
        traceDataStats[i].numSystemAffectingBlockedSamples = numSystemAffectingBlockedSamples;
        traceDataStats[i].systemAffectingBlockedTime = systemAffectingBlockedTime;
    }
    
    fprintf(stdout, "\nTraceName|MinSampleSize|MaxSampleSize|MedianSampleSize|#Samples|#BlockedSamples|%%BlockedTime|%%LessSamples|#SystemAffectingBlockedSamples|%%SystemAffectingBlockedTime" );
    for( int i = 0 ; i < numTraces; i++){
        
        int64_t numExpectedSamples = (traceDataStats[i].maxTime - traceDataStats[i].minTime) / traceDataStats[i].medianSampleSize;
        int64_t numActualSamples = traces[i].size();
        double percentLessSamples = (numExpectedSamples - numActualSamples) * 100.0 / numExpectedSamples;
        double percentBlockedTime = (100.0 * traceDataStats[i].blockedTime) / (traceDataStats[i].maxTime - traceDataStats[i].minTime);
        double percentSystemAffectingBlockedTime = (100.0 * traceDataStats[i].systemAffectingBlockedTime) / (traceDataStats[i].maxTime - traceDataStats[i].minTime);
        
        fprintf(stdout, "\n %*s | %10lu | %10lu | %10lu | %10lu | %10lu | %lf | %lf | %10lu | %lf", 100, traceDataStats[i].traceFileName.c_str(), traceDataStats[i].minSampleSize, traceDataStats[i].maxSampleSize, traceDataStats[i].medianSampleSize, traces[i].size(), traceDataStats[i].numBlockedSamples, percentBlockedTime, percentLessSamples,traceDataStats[i].numSystemAffectingBlockedSamples, percentSystemAffectingBlockedTime  );
    }
    
}

void
Analysis::Raw::analyzeSampleBlocking(const std::vector<string>& profileFiles)
{
    using namespace Analysis::Util;
    
    traces.resize(profileFiles.size());
    traceDataStats.resize(profileFiles.size());
    uint validTraces = 0;
    
    for (uint i = 0; i < profileFiles.size(); ++i) {
        const char* filenm = profileFiles[i].c_str();
        ProfType_t ty = getProfileType(filenm);
        if (ty == ProfType_CallpathTrace) {
            if (!filenm) { continue; }
            try {
                FILE* fs = hpcio_fopen_r(filenm);
                if (!fs) {
                    DIAG_EMsg("error opening trace file '" << filenm << "'");
                    continue;
                }
                
                hpctrace_fmt_hdr_t hdr;
                
                int ret = hpctrace_fmt_hdr_fread(&hdr, fs);
                if (ret != HPCFMT_OK) {
                    DIAG_EMsg("error reading trace file '" << filenm << "'");
                    continue;
                }
                
                // Read trace records and record into a vector
                while ( !feof(fs) ) {
                    hpctrace_fmt_datum_t datum;
                    ret = hpctrace_fmt_datum_fread(&datum, hdr.flags, fs);
                    if (ret == HPCFMT_EOF) {
                        break;
                    }
                    else if (ret == HPCFMT_ERR) {
                        traces[validTraces].resize(0);
                        hpcio_fclose(fs);
                        // drop the entire trace
                        DIAG_Throw("error reading trace file '" << filenm << "'");
                    }
                    
                    traces[validTraces].push_back(datum.time);
                }
                hpcio_fclose(fs);
                // Drop traces with fewer than 2 records
                if (traces[validTraces].size() < 2) {
                    traces[validTraces].resize(0);
                } else {
                    traceDataStats[validTraces].traceFileName = profileFiles[validTraces];
                    validTraces++;
                }
            }
            catch (...) {
                DIAG_EMsg("While reading '" << filenm << "'...");
            }
            
        }
        else {
            DIAG_EMsg("Ignoring " << filenm << " since it is not a trace file");
        }
        
    }
    
    // Resize to valid number of traces
    traces.resize(validTraces);
    traceDataStats.resize(validTraces);
                          
    FindSampleStats(validTraces);
    analyzeSampleBlockingImpact(validTraces);
}


void
Analysis::Raw::writeAsText_callpath(const char* filenm)
{
    if (!filenm) { return; }
    
    Prof::CallPath::Profile* prof = NULL;
    try {
        prof = Prof::CallPath::Profile::make(filenm, 0/*rFlags*/, stdout);
    }
    catch (...) {
        DIAG_EMsg("While reading '" << filenm << "'...");
        throw;
    }
    delete prof;
}


void
Analysis::Raw::writeAsText_callpathMetricDB(const char* filenm)
{
    if (!filenm) { return; }
    
    try {
        FILE* fs = hpcio_fopen_r(filenm);
        if (!fs) {
            DIAG_Throw("error opening metric-db file '" << filenm << "'");
        }
        
        hpcmetricDB_fmt_hdr_t hdr;
        int ret = hpcmetricDB_fmt_hdr_fread(&hdr, fs);
        if (ret != HPCFMT_OK) {
            DIAG_Throw("error reading metric-db file '" << filenm << "'");
        }
        
        hpcmetricDB_fmt_hdr_fprint(&hdr, stdout);
        
        for (uint nodeId = 1; nodeId < hdr.numNodes + 1; ++nodeId) {
            fprintf(stdout, "(%6u: ", nodeId);
            for (uint mId = 0; mId < hdr.numMetrics; ++mId) {
                double mval = 0;
                ret = hpcfmt_real8_fread(&mval, fs);
                if (ret != HPCFMT_OK) {
                    DIAG_Throw("error reading trace file '" << filenm << "'");
                }
                fprintf(stdout, "%12g ", mval);
            }
            fprintf(stdout, ")\n");
        }
        
        hpcio_fclose(fs);
    }
    catch (...) {
        DIAG_EMsg("While reading '" << filenm << "'...");
        throw;
    }
}


void
Analysis::Raw::writeAsText_callpathTrace(const char* filenm)
{
    if (!filenm) { return; }
    
    try {
        FILE* fs = hpcio_fopen_r(filenm);
        if (!fs) {
            DIAG_Throw("error opening trace file '" << filenm << "'");
        }
        
        hpctrace_fmt_hdr_t hdr;
        
        int ret = hpctrace_fmt_hdr_fread(&hdr, fs);
        if (ret != HPCFMT_OK) {
            DIAG_Throw("error reading trace file '" << filenm << "'");
        }
        
        hpctrace_fmt_hdr_fprint(&hdr, stdout);
        
        // Read trace records and exit on EOF
        while ( !feof(fs) ) {
            hpctrace_fmt_datum_t datum;
            ret = hpctrace_fmt_datum_fread(&datum, hdr.flags, fs);
            if (ret == HPCFMT_EOF) {
                break;
            }
            else if (ret == HPCFMT_ERR) {
                DIAG_Throw("error reading trace file '" << filenm << "'");
            }
            
            hpctrace_fmt_datum_fprint(&datum, hdr.flags, stdout);
        }
        
        hpcio_fclose(fs);
    }
    catch (...) {
        DIAG_EMsg("While reading '" << filenm << "'...");
        throw;
    }
}


void
Analysis::Raw::writeAsText_flat(const char* filenm)
{
    if (!filenm) { return; }
    
    Prof::Flat::ProfileData prof;
    try {
        prof.openread(filenm);
    }
    catch (...) {
        DIAG_EMsg("While reading '" << filenm << "'...");
        throw;
    }
    
    prof.dump(std::cout);
}


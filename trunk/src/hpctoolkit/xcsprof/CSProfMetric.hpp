// -*-Mode: C++;-*-
// $Id$

//***************************************************************************//
//
// File:
//    CSProfMetric.H
//
// Purpose:
//    call stack profile have multiple metrics
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************//

#ifndef CSProfMetric_H 
#define CSProfMetric_H

//************************* System Include Files ****************************//

#include <iostream>

//*************************** User Include Files ****************************//

#include <include/general.h> 

//*************************** Forward Declarations **************************//

//***************************************************************************//
// CSProfMetric
//***************************************************************************//

class CSProfileMetric
{
public:
  CSProfileMetric() : period(0) { }
  ~CSProfileMetric() { }

  // Name, Description: The metric name and a description
  // Period: The sampling period (whether event or instruction based)
  const char*  GetName()        const { return name; }
  const char*  GetDescription() const { return description; }
  unsigned int GetFlags()      const { return flags; }
  unsigned int GetPeriod()      const { return period; }
  
  void SetName(const char* s)        { name = s; }
  void SetDescription(const char* s) { description = s; }
  void SetFlags(unsigned int p)      { flags = p; }
  void SetPeriod(unsigned int p)     { period = p; }

  void Dump(std::ostream& os = std::cerr) const;
  void DDump() const; 

private:
  // Should not be used  
  CSProfileMetric(const CSProfileMetric& m) { }
  CSProfileMetric& operator=(const CSProfileMetric& m) { return *this; }

protected:
private:  
  String name;
  String description;
  unsigned int flags;  // flags of the metric
  unsigned int period; // sampling period
};

#endif




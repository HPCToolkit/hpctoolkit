#ifndef NameMappings_h
#define NameMappings_h

//******************************************************************************
//******************************************************************************
// NameMappings.h
//
// Purpose: 
//    To support renamings, this file supports one operation: normalize_name,
//    while maps input name into its output name. If no remapping exists, the
//    input name and the output name will agree.
//******************************************************************************
//******************************************************************************



//******************************************************************************
// interface operations
//******************************************************************************

const char *normalize_name(const char *in, bool &fake_procedured);

#endif

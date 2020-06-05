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


/**
 * Get the normalized name, and retrieve the type of the procedure
 * 
 * Possible values of Type of procedures:
    const int  TYPE_PLACE_FOLDER = 1;
    const int  TYPE_INVISIBLE    = 2;
 **/ 
const char *normalize_name(const char *in, int &type_procedure);

#endif

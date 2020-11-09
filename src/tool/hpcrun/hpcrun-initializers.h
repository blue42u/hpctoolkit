#ifndef hpcrun_initializers_h
#define hpcrun_initializers_h
//******************************************************************************
// file: hpcrun-initializers.h
// purpose: 
//   interface for deferring a call to an initializer.
//******************************************************************************

//******************************************************************************
// local includes 
//******************************************************************************

#include "closure-registry.h"



//******************************************************************************
// interface functions
//******************************************************************************

__attribute__((visibility("default")))
void hpcrun_initializers_defer(closure_t *c);

__attribute__((visibility("default")))
void hpcrun_initializers_apply();
#endif

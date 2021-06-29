/******************************************************************************
**  MODULE:        MSXPROJ.C
**  PROJECT:       EPANET-MSX
**  DESCRIPTION:   project data manager used by the EPANET Multi-Species
**                 Extension toolkit.
**  COPYRIGHT:     Copyright (C) 2007 Feng Shang, Lewis Rossman, and James Uber.
**                 All Rights Reserved. See license information in LICENSE.TXT.
**  AUTHORS:       L. Rossman, US EPA - NRMRL
**                 F. Shang, University of Cincinnati
**                 J. Uber, University of Cincinnati
**  VERSION:       1.1.00
**  LAST UPDATE:   2/8/11
**  Bug fix:       Bug ID 08, Feng Shang 01/07/2008
**                 Memory leak fixed, T. Taxon - 9/7/10
******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "msxutils.h"
#include "mathexpr.h"
#include "hash.h"

//  Local variables
//-----------------
static alloc_handle_t  *HashPool;           // Memory pool for hash tables
static HTtable  *Htable[MAX_OBJECTS];       // Hash tables for object ID names

static char * Errmsg[] =
    {"unknown error code.",
     "Error 501 - insufficient memory available.",
     "Error 502 - no EPANET data file supplied.",
     "Error 503 - could not open MSX input file.",
     "Error 504 - could not open hydraulic results file.",
     "Error 505 - could not read hydraulic results file.",
     "Error 506 - could not read MSX input file.",
     "Error 507 - too few pipe reaction expressions.",
     "Error 508 - too few tank reaction expressions.",
     "Error 509 - could not open differential equation solver.",
     "Error 510 - could not open algebraic equation solver.",
     "Error 511 - could not open binary results file.",
     "Error 512 - read/write error on binary results file.",
     "Error 513 - could not integrate reaction rate expressions.",
     "Error 514 - could not solve reaction equilibrium expressions.",
     "Error 515 - reference made to an unknown type of object.",
     "Error 516 - reference made to an illegal object index.",
     "Error 517 - reference made to an undefined object ID.",
     "Error 518 - invalid property values were specified.",
     "Error 519 - an MSX project was not opened.",
     "Error 520 - an MSX project is already opened.",
     "Error 521 - could not open MSX report file.",                            //(LR-11/20/07)

     "Error 522 - could not compile chemistry functions.",                     //1.1.00
     "Error 523 - could not load functions from compiled chemistry file.",     //1.1.00
	 "Error 524 - illegal math operation."};                                   //1.1.00

//  Imported functions
//--------------------
int    MSXinp_countMsxObjects(MSXproject *MSX);
int    MSXinp_countNetObjects(MSXproject *MSX);
int    MSXinp_readNetData(MSXproject *MSX);
int    MSXinp_readMsxData(MSXproject *MSX);

//  Exported functions
//--------------------
int    MSXproj_open(MSXproject *MSX, char *fname);
int    MSXproj_addObject(int type, char *id, int n);
int    MSXproj_findObject(int type, char *id);
char * MSXproj_findID(int type, char *id);
char * MSXproj_getErrmsg(int errcode);

//  Local functions
//-----------------
static void   setDefaults(MSXproject *MSX);
static int    convertUnits(MSXproject *MSX);
static int    createObjects(MSXproject *MSX);
static void   deleteObjects(MSXproject *MSX);
static int    createHashTables(void);
static void   deleteHashTables(void);

static int    openRptFile(MSXproject *MSX);                                               //(LR-11/20/07)

static int  buildadjlists(MSXproject *MSX);
static void freeadjlists(MSXproject *MSX);


//=============================================================================

int  MSXproj_open(MSXproject *MSX, char *fname)
/**
**  Purpose:
**    opens an EPANET-MSX project.
**
**  Input:
**    fname = name of EPANET-MSX input file
**
**  Returns:
**    an error code (0 if no error)
*/
{
// --- initialize data to default values

    int errcode = 0;
    MSX->ProjectOpened = FALSE; //TODO make getters??
    MSX->QualityOpened = FALSE;
    setDefaults(MSX);

// --- open the MSX input file

    strcpy(MSX->MsxFile.name, fname); //TODO setter and getter?
    if ((MSX->MsxFile.file = fopen(fname,"rt")) == NULL) return ERR_OPEN_MSX_FILE;

// --- create hash tables to look up object ID names

    CALL(errcode, createHashTables());

// --- allocate memory for the required number of objects

    CALL(errcode, MSXinp_countMsxObjects(MSX));
    CALL(errcode, MSXinp_countNetObjects(MSX));
    CALL(errcode, createObjects(MSX));

// --- read in the EPANET and MSX object data

    CALL(errcode, MSXinp_readNetData(MSX));
    CALL(errcode, MSXinp_readMsxData(MSX));

    //TODO getter?
    if (strcmp(MSX->RptFile.name, ""))                                          //(FS-01/07/2008, to fix bug 08)
	CALL(errcode, openRptFile(MSX));                                              //(LR-11/20/07, to fix bug 08)

// --- convert user's units to internal units

    CALL(errcode, convertUnits(MSX));



    // Build nodal adjacency lists 
    if (MSX->Adjlist == NULL) //TODO getter?
    {
        errcode = buildadjlists(MSX);
        if (errcode) return errcode;
    }
    // printf("ptr: %ld\n", MSX->Adjlist);


// --- close input file

    //TODO getters and setters?
    if ( MSX->MsxFile.file ) fclose(MSX->MsxFile.file);
    MSX->MsxFile.file = NULL;
    if ( !errcode ) MSX->ProjectOpened = TRUE;
    return errcode;
}

//=============================================================================

void MSXproj_close(MSXproject *MSX)
/**
**  Purpose:
**    closes the current EPANET-MSX project.
**
**  Input:
**    none
*/
{
    // --- close all files

    if ( MSX->RptFile.file ) fclose(MSX->RptFile.file);                          //(LR-11/20/07, to fix bug 08)
    if ( MSX->HydFile.file ) fclose(MSX->HydFile.file);
    if ( MSX->TmpOutFile.file && MSX->TmpOutFile.file != MSX->OutFile.file )
        fclose(MSX->TmpOutFile.file);
    if ( MSX->OutFile.file ) fclose(MSX->OutFile.file);

    // --- delete all temporary files

    if ( MSX->HydFile.mode == SCRATCH_FILE ) remove(MSX->HydFile.name);
    if ( MSX->OutFile.mode == SCRATCH_FILE ) remove(MSX->OutFile.name);
    remove(MSX->TmpOutFile.name);

    // --- free all allocated memory

    MSX->RptFile.file = NULL;                                                   //(LR-11/20/07, to fix bug 08)
    MSX->HydFile.file = NULL;
    MSX->OutFile.file = NULL;
    MSX->TmpOutFile.file = NULL;
    deleteObjects(MSX);
    deleteHashTables();
    MSX->ProjectOpened = FALSE;
}

//=============================================================================

int   MSXproj_addObject(int type, char *id, int n)
/**
**  Purpose:
**    adds an object ID to the project's hash tables.
**
**  Input:
**    type = object type
**    id   = object ID string
**    n    = object index.
**
**  Returns:
**    0 if object already added, 1 if not, -1 if hashing fails.
*/
{
    int  result;
    int  len;
    char *newID;

// --- do nothing if object already exists in a hash table

    if ( MSXproj_findObject(type, id) > 0 ) return 0;

// --- use memory from the hash tables' common memory pool to store
//     a copy of the object's ID string

    len = (int) strlen(id) + 1;
    newID = (char *) Alloc(len*sizeof(char));
    strcpy(newID, id);

// --- insert object's ID into the hash table for that type of object

    result = (int) HTinsert(Htable[type], newID, n);
    if ( result == 0 ) result = -1;
    return result;
}

//=============================================================================

int   MSXproj_findObject(int type, char *id)
/**
**  Purpose:
**    uses hash table to find index of an object with a given ID.
**
**  Input:
**    type = object type
**    id   = object ID.
**
**  Returns:
**    index of object with given ID, or -1 if ID not found.
*/
{
    return HTfind(Htable[type], id);
}

//=============================================================================

char * MSXproj_findID(int type, char *id)
/**
**  Purpose:
**    uses hash table to find address of given string entry.
**
**  Input:
**    type = object type
**    id   = ID name being sought.
**
**  Returns:
**    pointer to location where object's ID string is stored.
*/
{
    return HTfindKey(Htable[type], id);
}

//=============================================================================

char * MSXproj_getErrmsg(int errcode)
/**
**  Purpose:
**    gets the text of an error message.
**
**  Input:
**    errcode = error code.
**
**  Returns:
**    text of error message.
*/
{
    if ( errcode <= ERR_FIRST || errcode >= ERR_MAX ) return Errmsg[0];
    else return Errmsg[errcode - ERR_FIRST];
}

//=============================================================================

void setDefaults(MSXproject *MSX)
/**
**  Purpose:
**    assigns default values to project variables.
**
**  Input:
**    none.
*/
{
    int i;
    MSX->RptFile.file = NULL;                                                   //(LR-11/20/07)
    MSX->HydFile.file = NULL;
    MSX->HydFile.mode = USED_FILE;
    MSX->OutFile.file = NULL;
    MSX->OutFile.mode = SCRATCH_FILE;
    MSX->TmpOutFile.file = NULL;
    MSXutils_getTempName(MSX->OutFile.name);                                    //1.1.00
    MSXutils_getTempName(MSX->TmpOutFile.name);                                 //1.1.00
    strcpy(MSX->RptFile.name, "");
    strcpy(MSX->Title, "");
    MSX->Rptflag = 0;
    for (i=0; i<MAX_OBJECTS; i++) MSX->Nobjects[i] = 0;
    MSX->Unitsflag = US;
    MSX->Flowflag = GPM;
    MSX->Statflag = SERIES;
    MSX->DefRtol = 0.001;
    MSX->DefAtol = 0.01;
    MSX->Solver = EUL;
    MSX->Coupling = NO_COUPLING;
    MSX->Compiler = NO_COMPILER;                                                //1.1.00
    MSX->AreaUnits = FT2;
    MSX->RateUnits = DAYS;
    MSX->Qstep = 300;
    MSX->Rstep = 3600;
    MSX->Rstart = 0;
    MSX->Dur = 0;
    MSX->Node = NULL;
    MSX->Link = NULL;
    MSX->Tank = NULL;
    MSX->D = NULL;
    MSX->Q = NULL;
    MSX->H = NULL;
    MSX->Species = NULL;
    MSX->Term = NULL;
    MSX->Const = NULL;
    MSX->Pattern = NULL;
    MSX->K = NULL;                                                              //1.1.00
    MSX->Adjlist = NULL;
}

//=============================================================================

int convertUnits(MSXproject *MSX)
/**
**  Purpose:
**    converts user's units to internal EPANET units.
**
**  Input:
**    none.
**
**  Returns:
**    an error code (0 if no error).
*/
{
// --- flow conversion factors (to cfs)
    double fcf[] = {1.0, GPMperCFS, MGDperCFS, IMGDperCFS, AFDperCFS,
                    LPSperCFS, LPMperCFS, MLDperCFS, CMHperCFS, CMDperCFS};

// --- rate time units conversion factors (to sec)
    double rcf[] = {1.0, 60.0, 3600.0, 86400.0};

    int i, m, errcode = 0;

// --- conversions for length & tank volume

    if ( MSX->Unitsflag == US )
    {
        MSX->Ucf[LENGTH_UNITS] = 1.0;
        MSX->Ucf[DIAM_UNITS]   = 12.0;
        MSX->Ucf[VOL_UNITS]    = 1.0;
    }
    else
    {
        MSX->Ucf[LENGTH_UNITS] = MperFT;
        MSX->Ucf[DIAM_UNITS]   = 1000.0*MperFT;
        MSX->Ucf[VOL_UNITS]    = M3perFT3;
    }

// --- conversion for surface area

    MSX->Ucf[AREA_UNITS] = 1.0;
    switch (MSX->AreaUnits)
    {
        case M2:  MSX->Ucf[AREA_UNITS] = M2perFT2;  break;
        case CM2: MSX->Ucf[AREA_UNITS] = CM2perFT2; break;
    }

// --- conversion for flow rate

    MSX->Ucf[FLOW_UNITS] = fcf[MSX->Flowflag];
    MSX->Ucf[CONC_UNITS] = LperFT3;

// --- conversion for reaction rate time

    MSX->Ucf[RATE_UNITS] = rcf[MSX->RateUnits];

// --- convert pipe diameter & length

    for (i=1; i<=MSX->Nobjects[LINK]; i++)
    {
        MSX->Link[i].diam /= MSX->Ucf[DIAM_UNITS];
        MSX->Link[i].len /=  MSX->Ucf[LENGTH_UNITS];
    }

// --- convert initial tank volumes

    for (i=1; i<=MSX->Nobjects[TANK]; i++)
    {
        MSX->Tank[i].v0 /= MSX->Ucf[VOL_UNITS];
        MSX->Tank[i].vMix /= MSX->Ucf[VOL_UNITS];
    }

// --- assign default tolerances to species

    for (m=1; m<=MSX->Nobjects[SPECIES]; m++)
    {
        if ( MSX->Species[m].rTol == 0.0 ) MSX->Species[m].rTol = MSX->DefRtol;
        if ( MSX->Species[m].aTol == 0.0 ) MSX->Species[m].aTol = MSX->DefAtol;
    }
    return errcode;
}


//=============================================================================

int createObjects(MSXproject *MSX)
/**
**  Purpose:
**    creates multi-species data objects.
**
**  Input:
**    none.
**
**  Returns:
**    an error code (0 if no error).
*/
{
    int i;

// --- create nodes, links, & tanks

    MSX->Node = (Snode *) calloc(MSX->Nobjects[NODE]+1, sizeof(Snode));
    MSX->Link = (Slink *) calloc(MSX->Nobjects[LINK]+1, sizeof(Slink));
    MSX->Tank = (Stank *) calloc(MSX->Nobjects[TANK]+1, sizeof(Stank));

// --- create species, terms, parameters, constants & time patterns

    MSX->Species = (Sspecies *) calloc(MSX->Nobjects[SPECIES]+1, sizeof(Sspecies));
    MSX->Term    = (Sterm *)    calloc(MSX->Nobjects[TERM]+1,  sizeof(Sterm));
    MSX->Param   = (Sparam *)   calloc(MSX->Nobjects[PARAMETER]+1, sizeof(Sparam));
    MSX->Const   = (Sconst *)   calloc(MSX->Nobjects[CONSTANT]+1, sizeof(Sconst));
    MSX->Pattern = (Spattern *) calloc(MSX->Nobjects[PATTERN]+1, sizeof(Spattern));
    MSX->K       = (double *)   calloc(MSX->Nobjects[CONSTANT]+1, sizeof(double));  //1.1.00

// --- create arrays for demands, heads, & flows

    MSX->D = (float *) calloc(MSX->Nobjects[NODE]+1, sizeof(float));
    MSX->H = (float *) calloc(MSX->Nobjects[NODE]+1, sizeof(float));
    MSX->Q = (float *) calloc(MSX->Nobjects[LINK]+1, sizeof(float));

// --- create arrays for current & initial concen. of each species for each node

    MSX->C0 = (double *) calloc(MSX->Nobjects[SPECIES]+1, sizeof(double));
    for (i=1; i<=MSX->Nobjects[NODE]; i++)
    {
        MSX->Node[i].c = (double *) calloc(MSX->Nobjects[SPECIES]+1, sizeof(double));
        MSX->Node[i].c0 = (double *) calloc(MSX->Nobjects[SPECIES]+1, sizeof(double));
        MSX->Node[i].rpt = 0;
    }

// --- create arrays for init. concen. & kinetic parameter values for each link

    for (i=1; i<=MSX->Nobjects[LINK]; i++)
    {
        MSX->Link[i].c0 = (double *)
            calloc(MSX->Nobjects[SPECIES]+1, sizeof(double));
        MSX->Link[i].reacted = (double *)
            calloc(MSX->Nobjects[SPECIES] + 1, sizeof(double));
        MSX->Link[i].param = (double *)
            calloc(MSX->Nobjects[PARAMETER]+1, sizeof(double));
        MSX->Link[i].rpt = 0;
    }

// --- create arrays for kinetic parameter values & current concen. for each tank

    for (i=1; i<=MSX->Nobjects[TANK]; i++)
    {
        MSX->Tank[i].param = (double *)
            calloc(MSX->Nobjects[PARAMETER]+1, sizeof(double));
        MSX->Tank[i].c = (double *)
            calloc(MSX->Nobjects[SPECIES]+1, sizeof(double));
        MSX->Tank[i].reacted = (double*)
            calloc(MSX->Nobjects[SPECIES] + 1, sizeof(double));
    }

// --- initialize contents of each time pattern object

    for (i=1; i<=MSX->Nobjects[PATTERN]; i++)
    {
        MSX->Pattern[i].length = 0;
        MSX->Pattern[i].first = NULL;
        MSX->Pattern[i].current = NULL;
    }

// --- initialize reaction rate & equil. formulas for each species

    for (i=1; i<=MSX->Nobjects[SPECIES]; i++)
    {
        MSX->Species[i].pipeExpr     = NULL;
        MSX->Species[i].tankExpr     = NULL;
        MSX->Species[i].pipeExprType = NO_EXPR;
        MSX->Species[i].tankExprType = NO_EXPR;
        MSX->Species[i].precision    = 2;
        MSX->Species[i].rpt = 0;
    }

// --- initialize math expressions for each intermediate term

    for (i=1; i<=MSX->Nobjects[TERM]; i++) MSX->Term[i].expr = NULL;
    return 0;
}

//=============================================================================

void deleteObjects(MSXproject *MSX)
/**
**  Purpose:
**    deletes multi-species data objects.
**
**  Input:
**    none.
*/
{
    int i;
    SnumList *listItem;
    Psource  source;                                                           //ttaxon - 9/7/10

// --- free memory used by nodes, links, and tanks

    if (MSX->Node) for (i=1; i<=MSX->Nobjects[NODE]; i++)
    {
        FREE(MSX->Node[i].c);
        FREE(MSX->Node[i].c0);

        // --- free memory used by water quality sources                       //ttaxon - 9/7/10

	if(MSX->Node[i].sources)
	{ 
            source = MSX->Node[i].sources; 
            while (source != NULL)
	    { 
                MSX->Node[i].sources = source->next; 
                FREE(source); 
                source = MSX->Node[i].sources; 
            } 
        }

    }
    if (MSX->Link) for (i=1; i<=MSX->Nobjects[LINK]; i++)
    {
        FREE(MSX->Link[i].c0);
        FREE(MSX->Link[i].param);
        FREE(MSX->Link[i].reacted);
    }
    if (MSX->Tank) for (i=1; i<=MSX->Nobjects[TANK]; i++)
    {
        FREE(MSX->Tank[i].param);
        FREE(MSX->Tank[i].c);
        FREE(MSX->Tank[i].reacted);
    }

// --- free memory used by time patterns

    if (MSX->Pattern) for (i=1; i<=MSX->Nobjects[PATTERN]; i++)
    {
        listItem = MSX->Pattern[i].first;
        while (listItem)
        {
            MSX->Pattern[i].first = listItem->next;
            free(listItem);
            listItem = MSX->Pattern[i].first;
        }
    }
    FREE(MSX->Pattern);

// --- free memory used for hydraulics results

    FREE(MSX->D);
    FREE(MSX->H);
    FREE(MSX->Q);
    FREE(MSX->C0);

// --- delete all nodes, links, and tanks

    FREE(MSX->Node);
    FREE(MSX->Link);
    FREE(MSX->Tank);

// --- free memory used by reaction rate & equilibrium expressions

    if (MSX->Species) for (i=1; i<=MSX->Nobjects[SPECIES]; i++)
    {
    // --- free the species tank expression only if it doesn't
    //     already point to the species pipe expression
        if ( MSX->Species[i].tankExpr != MSX->Species[i].pipeExpr )
        {
            mathexpr_delete(MSX->Species[i].tankExpr);
        }
        mathexpr_delete(MSX->Species[i].pipeExpr);
    }

// --- delete all species, parameters, and constants

    FREE(MSX->Species);
    FREE(MSX->Param);
    FREE(MSX->Const);
    FREE(MSX->K);                                                               //1.1.00

// --- free memory used by intermediate terms

    if (MSX->Term) for (i=1; i<=MSX->Nobjects[TERM]; i++)
        mathexpr_delete(MSX->Term[i].expr);
    FREE(MSX->Term);
}

//=============================================================================

int createHashTables()
/**
**  Purpose:
**    allocates memory for object ID hash tables.
**
**  Input:
**    none.
**
**  Returns:
**    an error code (0 if no error).
*/
{   int j;

// --- create a hash table for each type of object

    for (j = 0; j < MAX_OBJECTS ; j++)
    {
         Htable[j] = HTcreate();
         if ( Htable[j] == NULL ) return ERR_MEMORY;
    }

// --- initialize the memory pool used to store object ID's

    HashPool = AllocInit();
    if ( HashPool == NULL ) return ERR_MEMORY;
    return 0;
}

//=============================================================================

void deleteHashTables()
/**
**  Purpose:
**    frees memory allocated for object ID hash tables.
**
**  Input:
**    none.
*/
{
    int j;

// --- free the hash tables

    for (j = 0; j < MAX_OBJECTS; j++)
    {
        if ( Htable[j] != NULL ) HTfree(Htable[j]);
    }

// --- free the object ID memory pool

    if ( HashPool )
    {
        AllocSetPool(HashPool);
        AllocFreePool();
    }
}

// New function added (LR-11/20/07, to fix bug 08)
int openRptFile(MSXproject *MSX)
{
    if ( MSX->RptFile.file ) fclose(MSX->RptFile.file);
    MSX->RptFile.file = fopen(MSX->RptFile.name, "wt");
    if ( MSX->RptFile.file == NULL ) return ERR_OPEN_RPT_FILE;
    return 0;
}

int  buildadjlists(MSXproject *MSX)   //from epanet2.2 for node sorting in WQ routing
/**
**--------------------------------------------------------------
** Input:   none
** Output:  returns error code
** Purpose: builds linked list of links adjacent to each node
**--------------------------------------------------------------
*/
{
    int       i, j, k;
    int       errcode = 0;
    Padjlist  alink;

    // Create an array of adjacency lists
    freeadjlists(MSX);
    MSX->Adjlist = (Padjlist*)calloc(MSX->Nobjects[NODE]+1, sizeof(Padjlist));
    if (MSX->Adjlist == NULL) return 101;
    for (i = 0; i <= MSX->Nobjects[NODE]; i++) 
        MSX->Adjlist[i] = NULL;

    // For each link, update adjacency lists of its end nodes
    for (k = 1; k <= MSX->Nobjects[LINK]; k++)
    {
        i = MSX->Link[k].n1;
        j = MSX->Link[k].n2;

        // Include link in start node i's list
        alink = (struct Sadjlist*) malloc(sizeof(struct Sadjlist));
        if (alink == NULL)
        {
            errcode = 101;
            break;
        }
        alink->node = j;
        alink->link = k;
        alink->next = MSX->Adjlist[i];
        MSX->Adjlist[i] = alink;

        // Include link in end node j's list
        alink = (struct Sadjlist*) malloc(sizeof(struct Sadjlist));
        if (alink == NULL)
        {
            errcode = 101;
            break;
        }
        alink->node = i;
        alink->link = k;
        alink->next = MSX->Adjlist[j];
        MSX->Adjlist[j] = alink;
    }
    if (errcode) freeadjlists(MSX);
    return errcode;
}


void  freeadjlists(MSXproject *MSX)            //from epanet2.2 for node sorting in WQ routing
/**
**--------------------------------------------------------------
** Input:   none
** Output:  none
** Purpose: frees memory used for nodal adjacency lists
**--------------------------------------------------------------
*/
{
    int   i;
    Padjlist alink;

    if (MSX->Adjlist == NULL) return;
    for (i = 0; i <= MSX->Nobjects[NODE]; i++)
    {
        for (alink = MSX->Adjlist[i]; alink != NULL; alink = MSX->Adjlist[i])
        {
            MSX->Adjlist[i] = alink->next;
            free(alink);
        }
    }
    free(MSX->Adjlist);
}

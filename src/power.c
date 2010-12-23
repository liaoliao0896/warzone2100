/*
	This file is part of Warzone 2100.
	Copyright (C) 1999-2004  Eidos Interactive
	Copyright (C) 2005-2010  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
/**
 * @file power.c
 *
 * Store PlayerPower and other power related stuff!
 *
 */

#include "lib/framework/frame.h"

#include "difficulty.h"
#include "intdisplay.h"
#include "mission.h"
#include "power.h"
#include "research.h"

#define EXTRACT_POINTS	    1
#define EASY_POWER_MOD      110
#define NORMAL_POWER_MOD    100
#define HARD_POWER_MOD      90
#define MAX_POWER           100000

//flag used to check for power calculations to be done or not
BOOL	powerCalculated;

PLAYER_POWER		asPower[MAX_PLAYERS];

/*allocate the space for the playerPower*/
BOOL allocPlayerPower(void)
{
	clearPlayerPower();
	powerCalculated = true;
	return true;
}

/*clear the playerPower */
void clearPlayerPower(void)
{
	UDWORD player;

	for (player = 0; player < MAX_PLAYERS; player++)
	{
		memset(&asPower[player], 0, sizeof(PLAYER_POWER));
	}
}

/*check the current power - if enough return true, else return false */
BOOL checkPower(UDWORD player, UDWORD quantity)
{
	ASSERT(player < MAX_PLAYERS, "checkPower: Bad player");

	//if not doing a check on the power - just return true
	if (!powerCalculated)
	{
		return true;
	}

	if (asPower[player].currentPower >= quantity)
	{
		return true;
	}
	return false;
}

/*check the current power - if enough subtracts the amount
 required to perform the task and returns true, else returns
 false */
BOOL usePower(UDWORD player, UDWORD quantity)
{
	ASSERT(player < MAX_PLAYERS, "usePower: Bad player");

	//if not doing a check on the power - just return true
	if (!powerCalculated)
	{
		return true;
	}

	//check there is enough first
	if (asPower[player].currentPower >= quantity)
	{
		asPower[player].currentPower -= quantity;
		return true;
	}
	return false;
}

//return the power when a structure/droid is deliberately destroyed
void addPower(UDWORD player, UDWORD quantity)
{
	ASSERT(player < MAX_PLAYERS, "addPower: Bad player (%u)", player);

	ASSERT(quantity <= MAX_POWER/2, "Power over or under flow. Tried to add %u.", quantity);
	if (quantity > MAX_POWER)
	{
		debug(LOG_WARNING, "Power over or under flow. Tried to add %u - ignored", quantity);
		return;
	}
	asPower[player].currentPower += quantity;
	if (asPower[player].currentPower > MAX_POWER)
	{
		asPower[player].currentPower = MAX_POWER;
	}
}

/*resets the power calc flag for all players*/
void powerCalc(BOOL on)
{
	powerCalculated = on;
}

/* Each Resource Extractor yields EXTRACT_POINTS per second until there are none
   left in the resource. */
static int32_t updateExtractedPower(STRUCTURE *psBuilding)
{
	RES_EXTRACTOR		*pResExtractor;
	int32_t				pointsToAdd, extractedPoints;
	int32_t				powmodifier;

	pResExtractor = (RES_EXTRACTOR *) psBuilding->pFunctionality;
	extractedPoints = 0;

	//only extracts points whilst its active ie associated with a power gen
	if (pResExtractor->active)
	{
		int overflowDiff;
		// Add modifier according to difficulty level
		if (getDifficultyLevel() == DL_EASY && !bMultiPlayer)
		{
			powmodifier = EASY_POWER_MOD/10;
		}
		else if (getDifficultyLevel() == DL_HARD && !bMultiPlayer)
		{
			powmodifier = HARD_POWER_MOD/10;
		}
		else
		{
			powmodifier = NORMAL_POWER_MOD/10;
		}
		// if the extractor hasn't been updated recently, now would be a good time.
		if (pResExtractor->timeLastUpdated < 20 && gameTime >= 20)
		{
			pResExtractor->timeLastUpdated = gameTime;
		}
		// include modifier as a %
		// Written this way to prevent rounding errors - do not "simplify"
		overflowDiff = pResExtractor->timeLastUpdated - (pResExtractor->timeLastUpdated % (GAME_TICKS_PER_SEC*10));
		pointsToAdd = ((gameTime - overflowDiff)                       * powmodifier * EXTRACT_POINTS) / (GAME_TICKS_PER_SEC * 10)
		            - ((pResExtractor->timeLastUpdated - overflowDiff) * powmodifier * EXTRACT_POINTS) / (GAME_TICKS_PER_SEC * 10);
		/*
		 * If you are thinking about rewriting the preceding section of code, there are a few things
		 * you should know.
		 * 
		 * First of all, division doesn't work intuitively.
		 * 3 / 2 = 1, not 1.5. These are integers, not floats.
		 *
		 * Second of all, timeLastUpdated is NOT necessarily the time you want to base off of.
		 * timeLastUpdated of an extractor that's just been created is some magic number around 12ms.
		 * gameTime, on the other hand, could be around 3 hours.
		 */
		ASSERT(pointsToAdd > -1 && pointsToAdd < 10, "pointsToAdd not in sensical range");

		if ((int)pResExtractor->timeLastUpdated - (int)gameTime > GAME_TICKS_PER_SEC || (int)gameTime - (int)pResExtractor->timeLastUpdated > GAME_TICKS_PER_SEC)
		{
			// extractor is not in the right time zone
			// we have a maximum time skip of less than a second, so this can't be caused by lag
			ASSERT(false, "Oil derrick out of sync.");
			pointsToAdd = 0;
		}
		pResExtractor->timeLastUpdated = gameTime;
		if (pointsToAdd)
		{
			extractedPoints += pointsToAdd;

			// If not having unlimited power, uncomment the following lines.

			/*
			if (pResExtractor->power > pointsToAdd)
			{
				extractedPoints += pointsToAdd;
				pResExtractor->power -= pointsToAdd;
			}
			else
			{
				extractedPoints += pResExtractor->power;
				pResExtractor->power = 0;
			}

			if (pResExtractor->power == 0)
			{
				// set the extractor to be inactive
				pResExtractor->active = false;
				// break the link between the power gen and the res extractor
				releaseResExtractor(psBuilding);
			}
			*/
		}
	}
	return extractedPoints;
}

//returns the relevant list based on OffWorld or OnWorld
static STRUCTURE* powerStructList(UBYTE player)
{
	ASSERT(player < MAX_PLAYERS, "powerStructList: Bad player");
	if (offWorldKeepLists)
	{
		return (mission.apsStructLists[player]);
	}
	else
	{
		return (apsStructLists[player]);
	}
}

/* Updates the current power based on the extracted power and a Power Generator*/
static void updateCurrentPower(POWER_GEN *psPowerGen, UDWORD player)
{
	int32_t		power, i, extractedPower;

	ASSERT(player < MAX_PLAYERS, "updateCurrentPower: Bad player");

	//each power gen can cope with its associated resource extractors
	extractedPower = 0;
	for (i=0; i < NUM_POWER_MODULES; i++)
	{
		if (psPowerGen->apResExtractors[i])
		{
			//check not died
			if (psPowerGen->apResExtractors[i]->died)
			{
				psPowerGen->apResExtractors[i] = NULL;
			}
			else
			{
				extractedPower += updateExtractedPower(psPowerGen->apResExtractors[i]);
			}
		}
	}

	// Written this way to prevent rounding errors - do not "simplify"
	power = ((asPower[player].extractedPower + extractedPower) * psPowerGen->multiplier) / 100
	      - (asPower[player].extractedPower * psPowerGen->multiplier) / 100;
	asPower[player].extractedPower += extractedPower;
	if (asPower[player].extractedPower > 100)
	{
		asPower[player].extractedPower -= 100; // just to prevent integer overflow
	}

	ASSERT_OR_RETURN(, power < 20 && power > -1, "Power out of sane range");
	if (power >= 0)
	{
		asPower[player].currentPower += power;
		if (asPower[player].currentPower > MAX_POWER)
		{
			asPower[player].currentPower = MAX_POWER;
		}
	}
}

/* Update current power based on what Power Generators exist */
void updatePlayerPower(UDWORD player)
{
	STRUCTURE		*psStruct;//, *psList;

	ASSERT(player < MAX_PLAYERS, "updatePlayerPower: Bad player");

	for (psStruct = powerStructList((UBYTE)player); psStruct != NULL; psStruct =
		psStruct->psNext)
	{
		if (psStruct->pStructureType->type == REF_POWER_GEN && psStruct->
			status == SS_BUILT)
		{
			updateCurrentPower((POWER_GEN *)psStruct->pFunctionality, player);
		}
	}

	// Check that the psLastPowered hasn't died
	if (asPower[player].psLastPowered && asPower[player].psLastPowered->died)
	{
		asPower[player].psLastPowered = NULL;
	}
}

// only used in multiplayer games.
void setPower(UDWORD player, UDWORD avail)
{
	ASSERT(player < MAX_PLAYERS, "setPower: Bad player (%u)", player);

	asPower[player].currentPower = avail;
}

UDWORD getPower(UDWORD player)
{
	ASSERT(player < MAX_PLAYERS, "setPower: Bad player (%u)", player);

	return asPower[player].currentPower;
}

/*sets the initial value for the power*/
void setPlayerPower(UDWORD power, UDWORD player)
{
	ASSERT(player < MAX_PLAYERS, "setPlayerPower: Bad player (%u)", player);

	asPower[player].currentPower = power;
}

/*Temp function to give all players some power when a new game has been loaded*/
void newGameInitPower(void)
{
	UDWORD		inc;

	for (inc=0; inc < MAX_PLAYERS; inc++)
	{
		addPower(inc, 400);
	}
}

/*accrue the power in the facilities that require it - returns true if use some power*/
BOOL accruePower(BASE_OBJECT *psObject)
{
	FACTORY					*psFactory;
	RESEARCH_FACILITY		*psResearch;
	REPAIR_FACILITY			*psRepair;
	SDWORD					powerDiff;
	UDWORD					count;
	BOOL					bPowerUsed = false;
	STRUCTURE			*psStructure;
	DROID				*psDroid, *psTarget;

	switch(psObject->type)
	{
	case OBJ_STRUCTURE:
		psStructure = (STRUCTURE *)psObject;
		// See if it needs power
		switch(psStructure->pStructureType->type)
		{
		case REF_FACTORY:
		case REF_CYBORG_FACTORY:
		case REF_VTOL_FACTORY:
			psFactory = (FACTORY *)psStructure->pFunctionality;
			// Check the factory is not on hold
			if (psFactory->timeStartHold)
			{
				break;
			}
			// Check the factory is active
			if (psFactory->psSubject)
			{
			    //check needs power
			    powerDiff = ((DROID_TEMPLATE *)psFactory->psSubject)->powerPoints -
				    psFactory->powerAccrued;
			    //if equal then don't need power
			    if (powerDiff)
			    {
				    if (POWER_PER_CYCLE >= powerDiff)
				    {
					    usePower(psStructure->player, powerDiff);
					    psFactory->powerAccrued += powerDiff;
					    bPowerUsed = true;
				    }
				    else if (powerDiff > POWER_PER_CYCLE)
				    {
					    usePower(psStructure->player, POWER_PER_CYCLE);
					    psFactory->powerAccrued += POWER_PER_CYCLE;
					    bPowerUsed = true;
				    }
			    }
    		}
	    	break;
	    case REF_RESEARCH:
		    //check the structure is active
		    psResearch = (RESEARCH_FACILITY  *)psStructure->pFunctionality;

		    //check the research facility is not on hold
            if (psResearch->timeStartHold)
            {
                break;
            }
		    if (psResearch->psSubject)
		    {
			    //check the research hasn't been cancelled
			    count = ((RESEARCH *)psResearch->psSubject)->ref - REF_RESEARCH_START;
			    if (IsResearchCancelled(asPlayerResList[selectedPlayer] + count)==false)
			    {
				    //check needs power
				    powerDiff = ((RESEARCH *)psResearch->psSubject)->researchPower -
					    psResearch->powerAccrued;
				    //if equal then don't need power
				    if (powerDiff)
				    {
					    //use the power if appropriate
					    if (POWER_PER_CYCLE >= powerDiff)
					    {
						    usePower(psStructure->player, powerDiff);
						    psResearch->powerAccrued += powerDiff;
						    bPowerUsed = true;
					    }
					    else if (powerDiff > POWER_PER_CYCLE)
					    {
						    usePower(psStructure->player, POWER_PER_CYCLE);
						    psResearch->powerAccrued += POWER_PER_CYCLE;
						    bPowerUsed = true;
					    }
				    }
			    }
		    }
		    break;
	    case REF_REPAIR_FACILITY:
		    //POWER REQUIRMENTS REMOVED - AB  22/09/98 - BACK IN - AB 07/01/99
		    psRepair = (REPAIR_FACILITY *)psStructure->pFunctionality;
            psDroid = (DROID *)psRepair->psObj;
            //check the droid hasn't died in the meantime
            if (psRepair->psObj && psRepair->psObj->died)
            {
                psRepair->psObj = NULL;
            }
		    if (psRepair->psObj)
		    {
			    //check if need power
                powerDiff = powerReqForDroidRepair(psDroid) - psDroid->powerAccrued;
                //if equal then don't need power
			    if (powerDiff > 0)
			    {
                    powerDiff /= POWER_FACTOR;
				    if (POWER_PER_CYCLE >= powerDiff)
				    {
					    usePower(psStructure->player, powerDiff);
                        //the unit accrues the power so more than one thing can be working on it
					    psDroid->powerAccrued += (powerDiff * POWER_FACTOR);
					    bPowerUsed = true;
				    }
				    else if (powerDiff > POWER_PER_CYCLE)
				    {
					    usePower(psStructure->player, POWER_PER_CYCLE);
					    psDroid->powerAccrued += (POWER_PER_CYCLE * POWER_FACTOR);
					    bPowerUsed = true;
				    }
			    }
		    }
		    break;
	    default:
		    //no need for power
		    bPowerUsed = false;
		    break;
        }
        break;
    case OBJ_DROID:
        psDroid = (DROID *)psObject;
        switch (psDroid->droidType)
        {
        case DROID_CONSTRUCT:
        case DROID_CYBORG_CONSTRUCT:
            //check trying to build something (and that hasn't been blown up)
            if (DroidIsBuilding(psDroid) && psDroid->psTarget && !psDroid->psTarget->died)
            {
                powerDiff = structPowerToBuild((STRUCTURE *)psDroid->psTarget) -
                    ((STRUCTURE *)psDroid->psTarget)->currentPowerAccrued;
			    //if equal then don't need power
			    if (powerDiff)
			    {
				    if (POWER_PER_CYCLE >= powerDiff)
				    {
					    usePower(psDroid->player, powerDiff);
					    ((STRUCTURE *)psDroid->psTarget)->currentPowerAccrued +=
                            powerDiff;
					    bPowerUsed = true;
				    }
				    else if (powerDiff > POWER_PER_CYCLE)
				    {
					    usePower(psDroid->player, POWER_PER_CYCLE);
					    ((STRUCTURE *)psDroid->psTarget)->currentPowerAccrued +=
                            POWER_PER_CYCLE;
					    bPowerUsed = true;
				    }
			    }
            }
            break;
        case DROID_REPAIR:
        case DROID_CYBORG_REPAIR:
            //check trying to repair something
            psTarget = NULL;
            if (DroidIsRepairing(psDroid))
            {
                psTarget = (DROID *)psDroid->psTarget;
            }
            else
            {
                //might have guard order but action of repair
                if (orderState(psDroid, DORDER_GUARD) && psDroid->action ==
                    DACTION_DROIDREPAIR)
                {
                    psTarget = (DROID *)psDroid->psActionTarget[0];
                }
            }
            //check the droid hasn't died in the meantime
            if (psTarget && psTarget->died)
            {
		setDroidTarget(psDroid, NULL);
                psTarget = NULL;
            }
            if (psTarget)
            {
                powerDiff = powerReqForDroidRepair(psTarget) - psTarget->powerAccrued;
			    //if equal then don't need power
			    if (powerDiff > 0)
			    {
                    powerDiff /= POWER_FACTOR;
				    if (POWER_PER_CYCLE >= powerDiff)
				    {
					    usePower(psDroid->player, powerDiff);
                        //the unit accrues the power so more than one thing can be working on it
					    psTarget->powerAccrued += (powerDiff * POWER_FACTOR);
					    bPowerUsed = true;
				    }
				    else if (powerDiff > POWER_PER_CYCLE)
				    {
					    usePower(psDroid->player, POWER_PER_CYCLE);
					    psTarget->powerAccrued += (POWER_PER_CYCLE * POWER_FACTOR);
					    bPowerUsed = true;
				    }
			    }
            }
            break;
        default:
		    //no need for power
		    bPowerUsed = false;
		    break;
        }
        break;
    default:
        ASSERT( false, "accruePower: Invalid object type" );
    }

	return bPowerUsed;
}


//informs the power array that a object has been destroyed
void powerDestroyObject(BASE_OBJECT *psObject)
{
	ASSERT(psObject != NULL, "invalid object");

	//check that this wasn't the last object that received the power
	if (asPower[psObject->player].psLastPowered == psObject)
	{
		updateLastPowered(NULL, psObject->player);
	}
}

/*checks if the Object to be powered next - returns true if power*/
BOOL getLastPowered(BASE_OBJECT *psObject)
{
	ASSERT(psObject != NULL, "invalid object");

	if (asPower[psObject->player].psLastPowered == NULL)
	{
		return true;
	}
	/*if we've got round to the last object again, by setting to NULL will
	enable the next object to get some power*/
	if (asPower[psObject->player].psLastPowered == psObject)
	{
		asPower[psObject->player].psLastPowered = NULL;
	}
	return false;
}

/*inform the players power struct that the last object to receive power has changed*/
void updateLastPowered(BASE_OBJECT *psObject, UBYTE player)
{
	ASSERT(player < MAX_PLAYERS, "updateLastPowered: Bad player (%u)", (unsigned int)player);
	ASSERT(psObject == NULL || psObject->died == 0 || psObject->died == NOT_CURRENT_LIST,
	       "updateLastPowered: Null or dead object");

	asPower[player].psLastPowered = psObject;
}

STRUCTURE *getRExtractor(STRUCTURE *psStruct)
{
STRUCTURE	*psCurr;
STRUCTURE	*psFirst;
BOOL		bGonePastIt;

	for(psCurr = apsStructLists[selectedPlayer],psFirst = NULL,bGonePastIt = false;
		psCurr; psCurr = psCurr->psNext)
	{
		if( psCurr->pStructureType->type == REF_RESOURCE_EXTRACTOR )
		{

			if(!psFirst)
			{
				psFirst = psCurr;
			}

			if(!psStruct)
			{
				return(psCurr);
			}
			else if(psCurr!=psStruct && bGonePastIt)
			{
				return(psCurr);
			}

			if(psCurr==psStruct)
			{
				bGonePastIt = true;
			}


		}
	}
	return(psFirst);
}

/*defines which structure types draw power - returns true if use power*/
BOOL structUsesPower(STRUCTURE *psStruct)
{
    BOOL    bUsesPower = false;

	ASSERT( psStruct != NULL,
		"structUsesPower: Invalid Structure pointer" );

    switch(psStruct->pStructureType->type)
    {
        case REF_FACTORY:
	    case REF_CYBORG_FACTORY:
    	case REF_VTOL_FACTORY:
	    case REF_RESEARCH:
	    case REF_REPAIR_FACILITY:
            bUsesPower = true;
            break;
        default:
            bUsesPower = false;
            break;
    }

    return bUsesPower;
}

/*defines which droid types draw power - returns true if use power*/
BOOL droidUsesPower(DROID *psDroid)
{
    BOOL    bUsesPower = false;

	ASSERT(psDroid != NULL,	"droidUsesPower: Invalid unit pointer" );

    switch(psDroid->droidType)
    {
        case DROID_CONSTRUCT:
	    case DROID_REPAIR:
        case DROID_CYBORG_CONSTRUCT:
        case DROID_CYBORG_REPAIR:
            bUsesPower = true;
            break;
        default:
            bUsesPower = false;
            break;
    }

    return bUsesPower;
}

//this is a check cos there is a problem with the power but not sure where!!
void powerCheck(BOOL bBeforePowerUsed, UBYTE player)
{
	static BASE_OBJECT	*psLastPowered = NULL;
	static BOOL		bPowerBefore = false;

	ASSERT(player < MAX_PLAYERS, "powerCheck: Bad player (%u)", (unsigned int)player);

	if (bBeforePowerUsed)
	{
		// Set what the lastPowered object is before using any power
		psLastPowered = asPower[player].psLastPowered;
		bPowerBefore = false;
		// Check that there is power available at start of loop
		if (asPower[player].currentPower > POWER_PER_CYCLE)
		{
			bPowerBefore = true;
		}
	}
	else
	{
		/* Check to see if we've been thru the whole list of structures and
		 * droids and not reset the lastPowered object in the power structure and
		 * there was some power at the start of the loop to use. */
		if (psLastPowered != NULL && psLastPowered == asPower[player].psLastPowered && bPowerBefore)
		{
			ASSERT(false, "Trouble at mill! bBeforePowerUsed=%d psLastPowered=%p asPower[%d].psLastPowered=%p", 
			       (int)bBeforePowerUsed, psLastPowered, player, asPower[player].psLastPowered );
			// Initialise so something can have some power next cycle
			asPower[player].psLastPowered = NULL;
		}
	}
}
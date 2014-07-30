#include "Building.h"

#include "Calc.h"
#include "CityInfo.h"
#include "Formation.h"
#include "Graphics.h"
#include "PlayerMessage.h"
#include "PlayerWarning.h"
#include "Resource.h"
#include "Routing.h"
#include "Sound.h"
#include "Terrain.h"
#include "TerrainGraphics.h"
#include "Walker.h"

#include "Data/Building.h"
#include "Data/CityInfo.h"
#include "Data/Constants.h"
#include "Data/Grid.h"
#include "Data/Scenario.h"
#include "Data/Settings.h"
#include "Data/State.h"

#include <string.h>

void Building_updateHighestIds()
{
	Data_Buildings_Extra.highestBuildingIdInUse = 0;
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		if (Data_Buildings[i].inUse) {
			Data_Buildings_Extra.highestBuildingIdInUse = i;
		}
	}
	if (Data_Buildings_Extra.highestBuildingIdInUse > Data_Buildings_Extra.highestBuildingIdEver) {
		Data_Buildings_Extra.highestBuildingIdEver = Data_Buildings_Extra.highestBuildingIdInUse;
	}
}

void Building_clearList()
{
	memset(Data_Buildings, 0, MAX_BUILDINGS * sizeof(struct Data_Building));
	Data_Buildings_Extra.highestBuildingIdEver = 0;
	Data_Buildings_Extra.createdSequence = 0;
}

static int isBuildingOnUndoList(int buildingId)
{
	// TODO move to undo stuff?
	return 0;
}

int Building_create(int type, int x, int y)
{
	int buildingId = 0;
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		if (!Data_Buildings[i].inUse && !isBuildingOnUndoList(i)) {
			buildingId = i;
			break;
		}
	}
	if (!buildingId) {
		PlayerWarning_show(PlayerWarning_DataLimitReached);
		return 0;
	}
	
	struct Data_Building *b = &Data_Buildings[buildingId];
	
	b->inUse = 3;
	b->__unknown_01 = 1;
	b->__unknown_02 = Data_CityInfo.__unknown_00a5; // TODO ??
	b->type = type;
	b->size = Constant_BuildingProperties[type].size;
	b->createdSequence = Data_Buildings_Extra.createdSequence++;
	b->sentiment.houseHappiness = 50;
	b->distanceFromEntry = 0;
	
	// house size
	b->houseSize = 0;
	if (type >= Building_HouseSmallTent && type <= Building_HouseMediumInsula) {
		b->houseSize = 1;
	} else if (type >= Building_HouseLargeInsula && type <= Building_HouseMediumVilla) {
		b->houseSize = 2;
	} else if (type >= Building_HouseLargeVilla && type <= Building_HouseMediumPalace) {
		b->houseSize = 3;
	} else if (type >= Building_HouseLargePalace && type <= Building_HouseLuxuryPalace) {
		b->houseSize = 4;
	}
	
	// subtype
	if (BuildingIsHouse(type)) {
		b->subtype.houseLevel = type - 10;
	} else {
		b->subtype.houseLevel = 0;
	}
	
	// input/output resources
	switch (type) {
		case Building_WheatFarm:
			b->outputResourceId = Resource_Wheat;
			break;
		case Building_VegetableFarm:
			b->outputResourceId = Resource_Vegetables;
			break;
		case Building_FruitFarm:
			b->outputResourceId = Resource_Fruit;
			break;
		case Building_OliveFarm:
			b->outputResourceId = Resource_Olives;
			break;
		case Building_VinesFarm:
			b->outputResourceId = Resource_Vines;
			break;
		case Building_PigFarm:
			b->outputResourceId = Resource_Meat;
			break;
		case Building_MarbleQuarry:
			b->outputResourceId = Resource_Marble;
			break;
		case Building_IronMine:
			b->outputResourceId = Resource_Iron;
			break;
		case Building_TimberYard:
			b->outputResourceId = Resource_Timber;
			break;
		case Building_ClayPit:
			b->outputResourceId = Resource_Clay;
			break;
		case Building_WineWorkshop:
			b->outputResourceId = Resource_Wine;
			b->subtype.workshopResource = WorkshopResource_VinesToWine;
			break;
		case Building_OilWorkshop:
			b->outputResourceId = Resource_Oil;
			b->subtype.workshopResource = WorkshopResource_OlivesToOil;
			break;
		case Building_WeaponsWorkshop:
			b->outputResourceId = Resource_Weapons;
			b->subtype.workshopResource = WorkshopResource_IronToWeapons;
			break;
		case Building_FurnitureWorkshop:
			b->outputResourceId = Resource_Furniture;
			b->subtype.workshopResource = WorkshopResource_TimberToFurniture;
			break;
		case Building_PotteryWorkshop:
			b->outputResourceId = Resource_Pottery;
			b->subtype.workshopResource = WorkshopResource_ClayToPottery;
			break;
		default:
			b->outputResourceId = Resource_None;
			break;
	}
	
	if (type == Building_Granary) {
		b->data.storage.resourceStored[Resource_None] = 2400;
	}
	
	b->x = x;
	b->y = y;
	b->gridOffset = GridOffset(x, y);
	b->__unknown_2c = b->houseGenerationDelay = Data_Grid_random[b->gridOffset] & 0x7f;
	b->fireProof = Constant_BuildingProperties[type].fireProof;
	b->isAdjacentToWater = Terrain_isAdjacentToWater(x, y, b->size);

	return buildingId;
}

void Building_delete(int buildingId)
{
	Building_deleteData(buildingId);
	memset(&Data_Buildings[buildingId], 0, sizeof(struct Data_Building));
}

void Building_deleteData(int buildingId)
{
	struct Data_Building *b = &Data_Buildings[buildingId];
	if (b->storageId) {
		Data_Building_Storages[b->storageId].inUse = 0;
	}
	if (b->type == Building_SenateUpgraded && b->gridOffset == Data_CityInfo.buildingSenateGridOffset) {
		Data_CityInfo.buildingSenateGridOffset = 0;
		Data_CityInfo.buildingSenateX = 0;
		Data_CityInfo.buildingSenateY = 0;
		Data_CityInfo.buildingSenatePlaced = 0;
	}
	if (b->type == Building_Dock) {
		--Data_CityInfo.numWorkingDocks;
	}
	if (b->type == Building_Barracks && b->gridOffset == Data_CityInfo.buildingBarracksGridOffset) {
		Data_CityInfo.buildingBarracksGridOffset = 0;
		Data_CityInfo.buildingBarracksX = 0;
		Data_CityInfo.buildingBarracksY = 0;
		Data_CityInfo.buildingBarracksPlaced = 0;
	}
	if (b->type == Building_DistributionCenter_Unused && b->gridOffset == Data_CityInfo.buildingDistributionCenterGridOffset) {
		Data_CityInfo.buildingDistributionCenterGridOffset = 0;
		Data_CityInfo.buildingDistributionCenterX = 0;
		Data_CityInfo.buildingDistributionCenterY = 0;
		Data_CityInfo.buildingDistributionCenterPlaced = 0;
	}
	if (b->type == Building_FortGround__) {
		Formation_deleteFortAndBanner(b->formationId);
	}
	if (b->type == Building_Hippodrome) {
		Data_CityInfo.buildingHippodromePlaced = 0;
	}
}

void Building_clearDeleted()
{
	int landRecalc = 0;
	int wallRecalc = 0;
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		struct Data_Building *b = &Data_Buildings[i];
		if (b->inUse == 3) {
			b->inUse = 1;
		}
		if (b->inUse != 1 || !b->houseSize) {
			if (b->inUse == 2 || b->inUse == 6) {
				if (b->type == Building_Tower || b->type == Building_Gatehouse) {
					wallRecalc = 1;
				}
				Terrain_removeBuildingFromGrids(i, b->x, b->y);
				landRecalc = 1;
				Building_delete(i);
			} else if (b->inUse == 4) {
				if (b->houseSize) {
					CityInfo_Population_removePeopleHomeRemoved(b->housePopulation);
				}
				Building_delete(i);
			} else if (b->inUse == 5) {
				Building_delete(i);
			}
		}
	}
	if (wallRecalc) {
		TerrainGraphics_updateAllWalls();
	}
	if (landRecalc) {
		Routing_determineLandCitizen();
		Routing_determineLandNonCitizen();
	}
}

int Building_getMainBuildingId(int buildingId)
{
	for (int guard = 0; guard < 9; guard++) {
		if (Data_Buildings[buildingId].prevPartBuildingId <= 0) {
			return buildingId;
		}
		buildingId = Data_Buildings[buildingId].prevPartBuildingId;
	}
	return 0;
}

void Building_collapse(int buildingId, int hasPlague)
{
	Data_State.undoAvailable = 0;
	struct Data_Building *b = &Data_Buildings[buildingId];
	b->fireRisk = 0;
	b->damageRisk = 0;
	if (b->houseSize && b->housePopulation) {
		CityInfo_Population_removePeopleHomeRemoved(b->housePopulation);
	}
	b->housePopulation = 0;
	b->houseSize = 0;
	b->outputResourceId = 0;
	b->distanceFromEntry = 0;
	Building_deleteData(buildingId);

	int watersideBuilding = 0;
	if (b->type == Building_Dock || b->type == Building_Wharf || b->type == Building_Shipyard) {
		watersideBuilding = 1;
	}
	int wasTent = b->houseSize && b->subtype.houseLevel <= HouseLevel_LargeTent;
	int numTiles;
	switch (b->size) {
		case 2: numTiles = 4; break;
		case 3: numTiles = 9; break;
		case 4: numTiles = 16; break;
		case 5: numTiles = 25; break;
		default: numTiles = 0; break;
	}
	Terrain_removeBuildingFromGrids(buildingId, b->x, b->y);
	if (Data_Grid_terrain[b->gridOffset] & Terrain_Water) {
		b->inUse = 5;
	} else {
		b->type = Building_BurningRuin;
		b->walkerId4 = 0;
		b->taxIncomeOrStorage = 0;
		b->fireDuration = (b->houseGenerationDelay & 7) + 1;
		b->fireProof = 1;
		b->size = 1;
		b->ruinHasPlague = hasPlague;
		int random = Data_Grid_random[b->gridOffset] & 3;
		int graphicId;
		if (wasTent) {
			graphicId = GraphicId(ID_Graphic_RubbleTent);
		} else {
			graphicId = GraphicId(ID_Graphic_RubbleGeneral) + 9 * random;
		}
		Terrain_addBuildingToGrids(buildingId, b->x, b->y, 1, graphicId, Terrain_Building);
	}
	static const int xTiles[] = {0, 1, 1, 0, 2, 2, 2, 1, 0, 3, 3, 3, 3, 2, 1, 0, 4, 4, 4, 4, 4, 3, 2, 1, 0, 5, 5, 5, 5, 5, 5, 4, 3, 2, 1, 0};
	static const int yTiles[] = {0, 0, 1, 1, 0, 1, 2, 2, 2, 0, 1, 2, 3, 3, 3, 3, 0, 1, 2, 3, 4, 4, 4, 4, 4, 0, 1, 2, 3, 4, 5, 5, 5, 5, 5, 5};
	for (int tile = 1; tile < numTiles; tile++) {
		int x = xTiles[tile] + b->x;
		int y = yTiles[tile] + b->y;
		if (Data_Grid_terrain[GridOffset(x, y)] & Terrain_Water) {
			continue;
		}
		int bid = Building_create(Building_BurningRuin, x, y);
		b = &Data_Buildings[bid];
		int random = Data_Grid_random[b->gridOffset] & 3;
		int graphicId;
		if (wasTent) {
			graphicId = GraphicId(ID_Graphic_RubbleTent);
		} else {
			graphicId = GraphicId(ID_Graphic_RubbleGeneral) + 9 * random;
		}
		Terrain_addBuildingToGrids(bid, b->x, b->y, 1, graphicId, Terrain_Building);
		b->fireDuration = (b->houseGenerationDelay & 7) + 1;
		b->walkerId4 = 0;
		b->fireProof = 1;
		b->ruinHasPlague = hasPlague;
	}
	if (watersideBuilding) {
		Routing_determineWater();
	}
}

void Building_collapseLinked(int buildingId, int callCollapse)
{
	// TODO
}

void Building_collapseLastPlaced()
{
	int highestSequence = 0;
	int buildingId = 0;
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		if (Data_Buildings[i].inUse == 1 || Data_Buildings[i].inUse == 3) {
			if (Data_Buildings[i].createdSequence > highestSequence) {
				highestSequence = Data_Buildings[i].createdSequence;
				buildingId = i;
			}
		}
	}
	if (buildingId) {
		PlayerMessage_post(1, 88, 0, Data_Buildings[buildingId].gridOffset);
		Data_State.undoAvailable = 0;
		Data_Buildings[buildingId].inUse = 4;
		TerrainGraphics_setBuildingAreaRubble(buildingId,
			Data_Buildings[buildingId].x, Data_Buildings[buildingId].y,
			Data_Buildings[buildingId].size);
		Walker_createDustCloud(Data_Buildings[buildingId].x, Data_Buildings[buildingId].y,
			Data_Buildings[buildingId].size);
		Building_collapseLinked(buildingId, 0);
		Routing_determineLandCitizen();
		Routing_determineLandNonCitizen();
	}
}

int Building_collapseFirstOfType(int buildingType)
{
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		if (Data_Buildings[i].inUse == 1 && Data_Buildings[i].type == buildingType) {
			int gridOffset = Data_Buildings[i].gridOffset;
			Data_State.undoAvailable = 0;
			Data_Buildings[i].inUse = 4;
			
			TerrainGraphics_setBuildingAreaRubble(i, Data_Buildings[i].x, Data_Buildings[i].y,
				Data_Buildings[i].size);
			Sound_Effects_playChannel(SoundChannel_Explosion);
			Routing_determineLandCitizen();
			Routing_determineLandNonCitizen();
			return gridOffset;
		}
	}
	return 0;
}

void Building_increaseDamageByEnemy(int gridOffset, int maxDamage)
{
	Data_Grid_buildingDamage[gridOffset]++;
	if (Data_Grid_buildingDamage[gridOffset] > maxDamage) {
		Building_destroyByEnemy(GridOffsetToX(gridOffset),
			GridOffsetToY(gridOffset), gridOffset);
	}
}

void Building_destroyByEnemy(int x, int y, int gridOffset)
{
	int buildingId = Data_Grid_buildingIds[gridOffset];
	if (buildingId <= 0) {
		if (Data_Grid_terrain[gridOffset] & Terrain_Wall) {
			Walker_killTowerSentriesAt(x, y);
		}
		TerrainGraphics_setBuildingAreaRubble(0, x, y, 1);
	} else {
		// TODO
	}
	// TODO
	Routing_determineLandCitizen();
	Routing_determineLandNonCitizen();
	Routing_determineWalls();
}

void Building_setDesirability()
{
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		if (Data_Buildings[i].inUse != 1) {
			continue;
		}
		struct Data_Building *b = &Data_Buildings[i];
		if (b->size == 1) {
			b->desirability = Data_Grid_desirability[b->gridOffset];
		} else {
			int maxDes = -9999;
			for (int y = 0; y < b->size; y++) {
				for (int x = 0; x < b->size; x++) {
					int gridOffset = GridOffset(b->x + x, b->y + y);
					if (Data_Grid_desirability[gridOffset] > maxDes) {
						maxDes = Data_Grid_desirability[gridOffset];
					}
				}
			}
			b->desirability = maxDes;
		}
		if (b->isAdjacentToWater) {
			b->desirability += 10;
		}
		switch (Data_Grid_elevation[b->gridOffset]) {
			case 0: break;
			case 1: b->desirability += 10; break;
			case 2: b->desirability += 12; break;
			case 3: b->desirability += 14; break;
			case 4: b->desirability += 16; break;
			default: b->desirability += 18; break;
		}
	}
}

void Building_decayHousesCovered()
{
	for (int i = 0; i < MAX_BUILDINGS; i++) {
		if (Data_Buildings[i].inUse && Data_Buildings[i].type != Building_Tower) {
			if (Data_Buildings[i].housesCovered <= 1) {
				Data_Buildings[i].housesCovered = 0;
			} else {
				Data_Buildings[i].housesCovered--;
			}
		}
	}
}

void Building_determineGraphicIdsForOrientedBuildings()
{
	int mapOrientation = Data_Settings_Map.orientation;
	int graphicOffset;
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		struct Data_Building *b = &Data_Buildings[i];
		if (!b->inUse) {
			continue;
		}
		int graphicId;
		switch (b->type) {
			case Building_Gatehouse:
				if (b->subtype.orientation == 1) {
					if (mapOrientation == 0 || mapOrientation == 4) {
						graphicId = GraphicId(ID_Graphic_Tower) + 1;
					} else {
						graphicId = GraphicId(ID_Graphic_Tower) + 2;
					}
				} else {
					if (mapOrientation == 0 || mapOrientation == 4) {
						graphicId = GraphicId(ID_Graphic_Tower) + 2;
					} else {
						graphicId = GraphicId(ID_Graphic_Tower) + 1;
					}
				}
				Terrain_addBuildingToGrids(i, b->x, b->y, b->size,
					graphicId, Terrain_Gatehouse | Terrain_Building);
				Terrain_addRoadsForGatehouse(b->x, b->y, b->subtype.orientation);
				break;
			case Building_TriumphalArch:
				if (b->subtype.orientation == 1) {
					if (mapOrientation == 0 || mapOrientation == 4) {
						graphicId = GraphicId(ID_Graphic_TriumphalArch);
					} else {
						graphicId = GraphicId(ID_Graphic_TriumphalArch) + 2;
					}
				} else {
					if (mapOrientation == 0 || mapOrientation == 4) {
						graphicId = GraphicId(ID_Graphic_TriumphalArch) + 2;
					} else {
						graphicId = GraphicId(ID_Graphic_TriumphalArch);
					}
				}
				Terrain_addBuildingToGrids(i, b->x, b->y, b->size,
					graphicId, Terrain_Building);
				Terrain_addRoadsForTriumphalArch(b->x, b->y, b->subtype.orientation);
				break;
			case Building_Hippodrome:
				if (mapOrientation == Direction_Top) {
					graphicId = GraphicId(ID_Graphic_Hippodrome2);
					switch (b->subtype.orientation) {
						case 0: case 3: graphicId += 0; break;
						case 1: case 4: graphicId += 2; break;
						case 2: case 5: graphicId += 4; break;
					}
				} else if (mapOrientation == Direction_Bottom) {
					graphicId = GraphicId(ID_Graphic_Hippodrome2);
					switch (b->subtype.orientation) {
						case 0: case 3: graphicId += 4; break;
						case 1: case 4: graphicId += 2; break;
						case 2: case 5: graphicId += 0; break;
					}
				} else if (mapOrientation == Direction_Left) {
					graphicId = GraphicId(ID_Graphic_Hippodrome1);
					switch (b->subtype.orientation) {
						case 0: case 3: graphicId += 0; break;
						case 1: case 4: graphicId += 2; break;
						case 2: case 5: graphicId += 4; break;
					}
				} else {
					graphicId = GraphicId(ID_Graphic_Hippodrome1);
					switch (b->subtype.orientation) {
						case 0: case 3: graphicId += 4; break;
						case 1: case 4: graphicId += 2; break;
						case 2: case 5: graphicId += 0; break;
					}
				}
				Terrain_addBuildingToGrids(i, b->x, b->y, b->size,
					graphicId, Terrain_Building);
				break;
			case Building_Shipyard:
				graphicOffset = (4 + b->data.other.dockOrientation - mapOrientation / 2) % 4;
				graphicId = GraphicId(ID_Graphic_Shipyard) + graphicOffset;
				Terrain_addWatersideBuildingToGrids(i, b->x, b->y, 2, graphicId);
				break;
			case Building_Wharf:
				graphicOffset = (4 + b->data.other.dockOrientation - mapOrientation / 2) % 4;
				graphicId = GraphicId(ID_Graphic_Wharf) + graphicOffset;
				Terrain_addWatersideBuildingToGrids(i, b->x, b->y, 2, graphicId);
				break;
			case Building_Dock:
				graphicOffset = (4 + b->data.other.dockOrientation - mapOrientation / 2) % 4;
				switch (graphicOffset) {
					case 0: graphicId = GraphicId(ID_Graphic_Dock1); break;
					case 1: graphicId = GraphicId(ID_Graphic_Dock2); break;
					case 2: graphicId = GraphicId(ID_Graphic_Dock3); break;
					default: graphicId = GraphicId(ID_Graphic_Dock4); break;
				}
				Terrain_addWatersideBuildingToGrids(i, b->x, b->y, 3, graphicId);
				break;
		}
	}
}

void BuildingStorage_clearList()
{
	memset(Data_Building_Storages, 0, MAX_STORAGES * sizeof(struct Data_Building_Storage));
}

int BuildingStorage_create()
{
	for (int i = 1; i < MAX_STORAGES; i++) {
		if (!Data_Building_Storages[i].inUse) {
			memset(&Data_Building_Storages[i], 0, sizeof(struct Data_Building_Storage));
			Data_Building_Storages[i].inUse = 1;
			return i;
		}
	}
	return 0;
}

void BuildingStorage_resetBuildingIds()
{
	for (int i = 1; i < MAX_STORAGES; i++) {
		Data_Building_Storages[i].buildingId = 0;
	}
	
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		if (!Data_Buildings[i].inUse) {
			continue;
		}
		if (Data_Buildings[i].type == Building_Granary || Data_Buildings[i].type == Building_Warehouse) {
			if (Data_Buildings[i].storageId) {
				int storageId = Data_Buildings[i].storageId;
				if (Data_Building_Storages[storageId].buildingId) {
					// is already connected to a building, create new
					// FIXME: not connected to building id
					Data_Buildings[i].storageId = BuildingStorage_create();
				} else {
					Data_Building_Storages[storageId].buildingId = i;
				}
			}
		}
	}
}

void Building_Industry_updateProduction()
{
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		struct Data_Building *b = &Data_Buildings[i];
		if (b->inUse != 1 || !b->outputResourceId) {
			continue;
		}
		b->data.industry.hasFullResource = 0;
		if (b->housesCovered <= 0 || b->numWorkers <= 0) {
			continue;
		}
		if (b->subtype.workshopResource && !b->loadsStored) {
			continue;
		}
		if (b->data.industry.curseDaysLeft) {
			b->data.industry.curseDaysLeft--;
		} else {
			if (b->data.industry.blessingDaysLeft) {
				b->data.industry.blessingDaysLeft--;
			}
			if (b->type == Building_MarbleQuarry) {
				b->data.industry.progress += b->numWorkers / 2;
			} else {
				b->data.industry.progress += b->numWorkers;
			}
			if (b->data.industry.blessingDaysLeft && BuildingIsFarm(b->type)) {
				b->data.industry.progress += b->numWorkers;
			}
			int maxProgress = 200;
			if (b->subtype.workshopResource) {
				maxProgress = 400;
			}
			if (b->data.industry.progress > maxProgress) {
				b->data.industry.progress = maxProgress;
			}
			if (BuildingIsFarm(b->type)) {
				TerrainGraphics_setBuildingFarm(i, b->x, b->y,
					GraphicId(ID_Graphic_FarmCrops) + 5 * (b->outputResourceId - 1),
					b->data.industry.progress);
			}
		}
	}
}

void Building_Industry_updateDoubleWheatProduction()
{
	if (Data_Scenario.climate == Climate_Northern) {
		return;
	}
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		struct Data_Building *b = &Data_Buildings[i];
		if (b->inUse != 1 || !b->outputResourceId) {
			continue;
		}
		if (b->housesCovered <= 0 || b->numWorkers <= 0) {
			continue;
		}
		if (b->type == Building_WheatFarm && !b->data.industry.curseDaysLeft) {
			b->data.industry.progress += b->numWorkers;
			if (b->data.industry.blessingDaysLeft) {
				b->data.industry.progress += b->numWorkers;
			}
			if (b->data.industry.progress > 200) {
				b->data.industry.progress = 200;
			}
			TerrainGraphics_setBuildingFarm(i, b->x, b->y,
				GraphicId(ID_Graphic_FarmCrops) + 5 * (b->outputResourceId - 1),
				b->data.industry.progress);
		}
	}
}

void Building_Industry_witherFarmCropsFromCeres(int bigCurse)
{
	int graphicId = GraphicId(ID_Graphic_FarmCrops);
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		struct Data_Building *b = &Data_Buildings[i];
		if (b->inUse == 1 && b->outputResourceId && BuildingIsFarm(b->type)) {
			b->data.industry.progress = 0;
			b->data.industry.blessingDaysLeft = 0;
			b->data.industry.curseDaysLeft = bigCurse ? 48 : 4;
			TerrainGraphics_setBuildingFarm(i, b->x, b->y,
				graphicId + 5 * (b->outputResourceId - 1),
				b->data.industry.progress);
		}
	}
}

void Building_Industry_blessFarmsFromCeres()
{
	int graphicId = GraphicId(ID_Graphic_FarmCrops);
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		struct Data_Building *b = &Data_Buildings[i];
		if (b->inUse == 1 && b->outputResourceId && BuildingIsFarm(b->type)) {
			b->data.industry.progress = 200;
			b->data.industry.curseDaysLeft = 0;
			b->data.industry.blessingDaysLeft = 16;
			TerrainGraphics_setBuildingFarm(i, b->x, b->y,
				graphicId + 5 * (b->outputResourceId - 1),
				b->data.industry.progress);
		}
	}
}

int Building_Industry_hasProducedResource(int buildingId)
{
	int target = 200;
	if (Data_Buildings[buildingId].subtype.workshopResource) {
		target = 400;
	}
	return Data_Buildings[buildingId].data.industry.progress >= target;
}

void Building_Industry_startNewProduction(int buildingId)
{
	struct Data_Building *b = &Data_Buildings[buildingId];
	b->data.industry.progress = 0;
	if (b->subtype.workshopResource) {
		if (b->loadsStored) {
			if (b->loadsStored > 1) {
				b->data.industry.hasFullResource = 1;
			}
			b->loadsStored--;
		}
	}
	if (BuildingIsFarm(b->type)) {
		TerrainGraphics_setBuildingFarm(buildingId, b->x, b->y,
			GraphicId(ID_Graphic_FarmCrops) + 5 * (b->outputResourceId - 1),
			b->data.industry.progress);
	}
}

int Building_Market_getDestinationGranaryWarehouse(int marketId)
{
	struct {
		int buildingId;
		int distance;
		int numBuildings;
	} resources[8];
	for (int i = 0; i < 8; i++) {
		resources[i].buildingId = 0;
		resources[i].numBuildings = 0;
		resources[i].distance = 40;
	}
	struct Data_Building *market = &Data_Buildings[marketId];
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		struct Data_Building *b = &Data_Buildings[i];
		if (b->inUse != 1) {
			continue;
		}
		if (b->type != Building_Granary && b->type != Building_Warehouse) {
			continue;
		}
		if (!b->hasRoadAccess || b->distanceFromEntry <= 0 ||
			b->roadNetworkId != market->roadNetworkId) {
			continue;
		}
		int distance = Calc_distanceMaximum(b->x, b->y, market->x, market->y);
		if (distance >= 40) {
			continue;
		}
		if (b->type == Building_Granary) {
			if (Data_Scenario.romeSuppliesWheat) {
				continue;
			}
			// foods
			if (b->data.storage.resourceStored[Resource_Wheat]) {
				resources[Inventory_Wheat].numBuildings++;
				if (distance < resources[Inventory_Wheat].distance) {
					resources[Inventory_Wheat].distance = distance;
					resources[Inventory_Wheat].buildingId = i;
				}
			}
			if (b->data.storage.resourceStored[Resource_Vegetables]) {
				resources[Inventory_Vegetables].numBuildings++;
				if (distance < resources[Inventory_Vegetables].distance) {
					resources[Inventory_Vegetables].distance = distance;
					resources[Inventory_Vegetables].buildingId = i;
				}
			}
			if (b->data.storage.resourceStored[Resource_Fruit]) {
				resources[Inventory_Fruit].numBuildings++;
				if (distance < resources[Inventory_Fruit].distance) {
					resources[Inventory_Fruit].distance = distance;
					resources[Inventory_Fruit].buildingId = i;
				}
			}
			if (b->data.storage.resourceStored[Resource_Meat]) {
				resources[Inventory_Meat].numBuildings++;
				if (distance < resources[Inventory_Meat].distance) {
					resources[Inventory_Meat].distance = distance;
					resources[Inventory_Meat].buildingId = i;
				}
			}
		} else if (b->type == Building_Warehouse) {
			// goods
			if (!Data_CityInfo.resourceStockpiled[Resource_Wine] &&
				Resource_getAmountStoredInWarehouse(i, Resource_Wine) > 0) {
				resources[Inventory_Wine].numBuildings++;
				if (distance < resources[Inventory_Wine].distance) {
					resources[Inventory_Wine].distance = distance;
					resources[Inventory_Wine].buildingId = i;
				}
			}
			if (!Data_CityInfo.resourceStockpiled[Resource_Oil] &&
				Resource_getAmountStoredInWarehouse(i, Resource_Oil) > 0) {
				resources[Inventory_Oil].numBuildings++;
				if (distance < resources[Inventory_Oil].distance) {
					resources[Inventory_Oil].distance = distance;
					resources[Inventory_Oil].buildingId = i;
				}
			}
			if (!Data_CityInfo.resourceStockpiled[Resource_Furniture] &&
				Resource_getAmountStoredInWarehouse(i, Resource_Furniture) > 0) {
				resources[Inventory_Furniture].numBuildings++;
				if (distance < resources[Inventory_Furniture].distance) {
					resources[Inventory_Furniture].distance = distance;
					resources[Inventory_Furniture].buildingId = i;
				}
			}
			if (!Data_CityInfo.resourceStockpiled[Resource_Pottery] &&
				Resource_getAmountStoredInWarehouse(i, Resource_Pottery) > 0) {
				resources[Inventory_Pottery].numBuildings++;
				if (distance < resources[Inventory_Pottery].distance) {
					resources[Inventory_Pottery].distance = distance;
					resources[Inventory_Pottery].buildingId = i;
				}
			}
		}
	}

	// update demands
	if (market->data.market.potteryDemand) {
		market->data.market.potteryDemand--;
	} else {
		resources[Inventory_Pottery].numBuildings = 0;
	}
	if (market->data.market.furnitureDemand) {
		market->data.market.furnitureDemand--;
	} else {
		resources[Inventory_Furniture].numBuildings = 0;
	}
	if (market->data.market.oilDemand) {
		market->data.market.oilDemand--;
	} else {
		resources[Inventory_Oil].numBuildings = 0;
	}
	if (market->data.market.wineDemand) {
		market->data.market.wineDemand--;
	} else {
		resources[Inventory_Wine].numBuildings = 0;
	}

	int canGo = 0;
	for (int i = 0; i < 8; i++) {
		if (resources[i].numBuildings) {
			canGo = 1;
			break;
		}
	}
	if (!canGo) {
		return 0;
	}
	// prefer food if we don't have it
	if (!market->data.market.food[Inventory_Wheat] &&
		resources[Inventory_Wheat].numBuildings) {
		market->data.market.fetchInventoryId = Inventory_Wheat;
		return resources[Inventory_Wheat].buildingId;
	} else if (!market->data.market.food[Inventory_Vegetables] &&
		resources[Inventory_Vegetables].numBuildings) {
		market->data.market.fetchInventoryId = Inventory_Vegetables;
		return resources[Inventory_Vegetables].buildingId;
	} else if (!market->data.market.food[Inventory_Fruit] &&
		resources[Inventory_Fruit].numBuildings) {
		market->data.market.fetchInventoryId = Inventory_Fruit;
		return resources[Inventory_Fruit].buildingId;
	} else if (!market->data.market.food[Inventory_Meat] &&
		resources[Inventory_Meat].numBuildings) {
		market->data.market.fetchInventoryId = Inventory_Meat;
		return resources[Inventory_Meat].buildingId;
	}
	// then prefer resource if we don't have it
	if (!market->data.market.pottery && resources[Inventory_Pottery].numBuildings) {
		market->data.market.fetchInventoryId = Inventory_Pottery;
		return resources[Inventory_Pottery].buildingId;
	} else if (!market->data.market.furniture && resources[Inventory_Furniture].numBuildings) {
		market->data.market.fetchInventoryId = Inventory_Furniture;
		return resources[Inventory_Furniture].buildingId;
	} else if (!market->data.market.oil && resources[Inventory_Oil].numBuildings) {
		market->data.market.fetchInventoryId = Inventory_Oil;
		return resources[Inventory_Oil].buildingId;
	} else if (!market->data.market.wine && resources[Inventory_Wine].numBuildings) {
		market->data.market.fetchInventoryId = Inventory_Wine;
		return resources[Inventory_Wine].buildingId;
	}
	// then prefer smallest stock below 50
	int minStock = 50;
	int fetchInventoryId = -1;
	if (resources[Inventory_Wheat].numBuildings &&
		market->data.market.food[Inventory_Wheat] < minStock) {
		minStock = market->data.market.food[Inventory_Wheat];
		fetchInventoryId = Inventory_Wheat;
	}
	if (resources[Inventory_Vegetables].numBuildings &&
		market->data.market.food[Inventory_Vegetables] < minStock) {
		minStock = market->data.market.food[Inventory_Vegetables];
		fetchInventoryId = Inventory_Vegetables;
	}
	if (resources[Inventory_Fruit].numBuildings &&
		market->data.market.food[Inventory_Fruit] < minStock) {
		minStock = market->data.market.food[Inventory_Fruit];
		fetchInventoryId = Inventory_Fruit;
	}
	if (resources[Inventory_Meat].numBuildings &&
		market->data.market.food[Inventory_Meat] < minStock) {
		minStock = market->data.market.food[Inventory_Meat];
		fetchInventoryId = Inventory_Meat;
	}
	if (resources[Inventory_Pottery].numBuildings &&
		market->data.market.pottery < minStock) {
		minStock = market->data.market.pottery;
		fetchInventoryId = Inventory_Pottery;
	}
	if (resources[Inventory_Furniture].numBuildings &&
		market->data.market.furniture < minStock) {
		minStock = market->data.market.furniture;
		fetchInventoryId = Inventory_Furniture;
	}
	if (resources[Inventory_Oil].numBuildings &&
		market->data.market.oil < minStock) {
		minStock = market->data.market.oil;
		fetchInventoryId = Inventory_Oil;
	}
	if (resources[Inventory_Wine].numBuildings &&
		market->data.market.wine < minStock) {
		minStock = market->data.market.wine;
		fetchInventoryId = Inventory_Wine;
	}

	if (fetchInventoryId == -1) {
		// all items well stocked: pick food below threshold
		if (resources[Inventory_Wheat].numBuildings &&
			market->data.market.food[Inventory_Wheat] < 600) {
			fetchInventoryId = Inventory_Wheat;
		}
		if (resources[Inventory_Vegetables].numBuildings &&
			market->data.market.food[Inventory_Vegetables] < 400) {
			fetchInventoryId = Inventory_Vegetables;
		}
		if (resources[Inventory_Fruit].numBuildings &&
			market->data.market.food[Inventory_Fruit] < 400) {
			fetchInventoryId = Inventory_Fruit;
		}
		if (resources[Inventory_Meat].numBuildings &&
			market->data.market.food[Inventory_Meat] < 400) {
			fetchInventoryId = Inventory_Meat;
		}
	}
	if (fetchInventoryId < 0) {
		return 0;
	}
	market->data.market.fetchInventoryId = fetchInventoryId;
	return resources[fetchInventoryId].buildingId;
}

int Building_Market_getMaxFoodStock(int buildingId)
{
	int maxStock = 0;
	if (buildingId > 0 && Data_Buildings[buildingId].type == Building_Market) {
		for (int i = 0; i < 4; i++) {
			if (Data_Buildings[buildingId].data.market.food[i] > maxStock) {
				maxStock = Data_Buildings[buildingId].data.market.food[i];
			}
		}
	}
	return maxStock;
}

int Building_Market_getMaxGoodsStock(int buildingId)
{
	int maxStock = 0;
	if (buildingId > 0 && Data_Buildings[buildingId].type == Building_Market) {
		if (Data_Buildings[buildingId].data.market.pottery > maxStock) {
			maxStock = Data_Buildings[buildingId].data.market.pottery;
		}
		if (Data_Buildings[buildingId].data.market.furniture > maxStock) {
			maxStock = Data_Buildings[buildingId].data.market.furniture;
		}
		if (Data_Buildings[buildingId].data.market.oil > maxStock) {
			maxStock = Data_Buildings[buildingId].data.market.oil;
		}
		if (Data_Buildings[buildingId].data.market.wine > maxStock) {
			maxStock = Data_Buildings[buildingId].data.market.wine;
		}
	}
	return maxStock;
}

int Building_Dock_getNumIdleDockers(int buildingId)
{
	// TODO
	return 0;
}

void Building_Dock_updateOpenWaterAccess()
{
	Routing_getDistanceWaterBoat(
		Data_Scenario.riverEntryPoint.x, Data_Scenario.riverEntryPoint.y);
	for (int i = 1; i < MAX_BUILDINGS; i++) {
		struct Data_Building *b = &Data_Buildings[i];
		if (b->inUse == 1 && !b->houseSize && b->type == Building_Dock) {
			if (Terrain_isAdjacentToOpenWater(b->x, b->y, 3)) {
				b->hasWaterAccess = 1;
			} else {
				b->hasWaterAccess = 0;
			}
		}
	}
}

int Building_Dock_isConnectedToOpenWater(int x, int y)
{
	Routing_getDistanceWaterBoat(
		Data_Scenario.riverEntryPoint.x, Data_Scenario.riverEntryPoint.y);
	if (Terrain_isAdjacentToOpenWater(x, y, 3)) {
		return 1;
	} else {
		return 0;
	}
}

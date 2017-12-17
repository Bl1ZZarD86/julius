#include "BuildingHouse.h"

#include "Building.h"
#include "Terrain.h"

#include "Data/State.h"

#include "building/building.h"
#include "game/resource.h"
#include "graphics/image.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/random.h"
#include "map/terrain.h"

static void merge(int buildingId);
static void prepareForMerge(int buildingId, int numTiles);
static void splitMerged(int buildingId);
static void splitSize2(int buildingId);
static void splitSize3(int buildingId);

#define MAX_DIR 4

static const int directionGridOffsets[] = {0, -163, -1, -162};
static const int directionOffsetX[] = { 0, -1, -1, 0 };
static const int directionOffsetY[] = { 0, -1, 0, -1 };
static const int tileGridOffsets[] = {
	0, 1, 162, 163, // 2x2
	2, 164, 326, 325, 324, // 3x3
	3, 165, 327, 489, 488, 487, 486 // 4x4
};

static const int houseGraphicGroup[20] = {
	26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 31, 31, 32, 32, 33, 33, 34, 34, 35, 35
};

static const int houseGraphicOffset[20] = {
	0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, 1, 0, 1, 0, 1
};

static const int houseGraphicNumTypes[20] = {
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1
};

static struct {
	int x;
	int y;
	int inventory[INVENTORY_MAX];
	int population;
} mergeData;

static int house_image_group(int level)
{
    return image_group(houseGraphicGroup[level]) + houseGraphicOffset[level];
}

static void create_house_tile(building_type type, int x, int y, int image_id, int population, int *inventory)
{
    int newBuildingId = Building_create(type, x, y);
    struct Data_Building *b = building_get(newBuildingId);
    b->housePopulation = population;
    for (int i = 0; i < INVENTORY_MAX; i++) {
        b->data.house.inventory[i] = inventory[i];
    }
    b->distanceFromEntry = 0;
    Terrain_addBuildingToGrids(newBuildingId, b->x, b->y, 1,
            image_id + (map_random_get(b->gridOffset) & 1), TERRAIN_BUILDING);
}

int BuildingHouse_canExpand(int buildingId, int numTiles)
{
	// merge with other houses
    struct Data_Building *house = building_get(buildingId);
	for (int dir = 0; dir < MAX_DIR; dir++) {
		int baseOffset = directionGridOffsets[dir] + house->gridOffset;
		int okTiles = 0;
		for (int i = 0; i < numTiles; i++) {
			int tileOffset = baseOffset + tileGridOffsets[i];
			if (map_terrain_is(tileOffset, TERRAIN_BUILDING)) {
				int tileBuildingId = map_building_at(tileOffset);
                struct Data_Building *otherHouse = building_get(tileBuildingId);
				if (tileBuildingId == buildingId) {
					okTiles++;
				} else if (BuildingIsInUse(tileBuildingId) && otherHouse->houseSize) {
					if (otherHouse->subtype.houseLevel <= house->subtype.houseLevel) {
						okTiles++;
					}
				}
			}
		}
		if (okTiles == numTiles) {
			mergeData.x = house->x + directionOffsetX[dir];
			mergeData.y = house->y + directionOffsetY[dir];
			return 1;
		}
	}
	// merge with houses and empty terrain
	for (int dir = 0; dir < MAX_DIR; dir++) {
		int baseOffset = directionGridOffsets[dir] + house->gridOffset;
		int okTiles = 0;
		for (int i = 0; i < numTiles; i++) {
			int tileOffset = baseOffset + tileGridOffsets[i];
			if (!map_terrain_is(tileOffset, TERRAIN_NOT_CLEAR)) {
				okTiles++;
			} else if (map_terrain_is(tileOffset, TERRAIN_BUILDING)) {
				int tileBuildingId = map_building_at(tileOffset);
				if (tileBuildingId == buildingId) {
					okTiles++;
				} else if (BuildingIsInUse(tileBuildingId) && Data_Buildings[tileBuildingId].houseSize) {
					if (Data_Buildings[tileBuildingId].subtype.houseLevel <= house->subtype.houseLevel) {
						okTiles++;
					}
				}
			}
		}
		if (okTiles == numTiles) {
			mergeData.x = house->x + directionOffsetX[dir];
			mergeData.y = house->y + directionOffsetY[dir];
			return 1;
		}
	}
	// merge with houses, empty terrain and gardens
	for (int dir = 0; dir < MAX_DIR; dir++) {
		int baseOffset = directionGridOffsets[dir] + house->gridOffset;
		int okTiles = 0;
		for (int i = 0; i < numTiles; i++) {
			int tileOffset = baseOffset + tileGridOffsets[i];
			if (!map_terrain_is(tileOffset, TERRAIN_NOT_CLEAR)) {
				okTiles++;
			} else if (map_terrain_is(tileOffset, TERRAIN_BUILDING)) {
				int tileBuildingId = map_building_at(tileOffset);
				if (tileBuildingId == buildingId) {
					okTiles++;
				} else if (BuildingIsInUse(tileBuildingId) && Data_Buildings[tileBuildingId].houseSize) {
					if (Data_Buildings[tileBuildingId].subtype.houseLevel <= house->subtype.houseLevel) {
						okTiles++;
					}
				}
			} else if (map_terrain_is(tileOffset, TERRAIN_GARDEN)) {
				okTiles++;
			}
		}
		if (okTiles == numTiles) {
			mergeData.x = house->x + directionOffsetX[dir];
			mergeData.y = house->y + directionOffsetY[dir];
			return 1;
		}
	}
	house->data.house.noSpaceToExpand = 1;
	return 0;
}

void BuildingHouse_checkForCorruption(int buildingId)
{
	struct Data_Building *b = building_get(buildingId);
	int houseGridOffset = b->gridOffset;
	int calcGridOffset = map_grid_offset(b->x, b->y);
	b->data.house.noSpaceToExpand = 0;
	if (houseGridOffset != calcGridOffset || map_building_at(houseGridOffset) != buildingId) {
		++Data_Buildings_Extra.incorrectHousePositions;
		for (int y = 0; y < Data_State.map.height; y++) {
			for (int x = 0; x < Data_State.map.width; x++) {
				int gridOffset = map_grid_offset(x, y);
				if (map_building_at(gridOffset) == buildingId) {
					b->gridOffset = gridOffset;
					b->x = map_grid_offset_to_x(gridOffset);
					b->y = map_grid_offset_to_y(gridOffset);
					return;
				}
			}
		}
		++Data_Buildings_Extra.unfixableHousePositions;
		b->state = BuildingState_Rubble;
	}
}

void BuildingHouse_checkMerge(int buildingId)
{
	struct Data_Building *b = building_get(buildingId);
	if (b->houseIsMerged) {
		return;
	}
	if ((map_random_get(b->gridOffset) & 7) >= 5) {
		return;
	}
	int numHouseTiles = 0;
	for (int i = 0; i < 4; i++) {
		int tileOffset = b->gridOffset + tileGridOffsets[i];
		if (map_terrain_is(tileOffset, TERRAIN_BUILDING)) {
			int tileBuildingId = map_building_at(tileOffset);
			if (tileBuildingId == buildingId) {
				numHouseTiles++;
			} else if (BuildingIsInUse(tileBuildingId) &&
					Data_Buildings[tileBuildingId].houseSize &&
					Data_Buildings[tileBuildingId].subtype.houseLevel == b->subtype.houseLevel &&
					!Data_Buildings[tileBuildingId].houseIsMerged) {
				numHouseTiles++;
			}
		}
	}
	if (numHouseTiles == 4) {
		mergeData.x = b->x + directionOffsetX[0];
		mergeData.y = b->y + directionOffsetY[0];
		merge(buildingId);
	}
}

static void split(int buildingId, int numTiles)
{
	int gridOffset = map_grid_offset(mergeData.x, mergeData.y);
	for (int i = 0; i < numTiles; i++) {
		int tileOffset = gridOffset + tileGridOffsets[i];
		if (map_terrain_is(tileOffset, TERRAIN_BUILDING)) {
			int tileBuildingId = map_building_at(tileOffset);
			if (tileBuildingId != buildingId && Data_Buildings[tileBuildingId].houseSize) {
				if (Data_Buildings[tileBuildingId].houseIsMerged == 1) {
					splitMerged(tileBuildingId);
				} else if (Data_Buildings[tileBuildingId].houseSize == 2) {
					splitSize2(buildingId);
				} else if (Data_Buildings[tileBuildingId].houseSize == 3) {
					splitSize3(buildingId);
				}
			}
		}
	}
}

static void prepareForMerge(int buildingId, int numTiles)
{
	for (int i = 0; i < INVENTORY_MAX; i++) {
		mergeData.inventory[i] = 0;
	}
	mergeData.population = 0;
	int gridOffset = map_grid_offset(mergeData.x, mergeData.y);
	for (int i = 0; i < numTiles; i++) {
		int tileOffset = gridOffset + tileGridOffsets[i];
		if (map_terrain_is(tileOffset, TERRAIN_BUILDING)) {
			int tileBuildingId = map_building_at(tileOffset);
            struct Data_Building *house = building_get(tileBuildingId);
			if (tileBuildingId != buildingId && house->houseSize) {
				mergeData.population += house->housePopulation;
				for (int i = 0; i < INVENTORY_MAX; i++) {
					mergeData.inventory[i] += house->data.house.inventory[i];
					house->housePopulation = 0;
					house->state = BuildingState_DeletedByGame;
				}
			}
		}
	}
}

void BuildingHouse_expandToLargeInsula(int buildingId)
{
	split(buildingId, 4);
	prepareForMerge(buildingId, 4);

	struct Data_Building *b = building_get(buildingId);
	b->type = BUILDING_HOUSE_LARGE_INSULA;
	b->subtype.houseLevel = HOUSE_LARGE_INSULA;
	b->size = b->houseSize = 2;
	b->housePopulation += mergeData.population;
	for (int i = 0; i < INVENTORY_MAX; i++) {
		b->data.house.inventory[i] += mergeData.inventory[i];
	}
	int graphicId = house_image_group(b->subtype.houseLevel) + (map_random_get(b->gridOffset) & 1);
	Terrain_removeBuildingFromGrids(buildingId, b->x, b->y);
	b->x = mergeData.x;
	b->y = mergeData.y;
	b->gridOffset = map_grid_offset(b->x, b->y);
	Terrain_addBuildingToGrids(buildingId, b->x, b->y, b->size, graphicId, TERRAIN_BUILDING);
}

void BuildingHouse_expandToLargeVilla(int buildingId)
{
	split(buildingId, 9);
	prepareForMerge(buildingId, 9);

	struct Data_Building *b = building_get(buildingId);
	b->type = BUILDING_HOUSE_LARGE_VILLA;
	b->subtype.houseLevel = HOUSE_LARGE_VILLA;
	b->size = b->houseSize = 3;
	b->housePopulation += mergeData.population;
	for (int i = 0; i < INVENTORY_MAX; i++) {
		b->data.house.inventory[i] += mergeData.inventory[i];
	}
	int graphicId = house_image_group(b->subtype.houseLevel);
	Terrain_removeBuildingFromGrids(buildingId, b->x, b->y);
	b->x = mergeData.x;
	b->y = mergeData.y;
	b->gridOffset = map_grid_offset(b->x, b->y);
	Terrain_addBuildingToGrids(buildingId, b->x, b->y, b->size, graphicId, TERRAIN_BUILDING);
}

void BuildingHouse_expandToLargePalace(int buildingId)
{
	split(buildingId, 16);
	prepareForMerge(buildingId, 16);

	struct Data_Building *b = building_get(buildingId);
	b->type = BUILDING_HOUSE_LARGE_PALACE;
	b->subtype.houseLevel = HOUSE_LARGE_PALACE;
	b->size = b->houseSize = 4;
	b->housePopulation += mergeData.population;
	for (int i = 0; i < INVENTORY_MAX; i++) {
		b->data.house.inventory[i] += mergeData.inventory[i];
	}
	int graphicId = house_image_group(b->subtype.houseLevel);
	Terrain_removeBuildingFromGrids(buildingId, b->x, b->y);
	b->x = mergeData.x;
	b->y = mergeData.y;
	b->gridOffset = map_grid_offset(b->x, b->y);
	Terrain_addBuildingToGrids(buildingId, b->x, b->y, b->size, graphicId, TERRAIN_BUILDING);
}

static void merge(int buildingId)
{
	prepareForMerge(buildingId, 4);

	struct Data_Building *b = building_get(buildingId);
	b->size = b->houseSize = 2;
	b->housePopulation += mergeData.population;
	for (int i = 0; i < INVENTORY_MAX; i++) {
		b->data.house.inventory[i] += mergeData.inventory[i];
	}
	int graphicId = image_group(houseGraphicGroup[b->subtype.houseLevel]) + 4;
	if (houseGraphicOffset[b->subtype.houseLevel]) {
		graphicId += 1;
	}
	
	Terrain_removeBuildingFromGrids(buildingId, b->x, b->y);
	b->x = mergeData.x;
	b->y = mergeData.y;
	b->gridOffset = map_grid_offset(b->x, b->y);
	b->houseIsMerged = 1;
	Terrain_addBuildingToGrids(buildingId, b->x, b->y, 2, graphicId, TERRAIN_BUILDING);
}

static void splitMerged(int buildingId)
{
	struct Data_Building *b = building_get(buildingId);
	int inventoryPerTile[INVENTORY_MAX];
	int inventoryRest[INVENTORY_MAX];
	for (int i = 0; i < INVENTORY_MAX; i++) {
		inventoryPerTile[i] = b->data.house.inventory[i] / 4;
		inventoryRest[i] = b->data.house.inventory[i] % 4;
	}
	int populationPerTile = b->housePopulation / 4;
	int populationRest = b->housePopulation % 4;

	Terrain_removeBuildingFromGrids(buildingId, b->x, b->y);

	// main tile
	b->size = b->houseSize = 1;
	b->houseIsMerged = 0;
	b->housePopulation = populationPerTile + populationRest;
	for (int i = 0; i < INVENTORY_MAX; i++) {
		b->data.house.inventory[i] = inventoryPerTile[i] + inventoryRest[i];
	}
	b->distanceFromEntry = 0;

	int graphicId = house_image_group(b->subtype.houseLevel);
	Terrain_addBuildingToGrids(buildingId, b->x, b->y, b->size,
		graphicId + (map_random_get(b->gridOffset) & 1), TERRAIN_BUILDING);
	
	// the other tiles (new buildings)
	create_house_tile(b->type, b->x + 1, b->y, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(b->type, b->x, b->y + 1, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(b->type, b->x + 1, b->y + 1, graphicId, populationPerTile, inventoryPerTile);
}

static void splitSize2(int buildingId)
{
	struct Data_Building *b = building_get(buildingId);
	int inventoryPerTile[INVENTORY_MAX];
	int inventoryRest[INVENTORY_MAX];
	for (int i = 0; i < INVENTORY_MAX; i++) {
		inventoryPerTile[i] = b->data.house.inventory[i] / 4;
		inventoryRest[i] = b->data.house.inventory[i] % 4;
	}
	int populationPerTile = b->housePopulation / 4;
	int populationRest = b->housePopulation % 4;

	Terrain_removeBuildingFromGrids(buildingId, b->x, b->y);

	// main tile
	b->type = BUILDING_HOUSE_MEDIUM_INSULA;
	b->subtype.houseLevel = b->type - 10;
	b->size = b->houseSize = 1;
	b->houseIsMerged = 0;
	b->housePopulation = populationPerTile + populationRest;
	for (int i = 0; i < INVENTORY_MAX; i++) {
		b->data.house.inventory[i] = inventoryPerTile[i] + inventoryRest[i];
	}
	b->distanceFromEntry = 0;

	int graphicId = house_image_group(b->subtype.houseLevel);
	Terrain_addBuildingToGrids(buildingId, b->x, b->y, b->size,
		graphicId + (map_random_get(b->gridOffset) & 1), TERRAIN_BUILDING);

	// the other tiles (new buildings)
	create_house_tile(b->type, b->x + 1, b->y, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(b->type, b->x, b->y + 1, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(b->type, b->x + 1, b->y + 1, graphicId, populationPerTile, inventoryPerTile);
}

static void splitSize3(int buildingId)
{
	struct Data_Building *b = building_get(buildingId);
	int inventoryPerTile[INVENTORY_MAX];
	int inventoryRest[INVENTORY_MAX];
	for (int i = 0; i < INVENTORY_MAX; i++) {
		inventoryPerTile[i] = b->data.house.inventory[i] / 9;
		inventoryRest[i] = b->data.house.inventory[i] % 9;
	}
	int populationPerTile = b->housePopulation / 9;
	int populationRest = b->housePopulation % 9;

	Terrain_removeBuildingFromGrids(buildingId, b->x, b->y);

	// main tile
	b->type = BUILDING_HOUSE_MEDIUM_INSULA;
	b->subtype.houseLevel = b->type - 10;
	b->size = b->houseSize = 1;
	b->houseIsMerged = 0;
	b->housePopulation = populationPerTile + populationRest;
	for (int i = 0; i < INVENTORY_MAX; i++) {
		b->data.house.inventory[i] = inventoryPerTile[i] + inventoryRest[i];
	}
	b->distanceFromEntry = 0;

	int graphicId = house_image_group(b->subtype.houseLevel);
	Terrain_addBuildingToGrids(buildingId, b->x, b->y, b->size,
		graphicId + (map_random_get(b->gridOffset) & 1), TERRAIN_BUILDING);

	// the other tiles (new buildings)
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x, b->y + 1, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x + 1, b->y + 1, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x + 2, b->y + 1, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x, b->y + 2, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x + 1, b->y + 2, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x + 2, b->y + 2, graphicId, populationPerTile, inventoryPerTile);
}

void BuildingHouse_devolveFromLargeInsula(int buildingId)
{
	splitSize2(buildingId);
}

void BuildingHouse_devolveFromLargeVilla(int buildingId)
{
	struct Data_Building *b = building_get(buildingId);
	int inventoryPerTile[INVENTORY_MAX];
	int inventoryRest[INVENTORY_MAX];
	for (int i = 0; i < INVENTORY_MAX; i++) {
		inventoryPerTile[i] = b->data.house.inventory[i] / 6;
		inventoryRest[i] = b->data.house.inventory[i] % 6;
	}
	int populationPerTile = b->housePopulation / 6;
	int populationRest = b->housePopulation % 6;

	Terrain_removeBuildingFromGrids(buildingId, b->x, b->y);

	// main tile
	b->type = BUILDING_HOUSE_MEDIUM_VILLA;
	b->subtype.houseLevel = b->type - 10;
	b->size = b->houseSize = 2;
	b->houseIsMerged = 0;
	b->housePopulation = populationPerTile + populationRest;
	for (int i = 0; i < INVENTORY_MAX; i++) {
		b->data.house.inventory[i] = inventoryPerTile[i] + inventoryRest[i];
	}
	b->distanceFromEntry = 0;

	int graphicId = house_image_group(b->subtype.houseLevel);
	Terrain_addBuildingToGrids(buildingId, b->x, b->y, b->size,
		graphicId + (map_random_get(b->gridOffset) & 1), TERRAIN_BUILDING);

	// the other tiles (new buildings)
	graphicId = house_image_group(HOUSE_MEDIUM_INSULA);
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x + 2, b->y, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x + 2, b->y + 1, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x, b->y + 2, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x + 1, b->y + 2, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x + 2, b->y + 2, graphicId, populationPerTile, inventoryPerTile);
}

void BuildingHouse_devolveFromLargePalace(int buildingId)
{
	struct Data_Building *b = building_get(buildingId);
	int inventoryPerTile[INVENTORY_MAX];
	int inventoryRest[INVENTORY_MAX];
	for (int i = 0; i < INVENTORY_MAX; i++) {
		inventoryPerTile[i] = b->data.house.inventory[i] / 8;
		inventoryRest[i] = b->data.house.inventory[i] % 8;
	}
	int populationPerTile = b->housePopulation / 8;
	int populationRest = b->housePopulation % 8;

	Terrain_removeBuildingFromGrids(buildingId, b->x, b->y);

	// main tile
	b->type = BUILDING_HOUSE_MEDIUM_PALACE;
	b->subtype.houseLevel = b->type - 10;
	b->size = b->houseSize = 3;
	b->houseIsMerged = 0;
	b->housePopulation = populationPerTile + populationRest;
	for (int i = 0; i < INVENTORY_MAX; i++) {
		b->data.house.inventory[i] = inventoryPerTile[i] + inventoryRest[i];
	}
	b->distanceFromEntry = 0;

	int graphicId = house_image_group(b->subtype.houseLevel);
	Terrain_addBuildingToGrids(buildingId, b->x, b->y, b->size, graphicId, TERRAIN_BUILDING);

	// the other tiles (new buildings)
	graphicId = house_image_group(HOUSE_MEDIUM_INSULA);
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x + 3, b->y, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x + 3, b->y + 1, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x + 3, b->y + 2, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x, b->y + 3, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x + 1, b->y + 3, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x + 2, b->y + 3, graphicId, populationPerTile, inventoryPerTile);
	create_house_tile(BUILDING_HOUSE_MEDIUM_INSULA, b->x + 3, b->y + 3, graphicId, populationPerTile, inventoryPerTile);
}

void BuildingHouse_changeTo(int buildingId, int buildingType)
{
	struct Data_Building *b = building_get(buildingId);
	b->type = buildingType;
	b->subtype.houseLevel = b->type - 10;
	int graphicId = image_group(houseGraphicGroup[b->subtype.houseLevel]);
	if (b->houseIsMerged) {
		graphicId += 4;
		if (houseGraphicOffset[b->subtype.houseLevel]) {
			graphicId += 1;
		}
	} else {
		graphicId += houseGraphicOffset[b->subtype.houseLevel];
		graphicId += map_random_get(b->gridOffset) & (houseGraphicNumTypes[b->subtype.houseLevel] - 1);
	}
	Terrain_addBuildingToGrids(buildingId, b->x, b->y, b->size, graphicId, TERRAIN_BUILDING);
}

static void create_vacant_lot(int x, int y, int image_id)
{
    int id = Building_create(BUILDING_HOUSE_VACANT_LOT, x, y);
    struct Data_Building *b = building_get(id);
    b->housePopulation = 0;
    b->distanceFromEntry = 0;
    Terrain_addBuildingToGrids(id, b->x + 1, b->y, 1, image_id, TERRAIN_BUILDING);
}

void BuildingHouse_changeToVacantLot(int buildingId)
{
	struct Data_Building *b = building_get(buildingId);
	b->type = BUILDING_HOUSE_VACANT_LOT;
	b->subtype.houseLevel = b->type - 10;
	int image_id = image_group(GROUP_BUILDING_HOUSE_VACANT_LOT);
	if (b->houseIsMerged) {
		Terrain_removeBuildingFromGrids(buildingId, b->x, b->y);
		b->houseIsMerged = 0;
		b->size = b->houseSize = 1;
		Terrain_addBuildingToGrids(buildingId, b->x, b->y, 1, image_id, TERRAIN_BUILDING);

        create_vacant_lot(b->x + 1, b->y, image_id);
        create_vacant_lot(b->x, b->y + 1, image_id);
        create_vacant_lot(b->x + 1, b->y + 1, image_id);
	} else {
		map_image_set(b->gridOffset, image_id);
	}
}
